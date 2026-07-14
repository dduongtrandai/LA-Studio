#Requires -Version 5.1

<#
.SYNOPSIS
    Build, stage, and package LA Studio into a Windows installer.
.DESCRIPTION
    Compiles the project, installs files to out/stage, deploys Qt dependencies,
    and runs Inno Setup to generate a single-file EXE installer.
#>

[CmdletBinding()]
param(
    [string] $Preset = "windows-msvc-release",
    [string] $QtRoot,
    [string] $VcpkgRoot,
    [string] $Version,
    [switch] $SkipInstaller
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

. (Join-Path $PSScriptRoot "cmake_helpers.ps1")

# Helper: Test if command exists
function Test-Command {
    param([string] $Name)
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Add-PathIfExists {
    param([string] $PathEntry)
    if (-not [string]::IsNullOrWhiteSpace($PathEntry) -and (Test-Path $PathEntry)) {
        $parts = $env:PATH -split ";"
        if ($parts -notcontains $PathEntry) {
            $env:PATH = "$PathEntry;$env:PATH"
        }
    }
}

function Ensure-Command {
    param(
        [string] $Name,
        [string[]] $FallbackPaths
    )
    if (Test-Command $Name) { return }
    foreach ($candidate in $FallbackPaths) {
        Add-PathIfExists -PathEntry $candidate
        if (Test-Command $Name) { return }
    }
    throw "$Name is required but was not found in PATH."
}

function Ensure-MsvcEnvironment {
    if (Test-Command "cl.exe") { return }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "cl.exe not found in PATH and vswhere.exe was not found. Install Visual Studio 2022 C++ workload."
    }
    $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($installPath)) {
        throw "cl.exe not found in PATH and Visual Studio with C++ tools was not detected."
    }
    $vcvars = Join-Path $installPath "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path -LiteralPath $vcvars)) {
        throw "cl.exe not found in PATH and vcvars64.bat was not found at '$vcvars'."
    }
    Write-Host ">> Initializing MSVC toolchain environment..." -ForegroundColor Cyan
    $envDump = & cmd.exe /d /c "`"$vcvars`" >nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to initialize MSVC environment from '$vcvars'."
    }
    foreach ($line in $envDump) {
        $idx = $line.IndexOf("=")
        if ($idx -gt 0) {
            $name = $line.Substring(0, $idx)
            $value = $line.Substring($idx + 1)
            Set-Item -Path "Env:$name" -Value $value
        }
    }
    if (-not (Test-Command "cl.exe")) {
        throw "MSVC environment was initialized but cl.exe is still unavailable."
    }
}

function Get-SourceAppVersion {
    $cmakePath = Join-Path $RepoRoot "CMakeLists.txt"
    $match = Select-String -LiteralPath $cmakePath -Pattern 'set\(LASTUDIO_VERSION\s+"([^"]+)"' | Select-Object -First 1
    if ($null -eq $match) {
        throw "Could not find LASTUDIO_VERSION in $cmakePath."
    }
    return $match.Matches[0].Groups[1].Value
}

function Normalize-AppVersion {
    param([string] $Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        $Value = Get-SourceAppVersion
    } else {
        $Value = $Value.Trim()
        if ($Value.StartsWith("v")) {
            $Value = $Value.Substring(1)
        }
    }
    if ($Value -notmatch '^\d+\.\d+\.\d+$') {
        throw "Version must use MAJOR.MINOR.PATCH format; got '$Value'."
    }
    return $Value
}



# Helper: Find Qt path
function Resolve-QtRoot {
    param([string] $Candidate)
    if (-not [string]::IsNullOrWhiteSpace($Candidate) -and (Test-Path -LiteralPath $Candidate)) { return $Candidate.Trim('"') }
    if (-not [string]::IsNullOrWhiteSpace($env:LA_QT) -and (Test-Path -LiteralPath $env:LA_QT)) { return $env:LA_QT }
    if (Test-Path "C:\Qt") {
        $latestQtRoot = Get-ChildItem "C:\Qt" -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } |
            Sort-Object { [version]$_.Name } -Descending |
            Select-Object -First 1
        if ($latestQtRoot) { return $latestQtRoot.FullName }
    }
    return $null
}

# Helper: Find vcpkg path
function Resolve-VcpkgRoot {
    param([string] $Candidate)
    if (-not [string]::IsNullOrWhiteSpace($Candidate)) { return $Candidate.Trim('"') }
    if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) { return $env:VCPKG_ROOT }
    $knownRoots = @(
        (Join-Path $RepoRoot ".deps\vcpkg"),
        "C:\vcpkg",
        "D:\vcpkg",
        "E:\dev\vcpkg",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\vcpkg",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\vcpkg",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\vcpkg"
    )
    foreach ($root in $knownRoots) {
        if (Test-Path (Join-Path $root "scripts\buildsystems\vcpkg.cmake")) { return $root }
    }
    return $null
}

# Helper: Find Inno Setup ISCC compiler
function Find-Iscc {
    if (Test-Command "iscc") { return "iscc" }
    $paths = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"),
        "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
        "C:\Program Files\Inno Setup 6\ISCC.exe",
        "C:\Program Files (x86)\Inno Setup 5\ISCC.exe",
        "C:\Program Files\Inno Setup 5\ISCC.exe"
    )
    foreach ($path in $paths) {
        if (Test-Path $path) { return $path }
    }
    return $null
}

function Test-CatalogRequiresWebp {
    $catalogPath = Join-Path $RepoRoot "data\catalog.json"
    if (-not (Test-Path -LiteralPath $catalogPath)) {
        return $false
    }
    return [bool](Select-String -LiteralPath $catalogPath -Pattern '"mimeType"\s*:\s*"image/webp"' -Quiet)
}

function Ensure-WebpImageFormatPlugin {
    param(
        [string] $QtPrefixPath,
        [string] $DeployRoot
    )

    if (-not (Test-CatalogRequiresWebp)) {
        return
    }

    $pluginName = "qwebp.dll"
    $sourcePlugin = Join-Path $QtPrefixPath "plugins\imageformats\$pluginName"
    $targetDir = Join-Path $DeployRoot "imageformats"
    $targetPlugin = Join-Path $targetDir $pluginName

    if (-not (Test-Path -LiteralPath $targetPlugin)) {
        if (-not (Test-Path -LiteralPath $sourcePlugin)) {
            throw "Catalog contains WebP thumbnails, but Qt WebP image plugin was not found at '$sourcePlugin'. Install the Qt Image Formats module (qtimageformats) for this Qt kit."
        }
        New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
        Copy-Item -LiteralPath $sourcePlugin -Destination $targetPlugin -Force
        Write-Host ">> Deployed Qt WebP image plugin: $targetPlugin" -ForegroundColor Cyan
    }

    if (-not (Test-Path -LiteralPath $targetPlugin)) {
        throw "Catalog contains WebP thumbnails, but '$targetPlugin' was not deployed."
    }
}

function Ensure-ArchiveExtractor {
    param(
        [string] $DeployRoot,
        [string] $VcpkgRoot
    )
    $target = Join-Path $DeployRoot "7z.exe"
    if (Test-Path -LiteralPath $target) { return }

    $source = $null
    if (Test-Command "7z.exe") {
        $source = (Get-Command "7z.exe").Source
    }

    if ([string]::IsNullOrWhiteSpace($source)) {
        $toolRoots = @(
            (Join-Path $VcpkgRoot "downloads\tools"),
            (Join-Path $RepoRoot ".deps\vcpkg\downloads\tools")
        ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique

        foreach ($toolRoot in $toolRoots) {
            if (-not (Test-Path -LiteralPath $toolRoot)) { continue }
            $candidate = Get-ChildItem -Path $toolRoot -Filter "7z.exe" -Recurse -File -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($null -ne $candidate) {
                $source = $candidate.FullName
                break
            }
        }
    }

    if ($null -eq $source) {
        throw "7z.exe is required to extract .tar.bz2 runtime packages on Windows. Install 7-Zip or provide the vcpkg tools cache."
    }

    Copy-Item -LiteralPath $source -Destination $target -Force
    $sevenZipDll = Join-Path (Split-Path -Parent $source) "7z.dll"
    if (Test-Path -LiteralPath $sevenZipDll) {
        Copy-Item -LiteralPath $sevenZipDll -Destination (Join-Path $DeployRoot "7z.dll") -Force
    }
}

function Copy-VcpkgRuntimeLibraries {
    param(
        [string] $BuildDirectory,
        [string] $Triplet,
        [string] $DeployDirectory
    )

    # vcpkg manifest mode places dynamic runtime dependencies in the build tree.
    # windeployqt deploys Qt libraries only, so these DLLs must be staged explicitly.
    $vcpkgBinDirectory = Join-Path $BuildDirectory "vcpkg_installed\\$Triplet\\bin"
    if (-not (Test-Path -LiteralPath $vcpkgBinDirectory)) {
        throw "vcpkg runtime directory was not found: $vcpkgBinDirectory"
    }

    $runtimeLibraries = Get-ChildItem -LiteralPath $vcpkgBinDirectory -Filter "*.dll" -File
    if ($runtimeLibraries.Count -eq 0) {
        throw "No vcpkg runtime DLLs were found in: $vcpkgBinDirectory"
    }

    foreach ($library in $runtimeLibraries) {
        Copy-Item -LiteralPath $library.FullName -Destination (Join-Path $DeployDirectory $library.Name) -Force
    }

    foreach ($requiredLibrary in @("libcurl.dll", "zlib1.dll")) {
        $stagedLibrary = Join-Path $DeployDirectory $requiredLibrary
        if (-not (Test-Path -LiteralPath $stagedLibrary)) {
            throw "Required runtime DLL was not staged: $stagedLibrary"
        }
    }
}

Ensure-Command -Name "cmake" -FallbackPaths @(
    "C:\Qt\Tools\CMake_64\bin",
    "C:\Program Files\CMake\bin"
)
Ensure-Command -Name "ninja" -FallbackPaths @(
    "C:\Qt\Tools\Ninja",
    "C:\Program Files\Ninja"
)
if ($Preset -like "*mingw*") {
    Add-PathIfExists -PathEntry "C:\Qt\Tools\mingw1310_64\bin"
} else {
    Ensure-MsvcEnvironment
}

# 1. Resolve build dependencies
$QtRoot = Resolve-QtRoot -Candidate $QtRoot
$VcpkgRoot = Resolve-VcpkgRoot -Candidate $VcpkgRoot
$Version = Normalize-AppVersion -Value $Version
$kitName = if ($Preset -like "*mingw*") { "mingw_64" } else { "msvc2022_64" }

if ([string]::IsNullOrWhiteSpace($QtRoot)) {
    throw "Qt root not found. Pass -QtRoot or set LA_QT environment variable."
}
if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    throw "vcpkg root not found. Pass -VcpkgRoot or set VCPKG_ROOT environment variable."
}

$qtPrefixPath = Join-Path $QtRoot $kitName
$windeployqt = Join-Path $qtPrefixPath "bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    throw "windeployqt not found at: $windeployqt"
}

# 2. Setup folders
$stageDir = Join-Path $RepoRoot "out\stage"
if (Test-Path $stageDir) {
    Write-Host ">> Cleaning old staging directory..." -ForegroundColor Cyan
    Remove-Item $stageDir -Recurse -Force -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null

# 3. Configure, Build and Install via CMake
Write-Host ">> Configuring CMake..." -ForegroundColor Cyan
$toolchainFile = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
$buildDir = Join-Path $RepoRoot "out\build\$Preset"
Remove-StaleCMakeBuildDirectory -BuildDirectory $buildDir -ExpectedSourceDirectory $RepoRoot

$cmakeArgs = @(
    "--preset", $Preset,
    "-DCMAKE_INSTALL_PREFIX=$($stageDir.Replace('\', '/'))",
    "-DCMAKE_PREFIX_PATH=$($qtPrefixPath.Replace('\', '/'))",
    "-DCMAKE_TOOLCHAIN_FILE=$($toolchainFile.Replace('\', '/'))",
    "-DVCPKG_ROOT=$($VcpkgRoot.Replace('\', '/'))"
)
if ($Preset -like "*mingw*") {
    $vcpkgTriplet = "x64-mingw-dynamic"
} else {
    $vcpkgTriplet = "x64-windows"
}
$cmakeArgs += "-DVCPKG_TARGET_TRIPLET=$vcpkgTriplet"
$cmakeArgs += "-DLASTUDIO_VERSION=$Version"

$env:VCPKG_ROOT = $VcpkgRoot
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }

Write-Host ">> Building application..." -ForegroundColor Cyan
& cmake --build --preset $Preset --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

Write-Host ">> Installing to staging folder..." -ForegroundColor Cyan
& cmake --install $buildDir
if ($LASTEXITCODE -ne 0) { throw "CMake install failed." }

# 4. Deploy Qt libraries and DLLs
$stagedExe = Join-Path $stageDir "bin\LA Studio.exe"
if (-not (Test-Path $stagedExe)) {
    throw "Staged executable not found at: $stagedExe"
}

Write-Host ">> Running windeployqt to deploy runtime dependencies..." -ForegroundColor Cyan
& $windeployqt --verbose 0 --qmldir qml --no-translations --compiler-runtime $stagedExe
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed." }
Ensure-WebpImageFormatPlugin -QtPrefixPath $qtPrefixPath -DeployRoot (Split-Path -Parent $stagedExe)
Write-Host ">> Deploying vcpkg runtime DLLs..." -ForegroundColor Cyan
Copy-VcpkgRuntimeLibraries -BuildDirectory $buildDir -Triplet $vcpkgTriplet -DeployDirectory (Split-Path -Parent $stagedExe)
Ensure-ArchiveExtractor -DeployRoot (Split-Path -Parent $stagedExe) -VcpkgRoot $VcpkgRoot

# 5. Build installer using Inno Setup
if ($SkipInstaller) {
    Write-Host "[SUCCESS] Application staged successfully at: $stageDir" -ForegroundColor Green
    exit 0
}

$isccPath = Find-Iscc
if ($null -eq $isccPath) {
    Write-Warning "Inno Setup Compiler (ISCC.exe) was not found."
    Write-Warning "The application has been successfully built and staged at: $stageDir"
    Write-Warning "To package it into an installer, please install Inno Setup 6 (https://jrsoftware.org/isdownload.php) and run: ISCC.exe $buildDir\installer.iss"
    exit 0
}

Write-Host ">> Compiling installer using Inno Setup..." -ForegroundColor Cyan
$installerScript = Join-Path $buildDir "installer.iss"
if (-not (Test-Path $installerScript)) {
    throw "Generated installer script not found at: $installerScript"
}
& $isccPath $installerScript
if ($LASTEXITCODE -ne 0) { throw "Installer compilation failed." }

$installerPath = Join-Path $RepoRoot "out\LA-Studio-Setup.exe"
Write-Host "[SUCCESS] Installer generated at: $installerPath" -ForegroundColor Green
