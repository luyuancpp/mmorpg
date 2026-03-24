<#
.SYNOPSIS
    Foreground process entrypoint for mprocs local development dashboard.

.DESCRIPTION
    Runs one Go service or one C++ node in the foreground so mprocs can capture
    live logs and restart processes in a single terminal UI.

.EXAMPLE
    pwsh -NoProfile -File tools/scripts/dev_mprocs_proc.ps1 -Command go-login

.EXAMPLE
    pwsh -NoProfile -File tools/scripts/dev_mprocs_proc.ps1 -Command cpp-scene
#>
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet(
        "go-db",
        "go-data-service",
        "go-player-locator",
        "go-login",
        "go-scene-manager",
        "cpp-gate",
        "cpp-scene"
    )]
    [string]$Command
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\..")
$GoRoot = Join-Path $RepoRoot "go"
$BinRoot = Join-Path $RepoRoot "bin"

function Invoke-GoService {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Dir,

        [Parameter(Mandatory = $true)]
        [string]$Entry
    )

    $svcDir = Join-Path $GoRoot $Dir
    if (-not (Test-Path $svcDir)) {
        throw "Go service directory not found: $svcDir"
    }

    Push-Location $svcDir
    try {
        & go run $Entry
        $code = if ($null -eq $LASTEXITCODE) { 0 } else { $LASTEXITCODE }
        exit $code
    }
    finally {
        Pop-Location
    }
}

function Invoke-CppNode {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Exe
    )

    $exePath = Join-Path $BinRoot $Exe
    if (-not (Test-Path $exePath)) {
        throw "C++ executable not found: $exePath. Build with msbuild game.sln first."
    }

    Push-Location $BinRoot
    try {
        & $exePath
        $code = if ($null -eq $LASTEXITCODE) { 0 } else { $LASTEXITCODE }
        exit $code
    }
    finally {
        Pop-Location
    }
}

switch ($Command) {
    "go-db" { Invoke-GoService -Dir "db" -Entry "db.go" }
    "go-data-service" { Invoke-GoService -Dir "data_service" -Entry "data_service.go" }
    "go-player-locator" { Invoke-GoService -Dir "player_locator" -Entry "player_locator.go" }
    "go-login" { Invoke-GoService -Dir "login" -Entry "login.go" }
    "go-scene-manager" { Invoke-GoService -Dir "scene_manager" -Entry "scene_manager_service.go" }
    "cpp-gate" { Invoke-CppNode -Exe "gate.exe" }
    "cpp-scene" { Invoke-CppNode -Exe "scene.exe" }
    default { throw "Unsupported command: $Command" }
}
