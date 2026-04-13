<#
.SYNOPSIS
    Run include-what-you-use (IWYU) or clang-tidy misc-include-cleaner.

.DESCRIPTION
    1. Generates compile_commands.json via CMake for one or more C++ node paths.
    2. Runs IWYU (if available) or clang-tidy misc-include-cleaner as fallback.
    3. Optionally applies safe include removals with -Fix.
    4. Supports fast mode by only checking changed files with -ChangedOnly.

    Tool auto-detection order: include-what-you-use -> clang-tidy.

.PARAMETER NodePath
    One or more node paths (relative to repo root).
    Default: cpp/nodes/scene

.PARAMETER Tool
    Which tool to use: auto (default), iwyu, or clang-tidy.

.PARAMETER Fix
    For clang-tidy: pass --fix to apply safe removals in-place.
    For IWYU: run fix_includes.py / iwyu_tool.py if available.

.PARAMETER BuildDir
    Override CMake build directory. Allowed only when exactly one NodePath is given.

.PARAMETER ChangedOnly
    Only analyze files changed in git diff range.

.PARAMETER DiffBase
    Optional base ref for git diff, used as "<DiffBase>...HEAD".
    If empty, auto-detect in this order:
      1) origin/$env:GITHUB_BASE_REF...HEAD
      2) HEAD~1...HEAD
#>
param(
    [string[]]$NodePath = @("cpp/nodes/scene"),
    [ValidateSet("auto", "iwyu", "clang-tidy")]
    [string]$Tool = "auto",
    [switch]$Fix,
    [string]$BuildDir = "",
    [switch]$ChangedOnly,
    [string]$DiffBase = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$PathSep = [System.IO.Path]::DirectorySeparatorChar
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot ([System.IO.Path]::Combine("..", "..")))

function Find-Tool([string]$name) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

function Find-ClangTidy {
    $direct = Find-Tool "clang-tidy"
    if ($direct) { return $direct }
    foreach ($ver in 20, 19, 18, 17, 16, 15) {
        $versioned = Find-Tool "clang-tidy-$ver"
        if ($versioned) {
            Write-Host "[iwyu-run] Found versioned clang-tidy: clang-tidy-$ver"
            return $versioned
        }
    }
    return $null
}

function Find-IwyuFix {
    foreach ($name in "fix_includes.py", "iwyu_tool.py") {
        $exe = Find-Tool $name
        if ($exe) { return $exe }
    }
    return $null
}

function Get-RelPath([string]$full) {
    $base = $RepoRoot.Path.TrimEnd($PathSep)
    return $full.Substring($base.Length + 1).Replace('\\', '/')
}

function Resolve-DiffRange {
    if (-not [string]::IsNullOrWhiteSpace($DiffBase)) {
        return "$DiffBase...HEAD"
    }
    if (-not [string]::IsNullOrWhiteSpace($env:GITHUB_BASE_REF)) {
        return "origin/$($env:GITHUB_BASE_REF)...HEAD"
    }
    return "HEAD~1...HEAD"
}

function Get-ChangedAbsFiles([string]$nodeRel) {
    $result = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
    $range = Resolve-DiffRange
    Write-Host "[iwyu-run] ChangedOnly enabled. Diff range: $range"

    $gitOut = & git -C $RepoRoot.Path diff --name-only --diff-filter=ACMRT $range -- $nodeRel 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "[iwyu-run] git diff failed for range '$range'. Falling back to full scan for $nodeRel."
        return $null
    }

    foreach ($line in $gitOut) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        $abs = Join-Path $RepoRoot.Path $line
        if ((Test-Path $abs) -and ($abs -match '\.(c|cc|cpp|cxx)$')) {
            $null = $result.Add((Resolve-Path $abs).Path)
        }
    }
    return $result
}

function Resolve-CompileDbEntry($compileDb, [string]$filePath) {
    $target = [System.IO.Path]::GetFullPath($filePath)
    foreach ($entry in $compileDb) {
        $entryFile = $entry.file
        if (-not [System.IO.Path]::IsPathRooted($entryFile)) {
            $entryFile = Join-Path $entry.directory $entryFile
        }
        $entryFull = [System.IO.Path]::GetFullPath($entryFile)
        if ($entryFull -eq $target) {
            return $entry
        }
    }
    return $null
}

$iwyuExe = Find-Tool "include-what-you-use"
$clangTidyExe = Find-ClangTidy

$resolvedTool = switch ($Tool) {
    "iwyu" {
        if (-not $iwyuExe) { throw "include-what-you-use not found in PATH." }
        "iwyu"
    }
    "clang-tidy" {
        if (-not $clangTidyExe) { throw "clang-tidy not found in PATH." }
        "clang-tidy"
    }
    "auto" {
        if ($iwyuExe) {
            Write-Host "[iwyu-run] Auto-selected tool: include-what-you-use"
            "iwyu"
        } elseif ($clangTidyExe) {
            Write-Host "[iwyu-run] Auto-selected tool: clang-tidy (misc-include-cleaner)"
            "clang-tidy"
        } else {
            throw "Neither include-what-you-use nor clang-tidy found in PATH."
        }
    }
}

if (-not [string]::IsNullOrWhiteSpace($BuildDir) -and $NodePath.Count -ne 1) {
    throw "-BuildDir can only be used with exactly one -NodePath value."
}

$reportLines = [System.Collections.Generic.List[string]]::new()
$totalHits = 0
$scannedNodeCount = 0
$skippedNodeCount = 0

foreach ($nodeRel in $NodePath) {
    if ([string]::IsNullOrWhiteSpace($nodeRel)) { continue }

    $absNode = Join-Path $RepoRoot.Path $nodeRel
    $absBuild = if ($BuildDir) { $BuildDir } else { Join-Path $absNode "build_iwyu" }

    if (-not (Test-Path (Join-Path $absNode "CMakeLists.txt"))) {
        throw "CMakeLists.txt not found in '$absNode'. Check -NodePath."
    }

    Write-Host "[iwyu-run] Node: $nodeRel"
    Write-Host "[iwyu-run] Generating compile_commands.json in '$absBuild' ..."

    if (-not (Test-Path $absBuild)) {
        New-Item -ItemType Directory -Path $absBuild | Out-Null
    }

    & cmake -S $absNode -B $absBuild `
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
        -DCMAKE_BUILD_TYPE=Debug `
        2>&1 | ForEach-Object { Write-Verbose $_ }

    $compileCommandsJson = Join-Path $absBuild "compile_commands.json"
    if (-not (Test-Path $compileCommandsJson)) {
        throw "cmake did not produce compile_commands.json in '$absBuild'."
    }

    $srcExts = @("*.cpp", "*.cc", "*.cxx", "*.c")
    $srcFiles = Get-ChildItem -Path $absNode -Recurse -File -Include $srcExts |
        Where-Object { $_.FullName -notmatch [regex]::Escape("${PathSep}third_party${PathSep}") }

    if ($ChangedOnly) {
        $changedSet = Get-ChangedAbsFiles $nodeRel
        if ($null -eq $changedSet) {
            # git diff failed: keep full scan as safe fallback
        } elseif ($changedSet.Count -gt 0) {
            $srcFiles = $srcFiles | Where-Object { $changedSet.Contains($_.FullName) }
        } else {
            $srcFiles = @()
        }
    }

    if ($srcFiles.Count -eq 0) {
        if ($ChangedOnly) {
            Write-Host "[iwyu-run] No changed C/C++ files for '$nodeRel'; skip."
        } else {
            Write-Warning "[iwyu-run] No source files to analyze for '$nodeRel'."
        }
        $skippedNodeCount++
        continue
    }

    Write-Host "[iwyu-run] Found $($srcFiles.Count) source file(s) to analyze in '$nodeRel'."
    $scannedNodeCount++

    $nodeHits = 0

    if ($resolvedTool -eq "clang-tidy") {
        $checksArg = "--checks=-*,misc-include-cleaner"
        $fixHint = if ($Fix) { " (--fix enabled)" } else { " (dry-run; use -Fix to apply)" }
        Write-Host "[iwyu-run] Running clang-tidy misc-include-cleaner$fixHint ..."

        foreach ($f in $srcFiles) {
            $rel = Get-RelPath $f.FullName
            $args = @($f.FullName, $checksArg, "-p", $absBuild, "--header-filter=.*")
            if ($Fix) { $args += "--fix" }

            $out = & $clangTidyExe @args 2>&1
            $hits = $out | Where-Object { $_ -match "warning:|error:" }
            if ($hits) {
                if ($nodeHits -eq 0) {
                    $reportLines.Add("### $nodeRel ###")
                }
                $reportLines.Add("=== $rel ===")
                $hits | ForEach-Object { $reportLines.Add("  $_") }
                $nodeHits += $hits.Count
            }
        }
    } else {
        Write-Host "[iwyu-run] Running include-what-you-use ..."
        $iwyuOut = [System.Collections.Generic.List[string]]::new()
        $compileDb = Get-Content $compileCommandsJson | ConvertFrom-Json

        foreach ($f in $srcFiles) {
            $rel = Get-RelPath $f.FullName
            $entry = Resolve-CompileDbEntry $compileDb $f.FullName
            if (-not $entry) { continue }

            $cmd = $entry.command
            if ([string]::IsNullOrWhiteSpace($cmd)) {
                continue
            }
            $cmdParts = $cmd -split '\\s+'
            if ($cmdParts.Count -lt 2) {
                continue
            }
            $iwyuArgs = $cmdParts[1..($cmdParts.Count - 1)]

            $out = & $iwyuExe @iwyuArgs 2>&1
            $joined = $out -join "`n"
            if ($joined -match "should (add|remove)") {
                if ($nodeHits -eq 0) {
                    $reportLines.Add("### $nodeRel ###")
                }
                $reportLines.Add("=== $rel ===")
                $out | ForEach-Object { $reportLines.Add("  $_") }
                $iwyuOut.AddRange([string[]]$out)
                $nodeHits++
            }
        }

        if ($Fix) {
            $fixIncludes = Find-IwyuFix
            if ($fixIncludes) {
                Write-Host "[iwyu-run] Applying fixes via $fixIncludes ..."
                $iwyuOut | & python $fixIncludes
            } else {
                Write-Warning "[iwyu-run] fix_includes.py / iwyu_tool.py not found; skipping auto-fix."
            }
        }
    }

    $totalHits += $nodeHits
}

$reportPath = Join-Path $RepoRoot.Path "bin" "iwyu_report.txt"
if ($reportLines.Count -gt 0) {
    $reportLines | Set-Content -Path $reportPath -Encoding UTF8
    Write-Host ""
    Write-Host "[iwyu-run] Report ($totalHits hit(s))"
    $reportLines | ForEach-Object { Write-Host $_ }
    Write-Host "[iwyu-run] Full report saved: $reportPath"
} else {
    Write-Host "[iwyu-run] No redundant includes detected."
    "No redundant includes detected." | Set-Content -Path $reportPath -Encoding UTF8
}

Write-Host "[iwyu-run] Summary: scanned_nodes=$scannedNodeCount skipped_nodes=$skippedNodeCount hits=$totalHits"
