#include "VivadoDetector.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <shlobj.h>
#include <shellapi.h>

namespace fs = std::filesystem;

static std::wstring GetConfigPath()
{
    wchar_t appData[MAX_PATH];
    if (SHGetSpecialFolderPathW(nullptr, appData, CSIDL_APPDATA, TRUE))
    {
        return std::wstring(appData) + L"\\vivlauncher\\paths.json";
    }
    return {};
}

static std::wstring GetSettingsPath()
{
    auto configPath = GetConfigPath();
    if (configPath.empty())
        return {};
    return (fs::path(configPath).parent_path() / L"settings.ini").native();
}

static std::vector<std::wstring> LoadCustomPaths()
{
    std::vector<std::wstring> paths;
    auto configPath = GetConfigPath();

    if (fs::exists(configPath))
    {
        std::wifstream file(configPath);
        if (file.is_open())
        {
            std::wstring line;
            while (std::getline(file, line))
            {
                line.erase(std::remove_if(line.begin(), line.end(),
                    [](wchar_t c) { return c == L'\r' || c == L'\n' || c == L' ' || c == L'\t'; }), line.end());

                if (!line.empty() && line[0] != L'#')
                {
                    fs::path vivadoBat = fs::path(line) / L"bin\\vivado.bat";
                    if (fs::exists(vivadoBat))
                    {
                        paths.push_back(line);
                    }
                }
            }
        }
    }

    return paths;
}

static fs::path FindVivadoExe(const fs::path& basePath)
{
    fs::path batPath = basePath / L"bin\\vivado.bat";
    if (fs::exists(batPath))
    {
        return batPath;
    }
    return {};
}

static VivadoInstall MakeInstall(const std::wstring& version, const fs::path& installPath,
                                 const fs::path& exePath)
{
    VivadoInstall inst;
    inst.version = version;
    inst.path = installPath.native();
    inst.exePath = exePath.native();
    return inst;
}

static std::vector<VivadoInstall> ScanOldStructure(const fs::path& basePath)
{
    std::vector<VivadoInstall> installs;

    if (!fs::exists(basePath) || !fs::is_directory(basePath))
    {
        return installs;
    }

    fs::path vivadoPath = basePath / L"Vivado";
    if (!fs::exists(vivadoPath) || !fs::is_directory(vivadoPath))
    {
        return installs;
    }

    for (const auto& entry : fs::directory_iterator(vivadoPath))
    {
        if (!entry.is_directory())
        {
            continue;
        }

        std::wstring version = entry.path().filename().native();
        auto exePath = FindVivadoExe(entry.path());

        if (exePath.empty())
        {
            continue;
        }

        installs.push_back(MakeInstall(version, entry.path(), exePath));
    }

    return installs;
}

static std::vector<VivadoInstall> ScanNewStructure(const fs::path& basePath)
{
    std::vector<VivadoInstall> installs;

    if (!fs::exists(basePath) || !fs::is_directory(basePath))
    {
        return installs;
    }

    for (const auto& entry : fs::directory_iterator(basePath))
    {
        if (!entry.is_directory())
        {
            continue;
        }

        std::wstring version = entry.path().filename().native();
        fs::path vivadoSubdir = entry.path() / L"Vivado";

        if (!fs::exists(vivadoSubdir) || !fs::is_directory(vivadoSubdir))
        {
            continue;
        }

        auto exePath = FindVivadoExe(vivadoSubdir);

        if (exePath.empty())
        {
            continue;
        }

        installs.push_back(MakeInstall(version, vivadoSubdir, exePath));
    }

    return installs;
}

static std::vector<VivadoInstall> ScanAMDStructure(const fs::path& basePath)
{
    std::vector<VivadoInstall> installs;

    if (!fs::exists(basePath) || !fs::is_directory(basePath))
    {
        return installs;
    }

    for (const auto& entry : fs::directory_iterator(basePath))
    {
        if (!entry.is_directory())
        {
            continue;
        }

        std::wstring version = entry.path().filename().native();
        // AMD's current installer uses AMDDesignTools\\<version>\\Vivado.
        fs::path installPath = entry.path() / L"Vivado";
        auto exePath = FindVivadoExe(installPath);

        if (exePath.empty())
        {
            continue;
        }

        installs.push_back(MakeInstall(version, installPath, exePath));
    }

    return installs;
}

static std::vector<VivadoInstall> ScanRegistry(HKEY root, PCWSTR subkey)
{
    std::vector<VivadoInstall> installs;
    HKEY hKey = nullptr;

    if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
    {
        return installs;
    }

    wchar_t buffer[512];
    DWORD cbSize = sizeof(buffer);
    DWORD type = REG_SZ;

    if (RegQueryValueExW(hKey, L"InstallPath", nullptr, &type, (LPBYTE)buffer, &cbSize) == ERROR_SUCCESS)
    {
        auto exePath = FindVivadoExe(buffer);

        if (!exePath.empty())
        {
            VivadoInstall inst;
            inst.version = L"Unknown";
            inst.path = buffer;
            inst.exePath = exePath.native();
            installs.push_back(inst);
        }
    }

    RegCloseKey(hKey);
    return installs;
}

static void AddFromPath(const fs::path& basePath, std::vector<VivadoInstall>& installs)
{
    auto exePath = FindVivadoExe(basePath);

    if (!exePath.empty())
    {
        VivadoInstall inst;
        inst.version = basePath.filename().native();
        inst.path = basePath.native();
        inst.exePath = exePath.native();
        installs.push_back(inst);
    }
}

static bool CompareVersions(const VivadoInstall& a, const VivadoInstall& b)
{
    auto parseVersion = [](const std::wstring& v) -> std::tuple<int, int>
    {
        int major = 0, minor = 0;
        if (v == L"Unknown") return {0, 0};
        swscanf_s(v.c_str(), L"%d.%d", &major, &minor);
        return {major, minor};
    };

    auto [amaj, amin] = parseVersion(a.version);
    auto [bmaj, bmin] = parseVersion(b.version);

    if (amaj != bmaj) return amaj > bmaj;
    return amin > bmin;
}

static void Deduplicate(std::vector<VivadoInstall>& installs)
{
    std::sort(installs.begin(), installs.end(), CompareVersions);
    auto it = std::unique(installs.begin(), installs.end(),
        [](const VivadoInstall& a, const VivadoInstall& b) {
            return a.exePath == b.exePath;
        });
    installs.erase(it, installs.end());
}

std::vector<VivadoInstall> DetectVivadoInstallations()
{
    std::vector<VivadoInstall> installs;

    // 1. Iterate through all logical drives
    wchar_t drives[512];
    DWORD length = GetLogicalDriveStringsW(ARRAYSIZE(drives) - 1, drives);
    if (length > 0 && length < ARRAYSIZE(drives))
    {
        wchar_t* drive = drives;
        while (*drive)
        {
            // Only scan local fixed drives to avoid network lag or removable media popups
            if (GetDriveTypeW(drive) == DRIVE_FIXED)
            {
                std::wstring drivePath = drive; // e.g., "C:\"
                
                auto oldStruct = ScanOldStructure(drivePath + L"Xilinx");
                installs.insert(installs.end(), oldStruct.begin(), oldStruct.end());

                auto newStruct = ScanNewStructure(drivePath + L"Xilinx");
                installs.insert(installs.end(), newStruct.begin(), newStruct.end());

                auto amdStruct = ScanAMDStructure(drivePath + L"AMDDesignTools");
                installs.insert(installs.end(), amdStruct.begin(), amdStruct.end());
            }
            drive += wcslen(drive) + 1;
        }
    }

    // 2. Scan Registry
    auto registry = ScanRegistry(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Xilinx\\Vivado");
    installs.insert(installs.end(), registry.begin(), registry.end());

    auto registryWow = ScanRegistry(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Xilinx\\Vivado");
    installs.insert(installs.end(), registryWow.begin(), registryWow.end());

    // 3. Scan Custom Paths
    auto custom = LoadCustomPaths();
    for (const auto& path : custom)
    {
        AddFromPath(path, installs);
    }

    // 4. Uniform deduplication and sorting
    Deduplicate(installs);
    return installs;
}

std::vector<VivadoInstall> AddCustomPath(const wchar_t* path)
{
    auto configPath = GetConfigPath();

    fs::create_directories(fs::path(configPath).parent_path());

    std::wofstream file(configPath, std::ios::app);
    if (file.is_open())
    {
        file << path << L"\n";
    }

    return DetectVivadoInstallations();
}

std::wstring LoadDefaultVersion()
{
    wchar_t value[64] = {};
    auto settingsPath = GetSettingsPath();
    GetPrivateProfileStringW(L"vivlauncher", L"default_version", L"", value,
                             ARRAYSIZE(value), settingsPath.c_str());
    return value;
}

bool SaveDefaultVersion(const std::wstring& version)
{
    auto settingsPath = GetSettingsPath();
    if (settingsPath.empty())
        return false;
    fs::create_directories(fs::path(settingsPath).parent_path());
    return WritePrivateProfileStringW(L"vivlauncher", L"default_version", version.c_str(),
                                      settingsPath.c_str()) != FALSE;
}

std::vector<RecentProject> LoadRecentProjects()
{
    std::vector<RecentProject> projects;
    auto settingsPath = GetSettingsPath();
    if (settingsPath.empty())
        return projects;

    for (int i = 0; i < 20; ++i)
    {
        wchar_t key[32], value[32768] = {};
        swprintf_s(key, L"recent_%d", i);
        GetPrivateProfileStringW(L"recent_projects", key, L"", value,
                                 ARRAYSIZE(value), settingsPath.c_str());
        if (!value[0])
            continue;

        std::wstring item = value;
        size_t separator = item.find(L'|');
        RecentProject project;
        project.version = separator == std::wstring::npos ? L"" : item.substr(0, separator);
        project.path = separator == std::wstring::npos ? item : item.substr(separator + 1);
        if (!project.path.empty() && fs::exists(project.path))
            projects.push_back(std::move(project));
    }
    return projects;
}

void RememberRecentProject(const std::wstring& path, const std::wstring& version)
{
    auto settingsPath = GetSettingsPath();
    if (settingsPath.empty())
        return;
    fs::create_directories(fs::path(settingsPath).parent_path());

    std::vector<RecentProject> projects{{path, version}};
    for (const auto& project : LoadRecentProjects())
    {
        if (_wcsicmp(project.path.c_str(), path.c_str()) != 0 && projects.size() < 20)
            projects.push_back(project);
    }

    for (int i = 0; i < 20; ++i)
    {
        wchar_t key[32];
        swprintf_s(key, L"recent_%d", i);
        if (i < (int)projects.size())
        {
            std::wstring value = projects[i].version + L"|" + projects[i].path;
            WritePrivateProfileStringW(L"recent_projects", key, value.c_str(), settingsPath.c_str());
        }
        else
        {
            WritePrivateProfileStringW(L"recent_projects", key, nullptr, settingsPath.c_str());
        }
    }
}

bool RegisterXprFileAssociation()
{
    wchar_t modulePath[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath));
    if (length == 0 || length >= ARRAYSIZE(modulePath))
        return false;

    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.xpr", 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    const wchar_t* progId = L"VivLauncher.Project";
    RegSetValueExW(key, nullptr, 0, REG_SZ, (const BYTE*)progId,
                   (DWORD)((wcslen(progId) + 1) * sizeof(wchar_t)));
    RegCloseKey(key);

    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Classes\\VivLauncher.Project\\shell\\open\\command",
                        0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    std::wstring command = L"\"" + std::wstring(modulePath) + L"\" \"%1\"";
    RegSetValueExW(key, nullptr, 0, REG_SZ, (const BYTE*)command.c_str(),
                   (DWORD)((command.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

static std::wstring QuoteIfNeeded(const wchar_t* path)
{
    if (wcschr(path, L' ') != nullptr)
        return L'"' + std::wstring(path) + L'"';
    return path;
}

std::wstring GetAbsolutePath(const std::wstring& path)
{
    if (path.empty())
        return {};

    DWORD capacity = MAX_PATH;
    for (;;)
    {
        std::wstring result(capacity, L'\0');
        DWORD length = GetFullPathNameW(path.c_str(), capacity, result.data(), nullptr);
        if (length == 0)
            return path;
        if (length < capacity - 1)
        {
            result.resize(length);
            return result;
        }
        capacity = length + 1;
    }
}

bool LaunchVivado(const wchar_t* vivadoExePath, const wchar_t* xprFilePath)
{
    std::wstring exeToLaunch = vivadoExePath;
    std::wstring cmdLine;

    std::wstring ext = fs::path(exeToLaunch).extension().native();
    std::transform(ext.begin(), ext.end(), ext.begin(), towlower);

    bool useVvgl = false;

    if (ext == L".bat" || ext == L".cmd")
    {
        fs::path vvglPath = fs::path(exeToLaunch).parent_path() / L"unwrapped\\win64.o\\vvgl.exe";
        if (fs::exists(vvglPath))
        {
            exeToLaunch = vvglPath.native();
            useVvgl = true;
        }
    }

    // argv[0]: application path itself (standard convention)
    cmdLine = QuoteIfNeeded(exeToLaunch.c_str());

    if (useVvgl)
    {
        cmdLine += L" ";
        cmdLine += QuoteIfNeeded(vivadoExePath);
    }

    if (xprFilePath && *xprFilePath)
    {
        cmdLine += L" ";
        cmdLine += QuoteIfNeeded(xprFilePath);
    }

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWDEFAULT;

    PROCESS_INFORMATION pi = {};

    std::wstring workingDirectory;
    if (xprFilePath && *xprFilePath)
        workingDirectory = fs::path(xprFilePath).parent_path().native();

    BOOL ok = CreateProcessW(
        exeToLaunch.c_str(),            // lpApplicationName — direct file path, Unicode-safe, no parsing
        cmdLine.data(),                 // lpCommandLine — argv for child process
        nullptr, nullptr,
        FALSE,
        0,
        nullptr,
        workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
        &si, &pi);

    if (ok)
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }

    return false;
}

CliOptions ParseCliArgs(int argc, wchar_t* argv[])
{
    CliOptions opts;

    for (int i = 1; i < argc; ++i)
    {
        std::wstring arg = argv[i];

        if (arg == L"-l" || arg == L"--list" || arg == L"list")
        {
            opts.listVersions = true;
        }
        else if (arg == L"-a" || arg == L"--add" || arg == L"add")
        {
            opts.addPath = true;
            if (i + 1 < argc)
            {
                opts.customPath = argv[++i];
            }
        }
        else if (arg == L"-h" || arg == L"--help" || arg == L"help")
        {
            opts.showHelp = true;
        }
        else if (arg[0] != L'-')
        {
            std::transform(arg.begin(), arg.end(), arg.begin(), towlower);
            if (arg.size() >= 4 && arg.compare(arg.size() - 4, 4, L".xpr") == 0)
            {
                opts.xprFile = argv[i];
            }
        }
    }

    return opts;
}
