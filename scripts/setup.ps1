#Requires -Version 5.1

<#
.SYNOPSIS
    Backward-compatible setup wrapper.

.DESCRIPTION
    setup.ps1 is kept for compatibility and now forwards to bootstrap.ps1.
#>

[CmdletBinding()]
param(
    [string] $Preset = "windows-msvc-release",
    [string] $QtRoot,
    [string] $VcpkgRoot,
    [switch] $Clean
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "[INFO] scripts/setup.ps1 is deprecated. Use scripts/bootstrap.ps1." -ForegroundColor Yellow

$args = @(
    "-Preset", $Preset
)
if (-not [string]::IsNullOrWhiteSpace($QtRoot)) { $args += @("-QtRoot", $QtRoot) }
if (-not [string]::IsNullOrWhiteSpace($VcpkgRoot)) { $args += @("-VcpkgRoot", $VcpkgRoot) }
if ($Clean) { $args += "-Clean" }

& (Join-Path $ScriptDir "bootstrap.ps1") @args
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
