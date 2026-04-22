# Repository Guidelines

## Project Structure & Module Organization

- `vivlauncher/`: Visual Studio C++ Win32 launcher application.
- Source: `vivlauncher/vivlauncher.cpp`
- Headers: `vivlauncher/framework.h`, `vivlauncher/vivlauncher.h`, `vivlauncher/targetver.h`
- Resources: `vivlauncher/vivlauncher.rc`, icons (`vivlauncher/*.ico`), resource IDs in `vivlauncher/Resource.h`
- Generated output (do not edit by hand): `vivlauncher/.vs/`, `vivlauncher/x64/`, `vivlauncher/vivlauncher/x64/`

## Build, Test, and Development Commands

Prereqs: Windows, Visual Studio 2022 (or Build Tools) with MSVC toolset `v145` and Windows SDK `10.0`.

- Visual Studio: open `vivlauncher/vivlauncher.slnx`, then build `Debug|x64` or `Release|x64`.
- Command line build:
  - `msbuild vivlauncher/vivlauncher.vcxproj /p:Configuration=Debug /p:Platform=x64`
  - `msbuild vivlauncher/vivlauncher.vcxproj /t:Clean /p:Configuration=Debug /p:Platform=x64`
- Output binaries typically land in `vivlauncher/x64/<Config>/` (e.g. `vivlauncher/x64/Debug/vivlauncher.exe`).

## Coding Style & Naming Conventions

- Language/settings: C++20, Unicode Win32 (`wWinMain`, `WCHAR`, `LPWSTR`).
- Keep the existing Visual Studio template formatting: 4-space indents; braces on their own line.
- Resource/ID naming follows Win32 conventions (`IDS_*`, `IDC_*`, `IDM_*`, `IDI_*`). Keep them consistent when adding new menu items, dialogs, or strings.

## Testing Guidelines

- No automated test suite is included. Validate changes by building `Debug|x64` and doing a quick smoke test (launch, open the About dialog, and exit cleanly).

## Commit & Pull Request Guidelines

- This workspace snapshot does not include `.git`, so commit-message conventions can’t be inferred from history. Use a simple, consistent pattern such as `type: summary` (e.g. `fix: improve window initialization`).
- PRs should include: what changed, which config/platform you built (`Debug|x64`, `Release|x64`), and screenshots when modifying UI/resources.
- Avoid committing generated artifacts; prefer ignoring `.vs/`, `x64/`, and build products like `*.pdb`, `*.tlog`, `*.ilk`, `*.idb`, `*.log`, `*.suo`, `*.user`.

## Agent-Specific Notes

- When adding/removing files, update `vivlauncher/vivlauncher.vcxproj` (and `vivlauncher/vivlauncher.vcxproj.filters` for Visual Studio filtering). For new resources, also update `vivlauncher/vivlauncher.rc` and `vivlauncher/Resource.h`.
