# ==== Configuration ====
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootPath = Resolve-Path (Join-Path $ScriptDir "..")
$ExcludePaths = @()

$OutputFile = Join-Path $RootPath "docs\tree.txt"
$MaxDepth = 100  # Max recursion depth (including root)

# ==== Main function ====
function Show-Tree {
    param (
        [string]$Path,
        [int]$Indent = 0,
        [int]$Depth = 20
    )

    # Check if path is in exclusion list
    foreach ($exclude in $ExcludePaths) {
        if ($Path -like "$exclude*") {
            return
        }
    }

    $prefix = ' ' * $Indent + '|-- '
    Add-Content -Path $OutputFile -Value "$prefix$(Split-Path $Path -Leaf)"

    # Stop recursion if max depth reached
    if ($Depth -ge $MaxDepth) {
        return
    }

    # List subdirectories
    Get-ChildItem -Path $Path -Directory -Force | Sort-Object Name | ForEach-Object {
        Show-Tree -Path $_.FullName -Indent ($Indent + 4) -Depth ($Depth + 1)
    }

    # List files
    Get-ChildItem -Path $Path -File -Force | Sort-Object Name | ForEach-Object {
        $filePrefix = ' ' * ($Indent + 4) + '|-- '
        Add-Content -Path $OutputFile -Value "$filePrefix$($_.Name)"
    }
}

# ==== Execution ====
# Create or clear the output file
Set-Content -Path $OutputFile -Value "" -Force
# Start building the tree
Show-Tree -Path $RootPath
