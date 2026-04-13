[CmdletBinding()]
param(
    [bool]$BuildRelease = $true,
    [bool]$BuildDebug = $true,
    [int]$Jobs = 0,
    [switch]$Clean,
    [switch]$SkipToolCheck
)

$RepoRoot = Split-Path -Parent $PSScriptRoot
$TargetScript = Join-Path $RepoRoot 'tools\scripts\third_party\build_grpc.ps1'

if (-not (Test-Path $TargetScript)) {
    Write-Error "Cannot find target script at $TargetScript"
}

& $TargetScript @PSBoundParameters
exit $LASTEXITCODE
