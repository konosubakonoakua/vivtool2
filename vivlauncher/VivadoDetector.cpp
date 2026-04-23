#include "VivadoDetector.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <shlobj.h>

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

        VivadoInstall inst;
        inst.version = version;
        inst.path = entry.path().native();
        inst.exePath = exePath.native();
        installs.push_back(inst);
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

        VivadoInstall inst;
        inst.version = version;
        inst.path = vivadoSubdir.native();
        inst.exePath = exePath.native();
        installs.push_back(inst);
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
        auto exePath = FindVivadoExe(entry.path());

        if (exePath.empty())
        {
            continue;
        }

        VivadoInstall inst;
        inst.version = version;
        inst.path = entry.path().native();
        inst.exePath = exePath.native();
        installs.push_back(inst);
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

bool LaunchVivado(const wchar_t* vivadoExePath, const wchar_t* xprFilePath)
{
    std::wstring exePath = vivadoExePath;
    std::wstring params;

    std::wstring ext = fs::path(exePath).extension().native();
    std::transform(ext.begin(), ext.end(), ext.begin(), towlower);

    if (ext == L".bat" || ext == L".cmd")
    {
        fs::path vvglPath = fs::path(exePath).parent_path() / L"unwrapped\\win64.o\\vvgl.exe";
        if (fs::exists(vvglPath))
        {
            exePath = vvglPath.native();
            // When using vvgl.exe, the first parameter must be the path to vivado.bat
            // Note: vvgl.exe may misinterpret double quotes in lpParameters.
            params = vivadoExePath;
        }
    }

    if (xprFilePath && *xprFilePath)
    {
        if (!params.empty())
        {
            params += L" ";
        }
        params += xprFilePath;
    }

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"open";
    sei.lpFile = exePath.c_str();
    sei.lpParameters = params.empty() ? nullptr : params.c_str();
    sei.nShow = SW_SHOWDEFAULT;

    if (ShellExecuteExW(&sei) && (size_t)sei.hInstApp > 32)
    {
        if (sei.hProcess)
        {
            CloseHandle(sei.hProcess);
        }
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