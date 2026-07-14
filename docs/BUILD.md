# Build from Source (Windows)

This project is standardized around a single primary build flow on Windows:

1. MSVC 2022 toolchain
2. Qt 6.x (`msvc2022_64` kit)
3. CMake + Ninja
4. vcpkg manifest mode

## Quick Start

```powershell
git clone https://github.com/dduongtrandai/LA-Studio.git
cd LA-Studio
.\scripts\bootstrap.bat
```

After a successful build, executable output is:

`out/build/windows-msvc-release/LA Studio.exe`

## Prerequisites

Install these before running bootstrap:

1. Visual Studio 2022 (or Build Tools) with MSVC x64 toolchain
2. Qt 6.5+ with `msvc2022_64` kit
3. CMake 3.21+
4. Ninja
5. Git

## Bootstrap Behavior

`scripts/bootstrap.ps1` performs:

1. Tool checks (`git`, `cmake`, `ninja`)
2. Qt detection from `-QtRoot`, `LA_QT`, or common `C:\Qt\...` paths
3. vcpkg detection from `-VcpkgRoot`, `VCPKG_ROOT`, common paths, or local clone to `.deps/vcpkg`
4. Build execution via `scripts/build.ps1`

## Common Commands

Default release build:

```powershell
.\scripts\bootstrap.bat
```

Faster development build (skip deployment step):

```powershell
.\scripts\bootstrap.bat -SkipDeploy
```

Clean rebuild:

```powershell
.\scripts\bootstrap.bat -Clean
```

Explicit Qt path:

```powershell
.\scripts\bootstrap.bat -QtRoot C:\Qt\6.9.3
```

Use MinGW preset (advanced path):

```powershell
.\scripts\bootstrap.bat -Preset windows-mingw-release -QtRoot C:\Qt\6.9.3
```

## CMake Presets

Primary presets:

1. `windows-msvc-release`
2. `windows-msvc-debug`
3. `windows-mingw-release`

Legacy aliases (`x64-release`, `x64-debug`, `mingw-release`) are retained for compatibility only.

`Debug` builds keep the console attached for developer logging. All non-Debug Windows builds are packaged as GUI apps, so the terminal is hidden when `LA Studio.exe` opens.

## Incremental Build and Build Speed

By default, CMake + Ninja already performs incremental builds. If you only change a few source files, only affected targets should rebuild.

If your build feels slow during daily development, the most common reason is deployment work after compile (for example, `windeployqt`), not full recompilation.

Recommended workflow:

1. Use `.\scripts\bootstrap.bat -SkipDeploy` while coding to reduce build time.
2. Run `.\scripts\bootstrap.bat` (without `-SkipDeploy`) before packaging, sharing binaries, or validating runtime dependencies.
3. Use `-Clean` only when you really need a full rebuild (toolchain change, cache corruption, major dependency switch).

## Notes

1. `CMakeUserPresets.json` is optional and user-local; it is not required for the official build path.
2. CI and local development should use `scripts/bootstrap.ps1` or `scripts/build.ps1` with explicit arguments.
