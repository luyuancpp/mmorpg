param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("pbgen-build", "pbgen-run", "tree", "naming-audit", "naming-apply")]
    [string]$Command,

    [string]$ConfigPath = "",

    [ValidateSet("snake", "kebab")]
    [string]$Style = "snake",

    [int]$MaxChanges = 0
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\..")
$PbgenDir = Join-Path $RepoRoot "tools\proto_generator\pbgen"

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

function Invoke-NamingAudit {
    $scriptPath = Join-Path $ScriptDir "normalize_names.ps1"
    if (-not (Test-Path $scriptPath)) {
        throw "normalize_names.ps1 not found: $scriptPath"
    }

    & $scriptPath -Mode audit -Style $Style -MaxChanges $MaxChanges
}

function Invoke-NamingApply {
    $scriptPath = Join-Path $ScriptDir "normalize_names.ps1"
    if (-not (Test-Path $scriptPath)) {
        throw "normalize_names.ps1 not found: $scriptPath"
    }

    & $scriptPath -Mode apply -Style $Style -MaxChanges $MaxChanges -Confirm:$false
}

switch ($Command) {
    "pbgen-build" { Invoke-PbgenBuild }
    "pbgen-run" { Invoke-PbgenRun }
    "tree" { Invoke-Tree }
    "naming-audit" { Invoke-NamingAudit }
    "naming-apply" { Invoke-NamingApply }
    default { throw "Unsupported command: $Command" }
}
