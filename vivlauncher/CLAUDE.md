# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```powershell
cd vivlauncher
.\build.ps1 [Debug|Release]       # Default: Release x64
```

Manual: `msbuild vivlauncher\vivlauncher.vcxproj /p:Configuration=Release /p:Platform=x64`

Requires Visual Studio 2022 with MSVC v145 toolset and Windows SDK 10.0.

Output: `vivlauncher\x64\<Config>\vivlauncher.exe`

## Architecture

**vivlauncher** — Windows desktop app (Win32, C++20, x64) that detects installed Vivado versions and launches the selected one with a project file.

### Entry and flow

`vivlauncher.cpp:87 wWinMain` — entry point. Parses CLI args, then branches:
- `--help` / `--list` / `--add` → print to attached console, exit.
- GUI mode → hidden `CreateWindowW` host window, then `GetXprPathFromArgs` (from argv or file-open dialog), then `DetectVivadoInstallations()`, then:
  - 0 found → `AddPathDialog` → retry → error if still none.
  - 1 found → `LaunchVivado` directly.
  - 2+ found → `SelectorDialog` with list.

### Detection (`VivadoDetector.cpp`)

`DetectVivadoInstallations()` iterates **all fixed drives** via `GetLogicalDriveStringsW`, scanning three path patterns per drive:
- `X:\Xilinx\Vivado\<version>\` — old structure
- `X:\Xilinx\<version>\Vivado\` — new structure
- `X:\AMDDesignTools\<version>\` — AMD post-acquisition structure

Plus registry (`HKLM\SOFTWARE\Xilinx\Vivado` and WOW6432Node) and custom paths persisted in `%APPDATA%\vivlauncher\paths.json`.

Each found installation checks for `bin\vivado.bat`. Results deduplicated by `exePath` and sorted descending by version number.

### Launch (`LaunchVivado`)

Prefer `bin\unwrapped\win64.o\vvgl.exe` when launching `.bat` files — resolves vvgl path from the bat's parent directory, passes the bat path as first argument. Uses `ShellExecuteExW`.

### Dialogs (`SelectorDialog.cpp`)

Two dialogs defined in `vivlauncher.rc`:
- `IDD_VERSION_SELECTOR` — ListBox with subclassed WndProc for keyboard (j/k/Enter/Esc/?) and mouse (wheel, middle-click) navigation.
- `IDD_ADD_PATH` — manual path entry with browse button.

Return value from `DialogBoxParamW` encodes 0 = cancel, 1+ = selected index (biased by +1).

### Coding conventions

- Strings: Unicode (`std::wstring`, `L"..."`). Wide Win32 API (`...W` suffixes).
- Indentation: 4 spaces, Allman braces.
- No automated tests. Verify manually with `-l` flag and GUI dialog.
