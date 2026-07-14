function Get-CMakeCacheSourceDir {
    param(
        [Parameter(Mandatory = $true)]
        [string] $BuildDirectory
    )

    $cacheFile = Join-Path $BuildDirectory "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cacheFile)) {
        return $null
    }

    $match = Select-String -LiteralPath $cacheFile -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$' | Select-Object -First 1
    if ($null -eq $match) {
        return $null
    }

    return $match.Matches[0].Groups[1].Value.Trim()
}

function Normalize-CMakePath {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Path
    )

    $normalized = [System.IO.Path]::GetFullPath($Path)
    return $normalized.TrimEnd('\', '/').ToLowerInvariant()
}

function Remove-StaleCMakeBuildDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string] $BuildDirectory,
        [Parameter(Mandatory = $true)]
        [string] $ExpectedSourceDirectory
    )

    $cachedSourceDirectory = Get-CMakeCacheSourceDir -BuildDirectory $BuildDirectory
    if ([string]::IsNullOrWhiteSpace($cachedSourceDirectory)) {
        return
    }

    if ((Normalize-CMakePath $cachedSourceDirectory) -eq (Normalize-CMakePath $ExpectedSourceDirectory)) {
        return
    }

    Write-Host ">> Detected stale CMake cache from '$cachedSourceDirectory'; removing '$BuildDirectory'" -ForegroundColor Yellow
    Remove-Item -LiteralPath $BuildDirectory -Recurse -Force
}
