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

struct CliOptions
{
    bool listVersions = false;
    bool addPath = false;
    std::wstring xprFile;
    std::wstring customPath;
};

CliOptions ParseCliArgs(int argc, wchar_t* argv[]);
std::vector<VivadoInstall> DetectVivadoInstallations();
std::vector<VivadoInstall> AddCustomPath(const wchar_t* path);
bool LaunchVivado(const wchar_t* vivadoExePath, const wchar_t* xprFilePath);