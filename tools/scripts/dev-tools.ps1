param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("pbgen-build", "pbgen-run", "tree")]
    [string]$Command,

    [string]$ConfigPath = ""
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\..")
$PbgenDir = Join-Path $RepoRoot "tools\proto-generator\pbgen"

function Invoke-PbgenBuild {
    Push-Location $PbgenDir
    try {
        go build -v ./...
    }
    finally {
        Pop-Location
    }
}

function Invoke-PbgenRun {
    Push-Location $PbgenDir
    try {
        if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
            $env:PROTO_GEN_CONFIG_PATH = Join-Path $PbgenDir "etc\proto_gen.yaml"
        }
        else {
            $env:PROTO_GEN_CONFIG_PATH = $ConfigPath
        }

        go run ./cmd
    }
    finally {
        Pop-Location
    }
}

function Invoke-Tree {
    $TreeScript = Join-Path $ScriptDir "tree.ps1"
    if (-not (Test-Path $TreeScript)) {
        throw "tree.ps1 not found: $TreeScript"
    }

    & $TreeScript
}

switch ($Command) {
    "pbgen-build" { Invoke-PbgenBuild }
    "pbgen-run" { Invoke-PbgenRun }
    "tree" { Invoke-Tree }
    default { throw "Unsupported command: $Command" }
}
