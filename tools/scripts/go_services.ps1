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

    [string[]]$Services = @(),

    # Per-service instance counts for local multi-open. Accepts either:
    #   * hashtable (in-session use):   -Counts @{ login = 2; scene_manager = 2 }
    #   * string list (pwsh -File):     -Counts login=2,scene_manager=2
    # Service name keys must match the catalogue. Count <= 1 means single instance
    # (uses the original etc/<svc>.yaml). Count > 1 generates derived yaml files
    # under run/etc/go_services/<svc>_<i>.yaml with ListenOn port = base + (i-1).
    # 'db' is intentionally restricted to a single instance (Kafka consumer + DB writer).
    [object]$Counts = @{},

    # Base port offset between instances. The N-th instance (1-based) listens on
    # base + (i-1) * PortStride. Default 1 keeps ports densely packed.
    [int]$PortStride = 1
)

$ErrorActionPreference = "Stop"

$ScriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot   = Resolve-Path (Join-Path $ScriptDir "..\..")
$GoRoot     = Join-Path $RepoRoot "go"
# Runtime artifacts (logs, pid files) live under run/, kept separate from
# build outputs in bin/. See docs/ops/log-management.md.
$RunDir     = Join-Path $RepoRoot "run"
$PidFile    = Join-Path $RunDir   "pids\go_services.pid.json"
$LogDir     = Join-Path $RunDir   "logs\go_services"
# Legacy fallback (older pid files written under bin/ before the run/ split).
$LegacyPidFile = Join-Path $RepoRoot "bin\go_services.pid.json"
# Built Go binaries still ship under bin/go_services\ alongside the C++ exes
# because that directory is the runtime working dir for local launches.
$GoBinDir   = Join-Path $RepoRoot "bin\go_services"

# ── Service catalogue ────────────────────────────────────────────────
# Order matters: infrastructure-layer services first, then domain services.
# Each entry:
#   Dir         relative path under go/
#   Entry       main .go file (used by `go run` and `go build`)
#   Port        base ListenOn port (instance #i listens on Port + (i-1)*PortStride)
#   Desc        human-readable description
#   ConfigFlag  command-line flag the entry uses to override the config path (e.g. -f, -loginService)
#   ConfigFile  default config file path relative to the service Dir
#   AllowMultiInstance  $false to forbid -Counts > 1 (e.g. db: single Kafka consumer)
$ServiceCatalogue = [ordered]@{
    db              = @{ Dir = "db";              Entry = "db.go";                    Port = 6000;  Desc = "DB (Kafka consumer + MySQL)";       ConfigFlag = "-f";            ConfigFile = "etc/db.yaml";                    AllowMultiInstance = $false }
    data_service    = @{ Dir = "data_service";    Entry = "data_service.go";          Port = 9000;  Desc = "Data Service (multi-zone Redis)"; ConfigFlag = "-f";            ConfigFile = "etc/data_service.yaml";          AllowMultiInstance = $true  }
    player_locator  = @{ Dir = "player_locator";  Entry = "player_locator.go";        Port = 50200; Desc = "Player Locator (Redis cache)";    ConfigFlag = "-f";            ConfigFile = "etc/player_locator.yaml";        AllowMultiInstance = $true  }
    login           = @{ Dir = "login";           Entry = "login.go";                 Port = 50000; Desc = "Login (gRPC + etcd)";            ConfigFlag = "-loginService"; ConfigFile = "etc/login.yaml";                 AllowMultiInstance = $true  }
    scene_manager   = @{ Dir = "scene_manager";   Entry = "scene_manager_service.go"; Port = 60300; Desc = "Scene Manager (Kafka + Redis)";    ConfigFlag = "-f";            ConfigFile = "etc/scene_manager_service.yaml"; AllowMultiInstance = $true  }
}

# Derived per-instance config files live here so the source tree stays clean.
$DerivedEtcDir = Join-Path $RunDir "etc\go_services"

# Normalize $Counts (which may arrive as a hashtable, string, or string[]) to a
# plain hashtable<string,int>. pwsh -File serializes hashtable args to a string,
# so we accept the string form 'name=N,name=N' as a fallback.
function ConvertTo-CountsHashtable {
    param($Raw)
    $result = @{}
    if ($null -eq $Raw) { return $result }
    if ($Raw -is [hashtable]) {
        foreach ($k in $Raw.Keys) { $result[[string]$k] = [int]$Raw[$k] }
        return $result
    }
    $parts = @()
    if ($Raw -is [string]) {
        $parts = $Raw -split '[,;]' | Where-Object { $_ -ne '' }
    } elseif ($Raw -is [System.Collections.IEnumerable]) {
        foreach ($item in $Raw) {
            if ($item -is [string]) { $parts += ($item -split '[,;]' | Where-Object { $_ -ne '' }) }
        }
    } else {
        throw "Unsupported -Counts value type: $($Raw.GetType().FullName)"
    }
    foreach ($p in $parts) {
        $kv = $p -split '=', 2
        if ($kv.Count -ne 2) { throw "Invalid -Counts entry '$p'. Expected 'name=N'." }
        $result[$kv[0].Trim()] = [int]$kv[1].Trim()
    }
    return $result
}

$Counts = ConvertTo-CountsHashtable -Raw $Counts

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

# Returns the requested instance count for a service. Defaults to 1.
# Validates AllowMultiInstance and rejects negative / huge counts.
function Get-ServiceInstanceCount {
    param([string]$Name)
    if (-not $Counts.ContainsKey($Name)) { return 1 }
    $n = [int]$Counts[$Name]
    if ($n -lt 1) { return 1 }
    if ($n -gt 32) { throw "Refusing to launch $n instances of '$Name' (cap=32)." }
    if ($n -gt 1 -and -not $ServiceCatalogue[$Name].AllowMultiInstance) {
        throw "Service '$Name' does not support multi-instance (AllowMultiInstance=false). Counts entry: $Name=$n."
    }
    return $n
}

# Builds the InstanceKey used everywhere (PID file, log file, derived yaml).
# Single instance preserves the legacy bare service name for backward compatibility.
function Get-InstanceKey {
    param([string]$Name, [int]$Index, [int]$Total)
    if ($Total -le 1) { return $Name }
    return "${Name}_${Index}"
}

# For instance #1 of a single-instance launch, return the original etc path so
# behavior is unchanged. Otherwise materialize a derived yaml in run/etc/go_services
# with ListenOn rewritten to base + (i-1)*PortStride.
function Resolve-InstanceConfig {
    param(
        [string]$Name,
        [hashtable]$Info,
        [int]$Index,
        [int]$Total
    )
    $svcDir   = Join-Path $GoRoot $Info.Dir
    $baseYaml = Join-Path $svcDir $Info.ConfigFile
    $port     = [int]$Info.Port + ($Index - 1) * $PortStride

    if ($Total -le 1) {
        return [pscustomobject]@{
            Path     = $Info.ConfigFile  # relative path; resolved by working dir
            FullPath = $baseYaml
            Port     = [int]$Info.Port
            Derived  = $false
        }
    }

    if (-not (Test-Path $baseYaml)) {
        throw "Base config not found for '$Name' instance #$Index : $baseYaml"
    }
    if (-not (Test-Path $DerivedEtcDir)) {
        New-Item -ItemType Directory -Path $DerivedEtcDir -Force | Out-Null
    }

    $derivedName = "${Name}_${Index}.yaml"
    $derivedPath = Join-Path $DerivedEtcDir $derivedName

    # Rewrite top-level ListenOn line. Keep the original host (e.g. 0.0.0.0 vs 127.0.0.1).
    # Use an instance Regex so the count-limited overload is available.
    $content = Get-Content $baseYaml -Raw
    $regex   = [regex]::new('(?m)^(ListenOn:\s*)([^:\s]+):(\d+)\s*$')
    if (-not $regex.IsMatch($content)) {
        Write-Warning "Did not find a top-level 'ListenOn:' line in $baseYaml; instance #$Index may collide on port $($Info.Port)."
    }
    $newContent = $regex.Replace(
        $content,
        { param($m) "$($m.Groups[1].Value)$($m.Groups[2].Value):$port" },
        1
    )
    Set-Content -Path $derivedPath -Value $newContent -Encoding UTF8 -NoNewline

    return [pscustomobject]@{
        Path     = $derivedPath
        FullPath = $derivedPath
        Port     = $port
        Derived  = $true
    }
}

# Normalize a PID-file entry to a hashtable { Pid; Port; Service }.
# Accepts legacy int values (pre multi-instance) for backward compatibility.
function Read-InstanceEntry {
    param($RawValue, [string]$FallbackService, [int]$FallbackPort)
    if ($null -eq $RawValue) { return $null }
    if ($RawValue -is [int] -or $RawValue -is [long]) {
        return @{ Pid = [int]$RawValue; Port = $FallbackPort; Service = $FallbackService }
    }
    $procId = if ($RawValue.PSObject.Properties.Name -contains 'Pid') { [int]$RawValue.Pid } else { 0 }
    $port   = if ($RawValue.PSObject.Properties.Name -contains 'Port') { [int]$RawValue.Port } else { $FallbackPort }
    $svc    = if ($RawValue.PSObject.Properties.Name -contains 'Service') { [string]$RawValue.Service } else { $FallbackService }
    return @{ Pid = $procId; Port = $port; Service = $svc }
}

function Read-PidFile {
    if (Test-Path $PidFile) {
        $raw = Get-Content $PidFile -Raw | ConvertFrom-Json
        if ($null -eq $raw) { return [pscustomobject]@{} }
        return $raw
    }
    # One-time migration: pick up legacy pid file if present, then rewrite
    # under the new run/ location on next Write-PidFile.
    if (Test-Path $LegacyPidFile) {
        $raw = Get-Content $LegacyPidFile -Raw | ConvertFrom-Json
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

# ── Helpers (startup monitoring) ──────────────────────────────────────
function Wait-ForStartupBanner {
    param(
        [string]$LogFile,
        [string]$ServiceName,
        [int]$TimeoutSeconds = 30
    )
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            if (Test-Path $LogFile) {
                $content = Get-Content $LogFile -Raw -ErrorAction SilentlyContinue
                if ($content -and ($content -match "STARTED SUCCESSFULLY")) {
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
    Write-Host "        [timeout] $ServiceName did not report startup within ${TimeoutSeconds}s - check logs" -ForegroundColor Yellow
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
    $launchedServices = @()

    if (-not (Test-Path $LogDir)) { New-Item -ItemType Directory -Path $LogDir -Force | Out-Null }

    foreach ($name in $names) {
        $info = $ServiceCatalogue[$name]
        $svcDir = Join-Path $GoRoot $info.Dir

        if (-not (Test-Path $svcDir)) {
            Write-Warning "Skipping '$name': directory $svcDir does not exist."
            continue
        }

        $count = Get-ServiceInstanceCount -Name $name

        for ($i = 1; $i -le $count; $i++) {
            $instanceKey = Get-InstanceKey -Name $name -Index $i -Total $count
            $cfg = Resolve-InstanceConfig -Name $name -Info $info -Index $i -Total $count

            # Skip if already running
            if ($pids.PSObject.Properties.Name -contains $instanceKey) {
                $existing = Read-InstanceEntry -RawValue $pids.$instanceKey -FallbackService $name -FallbackPort $cfg.Port
                if ($existing -and $existing.Pid -gt 0) {
                    try {
                        $proc = Get-Process -Id $existing.Pid -ErrorAction Stop
                        if (-not $proc.HasExited) {
                            Write-Host "[skip]  $instanceKey (PID $($existing.Pid) already running on :$($existing.Port))" -ForegroundColor Yellow
                            continue
                        }
                    } catch {
                        # process gone; will restart
                    }
                }
            }

            $logOut  = Join-Path $LogDir "$instanceKey.stdout.log"
            $logErr  = Join-Path $LogDir "$instanceKey.stderr.log"

            $configFlag = $info.ConfigFlag
            $configArg  = $cfg.Path

            if ($UseExe) {
                $exePath = Resolve-GoExecutablePath -ServiceName $name -ServiceDir $svcDir
                if (-not $exePath) {
                    Write-Warning "Skipping '$instanceKey': executable not found. Run -Command build first or place $name.exe in bin\go_services\ or $svcDir."
                    continue
                }

                Write-Host "[start-exe] $instanceKey  :$($cfg.Port)  ($($info.Desc))" -ForegroundColor Cyan

                $proc = Start-Process -FilePath $exePath `
                    -ArgumentList @($configFlag, $configArg) `
                    -WorkingDirectory $svcDir `
                    -RedirectStandardOutput $logOut `
                    -RedirectStandardError  $logErr `
                    -PassThru `
                    -WindowStyle Hidden
            }
            else {
                Write-Host "[start] $instanceKey  :$($cfg.Port)  ($($info.Desc))" -ForegroundColor Cyan

                $proc = Start-Process -FilePath "go" `
                    -ArgumentList @("run", $info.Entry, $configFlag, $configArg) `
                    -WorkingDirectory $svcDir `
                    -RedirectStandardOutput $logOut `
                    -RedirectStandardError  $logErr `
                    -PassThru `
                    -WindowStyle Hidden
            }

            $entry = [pscustomobject]@{
                Pid     = $proc.Id
                Port    = $cfg.Port
                Service = $name
            }
            $pids | Add-Member -NotePropertyName $instanceKey -NotePropertyValue $entry -Force
            if ($cfg.Derived) {
                Write-Host "        PID $($proc.Id)  cfg -> $($cfg.FullPath)" -ForegroundColor DarkGray
            }
            Write-Host "        logs -> run\logs\go_services\$instanceKey.*.log" -ForegroundColor DarkGray
            $launchedServices += @{ Name = $instanceKey; LogFile = $logOut }
        }
    }

    Write-PidFile $pids

    if ($launchedServices.Count -gt 0) {
        Write-Host "`nWaiting for startup confirmation..." -ForegroundColor DarkGray
        foreach ($svc in $launchedServices) {
            Wait-ForStartupBanner -LogFile $svc.LogFile -ServiceName $svc.Name
        }
    }

    Write-Host "`nAll requested services launched. Use -Command status to check health." -ForegroundColor Green
}

function Invoke-Stop {
    $pids = Read-PidFile
    if (@($pids.PSObject.Properties).Count -eq 0) {
        Write-Host "No tracked services to stop." -ForegroundColor Yellow
        return
    }

    # Build the target instance key list. -Services filters by base service name
    # and expands to every running instance of that service (e.g. login_1, login_2).
    if ($Services.Count -gt 0) {
        $targetServices = Resolve-ServiceList -Requested $Services
        $instanceKeys = @()
        foreach ($prop in $pids.PSObject.Properties) {
            $info = if ($ServiceCatalogue.Contains($prop.Name)) { $ServiceCatalogue[$prop.Name] } else { $null }
            $port = if ($info) { $info.Port } else { 0 }
            $entry = Read-InstanceEntry -RawValue $prop.Value -FallbackService $prop.Name -FallbackPort $port
            if ($entry -and ($targetServices -contains $entry.Service)) {
                $instanceKeys += $prop.Name
            }
        }
    } else {
        $instanceKeys = @($pids.PSObject.Properties.Name)
    }

    foreach ($key in $instanceKeys) {
        if ($pids.PSObject.Properties.Name -notcontains $key) { continue }
        $entry = Read-InstanceEntry -RawValue $pids.$key -FallbackService $key -FallbackPort 0
        $procId = $entry.Pid
        try {
            $proc = Get-Process -Id $procId -ErrorAction Stop
            if (-not $proc.HasExited) {
                Stop-Process -Id $procId -Force
                Write-Host "[stop]  $key (PID $procId)" -ForegroundColor Magenta
            } else {
                Write-Host "[gone]  $key (PID $procId already exited)" -ForegroundColor DarkGray
            }
        } catch {
            Write-Host "[gone]  $key (PID $procId not found)" -ForegroundColor DarkGray
        }
        $pids.PSObject.Properties.Remove($key)
    }

    Write-PidFile $pids
}

function Invoke-Status {
    $pids = Read-PidFile
    if (@($pids.PSObject.Properties).Count -eq 0) {
        Write-Host "No tracked services." -ForegroundColor Yellow
        return
    }
    Write-Host ("{0,-22} {1,-8} {2,-8} {3}" -f "INSTANCE", "PID", "PORT", "STATUS") -ForegroundColor White
    Write-Host ("{0,-22} {1,-8} {2,-8} {3}" -f "--------", "---", "----", "------")
    foreach ($prop in $pids.PSObject.Properties) {
        $key = $prop.Name
        # Derive base service name for catalogue lookup (login_2 -> login).
        $baseName = if ($ServiceCatalogue.Contains($key)) { $key } elseif ($key -match '^(?<svc>.+)_\d+$' -and $ServiceCatalogue.Contains($Matches.svc)) { $Matches.svc } else { $key }
        $basePort = if ($ServiceCatalogue.Contains($baseName)) { $ServiceCatalogue[$baseName].Port } else { 0 }
        $entry    = Read-InstanceEntry -RawValue $prop.Value -FallbackService $baseName -FallbackPort $basePort
        $procId   = $entry.Pid
        $port     = if ($entry.Port -gt 0) { $entry.Port } else { $basePort }
        $status = "UNKNOWN"
        try {
            $proc = Get-Process -Id $procId -ErrorAction Stop
            $status = if ($proc.HasExited) { "EXITED" } else { "RUNNING" }
        } catch {
            $status = "GONE"
        }
        $color = switch ($status) { "RUNNING" { "Green" } "EXITED" { "Red" } default { "DarkGray" } }
        Write-Host ("{0,-22} {1,-8} {2,-8} {3}" -f $key, $procId, $port, $status) -ForegroundColor $color
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
