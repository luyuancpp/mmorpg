[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter()]
    [string]$RootPath = "",

    [Parameter()]
    [ValidateSet("audit", "apply")]
    [string]$Mode = "audit",

    [Parameter()]
    [ValidateSet("snake", "kebab")]
    [string]$Style = "snake",

    [Parameter()]
    [switch]$RenameFiles,

    [Parameter()]
    [switch]$RenameDirectories,

    [Parameter()]
    [string[]]$ExcludeRelativePaths = @(
        "third_party",
        ".git",
        ".vs",
        ".idea",
        ".vscode",
        "_copilot_session_transfer",
        "x64",
        "bin",
        "bin/logs",
        "generated",
        "cpp/generated",
        "go/generated",
        "cpp/bin",
        "cpp/libs/engine/muduo_windows",
        "cpp/libs/base/muduo_windows"
    ),

    [Parameter()]
    [string[]]$IncludeRelativePaths = @(
        "cpp",
        "go",
        "java",
        "proto",
        "docs",
        "tools",
        "scripts",
        "data",
        "deploy",
        "robot",
        "test",
        "etc"
    ),

    [Parameter()]
    [int]$MaxChanges = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RootPath)) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $RootPath = (Resolve-Path (Join-Path $scriptDir "..\..")).Path
}
else {
    $RootPath = (Resolve-Path $RootPath).Path
}

$normalizeSeparators = {
    param([string]$Path)
    return (($Path -replace "\\", "/").Trim("/"))
}

$normalizedExcludes = @()
foreach ($rel in $ExcludeRelativePaths) {
    $candidate = (& $normalizeSeparators $rel).ToLowerInvariant()
    if (-not [string]::IsNullOrWhiteSpace($candidate)) {
        $normalizedExcludes += $candidate
    }
}

$normalizedIncludes = @()
foreach ($rel in $IncludeRelativePaths) {
    $candidate = (& $normalizeSeparators $rel).ToLowerInvariant()
    if (-not [string]::IsNullOrWhiteSpace($candidate)) {
        $normalizedIncludes += $candidate
    }
}

if (-not $RenameFiles.IsPresent -and -not $RenameDirectories.IsPresent) {
    $RenameFiles = $true
    $RenameDirectories = $true
}

function Convert-ToNamingStyle {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [bool]$IsFile,
        [Parameter(Mandatory = $true)]
        [string]$CurrentStyle
    )

    $baseName = $Name
    $extension = ""

    if ($IsFile) {
        $extension = [System.IO.Path]::GetExtension($Name)
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($Name)
    }

    $s = $baseName.ToLowerInvariant()
    $s = $s -replace "\s+", "_"
    $s = $s -replace "[^a-z0-9_-]", "_"
    $s = $s -replace "[_-]{2,}", "_"
    $s = $s.Trim('_', '-')

    if ($CurrentStyle -eq "kebab") {
        $s = $s -replace "_", "-"
        $s = $s -replace "-{2,}", "-"
        $s = $s.Trim('-')
    }
    else {
        $s = $s -replace "-", "_"
        $s = $s -replace "_{2,}", "_"
        $s = $s.Trim('_')
    }

    if ([string]::IsNullOrWhiteSpace($s)) {
        $s = "unnamed"
    }

    # Only prefix directories (not data/config files) that start with a digit
    if (-not $IsFile -and $s -match "^[0-9]") {
        $prefix = if ($CurrentStyle -eq "kebab") { "n-" } else { "n_" }
        $s = $prefix + $s
    }

    return "$s$extension"
}

function Test-IsSkippedFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    # Skip Visual Studio / MSBuild / protobuf generated files
    $skipPattern = "(\.vcxproj(\.filters|\.user)?$|\.sln$|\.slnlaunch\.user$|\.tlog$|\.recipe$|\.pb\.(h|cc|go)$)"
    if ($Name -match $skipPattern) { return $true }

    # Skip double-extension template files: <base>.<code_ext>.<tmpl_ext>
    # e.g. event_handler.h.tmpl, config_template.cpp.jinja, proto_template.proto.j2
    $tmplExtensions = @(".tmpl", ".jinja", ".jinja2", ".j2")
    $codeExtensions = @(".h", ".cpp", ".c", ".go", ".proto", ".py", ".java", ".ts", ".js")
    $outerExt = [System.IO.Path]::GetExtension($Name)
    if ($tmplExtensions -contains $outerExt) {
        $innerExt = [System.IO.Path]::GetExtension([System.IO.Path]::GetFileNameWithoutExtension($Name))
        if ($codeExtensions -contains $innerExt) {
            return $true
        }
    }

    # Skip standard filenames that must not be renamed
    $standardNames = @("docker-compose.yml", "docker-compose.yaml", "maven-wrapper.properties", "README-DEPRECATED.md")
    if ($standardNames -contains $Name) { return $true }

    # Skip protoc plugins (standard naming uses hyphens: protoc-gen-*)
    if ($Name -match "^protoc-gen-") { return $true }

    return $false
}

function Test-IsExcluded {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RelativePath,
        [Parameter(Mandatory = $true)]
        [string[]]$Excludes
    )

    $normalizedRel = (& $normalizeSeparators $RelativePath).ToLowerInvariant()
    foreach ($ex in $Excludes) {
        if ($normalizedRel -eq $ex -or $normalizedRel.StartsWith($ex + "/")) {
            return $true
        }
    }

    return $false
}

function Test-IsIncluded {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RelativePath,
        [Parameter(Mandatory = $true)]
        [string[]]$Includes
    )

    if (@($Includes).Count -eq 0) {
        return $true
    }

    $normalizedRel = (& $normalizeSeparators $RelativePath).ToLowerInvariant()
    foreach ($inc in $Includes) {
        if ($normalizedRel -eq $inc -or $normalizedRel.StartsWith($inc + "/")) {
            return $true
        }
    }

    return $false
}

function Test-IsArtifactPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RelativePath
    )

    $normalizedRel = (& $normalizeSeparators $RelativePath).ToLowerInvariant()
    $artifactPattern = "(^|/)(x64|debug|release|vsout|build|out|obj|dist|cmakefiles|cmakecache\.txt)(/|$)"
    return $normalizedRel -match $artifactPattern
}

$allItems = Get-ChildItem -LiteralPath $RootPath -Recurse -Force
$proposals = @()

foreach ($item in $allItems) {
    $relative = $item.FullName.Substring($RootPath.Length).TrimStart([char]'\', [char]'/')

    if ([string]::IsNullOrWhiteSpace($relative)) {
        continue
    }

    if (Test-IsExcluded -RelativePath $relative -Excludes $normalizedExcludes) {
        continue
    }

    if (-not (Test-IsIncluded -RelativePath $relative -Includes $normalizedIncludes)) {
        continue
    }

    if (Test-IsArtifactPath -RelativePath $relative) {
        continue
    }

    if ($item.Name.StartsWith('.')) {
        continue
    }

    if ($item.PSIsContainer -and -not $RenameDirectories.IsPresent) {
        continue
    }

    if (-not $item.PSIsContainer -and -not $RenameFiles.IsPresent) {
        continue
    }

    if (-not $item.PSIsContainer -and (Test-IsSkippedFile -Name $item.Name)) {
        continue
    }

    $newName = Convert-ToNamingStyle -Name $item.Name -IsFile (-not $item.PSIsContainer) -CurrentStyle $Style
    if ($newName -eq $item.Name) {
        continue
    }

    $parentDir = if ($item.PSIsContainer) { $item.Parent.FullName } else { $item.DirectoryName }
    $targetPath = Join-Path $parentDir $newName

    $proposals += [PSCustomObject]@{
        FullName   = $item.FullName
        Relative   = $relative
        ParentDir  = $parentDir
        Depth      = ($relative -split "[\\/]").Length
        IsDir      = $item.PSIsContainer
        OldName    = $item.Name
        NewName    = $newName
        TargetPath = $targetPath
        Conflict   = $false
        Reason     = ""
    }
}

$seenTargets = @{}
foreach ($p in $proposals) {
    $key = ($p.TargetPath).ToLowerInvariant()
    if ($seenTargets.ContainsKey($key)) {
        $p.Conflict = $true
        $p.Reason = "duplicate-target"
        $first = $seenTargets[$key]
        $first.Conflict = $true
        if ([string]::IsNullOrWhiteSpace($first.Reason)) {
            $first.Reason = "duplicate-target"
        }
    }
    else {
        $seenTargets[$key] = $p
    }
}

foreach ($p in $proposals | Where-Object { -not $_.Conflict }) {
    if (Test-Path -LiteralPath $p.TargetPath) {
        $p.Conflict = $true
        $p.Reason = "target-exists"
    }
}

$valid = @($proposals | Where-Object { -not $_.Conflict })
$invalid = @($proposals | Where-Object { $_.Conflict })

if ($MaxChanges -gt 0) {
    $valid = $valid | Select-Object -First $MaxChanges
}

Write-Host "RootPath: $RootPath"
Write-Host "Mode: $Mode | Style: $Style"
Write-Host "Candidates: $($proposals.Count) | Valid: $($valid.Count) | Conflicts: $($invalid.Count)"

if ($invalid.Count -gt 0) {
    Write-Host "Conflicts (first 20):"
    $invalid | Select-Object -First 20 | ForEach-Object {
        Write-Host ("  - {0} -> {1} [{2}]" -f $_.Relative, $_.NewName, $_.Reason)
    }
}

if ($Mode -eq "audit") {
    Write-Host "Planned renames (first 200):"
    $valid | Sort-Object Depth -Descending | Select-Object -First 200 | ForEach-Object {
        Write-Host ("  - {0} -> {1}" -f $_.Relative, $_.NewName)
    }
    return
}

$ordered = $valid | Sort-Object Depth -Descending
$renamed = 0
$failed = 0

foreach ($p in $ordered) {
    if (-not (Test-Path -LiteralPath $p.FullName)) {
        $failed++
        Write-Warning ("Skipped missing path: {0}" -f $p.Relative)
        continue
    }

    if ($PSCmdlet.ShouldProcess($p.Relative, ("Rename to " + $p.NewName))) {
        try {
            Rename-Item -LiteralPath $p.FullName -NewName $p.NewName -ErrorAction Stop
            $renamed++
        }
        catch {
            $failed++
            Write-Warning ("Failed rename: {0} -> {1}. {2}" -f $p.Relative, $p.NewName, $_.Exception.Message)
        }
    }
}

Write-Host ("Completed. Renamed: {0}, Failed: {1}" -f $renamed, $failed)
