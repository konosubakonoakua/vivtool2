#pragma once

#include <windows.h>
#include <string>
#include <vector>

struct VivadoInstall
{
    std::wstring version;
    std::wstring path;
    std::wstring exePath;
};

struct RecentProject
{
    std::wstring path;
    std::wstring version;
};

struct CliOptions
{
    bool listVersions = false;
    bool addPath = false;
    bool showHelp = false;
    std::wstring xprFile;
    std::wstring customPath;
};

CliOptions ParseCliArgs(int argc, wchar_t* argv[]);
std::vector<VivadoInstall> DetectVivadoInstallations();
std::vector<VivadoInstall> AddCustomPath(const wchar_t* path);
std::wstring GetAbsolutePath(const std::wstring& path);
std::wstring LoadDefaultVersion();
bool SaveDefaultVersion(const std::wstring& version);
std::vector<RecentProject> LoadRecentProjects();
void RememberRecentProject(const std::wstring& path, const std::wstring& version);
bool RegisterXprFileAssociation();
bool LaunchVivado(const wchar_t* vivadoExePath, const wchar_t* xprFilePath);
