<#
.SYNOPSIS
    Run include-what-you-use (IWYU) or clang-tidy misc-include-cleaner on a C++ node.

.DESCRIPTION
    1. Generates compile_commands.json via CMake for the target node.
    2. Runs IWYU (if available) or clang-tidy misc-include-cleaner as fallback.
    3. Optionally applies safe include removals with -Fix.

    Tool auto-detection order: include-what-you-use -> clang-tidy.

.PARAMETER NodePath
    Path to the CMake node directory, relative to the repo root.
    Defaults to "cpp/nodes/scene".
    Example: "cpp/nodes/gate", "cpp/nodes/scene"

.PARAMETER Tool
    Which tool to use: 'auto' (default), 'iwyu', or 'clang-tidy'.

.PARAMETER Fix
    For clang-tidy: pass --fix to apply safe removals in-place.
    For IWYU: run fix_includes.py if it exists in PATH.

.PARAMETER BuildDir
    Override the CMake build directory. Defaults to <NodePath>/build_iwyu.

.EXAMPLE
    # Dry-run analysis on the scene node
    pwsh -File tools/scripts/iwyu_run.ps1

    # Analysis on the gate node
    pwsh -File tools/scripts/iwyu_run.ps1 -NodePath cpp/nodes/gate

    # Apply fixes via clang-tidy
    pwsh -File tools/scripts/iwyu_run.ps1 -Tool clang-tidy -Fix
#>
param(
    [string]$NodePath  = "cpp/nodes/scene",
    [ValidateSet("auto", "iwyu", "clang-tidy")]
    [string]$Tool      = "auto",
    [switch]$Fix,
    [string]$BuildDir  = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$PathSep  = [System.IO.Path]::DirectorySeparatorChar

$RepoRoot  = Resolve-Path (Join-Path $PSScriptRoot ([System.IO.Path]::Combine("..", "..")))
$AbsNode   = Join-Path $RepoRoot $NodePath
$AbsBuild  = if ($BuildDir) { $BuildDir } else { Join-Path $AbsNode "build_iwyu" }

# ─── helper ───────────────────────────────────────────────────────────────────
function Find-Tool([string]$name) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    return $cmd ? $cmd.Source : $null
}

# On Linux, clang-tidy may be installed as clang-tidy-18, clang-tidy-17, etc.
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

# On Linux, fix_includes.py may be 'iwyu_tool.py' or 'fix_includes.py'
function Find-IwyuFix {
    foreach ($name in "fix_includes.py", "iwyu_tool.py") {
        $exe = Find-Tool $name
        if ($exe) { return $exe }
    }
    return $null
}

function Require-CMakeLists {
    if (-not (Test-Path (Join-Path $AbsNode "CMakeLists.txt"))) {
        throw "CMakeLists.txt not found in '$AbsNode'. Check -NodePath."
    }
}

# Cross-platform relative path (always forward slashes in output)
function Get-RelPath([string]$full) {
    $base = $RepoRoot.Path.TrimEnd($PathSep)
    return $full.Substring($base.Length + 1).Replace('\','/')
}

# ─── resolve tool ─────────────────────────────────────────────────────────────
$iwyuExe      = Find-Tool "include-what-you-use"
$clangTidyExe = Find-ClangTidy

$resolvedTool = switch ($Tool) {
    "iwyu"       {
        if (-not $iwyuExe) { throw "include-what-you-use not found in PATH.`nWindows: install LLVM; Linux: sudo apt install iwyu" }
        "iwyu"
    }
    "clang-tidy" {
        if (-not $clangTidyExe) { throw "clang-tidy not found in PATH.`nWindows: winget install LLVM.LLVM; Linux: sudo apt install clang-tidy" }
        "clang-tidy"
    }
    "auto"       {
        if ($iwyuExe)          { Write-Host "[iwyu-run] Auto-selected tool: include-what-you-use"; "iwyu" }
        elseif ($clangTidyExe) { Write-Host "[iwyu-run] Auto-selected tool: clang-tidy (misc-include-cleaner)"; "clang-tidy" }
        else {
            throw @"
Neither include-what-you-use nor clang-tidy found in PATH.
  Windows : winget install LLVM.LLVM
  Linux   : sudo apt install clang-tidy   (or: sudo apt install iwyu)
"@
        }
    }
}

# ─── step 1: generate compile_commands.json ───────────────────────────────────
Require-CMakeLists
Write-Host "[iwyu-run] Generating compile_commands.json in '$AbsBuild' ..."

if (-not (Test-Path $AbsBuild)) { New-Item -ItemType Directory -Path $AbsBuild | Out-Null }

& cmake -S $AbsNode -B $AbsBuild `
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
    -DCMAKE_BUILD_TYPE=Debug `
    2>&1 | ForEach-Object { Write-Verbose $_ }

$compileCommandsJson = Join-Path $AbsBuild "compile_commands.json"
if (-not (Test-Path $compileCommandsJson)) {
    throw "cmake did not produce compile_commands.json in '$AbsBuild'."
}
Write-Host "[iwyu-run] compile_commands.json ready."

# ─── step 2: collect source files ─────────────────────────────────────────────
$srcExts = @("*.cpp","*.cc","*.cxx","*.c")
$srcFiles = Get-ChildItem -Path $AbsNode -Recurse -File -Include $srcExts |
    Where-Object { $_.FullName -notmatch [regex]::Escape("${PathSep}third_party${PathSep}") }

if ($srcFiles.Count -eq 0) {
    Write-Warning "[iwyu-run] No source files found under '$AbsNode'."
    exit 0
}

Write-Host "[iwyu-run] Found $($srcFiles.Count) source file(s) to analyse."

# ─── step 3: run analysis ─────────────────────────────────────────────────────
$reportLines = [System.Collections.Generic.List[string]]::new()
$fixCount    = 0

switch ($resolvedTool) {

    "clang-tidy" {
        $checksArg = "--checks=-*,misc-include-cleaner"
        $fixArg    = if ($Fix) { "--fix" } else { "" }
        $fixHint   = if ($Fix) { " (--fix enabled)" } else { " (dry-run; use -Fix to apply)" }
        Write-Host "[iwyu-run] Running clang-tidy misc-include-cleaner$fixHint ..."

        foreach ($f in $srcFiles) {
            $rel  = Get-RelPath $f.FullName
            $args = @($f.FullName, $checksArg, "-p", $AbsBuild, "--header-filter=.*")
            if ($Fix) { $args += "--fix" }

            $out = & $clangTidyExe @args 2>&1
            $hits = $out | Where-Object { $_ -match "warning:|error:" }
            if ($hits) {
                $reportLines.Add("=== $rel ===")
                $hits | ForEach-Object { $reportLines.Add("  $_") }
                $fixCount += $hits.Count
            }
        }
    }

    "iwyu" {
        Write-Host "[iwyu-run] Running include-what-you-use ..."
        $iwyuOut = [System.Collections.Generic.List[string]]::new()

        foreach ($f in $srcFiles) {
            $rel = Get-RelPath $f.FullName
            # Read compile flags from compile_commands.json
            $db  = Get-Content $compileCommandsJson | ConvertFrom-Json
            $entry = $db | Where-Object { $_.file -like "*$($f.Name)" } | Select-Object -First 1
            if (-not $entry) { continue }

            # Build IWYU invocation from compile command
            $cmdParts = $entry.command -split '\s+'
            $iwyuArgs = $cmdParts[1..$cmdParts.Length]   # drop the compiler exe

            $out = & $iwyuExe @iwyuArgs 2>&1
            $report = $out -join "`n"
            if ($report -match "should (add|remove)") {
                $reportLines.Add("=== $rel ===")
                $out | ForEach-Object { $reportLines.Add("  $_") }
                $iwyuOut.AddRange([string[]]$out)
                $fixCount++
            }
        }

        if ($Fix) {
            $fixIncludes = Find-IwyuFix
            if ($fixIncludes) {
                Write-Host "[iwyu-run] Applying fixes via $fixIncludes ..."
                $iwyuOut | & python $fixIncludes
            } else {
                Write-Warning "[iwyu-run] fix_includes.py / iwyu_tool.py not found in PATH; skipping auto-fix."
                Write-Warning "[iwyu-run]   Linux  : sudo apt install iwyu  (includes fix_includes.py)"
                Write-Warning "[iwyu-run]   Windows: extract fix_includes.py from the LLVM/IWYU package."
            }
        }
    }
}

# ─── step 4: report ───────────────────────────────────────────────────────────
$reportPath = Join-Path $RepoRoot.Path "bin" "iwyu_report.txt"
if ($reportLines.Count -gt 0) {
    $reportLines | Set-Content -Path $reportPath -Encoding UTF8
    Write-Host ""
    Write-Host "[iwyu-run] ── Report ($fixCount hit(s)) ──────────────────────────────────"
    $reportLines | ForEach-Object { Write-Host $_ }
    Write-Host "[iwyu-run] Full report saved: $reportPath"
} else {
    Write-Host "[iwyu-run] No redundant includes detected. Project is clean."
    "No redundant includes detected." | Set-Content -Path $reportPath -Encoding UTF8
}
