# vivlauncher

A Windows desktop application for launching Xilinx Vivado with automatic version detection and selection.

## Features

- Automatically detects installed Vivado versions from multiple locations:
  - `C:\Xilinx` (legacy structure)
  - `C:\Xilinx` (newer structure)
  - `C:\AMDDesignTools`
  - Windows Registry
  - Custom user-added paths
- Version selection dialog when multiple installations are found
- CLI options for listing installations and adding custom paths
- Remembers user-added custom paths between sessions

## Usage

### GUI Mode

Double-click `vivlauncher.exe` or pass a project file as argument:

```
vivlauncher.exe project.xpr
```

If no project file is provided, a file open dialog will appear to select a `.xpr` file.

When multiple Vivado versions are detected, a selection dialog lets you choose which version to launch.

### CLI Options

```
vivlauncher.exe -l, --list        List all detected Vivado versions
vivlauncher.exe -a, --add <path>  Add custom installation path
vivlauncher.exe -h, --help        Show help message
```

## Building

### Prerequisites

- Windows
- Visual Studio 2022 with MSVC toolset v145
- Windows SDK 10.0

### Build Commands

**Visual Studio:**
Open `vivlauncher/vivlauncher.slnx` and build `Debug|x64` or `Release|x64`.

**Command Line:**
```batch
msbuild vivlauncher\vivlauncher.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Output: `vivlauncher\x64\Debug\vivlauncher.exe` or `vivlauncher\x64\Release\vivlauncher.exe`

## Technical Details

- Language: C++20
- Framework: Win32 (native Windows API)
- UI: Native Win32 dialogs
- Project structure follows standard Visual Studio Win32 template