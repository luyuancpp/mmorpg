<#
.SYNOPSIS
    Run the green L1 unit-test subset for the go side of the data-consistency
    stress harness. Designed to be cheap enough to run on every CI build.

.DESCRIPTION
    The L1 layer (see docs/design/data-consistency-stress-testing.md) is the
    only one that needs no live Kafka/MySQL/Redis — it uses miniredis +
    in-process mocks. Anything heavier (data_stress, verifier end-to-end,
    chaos_test.ps1) is L2+ and must be invoked manually.

    The two test sets we lock in for CI:

      * db/internal/kafka          – KeyOrderedKafkaConsumer
                                     (processTaskBatch coalescing,
                                      retry-queue write-back, applied-seq
                                      monotonic guard). These are the three
                                      regression tests for TC3 / TC5a / TC5b.
      * db/internal/stresstest     – ComputeSig golden vectors that the cpp
                                     Scene-side probe also reproduces, plus
                                     the round-trip / tampering tests.

    Player storage placement (docs/design/player-storage-placement.md), all
    hermetic — no broker / MySQL / Redis; SQL goes through the in-memory
    driver db/internal/dbtest, files read are repo files only:

      * db/internal/logic/pkg/proto_sql – StoreRegistry (on-demand open,
                                     admission, backoff), schema gate and
                                     the table-schema contract.
      * db/internal/config         – Placement block parsing + etc/db.yaml.
      * db/cmd/migrate             – -storage-id target resolution.
      * shared/placement           – key / value codec vectors (separate
                                     go module, run from go/shared).

    db_task 分区契约(docs/design/db-task-kafka-partition-contract.md):

      * shared/kafkautil           – CRC-32(key) mod N 的 golden 向量(与 C++
                                     librdkafka 测试共读)、key 编码、上线抽样
                                     解析器。broker 集成用例没有
                                     KAFKA_INTEGRATION_BROKER 时 Skip,抽样复核
                                     没有 DBTASK_AUDIT_DUMP 时 Skip。
      * login/internal/kafka       – db_task 主生产者配置与分区器、key 契约校验、
                                     db_task topic 拒绝、LeaderNotAvailable 自锁
                                     回归(只用 sarama mocks;broker 集成用例没有
                                     KAFKA_INTEGRATION_BROKER 时 Skip)。

    task:result 读回执通知按 login 实例分频道(2026-10-18,只用进程内 miniredis):

      * shared/taskresult          – 旧频道字面值、实例频道拼法与回复目标校验。
      * login/internal/dispatcher  – 双频道订阅、按频道送达与计数、TTL 兜底取回
                                     (salvaged / timed_out,不依赖墙钟)。
      * login/internal/logic/pkg/dataloader
                                   – 读任务带回复目标,以及同包的新号种子、
                                     冷恢复提交屏障用例。
      * db/internal/metrics        – (2026-10-18 第 2 批起)db_task_result_notify_total
                                     等预建序列与取值字面量(告警按它们写),以及与
                                     stress_summarize.ps1 列名的对照;同包既有用例
                                     钉住 db 的其余预建序列。只读仓库内文件。
                                     go/db 侧的定向发布用例在 db/internal/kafka 里。

    Anything that imports a real broker (sarama integration tests) is left
    out — those go in the chaos_test.ps1 path instead.

.PARAMETER Verbose
    Pass -v to `go test`. Useful when something goes red.

.PARAMETER Race
    Run with -race. Slower but catches the kind of consumer/worker races
    that are easy to introduce in processTaskBatch.

.EXAMPLE
    pwsh -File go/test.ps1
    pwsh -File go/test.ps1 -Verbose -Race
#>
param(
    [switch]$Verbose,
    [switch]$Race
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$GoRoot = $PSScriptRoot

# Packages that make up the L1 green set, grouped by go module (each module
# is tested from its own directory). Add to this list ONLY when the new
# package is hermetic (no live broker/db) and consistently green.
$L1Suites = @(
    @{
        Dir      = Join-Path $GoRoot 'db'
        Packages = @(
            './internal/kafka/...'
            './internal/stresstest/...'
            './internal/logic/pkg/proto_sql/...'
            './internal/config/...'
            './cmd/migrate/...'
            './internal/metrics/...'
        )
    }
    @{
        Dir      = Join-Path $GoRoot 'shared'
        Packages = @(
            './placement/...'
            './kafkautil/...'
            './taskresult/...'
        )
    }
    @{
        Dir      = Join-Path $GoRoot 'login'
        Packages = @(
            './internal/kafka/...'
            './internal/dispatcher/...'
            './internal/logic/pkg/dataloader/...'
        )
    }
)

$flags = @()
if ($Verbose) { $flags += '-v' }
if ($Race)    { $flags += '-race' }
# `-count=1` defeats the test cache so a green build genuinely re-runs.
$flags += '-count=1'

foreach ($suite in $L1Suites) {
    $packages = $suite.Packages
    Push-Location $suite.Dir
    try {
        Write-Host ">>> go test L1 [$(Split-Path -Leaf $suite.Dir)] ($($packages -join ' '))" -ForegroundColor Cyan
        & go test @flags @packages
        if ($LASTEXITCODE -ne 0) {
            Write-Host "L1_TESTS_FAIL exit=$LASTEXITCODE" -ForegroundColor Red
            exit $LASTEXITCODE
        }
    }
    finally {
        Pop-Location
    }
}
Write-Host "L1_TESTS_OK" -ForegroundColor Green
