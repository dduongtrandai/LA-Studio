#Requires -Version 5.1

<#
.SYNOPSIS
    One-command bootstrap for building LA Studio on Windows.

.DESCRIPTION
    Validates required tools, resolves Qt/vcpkg paths, and calls scripts/build.ps1.
#>

[CmdletBinding()]
param(
    [string] $Preset = "windows-msvc-release",
    [string] $QtRoot,
    [string] $VcpkgRoot,
    [string] $Version,
    [switch] $AllowMingwFallback,
    [switch] $Clean,
    [switch] $SkipDeploy
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

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

function Test-QtKitReady {
    param(
        [string] $Root,
        [string] $BuildPreset
    )

    if ([string]::IsNullOrWhiteSpace($Root)) {
        return $false
    }

    $kit = if ($BuildPreset -like "*mingw*") { "mingw_64" } else { "msvc2022_64" }
    return (Test-Path (Join-Path $Root "$kit\lib\cmake\Qt6\Qt6Config.cmake"))
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

Ensure-Command -Name "git" -FallbackPaths @(
    "C:\Program Files\Git\cmd",
    "C:\Program Files\Git\bin"
)
Ensure-Command -Name "cmake" -FallbackPaths @(
    "C:\Qt\Tools\CMake_64\bin",
    "C:\Program Files\CMake\bin"
)
Ensure-Command -Name "ninja" -FallbackPaths @(
    "C:\Qt\Tools\Ninja",
    "C:\Program Files\Ninja"
)

$effectivePreset = $Preset
$resolvedQtRoot = Resolve-QtRoot -Candidate $QtRoot -BuildPreset $effectivePreset

if ([string]::IsNullOrWhiteSpace($resolvedQtRoot) -and $effectivePreset -eq "windows-msvc-release" -and $AllowMingwFallback) {
    $fallbackQtRoot = Resolve-QtRoot -Candidate $QtRoot -BuildPreset "windows-mingw-release"
    if (-not [string]::IsNullOrWhiteSpace($fallbackQtRoot)) {
        Write-Warning "Qt MSVC kit was not detected. Falling back to windows-mingw-release."
        $effectivePreset = "windows-mingw-release"
        $resolvedQtRoot = $fallbackQtRoot
    }
}

if ([string]::IsNullOrWhiteSpace($resolvedQtRoot)) {
    $qtBase = if (-not [string]::IsNullOrWhiteSpace($QtRoot)) { $QtRoot } elseif (-not [string]::IsNullOrWhiteSpace($env:LA_QT)) { $env:LA_QT } else { "C:\Qt\6.9.3" }
    if ($effectivePreset -eq "windows-msvc-release" -and (Test-Path (Join-Path $qtBase "msvc2022_64"))) {
        throw "Detected '$qtBase\msvc2022_64' but missing '$qtBase\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake'. Your Qt MSVC kit appears incomplete. Install required Qt modules (Qt Base/Quick/QuickControls2/Multimedia/Network/Concurrent/Sql) for msvc2022_64."
    }
    throw "Qt root not detected for preset '$effectivePreset'. Pass -QtRoot, for example: .\scripts\bootstrap.ps1 -QtRoot C:\Qt\6.9.3"
}

$localVcpkg = Join-Path $RepoRoot ".deps\vcpkg"
$useManagedLocalVcpkg = [string]::IsNullOrWhiteSpace($VcpkgRoot)

if ($useManagedLocalVcpkg) {
    if (-not (Test-Path (Join-Path $localVcpkg "scripts\buildsystems\vcpkg.cmake"))) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $localVcpkg) | Out-Null
        Write-Host ">> Cloning vcpkg to $localVcpkg" -ForegroundColor Cyan
        git clone https://github.com/microsoft/vcpkg.git $localVcpkg
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        Write-Host ">> Bootstrapping vcpkg" -ForegroundColor Cyan
        & (Join-Path $localVcpkg "bootstrap-vcpkg.bat")
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    $resolvedVcpkgRoot = $localVcpkg
} else {
    $resolvedVcpkgRoot = Resolve-VcpkgRoot -Candidate $VcpkgRoot
}

if ([string]::IsNullOrWhiteSpace($resolvedVcpkgRoot)) {
    throw "vcpkg root was not detected. Pass -VcpkgRoot <path-to-vcpkg>."
}

Write-Host ">> Qt root: $resolvedQtRoot" -ForegroundColor DarkGray
Write-Host ">> vcpkg root: $resolvedVcpkgRoot" -ForegroundColor DarkGray
Write-Host ">> Preset: $effectivePreset" -ForegroundColor DarkGray

$buildScript = Join-Path $PSScriptRoot "build.ps1"
if ($Clean) {
    & $buildScript -Preset $effectivePreset -QtRoot $resolvedQtRoot -VcpkgRoot $resolvedVcpkgRoot -Version $Version -Clean -SkipDeploy:$SkipDeploy
} else {
    & $buildScript -Preset $effectivePreset -QtRoot $resolvedQtRoot -VcpkgRoot $resolvedVcpkgRoot -Version $Version -SkipDeploy:$SkipDeploy
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
