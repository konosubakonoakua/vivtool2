# GEMINI.md - vivtool2

## Project Overview

**vivtool2** is a Windows desktop application (`vivlauncher`) designed to streamline the process of launching Xilinx Vivado. It features automatic version detection from common installation paths and the Windows Registry, providing a selection dialog when multiple versions are found.

- **Primary Language:** C++20
- **Framework:** Native Win32 API
- **Target Platform:** Windows (x64)
- **IDE:** Visual Studio 2022

### Architecture
The project is structured as a single Visual Studio solution containing the `vivlauncher` project.
- `vivlauncher.cpp`: Entry point (`wWinMain`) and CLI argument parsing.
- `VivadoDetector.cpp/h`: Logic for scanning directories and the registry for Vivado installations.
- `SelectorDialog.cpp/h`: Win32 dialog-based UI for version selection.
- `vivlauncher.rc`: Resource definitions (dialogs, icons, strings).

## Building and Running

### Prerequisites
- Windows 10/11
- Visual Studio 2022 with **MSVC v145** toolset
- Windows SDK 10.0

### Build Commands

**Visual Studio:**
1. Open `vivlauncher/vivlauncher.slnx`.
2. Select `Debug` or `Release` configuration and `x64` platform.
3. Build Solution (F7).

**PowerShell Script (Recommended):**
```powershell
cd vivlauncher
.\build.ps1 [Debug|Release]
```
By default, it builds the `Release` configuration for `x64`.

**Manual Command Line (MSBuild):**
```batch
msbuild vivlauncher\vivlauncher.vcxproj /p:Configuration=Release /p:Platform=x64
```

### Running
The application can be run in GUI or CLI mode.
- **GUI:** `vivlauncher.exe [project.xpr]`
- **CLI:**
  - `vivlauncher.exe -l` (List detected versions)
  - `vivlauncher.exe -a <path>` (Add custom installation path)

Output binaries are located in `vivlauncher/x64/<Configuration>/`.

## Development Conventions

### Coding Style
- **Indentation:** 4 spaces.
- **Braces:** Allman style (braces on their own line).
- **Naming:** Follows standard Win32/Visual Studio template conventions (`InitInstance`, `WndProc`).
- **Strings:** Use Unicode (`WCHAR`, `std::wstring`, `L"string"` literals) throughout.

### UI & Resources
- Use `Resource.h` for ID management (`IDD_*`, `IDC_*`, `IDI_*`).
- Native Win32 dialogs are defined in `vivlauncher.rc`. Use the Visual Studio Resource Editor for modifications when possible.

### Testing
- No automated test framework is present.
- **Manual Verification:** Build in `Debug|x64`, verify CLI flags (`-l`), and test the version selection dialog by ensuring it detects local Vivado installations or custom paths.

## Key Files
- `vivlauncher/vivlauncher.cpp`: Application lifecycle and CLI integration.
- `vivlauncher/VivadoDetector.cpp`: Core logic for finding Xilinx installations.
- `vivlauncher/SelectorDialog.cpp`: Implementation of the version selection UI.
- `vivlauncher/vivlauncher.vcxproj`: Project configuration and build settings.
