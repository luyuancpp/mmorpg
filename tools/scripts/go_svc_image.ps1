<#
.SYNOPSIS
    Build and push Docker images for Go micro-services.

.DESCRIPTION
    Iterates over the Go service catalogue and builds each service into its
    own Docker image using deploy/k8s/Dockerfile.go-svc.
    Image naming: {Registry}/{ServiceName}:{Tag}

.EXAMPLE
    # Build all Go service images
    pwsh -File tools/scripts/go_svc_image.ps1 -Command build-all `
        -Registry ghcr.io/luyuancpp -Tag v1

    # Push all Go service images
    pwsh -File tools/scripts/go_svc_image.ps1 -Command push-all `
        -Registry ghcr.io/luyuancpp -Tag v1

    # Build + push in one shot
    pwsh -File tools/scripts/go_svc_image.ps1 -Command release-all `
        -Registry ghcr.io/luyuancpp -Tag v1
#>
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("build-all", "push-all", "release-all", "list")]
    [string]$Command,

    [string]$Registry = "ghcr.io/luyuancpp",
    [string]$Tag = "latest",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Resolve-Path (Join-Path $ScriptDir "..\..")
$GoRoot    = Join-Path $RepoRoot "go"
$Dockerfile = Join-Path $RepoRoot "deploy\k8s\Dockerfile.go-svc"

# Service catalogue: name → { Dir (in go/), Entry (.go file), ImageName }
$Catalogue = [ordered]@{
    db              = @{ Dir = "db";              Entry = "db.go";                  ImageName = "mmorpg-db" }
    "data-service"  = @{ Dir = "data_service";    Entry = "data_service.go";         ImageName = "mmorpg-data-service" }
    login           = @{ Dir = "login";           Entry = "login.go";               ImageName = "mmorpg-login" }
    "player-locator"= @{ Dir = "player_locator";  Entry = "player_locator.go";      ImageName = "mmorpg-player-locator" }
    "scene-manager" = @{ Dir = "scene_manager";   Entry = "scene_manager_service.go"; ImageName = "mmorpg-scene-manager" }
}

function Get-ImageFullName {
    param([string]$ImageName)
    return "$Registry/${ImageName}:${Tag}"
}

function Invoke-BuildAll {
    if (-not (Test-Path $Dockerfile)) {
        throw "Dockerfile not found: $Dockerfile"
    }

    foreach ($kv in $Catalogue.GetEnumerator()) {
        $svc  = $kv.Key
        $info = $kv.Value
        $fullImage = Get-ImageFullName -ImageName $info.ImageName

        Write-Host "[build] $svc -> $fullImage" -ForegroundColor Cyan

        $buildArgs = @(
            "build",
            "-f", $Dockerfile,
            "--build-arg", "SERVICE=$($info.Dir)",
            "--build-arg", "ENTRY=$($info.Entry)",
            "-t", $fullImage,
            $GoRoot
        )

        if ($DryRun) {
            Write-Host "  [dry-run] docker $($buildArgs -join ' ')" -ForegroundColor DarkGray
        } else {
            & docker @buildArgs
            if ($LASTEXITCODE -ne 0) {
                throw "Docker build failed for $svc"
            }
            Write-Host "  [ok] $fullImage" -ForegroundColor Green
        }
    }
}

function Invoke-PushAll {
    foreach ($kv in $Catalogue.GetEnumerator()) {
        $svc  = $kv.Key
        $info = $kv.Value
        $fullImage = Get-ImageFullName -ImageName $info.ImageName

        Write-Host "[push] $svc -> $fullImage" -ForegroundColor Magenta

        if ($DryRun) {
            Write-Host "  [dry-run] docker push $fullImage" -ForegroundColor DarkGray
        } else {
            & docker push $fullImage
            if ($LASTEXITCODE -ne 0) {
                throw "Docker push failed for $svc"
            }
            Write-Host "  [ok] $fullImage" -ForegroundColor Green
        }
    }
}

function Invoke-List {
    Write-Host "`nGo service images (registry=$Registry tag=$Tag):`n" -ForegroundColor Cyan
    Write-Host ("{0,-18} {1,-30} {2}" -f "SERVICE", "IMAGE", "FULL") -ForegroundColor White
    Write-Host ("{0,-18} {1,-30} {2}" -f "-------", "-----", "----")
    foreach ($kv in $Catalogue.GetEnumerator()) {
        $full = Get-ImageFullName -ImageName $kv.Value.ImageName
        Write-Host ("{0,-18} {1,-30} {2}" -f $kv.Key, $kv.Value.ImageName, $full)
    }
    Write-Host ""
}

switch ($Command) {
    "build-all"   { Invoke-BuildAll }
    "push-all"    { Invoke-PushAll }
    "release-all" { Invoke-BuildAll; Invoke-PushAll }
    "list"        { Invoke-List }
}
