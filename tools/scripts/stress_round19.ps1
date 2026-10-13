<#
.SYNOPSIS
  One-click Round 19 login-stress orchestrator (1 zone x 45k), verifying the two
  R2 gate-token-expiry fixes (gateTokenTTL 5min->10min + robot retry accounting).

.DESCRIPTION
  Drives the full AGENTS.md §6.2 clean-stress protocol in phases:

    plan       (default) print exactly what each phase WILL do, touch nothing.
    baseline   verify prev-summary.txt (R18) exists as the comparison baseline.
    wipe       redis FLUSHALL + mysql truncate(zone_1_db) + etcd del --prefix
               + kafka offset reset + prune old robot/cpp logs.   [DESTRUCTIVE]
    build      go-svc-build (Go only — NEVER compiles C++, uses existing gate.exe).
    start      dev-start-exe (Go services + cpp gate/scene from prebuilt exe).
    run        launch the 45k robot + background stress_snap (prom snapshots).
    summarize  run stress_summarize.ps1 and print the R18-vs-R19 comparison table.
    all        baseline -> wipe -> build -> start -> run -> (wait) -> summarize.

  SAFETY:
    * Default phase is 'plan' — nothing happens without -Execute.
    * The destructive 'wipe' additionally requires -WipeData (double gate). It
      DESTROYS whatever stack is currently up (incl. the currency-crash task).
    * This script NEVER triggers C++ compilation (repo memory cpp-build-policy).
      gate.exe/scene.exe must already be built.

.EXAMPLE
  # 1) Dry-run the whole plan (safe, prints steps only):
  pwsh tools/scripts/stress_round19.ps1 -Phase all

  # 2) Actually run end-to-end (will wipe data!):
  pwsh tools/scripts/stress_round19.ps1 -Phase all -Execute -WipeData

  # 3) Run phases one at a time:
  pwsh tools/scripts/stress_round19.ps1 -Phase wipe  -Execute -WipeData
  pwsh tools/scripts/stress_round19.ps1 -Phase build -Execute
  pwsh tools/scripts/stress_round19.ps1 -Phase start -Execute
  pwsh tools/scripts/stress_round19.ps1 -Phase run   -Execute
  # ...wait ~18 min...
  pwsh tools/scripts/stress_round19.ps1 -Phase summarize -Execute -RunDir robot/logs/stress-round19-<ts>
#>
[CmdletBinding()]
param(
    [ValidateSet('plan','baseline','wipe','build','start','run','summarize','all')]
    [string]$Phase = 'plan',

    # Master safety switch — without it every phase is a dry-run print.
    [switch]$Execute,

    # Second gate required specifically for the destructive 'wipe' phase.
    [switch]$WipeData,

    [int]$Zone = 1,
    [string]$RobotConfig = 'robot/etc/robot.stress-3zone-z1.yaml',  # 45k, profile=stress (R18 config)
    [string]$PrevSummary = 'docs/stress/prev-summary.txt',          # R18 baseline
    [string]$Stages = '2,5,10,15,18',
    [int]$SteadyMinutes = 18,   # how long 'all' waits before summarize
    [string]$RunDir = '',       # reused by 'summarize'; auto-created by 'run'/'all'

    # mysql tables to truncate in zone_<N>_db (proven set from currency_crash_window.ps1)
    [string[]]$MySqlTables = @(
        'player_database','player_database_1','player_centre_database',
        'user_accounts','account_share_database'
    ),

    # data_service 全局库(go/data_service/etc/data_service.yaml 的 SnapshotMySQL.DBName):
    # 映射持久化的路由表在这里(见 Clear-GlobalRouteTables)。
    [string]$GlobalDbName = 'testdb'
)

$ErrorActionPreference = 'Stop'
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = (Resolve-Path (Join-Path $ScriptDir '..\..')).Path
$DevTools  = Join-Path $ScriptDir 'dev_tools.ps1'
$SnapPs1   = Join-Path $ScriptDir 'stress_snap.ps1'
$SummPs1   = Join-Path $ScriptDir 'stress_summarize.ps1'

function Hdr($t)  { Write-Host "`n=== $t ===" -ForegroundColor Cyan }
function Plan($t) { Write-Host "  [plan] $t" -ForegroundColor DarkGray }
function Do1($t)  { Write-Host "  [exec] $t" -ForegroundColor Green }
function Warn($t) { Write-Host "  [warn] $t" -ForegroundColor Yellow }

# Resolve the mysql root password the same way the rest of the repo does.
$MysqlPwd = if ($env:MYSQL_PASSWORD) { $env:MYSQL_PASSWORD } else { 'Mmorpg#2026db' }
$DbName   = "zone_${Zone}_db"

# ────────────────────────────────────────────────────────────────────────────
function Invoke-Baseline {
    Hdr 'Phase: baseline (verify R18 comparison baseline exists)'
    $p = Join-Path $RepoRoot $PrevSummary
    if (Test-Path $p) {
        Do1 "prev-summary present: $PrevSummary ($((Get-Item $p).Length) bytes)"
    } else {
        Warn "MISSING baseline: $PrevSummary"
        Warn "CLAUDE.md §6.2: prev-summary.txt must exist before the next round."
        Warn "Store the R18 stress_summarize output there first, or you must NOT proceed."
        if ($Execute) { throw "baseline missing — refusing to continue (CLAUDE §6.2)" }
    }
}

# ────────────────────────────────────────────────────────────────────────────
# 全局库里映射持久化的路由表(docs/design/player-route-mapping-durability-proposal.md §8.2,第 1a 批):
# 三张数据表 TRUNCATE,两张单行表原地复位(route_meta 只复位 writable;route_reconcile 清 leader / 租约 / 水位,
# 2026-10-12 G1a 从 route_meta 拆出)。与 currency_crash_window.ps1 的同名函数同一口径,改一处两处同改。
#
# route_meta / route_reconcile 不 TRUNCATE:清库可能发生在服务运行中,meta 行没了之后 db_first 的建角会一直 fail-closed
# (route_meta 缺行),直到下次 -migrate;第 1b 批的对账 leader 可能在跑,对账行没了它的租约 UPDATE 会落空。
# 原地复位不换 generation。
#
# 比 Invoke-Wipe 里 zone 表的清理更严格 —— SQL 失败就 throw,不 2>&1 | Out-Null 吞掉:Redis 已 FLUSHALL 而路由表还留着
# 上一轮的行,db_first 下就是「库有行、缓存全无」,同一个 id 再注册会落进 RowMissing / 冲突 / 已存在分支,
# audit 全是 db_only_unpublished —— 等于拿上一轮的映射跑这一轮,而且没有任何报错。
# 只处理已存在的表:没跑过迁移的库里一张都没有,是合法状态(跑过 G1a 之前迁移的库没有 route_reconcile,同样合法)。
#
# 本脚本只连本机 mysql 容器。TiDB 环境的清库按 docs/design/tidb-phase1-and-launch-readiness.md §3.5 做:
# TiDB 上 TRUNCATE 会换表 ID、丢掉预切与 table attributes,清完要重跑 `route_tool -f <yaml> tidb-prepare -zones=… -dry-run=false`。
$RouteDataTables        = @('player_route', 'zone_merge_fence', 'zone_merged_into')
$RouteMetaResetSql      = 'UPDATE route_meta SET writable=1 WHERE id=1;'
$RouteReconcileResetSql = 'UPDATE route_reconcile SET leader=NULL, lease_until_ms=NULL, watermark_ms=0, cache_verified_ms=0 WHERE id=1;'

# 在全局库所在的 MySQL 上执行一段 SQL(不指定默认库,语句自己带库名或 USE),非 0 退出码即 throw。
# 返回 stdout 的非空行(-N -B:不带列名、tab 分隔);stderr(含口令警告)不进返回值。
function Invoke-GlobalDbSql {
    param(
        [Parameter(Mandatory = $true)][string]$Sql,
        [Parameter(Mandatory = $true)][string]$What,
        [bool]$HasDocker
    )
    # 函数内局部改回 Continue:脚本级是 Stop,而 mysql 每次都往 stderr 写口令警告。Windows PowerShell 5.1 /
    # pwsh < 7.2 下 Stop + `2>&1` 会把这行 stderr 当 NativeCommandError 直接抛出,清库在查表那一步就中断 ——
    # 成败就取决于宿主版本了。这里只认 $LASTEXITCODE(下面非 0 即 throw),stderr 只是收进 $raw 供报错引用。
    # 赋值只作用于本函数作用域,不影响脚本其它部分的 Stop。
    $ErrorActionPreference = 'Continue'
    $mysqlArgs = @('-uroot', "-p$MysqlPwd", '-N', '-B')
    if (Get-Command mysql -ErrorAction SilentlyContinue) {
        $raw = @($Sql | & mysql @mysqlArgs 2>&1)
    } elseif ($HasDocker) {
        $raw = @($Sql | & docker exec -i mysql mysql @mysqlArgs 2>&1)
    } else {
        throw 'mysql client not on PATH and docker unavailable'
    }
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        $errText = ($raw | ForEach-Object { "$_" }) -join ' | '
        throw "global DB ${What} failed (mysql exit ${code}): $errText"
    }
    return @($raw | Where-Object { $_ -isnot [System.Management.Automation.ErrorRecord] } |
        ForEach-Object { "$_".Trim() } | Where-Object { $_ })
}

function Clear-GlobalRouteTables {
    param([bool]$HasDocker)
    # 库名要拼进 SQL:只认普通标识符,防的是参数里带引号 / 分号。
    if ($GlobalDbName -notmatch '^[A-Za-z0-9_]+$') { throw "GlobalDbName '$GlobalDbName' is not a plain identifier" }
    $names = (@($RouteDataTables) + @('route_meta', 'route_reconcile') | ForEach-Object { "'$_'" }) -join ','
    $existing = @(Invoke-GlobalDbSql -HasDocker $HasDocker -What 'route table lookup' -Sql (
        "SELECT TABLE_NAME FROM information_schema.TABLES WHERE TABLE_SCHEMA = '$GlobalDbName' AND TABLE_NAME IN ($names);"))
    $stmts = @()
    foreach ($t in $RouteDataTables) {
        if ($existing -contains $t) { $stmts += "TRUNCATE TABLE $t;" }
    }
    if ($existing -contains 'route_meta') { $stmts += $RouteMetaResetSql }
    if ($existing -contains 'route_reconcile') { $stmts += $RouteReconcileResetSql }
    if ($stmts.Count -eq 0) {
        Warn "no route tables in $GlobalDbName (migration never ran) - nothing to wipe"
        return
    }
    $sql = (@("USE $GlobalDbName;") + $stmts) -join "`n"
    Invoke-GlobalDbSql -HasDocker $HasDocker -What 'route table wipe' -Sql $sql | Out-Null
    Do1 "global DB ${GlobalDbName}: $($stmts -join ' ')"
}

# ────────────────────────────────────────────────────────────────────────────
function Invoke-Wipe {
    Hdr 'Phase: wipe  [DESTRUCTIVE]  redis + mysql + etcd + kafka + logs'
    $hasDocker = [bool](Get-Command docker -ErrorAction SilentlyContinue)

    if (-not $Execute -or -not $WipeData) {
        Plan 'redis : docker exec redis redis-cli FLUSHALL'
        Plan "mysql : TRUNCATE $($MySqlTables -join ', ') in $DbName"
        Plan "mysql : TRUNCATE $($RouteDataTables -join ', ') in $GlobalDbName (only tables that exist; fail-closed)"
        Plan "mysql : route_meta(id=1) writable reset in place in ${GlobalDbName}: $RouteMetaResetSql"
        Plan "mysql : route_reconcile(id=1) reset in place in ${GlobalDbName} (if the table exists): $RouteReconcileResetSql"
        Plan 'etcd  : docker exec etcd etcdctl del "" --prefix'
        Plan 'kafka : reset all consumer groups --to-latest (best-effort)'
        Plan 'logs  : prune robot/logs/stress-* (keep newest 1), bin/log/*, run/logs/*'
        if ($Execute -and -not $WipeData) {
            Warn 'wipe is gated behind BOTH -Execute AND -WipeData. Add -WipeData to actually wipe.'
        }
        return
    }

    Warn 'About to DESTROY all runtime data (redis/mysql/etcd/kafka) — this also'
    Warn 'tears down the stack currently up for the currency-crash task.'

    # redis
    Do1 'redis FLUSHALL'
    if (Get-Command redis-cli -ErrorAction SilentlyContinue) { & redis-cli FLUSHALL | Out-Null }
    elseif ($hasDocker) { & docker exec redis redis-cli FLUSHALL | Out-Null }
    else { throw 'redis-cli not on PATH and docker unavailable' }

    # mysql
    Do1 "mysql TRUNCATE in $DbName"
    $sqlBody = ($MySqlTables | ForEach-Object { "TRUNCATE TABLE $_;" }) -join "`n"
    $sql = "SET FOREIGN_KEY_CHECKS=0;`n$sqlBody`nSET FOREIGN_KEY_CHECKS=1;"
    if (Get-Command mysql -ErrorAction SilentlyContinue) {
        $sql | & mysql -uroot "-p$MysqlPwd" $DbName 2>&1 | Out-Null
    } elseif ($hasDocker) {
        $sql | & docker exec -i mysql mysql -uroot "-p$MysqlPwd" $DbName 2>&1 | Out-Null
    } else { throw 'mysql client not on PATH and docker unavailable' }

    # 全局库的路由表(fail-closed,理由见 Clear-GlobalRouteTables 上方注释)
    Do1 "mysql route tables in $GlobalDbName"
    Clear-GlobalRouteTables -HasDocker $hasDocker

    # etcd — delete all keys (node-id allocs, service discovery, locks)
    Do1 'etcd del "" --prefix'
    if ($hasDocker) {
        try { & docker exec etcd etcdctl del "" --prefix 2>&1 | Out-Null }
        catch { Warn "etcd wipe skipped (non-fatal): $($_.Exception.Message)" }
    } else { Warn 'docker unavailable — skipped etcd wipe' }

    # kafka offsets — best-effort (group must be idle; non-fatal otherwise)
    Do1 'kafka reset all consumer-group offsets --to-latest'
    if ($hasDocker) {
        try {
            & docker exec kafka /opt/kafka/bin/kafka-consumer-groups.sh `
                --bootstrap-server kafka:29092 --all-groups --all-topics `
                --reset-offsets --to-latest --execute 2>&1 | Out-Null
        } catch { Warn "kafka offset reset skipped (non-fatal): $($_.Exception.Message)" }
    }

    # logs — keep newest stress dir for diffing, prune the rest
    Do1 'prune old logs'
    $stressGlob = Join-Path $RepoRoot 'robot\logs\stress-*'
    Get-ChildItem $stressGlob -Directory -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -Skip 1 |
        ForEach-Object { Remove-Item $_.FullName -Recurse -Force -ErrorAction SilentlyContinue }
    foreach ($d in @('bin\log','run\logs')) {
        $p = Join-Path $RepoRoot $d
        if (Test-Path $p) { Get-ChildItem $p -File -Recurse -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue }
    }
}

# ────────────────────────────────────────────────────────────────────────────
function Invoke-Build {
    Hdr 'Phase: build (Go services ONLY — never compiles C++)'
    if (-not $Execute) {
        Plan "pwsh $DevTools -Command go-svc-build"
        Plan 'NOTE: gate.exe/scene.exe must already be built (C++ build is manual).'
        return
    }
    Do1 'go-svc-build (login/db/scene_manager/data_service/player_locator)'
    & pwsh -NoProfile -File $DevTools -Command go-svc-build
    if ($LASTEXITCODE -ne 0) { throw 'go-svc-build failed' }
}

# ────────────────────────────────────────────────────────────────────────────
function Invoke-Start {
    Hdr 'Phase: start (dev-start-exe: Go services + cpp gate/scene from prebuilt exe)'
    if (-not $Execute) {
        Plan "pwsh $DevTools -Command dev-start-exe"
        Plan 'Also ensure Java sa-token(18080)+gateway-node(8081) are up (started separately).'
        return
    }
    Do1 'dev-start-exe'
    & pwsh -NoProfile -File $DevTools -Command dev-start-exe
}

# ────────────────────────────────────────────────────────────────────────────
function New-RunDir {
    $ts = Get-Date -Format 'yyyyMMdd-HHmmss'
    $rd = "robot/logs/stress-round19-$ts"
    New-Item -ItemType Directory -Path (Join-Path $RepoRoot $rd) -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $RepoRoot "$rd/prom-snapshots") -Force | Out-Null
    return $rd
}

function Invoke-Run {
    Hdr 'Phase: run (launch 45k robot + background stress_snap)'
    $robotDir = Join-Path $RepoRoot 'robot'
    $robotExe = Join-Path $robotDir 'robot.exe'
    $cfgFull  = Join-Path $RepoRoot $RobotConfig

    if (-not $Execute) {
        Plan "build robot.exe if missing: (cd robot; go build -o robot.exe .)"
        Plan "robot.exe -c $RobotConfig   (45k, profile=stress, fire-and-forget)"
        Plan "stress_snap.ps1 -RunDir robot/logs/stress-round19-<ts> -StartTime <now> -Stages $Stages  (detached process)"
        return
    }
    if (-not (Test-Path $cfgFull)) { throw "robot config not found: $RobotConfig" }

    if (-not (Test-Path $robotExe)) {
        Do1 'building robot.exe'
        Push-Location $robotDir
        try { & go build -o robot.exe . ; if ($LASTEXITCODE -ne 0) { throw 'robot build failed' } }
        finally { Pop-Location }
    }

    $rd = if ($RunDir) { $RunDir } else { New-RunDir }
    $rdFull = Join-Path $RepoRoot $rd
    $script:RunDir = $rd
    $startTime = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'

    # Robot: fire-and-forget detached (avoid handle-inheritance pipe deadlocks).
    Do1 "launch robot ($RobotConfig) -> $rd"
    Start-Process -FilePath $robotExe `
        -ArgumentList @('-c', $cfgFull) `
        -WorkingDirectory $robotDir `
        -RedirectStandardOutput (Join-Path $rdFull 'robot.stdout') `
        -RedirectStandardError  (Join-Path $rdFull 'robot.stderr') `
        -WindowStyle Hidden | Out-Null

    # Snapshots: detached process so it survives this orchestrator exiting.
    # (Start-Job dies with the parent pwsh process, which would silently drop
    #  every prom snapshot — must be a real detached Start-Process instead.
    #  Use -Command (not -File): the -File form exited immediately under
    #  -WindowStyle Hidden, dropping every snapshot.)
    Do1 "start detached stress_snap (stages=$Stages, start=$startTime)"
    $snapCmd  = "& '$SnapPs1' -RunDir '$rd' -StartTime '$startTime' -Stages '$Stages'"
    $snapProc = Start-Process -FilePath 'pwsh' `
        -ArgumentList @('-NoProfile', '-Command', $snapCmd) `
        -WorkingDirectory $RepoRoot `
        -WindowStyle Hidden -PassThru
    Write-Host "  snap PID = $($snapProc.Id)" -ForegroundColor DarkGray

    Write-Host "  RunDir   = $rd"          -ForegroundColor Green
    Write-Host "  StartTime= $startTime"   -ForegroundColor Green
    Write-Host "  tail: Get-Content $rd/robot.stderr -Wait" -ForegroundColor DarkGray
}

# ────────────────────────────────────────────────────────────────────────────
function Invoke-Summarize {
    Hdr 'Phase: summarize (R18 vs R19 comparison table)'
    if (-not $RunDir) { throw 'summarize requires -RunDir (the round19 run dir)' }
    if (-not $Execute) {
        Plan "pwsh $SummPs1 -RunDir $RunDir"
        Plan "Then 2D-compare against $PrevSummary (R18). PASS = token expired -> ~0,"
        Plan 'login_fail/gave-up down sharply, pipeline enter_fail=0/DB err=0/Kafka lag=0.'
        return
    }
    Do1 "stress_summarize -RunDir $RunDir"
    & pwsh -NoProfile -File $SummPs1 -RunDir $RunDir
    Write-Host "`nCompare the table above against baseline: $PrevSummary" -ForegroundColor Yellow
    Write-Host 'PASS criteria: token expired -> ~0; login_fail & gave-up down sharply;' -ForegroundColor Yellow
    Write-Host '              pipeline stays enter_fail=0 / DB err=0 / Kafka lag=0.'      -ForegroundColor Yellow
    Write-Host 'Do NOT claim "fixed" without this comparison table (CLAUDE §6).'          -ForegroundColor Yellow
}

# ────────────────────────────────────────────────────────────────────────────
Write-Host "Round 19 orchestrator — phase=$Phase  Execute=$Execute  WipeData=$WipeData" -ForegroundColor Magenta
if (-not $Execute) { Write-Host '(dry-run: nothing will be changed; add -Execute to act)' -ForegroundColor DarkYellow }

switch ($Phase) {
    'plan'      { Invoke-Baseline; Invoke-Wipe; Invoke-Build; Invoke-Start; Invoke-Run; if (-not $RunDir) { $script:RunDir = 'robot/logs/stress-round19-<ts>' }; Invoke-Summarize }
    'baseline'  { Invoke-Baseline }
    'wipe'      { Invoke-Wipe }
    'build'     { Invoke-Build }
    'start'     { Invoke-Start }
    'run'       { Invoke-Run }
    'summarize' { Invoke-Summarize }
    'all' {
        Invoke-Baseline
        Invoke-Wipe
        Invoke-Build
        Invoke-Start
        if ($Execute) { Write-Host "`n[all] waiting 20s for services to settle..." -ForegroundColor DarkGray; Start-Sleep -Seconds 20 }
        Invoke-Run
        if ($Execute) {
            Write-Host "`n[all] robot running; waiting $SteadyMinutes min for steady-state before summarize..." -ForegroundColor Cyan
            Start-Sleep -Seconds ($SteadyMinutes * 60)
            Invoke-Summarize
        } else {
            if (-not $RunDir) { $script:RunDir = 'robot/logs/stress-round19-<ts>' }
            Invoke-Summarize
        }
    }
}

Write-Host "`nDone (phase=$Phase)." -ForegroundColor Magenta
