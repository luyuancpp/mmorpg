<#
.SYNOPSIS
    Starts an mprocs dashboard after verifying mprocs is installed.

.DESCRIPTION
    Provides a friendlier entrypoint for VS Code tasks. If mprocs is missing,
    prints a clear installation command instead of failing with a generic shell error.

.EXAMPLE
    pwsh -NoProfile -File tools/scripts/start_mprocs.ps1 -ConfigPath tools/dev/mprocs.yaml
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$ConfigPath
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\..")

$resolvedConfigPath = $ConfigPath
if (-not [System.IO.Path]::IsPathRooted($resolvedConfigPath)) {
    $resolvedConfigPath = Join-Path $RepoRoot $resolvedConfigPath
}

if (-not (Test-Path $resolvedConfigPath)) {
    throw "mprocs config not found: $resolvedConfigPath"
}

function Resolve-MprocsExecutable {
    $cmd = Get-Command mprocs -ErrorAction SilentlyContinue
    if ($null -ne $cmd) {
        return $cmd.Source
    }

    $candidates = @(
        (Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Links\mprocs.exe"),
        (Join-Path $env:USERPROFILE "AppData\Local\Microsoft\WinGet\Links\mprocs.exe")
    )

    foreach ($candidate in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path $candidate)) {
            return $candidate
        }
    }

    $packagesRoot = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"
    if (Test-Path $packagesRoot) {
        $exe = Get-ChildItem $packagesRoot -Recurse -Filter "mprocs.exe" -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName
        if (-not [string]::IsNullOrWhiteSpace($exe)) {
            return $exe
        }
    }

    return $null
}

$mprocsExe = Resolve-MprocsExecutable
if ([string]::IsNullOrWhiteSpace($mprocsExe)) {
    Write-Host "mprocs is not installed or not on PATH." -ForegroundColor Yellow
    Write-Host "Install it with:" -ForegroundColor Yellow
    Write-Host "  winget install mprocs" -ForegroundColor Cyan
    exit 1
}

& $mprocsExe -c $resolvedConfigPath
$code = if ($null -eq $LASTEXITCODE) { 0 } else { $LASTEXITCODE }
exit $code
