Write-Host ">>> Running Sync-SubmoduleBranches.ps1..." -ForegroundColor Green

$repoRoot = git rev-parse --show-toplevel
Set-Location $repoRoot
Write-Host ">>> Repository root: $repoRoot" -ForegroundColor Cyan

git submodule update --init --recursive
Write-Host ">>> Submodules initialized" -ForegroundColor Cyan

$submodules = git config -f .gitmodules --get-regexp path

if (-not $submodules) {
    Write-Host "⚠️  No submodules found. Please check your .gitmodules file." -ForegroundColor Red
    exit
}

foreach ($line in $submodules) {
    Write-Host "`n>>> Processing: $line" -ForegroundColor Yellow

    $parts = $line -split '\s+'
    if ($parts.Length -ne 2) { continue }

    $key = $parts[0]
    $path = $parts[1]

    $name = $key -replace '^submodule\.', '' -replace '\.path$', ''
    $branch = git config -f .gitmodules submodule.$name.branch

    if (![string]::IsNullOrEmpty($branch)) {
        Write-Host ">>> Setting submodule '$name' at '$path' to track branch '$branch'" -ForegroundColor Cyan

        git config submodule.$name.branch "$branch"

        Push-Location $path
        Write-Host ">>> Entering $path" -ForegroundColor DarkGray
        git fetch origin 2>&1 | Write-Host
        git checkout "$branch" 2>&1 | Write-Host
        git pull origin "$branch" 2>&1 | Write-Host
        Pop-Location
    }
    else {
        Write-Host ">>> Submodule '$name' has no branch configured. Skipping." -ForegroundColor DarkYellow
    }
}

Write-Host "`n✅ All submodules processed." -ForegroundColor Green
