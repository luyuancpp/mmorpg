<#
.SYNOPSIS
    C++ node launcher for local Windows development.

.DESCRIPTION
    Starts, stops, or queries C++ game-server nodes (gate, scene) from the bin/ directory.
    Each node runs as a separate background process with PID tracking and per-node logging.

.PARAMETER Command
    start   – launch selected nodes (default: all)
    stop    – stop running nodes launched by this script
    status  – show which nodes are currently running
    list    – print the available node catalogue

.PARAMETER Nodes
    Comma-separated node names to start (e.g. "gate,scene").
    Defaults to all nodes.

.PARAMETER GateCount
    Number of gate.exe instances to launch (default: 1).

.PARAMETER SceneCount
    Number of scene.exe instances to launch (default: 1).

.EXAMPLE
    # Start all nodes with default counts
    pwsh -File tools/scripts/cpp_nodes.ps1 -Command start

    # Start 2 gates and 4 scenes
    pwsh -File tools/scripts/cpp_nodes.ps1 -Command start -GateCount 2 -SceneCount 4

    # Start only scene nodes
    pwsh -File tools/scripts/cpp_nodes.ps1 -Command start -Nodes scene

    # Check what's running
    pwsh -File tools/scripts/cpp_nodes.ps1 -Command status

    # Stop all nodes
    pwsh -File tools/scripts/cpp_nodes.ps1 -Command stop
#>
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("start", "stop", "status", "list")]
    [string]$Command,

    [string[]]$Nodes = @(),

    [int]$GateCount  = 1,
    [int]$SceneCount = 1
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Resolve-Path (Join-Path $ScriptDir "..\..")
$BinDir    = Join-Path $RepoRoot "bin"
# Runtime artifacts (logs, pid files) live under run/, kept separate from
# build outputs and the C++ runtime working dir in bin/.
# See docs/ops/log-management.md.
$RunDir    = Join-Path $RepoRoot "run"
$PidFile   = Join-Path $RunDir   "pids\cpp_nodes.pid.json"
$LogDir    = Join-Path $RunDir   "logs\cpp_nodes"
# Legacy fallback for pid file written under bin/ before the run/ split.
$LegacyPidFile = Join-Path $RepoRoot "bin\cpp_nodes.pid.json"

# ── Node catalogue ───────────────────────────────────────────────
$NodeCatalogue = [ordered]@{
    gate  = @{ Exe = "gate.exe";  Desc = "Gate Node (client TCP bridge + Kafka routing)" }
    scene = @{ Exe = "scene.exe"; Desc = "Scene Node (gameplay + ECS systems)" }
}

# ── Helpers ──────────────────────────────────────────────────────
function Resolve-NodeList {
    param([string[]]$Requested)
    if ($Requested.Count -eq 0) {
        return @($NodeCatalogue.Keys)
    }
    foreach ($n in $Requested) {
        if (-not $NodeCatalogue.Contains($n)) {
            throw "Unknown node '$n'. Run with -Command list to see available nodes."
        }
    }
    return $Requested
}

function Get-InstanceCount {
    param([string]$NodeName)
    switch ($NodeName) {
        "gate"  { return $GateCount }
        "scene" { return $SceneCount }
        default { return 1 }
    }
}

function Wait-ForStartupBanner {
    param(
        [string]$LogFile,
        [string]$InstanceKey,
        [int]$TimeoutSeconds = 30
    )
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            if (Test-Path $LogFile) {
                $content = Get-Content $LogFile -Raw -ErrorAction SilentlyContinue
                if ($content -and ($content -match "STARTED SUCCESSFULLY")) {
                    # Extract lines between the first and last ===== separator
                    $lines = $content -split "`n"
                    $inBanner = $false
                    foreach ($line in $lines) {
                        $trimmed = $line.Trim()
                        if ($trimmed -match '^={5,}') {
                            if ($inBanner) {
                                Write-Host "        $trimmed" -ForegroundColor Green
                                break
                            }
                            $inBanner = $true
                        }
                        if ($inBanner -and $trimmed.Length -gt 0) {
                            Write-Host "        $trimmed" -ForegroundColor Green
                        }
                    }
                    return
                }
            }
        } catch {
            # File might be locked briefly; retry on next iteration
        }
        Start-Sleep -Milliseconds 500
    }
    Write-Host "        [timeout] $InstanceKey did not report startup within ${TimeoutSeconds}s - check logs" -ForegroundColor Yellow
}

function Read-PidFile {
    if (Test-Path $PidFile) {
        return Get-Content $PidFile -Raw | ConvertFrom-Json
    }
    if (Test-Path $LegacyPidFile) {
        return Get-Content $LegacyPidFile -Raw | ConvertFrom-Json
    }
    return [pscustomobject]@{}
}

function Write-PidFile {
    param($Map)
    $dir = Split-Path -Parent $PidFile
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $Map | ConvertTo-Json | Set-Content $PidFile -Encoding UTF8
}

# ── Commands ─────────────────────────────────────────────────────
function Invoke-Start {
    $names = Resolve-NodeList -Requested $Nodes
    $pids  = Read-PidFile
    $launchedInstances = @()

    if (-not (Test-Path $LogDir)) { New-Item -ItemType Directory -Path $LogDir -Force | Out-Null }

    # muduo AsyncLogging fopen("a") does NOT create intermediate directories on
    # Windows; a missing folder triggers assert(fp_) in FileUtil::AppendFile and
    # aborts gate.exe / scene.exe before any log line reaches disk. Node uses
    # cwd-relative "logs/cpp_nodes/<short>" as the rolled-log basename, so make
    # sure that folder exists under the working directory (bin\) before launch.
    $MuduoLogDir = Join-Path $BinDir "logs\cpp_nodes"
    if (-not (Test-Path $MuduoLogDir)) { New-Item -ItemType Directory -Path $MuduoLogDir -Force | Out-Null }

    foreach ($name in $names) {
        $info = $NodeCatalogue[$name]
        $exePath = Join-Path $BinDir $info.Exe

        if (-not (Test-Path $exePath)) {
            Write-Warning "Skipping '$name': $exePath not found. Build the solution first (msbuild game.sln)."
            continue
        }

        $count = Get-InstanceCount -NodeName $name

        for ($i = 1; $i -le $count; $i++) {
            $instanceKey = if ($count -eq 1) { $name } else { "${name}_${i}" }

            # Skip if already running
            if ($pids.PSObject.Properties.Name -contains $instanceKey) {
                $existingPid = $pids.$instanceKey
                try {
                    $proc = Get-Process -Id $existingPid -ErrorAction Stop
                    if (-not $proc.HasExited) {
                        Write-Host "[skip]  $instanceKey (PID $existingPid already running)" -ForegroundColor Yellow
                        continue
                    }
                } catch {
                    # process gone; will restart
                }
            }

            $logOut = Join-Path $LogDir "$instanceKey.stdout.log"
            $logErr = Join-Path $LogDir "$instanceKey.stderr.log"

            Write-Host "[start] $instanceKey  ($($info.Desc))" -ForegroundColor Cyan

            $proc = Start-Process -FilePath $exePath `
                -WorkingDirectory $BinDir `
                -RedirectStandardOutput $logOut `
                -RedirectStandardError  $logErr `
                -PassThru `
                -WindowStyle Hidden

            $pids | Add-Member -NotePropertyName $instanceKey -NotePropertyValue $proc.Id -Force
            Write-Host "        PID $($proc.Id)  logs -> run\logs\cpp_nodes\$instanceKey.*.log" -ForegroundColor DarkGray
            $launchedInstances += @{ Key = $instanceKey; LogFile = $logOut }

            # Small delay between launches (same pattern as original start_server.bat)
            if ($i -lt $count) {
                Start-Sleep -Milliseconds 500
            }
        }

        # Delay between different node types
        Start-Sleep -Milliseconds 500
    }

    Write-PidFile $pids

    if ($launchedInstances.Count -gt 0) {
        Write-Host "`nWaiting for startup confirmation..." -ForegroundColor DarkGray
        foreach ($inst in $launchedInstances) {
            Wait-ForStartupBanner -LogFile $inst.LogFile -InstanceKey $inst.Key
        }
    }

    Write-Host "`nAll requested nodes launched. Use -Command status to check." -ForegroundColor Green
}

function Invoke-Stop {
    $pids = Read-PidFile
    if (@($pids.PSObject.Properties).Count -eq 0) {
        Write-Host "No tracked nodes to stop." -ForegroundColor Yellow
        return
    }

    $requestedNames = if ($Nodes.Count -gt 0) { Resolve-NodeList -Requested $Nodes } else { $null }

    foreach ($prop in @($pids.PSObject.Properties)) {
        $instanceKey = $prop.Name
        $procId = $prop.Value

        # Filter by node name if specified
        if ($null -ne $requestedNames) {
            $baseName = ($instanceKey -split '_')[0]
            if ($baseName -notin $requestedNames) { continue }
        }

        try {
            $proc = Get-Process -Id $procId -ErrorAction Stop
            if (-not $proc.HasExited) {
                Stop-Process -Id $procId -Force
                Write-Host "[stop]  $instanceKey (PID $procId)" -ForegroundColor Magenta
            } else {
                Write-Host "[gone]  $instanceKey (PID $procId already exited)" -ForegroundColor DarkGray
            }
        } catch {
            Write-Host "[gone]  $instanceKey (PID $procId not found)" -ForegroundColor DarkGray
        }
        $pids.PSObject.Properties.Remove($instanceKey)
    }

    Write-PidFile $pids
}

function Invoke-Status {
    $pids = Read-PidFile
    if (@($pids.PSObject.Properties).Count -eq 0) {
        Write-Host "No tracked nodes." -ForegroundColor Yellow
        return
    }
    Write-Host ("{0,-18} {1,-8} {2}" -f "NODE", "PID", "STATUS") -ForegroundColor White
    Write-Host ("{0,-18} {1,-8} {2}" -f "----", "---", "------")
    foreach ($prop in $pids.PSObject.Properties) {
        $instanceKey = $prop.Name
        $procId = $prop.Value
        $status = "UNKNOWN"
        try {
            $proc = Get-Process -Id $procId -ErrorAction Stop
            $status = if ($proc.HasExited) { "EXITED" } else { "RUNNING" }
        } catch {
            $status = "GONE"
        }
        $color = switch ($status) { "RUNNING" { "Green" } "EXITED" { "Red" } default { "DarkGray" } }
        Write-Host ("{0,-18} {1,-8} {2}" -f $instanceKey, $procId, $status) -ForegroundColor $color
    }
}

function Invoke-List {
    Write-Host "`nAvailable C++ nodes:`n" -ForegroundColor Cyan
    Write-Host ("{0,-10} {1,-15} {2}" -f "NAME", "EXECUTABLE", "DESCRIPTION") -ForegroundColor White
    Write-Host ("{0,-10} {1,-15} {2}" -f "----", "----------", "-----------")
    foreach ($kv in $NodeCatalogue.GetEnumerator()) {
        Write-Host ("{0,-10} {1,-15} {2}" -f $kv.Key, $kv.Value.Exe, $kv.Value.Desc)
    }
    Write-Host "`nInstance counts (adjustable): -GateCount $GateCount  -SceneCount $SceneCount" -ForegroundColor DarkGray
    Write-Host ""
}

# ── Dispatch ─────────────────────────────────────────────────────
switch ($Command) {
    "start"  { Invoke-Start }
    "stop"   { Invoke-Stop }
    "status" { Invoke-Status }
    "list"   { Invoke-List }
}
