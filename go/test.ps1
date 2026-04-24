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
$DbDir  = Join-Path $GoRoot 'db'

# Packages that make up the L1 green set. Add to this list ONLY when the
# new package is hermetic (no live broker/db) and consistently green.
$L1Packages = @(
    './internal/kafka/...'
    './internal/stresstest/...'
)

$flags = @()
if ($Verbose) { $flags += '-v' }
if ($Race)    { $flags += '-race' }
# `-count=1` defeats the test cache so a green build genuinely re-runs.
$flags += '-count=1'

Push-Location $DbDir
try {
    Write-Host ">>> go test L1 ($($L1Packages -join ' '))" -ForegroundColor Cyan
    & go test @flags @L1Packages
    if ($LASTEXITCODE -ne 0) {
        Write-Host "L1_TESTS_FAIL exit=$LASTEXITCODE" -ForegroundColor Red
        exit $LASTEXITCODE
    }
    Write-Host "L1_TESTS_OK" -ForegroundColor Green
}
finally {
    Pop-Location
}
