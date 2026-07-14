#Requires -Version 5.1

<#
.SYNOPSIS
    Build and run unit tests for LA Studio.

.DESCRIPTION
    Resolves dependencies (Qt, vcpkg), compiles the unit tests target, and runs the tests.
#>

param(
    [string] $Preset = "windows-msvc-release",
    [string] $QtRoot,
    [string] $VcpkgRoot,
    [switch] $NoBuild,
    [switch] $Verbose
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

. (Join-Path $PSScriptRoot "cmake_helpers.ps1")

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

    if (Test-Command $Name) {
        return
    }

    foreach ($candidate in $FallbackPaths) {
        Add-PathIfExists -PathEntry $candidate
        if (Test-Command $Name) {
            return
        }
    }

    throw "$Name is required but was not found in PATH."
}

function Resolve-QtRoot {
    param([string] $Candidate, [string] $BuildPreset)

    $kit = if ($BuildPreset -like "*mingw*") { "mingw_64" } else { "msvc2022_64" }
    $options = @()
    if (-not [string]::IsNullOrWhiteSpace($Candidate)) { $options += $Candidate.Trim('"') }
    if (-not [string]::IsNullOrWhiteSpace($env:LA_QT)) { $options += $env:LA_QT }
    if (Test-Path "C:\Qt") {
        $options += Get-ChildItem "C:\Qt" -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } |
            Sort-Object { [version]$_.Name } -Descending |
            ForEach-Object { $_.FullName }
    }
    $options += @("C:\Qt\6.9.3", "C:\Qt\6.9.1", "C:\Qt\6.8.3", "C:\Qt\6.8.2", "C:\Qt\6.8.1", "C:\Qt\6.8.0")

    foreach ($root in $options | Select-Object -Unique) {
        if (Test-Path (Join-Path $root "$kit\lib\cmake\Qt6\Qt6Config.cmake")) {
            return $root
        }
    }
    return $null
}

function Resolve-VcpkgRoot {
    param([string] $Candidate)

    $options = @()
    if (-not [string]::IsNullOrWhiteSpace($Candidate)) { $options += $Candidate.Trim('"') }
    $options += @(
        (Join-Path $RepoRoot ".deps\vcpkg")
    )
    if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) { $options += $env:VCPKG_ROOT }
    $options += @(
        "C:\vcpkg",
        "D:\vcpkg",
        "E:\dev\vcpkg",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\vcpkg",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\vcpkg",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\vcpkg"
    )

    foreach ($root in $options | Select-Object -Unique) {
        if (Test-Path (Join-Path $root "scripts\buildsystems\vcpkg.cmake")) {
            return $root
        }
    }
    return $null
}

function Ensure-MsvcEnvironment {
    if (Test-Command "cl.exe") {
        return
    }

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

    Write-Host ">> Initializing MSVC toolchain environment" -ForegroundColor Cyan
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

# Resolve and ensure commands
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

$resolvedQtRoot = Resolve-QtRoot -Candidate $QtRoot -BuildPreset $Preset
if ([string]::IsNullOrWhiteSpace($resolvedQtRoot)) {
    throw "Qt root not detected for preset '$Preset'. Pass -QtRoot or set LA_QT."
}

$kit = if ($Preset -like "*mingw*") { "mingw_64" } else { "msvc2022_64" }
$qtBin = Join-Path $resolvedQtRoot "$kit\bin"
Add-PathIfExists -PathEntry $qtBin

$buildDir = Join-Path $RepoRoot "out\build\$Preset"
Remove-StaleCMakeBuildDirectory -BuildDirectory $buildDir -ExpectedSourceDirectory $RepoRoot

# 1. Build unit tests if requested
if (-not $NoBuild) {
    Write-Host ">> Building unit tests target..." -ForegroundColor Cyan
    # Check if build directory exists and is configured.
    # If not configured, we bootstrap first.
    if (-not (Test-Path (Join-Path $buildDir "build.ninja"))) {
        Write-Host ">> Build directory not configured. Bootstrapping first..." -ForegroundColor Yellow
        $bootstrapScript = Join-Path $PSScriptRoot "bootstrap.ps1"
        $resolvedVcpkgRoot = Resolve-VcpkgRoot -Candidate $VcpkgRoot
        & $bootstrapScript -Preset $Preset -QtRoot $resolvedQtRoot -VcpkgRoot $resolvedVcpkgRoot
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    } else {
        # Build just the unit tests target
        cmake --build $buildDir --target LAStudioUnitTests --parallel
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
}

# 2. Run the unit tests
$exePath = Join-Path $buildDir "LAStudioUnitTests.exe"
if (-not (Test-Path -LiteralPath $exePath)) {
    throw "Tests binary not found: $exePath. Please build first."
}

Write-Host ">> Running unit tests..." -ForegroundColor Cyan
$testArgs = @()
if ($Verbose) {
    $testArgs += "-v2"
}
if ($args) {
    $testArgs += $args
}

& $exePath @testArgs
exit $LASTEXITCODE
