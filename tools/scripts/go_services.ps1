<#
.SYNOPSIS
    Unified Go micro-service launcher for local Windows development.

.DESCRIPTION
    Starts one, several, or all implemented go-zero services in the go/ directory.
    Each service runs as a separate background process; a stop command terminates them.

.PARAMETER Command
    start   – build & launch selected services (default: all)
    stop    – gracefully stop running services launched by this script
    status  – show which services are currently running
    list    – print the available service catalogue
    build   – compile native binaries for selected services -> bin\go_services\

.PARAMETER Services
    Comma-separated service names to start (e.g. "login,db").
    Defaults to all implemented services.

.EXAMPLE
    # Start every implemented Go service
    pwsh -File tools/scripts/go_services.ps1 -Command start

    # Start only login and db
    pwsh -File tools/scripts/go_services.ps1 -Command start -Services login,db

    # Check what's running
    pwsh -File tools/scripts/go_services.ps1 -Command status

    # Stop all services launched by this script
    pwsh -File tools/scripts/go_services.ps1 -Command stop
#>
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("start", "start-exe", "stop", "status", "list", "build")]
    [string]$Command,

    [string[]]$Services = @()
)

$ErrorActionPreference = "Stop"

$ScriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot   = Resolve-Path (Join-Path $ScriptDir "..\..")
$GoRoot     = Join-Path $RepoRoot "go"
$PidFile    = Join-Path $RepoRoot "bin\go_services.pid.json"
$LogDir     = Join-Path $RepoRoot "bin\logs\go_services"
$GoBinDir   = Join-Path $RepoRoot "bin\go_services"

# ── Service catalogue ────────────────────────────────────────────────
# Order matters: infrastructure-layer services first, then domain services.
$ServiceCatalogue = [ordered]@{
    db              = @{ Dir = "db";              Entry = "db.go";                     Port = 6000;  Desc = "DB (Kafka consumer + MySQL)" }
    data_service    = @{ Dir = "data_service";    Entry = "data_service.go";            Port = 9000;  Desc = "Data Service (multi-zone Redis)" }
    player_locator  = @{ Dir = "player_locator";  Entry = "player_locator.go";         Port = 50200; Desc = "Player Locator (Redis cache)" }
    login           = @{ Dir = "login";           Entry = "login.go";                  Port = 50000; Desc = "Login (gRPC + etcd)" }
    scene_manager   = @{ Dir = "scene_manager";   Entry = "scene_manager_service.go";    Port = 60300; Desc = "Scene Manager (Kafka + Redis)" }
}

# ── Helpers ──────────────────────────────────────────────────────────
function Resolve-ServiceList {
    param([string[]]$Requested)
    if ($Requested.Count -eq 0) {
        return @($ServiceCatalogue.Keys)
    }
    foreach ($s in $Requested) {
        if (-not $ServiceCatalogue.Contains($s)) {
            throw "Unknown service '$s'. Run with -Command list to see available services."
        }
    }
    return $Requested
}

function Read-PidFile {
    if (Test-Path $PidFile) {
        $raw = Get-Content $PidFile -Raw | ConvertFrom-Json
        if ($null -eq $raw) { return [pscustomobject]@{} }
        return $raw
    }
    return [pscustomobject]@{}
}

function Write-PidFile {
    param($Map)
    $dir = Split-Path -Parent $PidFile
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $Map | ConvertTo-Json | Set-Content $PidFile -Encoding UTF8
}

# ── Commands ─────────────────────────────────────────────────────────
function Resolve-GoExecutablePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ServiceName,
        [Parameter(Mandatory = $true)]
        [string]$ServiceDir
    )

    $preferred = Join-Path $GoBinDir "$ServiceName.exe"
    if (Test-Path $preferred) {
        return $preferred
    }

    $fallback = Join-Path $ServiceDir "$ServiceName.exe"
    if (Test-Path $fallback) {
        return $fallback
    }

    return $null
}

function Invoke-Start {
    param(
        [switch]$UseExe
    )

    $names = Resolve-ServiceList -Requested $Services
    $pids  = Read-PidFile

    if (-not (Test-Path $LogDir)) { New-Item -ItemType Directory -Path $LogDir -Force | Out-Null }

    foreach ($name in $names) {
        $info = $ServiceCatalogue[$name]
        $svcDir = Join-Path $GoRoot $info.Dir

        if (-not (Test-Path $svcDir)) {
            Write-Warning "Skipping '$name': directory $svcDir does not exist."
            continue
        }

        # Skip if already running
        if ($pids.PSObject.Properties.Name -contains $name) {
            $existingPid = $pids.$name
            try {
                $proc = Get-Process -Id $existingPid -ErrorAction Stop
                if (-not $proc.HasExited) {
                    Write-Host "[skip]  $name (PID $existingPid already running on :$($info.Port))" -ForegroundColor Yellow
                    continue
                }
            } catch {
                # process gone; will restart
            }
        }

        $logOut  = Join-Path $LogDir "$name.stdout.log"
        $logErr  = Join-Path $LogDir "$name.stderr.log"

        if ($UseExe) {
            $exePath = Resolve-GoExecutablePath -ServiceName $name -ServiceDir $svcDir
            if (-not $exePath) {
                Write-Warning "Skipping '$name': executable not found. Run -Command build first or place $name.exe in bin\\go_services\\ or $svcDir."
                continue
            }

            Write-Host "[start-exe] $name  :$($info.Port)  ($($info.Desc))" -ForegroundColor Cyan

            $proc = Start-Process -FilePath $exePath `
                -WorkingDirectory $svcDir `
                -RedirectStandardOutput $logOut `
                -RedirectStandardError  $logErr `
                -PassThru `
                -WindowStyle Hidden
        }
        else {
            Write-Host "[start] $name  :$($info.Port)  ($($info.Desc))" -ForegroundColor Cyan

            $proc = Start-Process -FilePath "go" `
                -ArgumentList "run", $info.Entry `
                -WorkingDirectory $svcDir `
                -RedirectStandardOutput $logOut `
                -RedirectStandardError  $logErr `
                -PassThru `
                -WindowStyle Hidden
        }

        $pids | Add-Member -NotePropertyName $name -NotePropertyValue $proc.Id -Force
        Write-Host "        PID $($proc.Id)  logs -> bin\logs\go_services\$name.*.log" -ForegroundColor DarkGray
    }

    Write-PidFile $pids
    Write-Host "`nAll requested services launched. Use -Command status to check health." -ForegroundColor Green
}

function Invoke-Stop {
    $pids = Read-PidFile
    if (@($pids.PSObject.Properties).Count -eq 0) {
        Write-Host "No tracked services to stop." -ForegroundColor Yellow
        return
    }

    $names = if ($Services.Count -gt 0) { Resolve-ServiceList -Requested $Services } else { @($pids.PSObject.Properties.Name) }

    foreach ($name in $names) {
        if ($pids.PSObject.Properties.Name -notcontains $name) { continue }
        $procId = $pids.$name
        try {
            $proc = Get-Process -Id $procId -ErrorAction Stop
            if (-not $proc.HasExited) {
                Stop-Process -Id $procId -Force
                Write-Host "[stop]  $name (PID $procId)" -ForegroundColor Magenta
            } else {
                Write-Host "[gone]  $name (PID $procId already exited)" -ForegroundColor DarkGray
            }
        } catch {
            Write-Host "[gone]  $name (PID $procId not found)" -ForegroundColor DarkGray
        }
        $pids.PSObject.Properties.Remove($name)
    }

    Write-PidFile $pids
}

function Invoke-Status {
    $pids = Read-PidFile
    if (@($pids.PSObject.Properties).Count -eq 0) {
        Write-Host "No tracked services." -ForegroundColor Yellow
        return
    }
    Write-Host ("{0,-18} {1,-8} {2,-8} {3}" -f "SERVICE", "PID", "PORT", "STATUS") -ForegroundColor White
    Write-Host ("{0,-18} {1,-8} {2,-8} {3}" -f "-------", "---", "----", "------")
    foreach ($prop in $pids.PSObject.Properties) {
        $name = $prop.Name
        $procId  = $prop.Value
        $port = if ($ServiceCatalogue.Contains($name)) { $ServiceCatalogue[$name].Port } else { "?" }
        $status = "UNKNOWN"
        try {
            $proc = Get-Process -Id $procId -ErrorAction Stop
            $status = if ($proc.HasExited) { "EXITED" } else { "RUNNING" }
        } catch {
            $status = "GONE"
        }
        $color = switch ($status) { "RUNNING" { "Green" } "EXITED" { "Red" } default { "DarkGray" } }
        Write-Host ("{0,-18} {1,-8} {2,-8} {3}" -f $name, $procId, $port, $status) -ForegroundColor $color
    }
}

function Invoke-List {
    Write-Host "`nAvailable Go services:`n" -ForegroundColor Cyan
    Write-Host ("{0,-18} {1,-8} {2}" -f "NAME", "PORT", "DESCRIPTION") -ForegroundColor White
    Write-Host ("{0,-18} {1,-8} {2}" -f "----", "----", "-----------")
    foreach ($kv in $ServiceCatalogue.GetEnumerator()) {
        Write-Host ("{0,-18} {1,-8} {2}" -f $kv.Key, $kv.Value.Port, $kv.Value.Desc)
    }
    Write-Host ""
}

function Invoke-Build {
    $names = Resolve-ServiceList -Requested $Services
    $outDir = Join-Path $RepoRoot "bin\go_services"
    if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }

    $failed = @()
    foreach ($name in $names) {
        $info   = $ServiceCatalogue[$name]
        $svcDir = Join-Path $GoRoot $info.Dir

        if (-not (Test-Path $svcDir)) {
            Write-Warning "Skipping '$name': directory $svcDir does not exist."
            $failed += $name
            continue
        }

        $outExe = Join-Path $outDir "$name.exe"
        Write-Host "[build] $name -> bin\go_services\$name.exe" -ForegroundColor Cyan

        Push-Location $svcDir
        try {
            go build -o $outExe "./$($info.Entry)"
            if ($LASTEXITCODE -ne 0) {
                Write-Host "[FAIL]  $name" -ForegroundColor Red
                $failed += $name
            } else {
                Write-Host "[ok]    $name" -ForegroundColor Green
            }
        } finally {
            Pop-Location
        }
    }

    if ($failed.Count -gt 0) {
        throw "Build failed for: $($failed -join ', ')"
    }

    Write-Host "`nAll requested services built -> bin\go_services\" -ForegroundColor Green
}

# ── Dispatch ─────────────────────────────────────────────────────────
switch ($Command) {
    "start"      { Invoke-Start }
    "start-exe"  { Invoke-Start -UseExe }
    "stop"   { Invoke-Stop }
    "status" { Invoke-Status }
    "list"   { Invoke-List }
    "build"  { Invoke-Build }
}
