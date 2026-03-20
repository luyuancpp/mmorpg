<#
.SYNOPSIS
    Build gRPC (Release + Debug) using Ninja + MSVC cl.exe, then install to
    grpc/install_vs2026/ (Release) and grpc/install_vs2026_dbg/ (Debug).

.DESCRIPTION
    This script automates the full gRPC build for the MMORPG project.
    It locates grpc source at ./grpc (relative to this script), uses vswhere
    to locate the newest MSVC toolchain, then runs CMake with the Ninja
    generator against both Release and Debug configurations.

    If cmake or ninja is missing or outdated, the script will auto-install /
    upgrade them via winget (requires Windows 10 1709+ or Windows 11).
    Visual Studio with C++ Desktop workload must be installed.

.PARAMETER BuildRelease
    Build the Release configuration (default: true).
.PARAMETER BuildDebug
    Build the Debug configuration (default: true).
.PARAMETER SkipToolCheck
    Skip the cmake/ninja version check and auto-install step.
.PARAMETER Jobs
    Parallel build jobs passed to ninja (-j). Default: number of logical CPUs.
.PARAMETER Clean
    If set, removes existing build directories before building.
.EXAMPLE
    # Build both Release and Debug (default)
    .\build_grpc.ps1

    # Release only, 8 parallel jobs
    .\build_grpc.ps1 -BuildDebug:$false -Jobs 8

    # Clean rebuild
    .\build_grpc.ps1 -Clean

    # Skip auto-install of cmake/ninja
    .\build_grpc.ps1 -SkipToolCheck
#>
[CmdletBinding()]
param(
    [bool]$BuildRelease  = $true,
    [bool]$BuildDebug    = $true,
    [int]   $Jobs          = 0,
    [switch]$Clean,
    [switch]$SkipToolCheck
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Resolve repository root from tools/scripts/third_party and locate grpc source.
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$GrpcRoot = Join-Path $RepoRoot 'third_party\grpc'
if (-not (Test-Path (Join-Path $GrpcRoot 'CMakeLists.txt'))) {
    Write-Error "Cannot find grpc CMakeLists.txt at $GrpcRoot."
}

function Ensure-WingetAvailable {
    $wg = Get-Command winget -ErrorAction SilentlyContinue
    if (-not $wg) {
        Write-Error "winget is not available. Install cmake and ninja manually, or install winget (App Installer) from the Microsoft Store."
    }
}

$ToolDefs = @{
    cmake = @{ WingetId = 'Kitware.CMake';      MinVersion = [version]'3.20.0' }
    ninja = @{ WingetId = 'Ninja-build.Ninja';  MinVersion = [version]'1.11.0' }
}

function Get-ToolVersion([string]$Name) {
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $cmd) { return $null }

    switch ($Name) {
        'cmake' {
            $out = & $cmd.Source --version 2>$null | Select-Object -First 1
            if ($out -match '(\d+\.\d+\.\d+)') { return [version]$Matches[1] }
        }
        'ninja' {
            $out = & $cmd.Source --version 2>$null | Select-Object -First 1
            if ($out -match '(\d+\.\d+\.\d+)') { return [version]$Matches[1] }
        }
    }

    return $null
}

function Ensure-Tool([string]$Name) {
    $def        = $ToolDefs[$Name]
    $currentVer = Get-ToolVersion $Name

    if ($currentVer -and $currentVer -ge $def.MinVersion) {
        Write-Host "[info] $Name $currentVer is OK (>= $($def.MinVersion))"
        return
    }

    Ensure-WingetAvailable

    if (-not $currentVer) {
        Write-Host "[install] $Name not found - installing via winget ..."
        winget install --id $def.WingetId --accept-source-agreements --accept-package-agreements --silent
    } else {
        Write-Host "[upgrade] $Name $currentVer is below minimum $($def.MinVersion) - upgrading via winget ..."
        winget upgrade --id $def.WingetId --accept-source-agreements --accept-package-agreements --silent
    }

    $wingetExit = $LASTEXITCODE

    # Refresh PATH so the new/existing binary is visible in this session.
    $machinePath = [System.Environment]::GetEnvironmentVariable('Path', 'Machine')
    $userPath    = [System.Environment]::GetEnvironmentVariable('Path', 'User')
    $env:Path    = "$machinePath;$userPath"

    $newVer = Get-ToolVersion $Name
    if ($newVer -and $newVer -ge $def.MinVersion) {
        Write-Host "[info] $Name $newVer is OK after winget (>= $($def.MinVersion))"
        return
    }

    if ($wingetExit -ne 0) {
        Write-Error "winget $Name install/upgrade failed (exit code $wingetExit) and $Name $newVer is still not adequate. Install manually."
    }

    if (-not $newVer) {
        Write-Error "$Name still not found on PATH after install. You may need to restart your terminal."
    }

    Write-Host "[info] $Name $newVer installed successfully"
}

if (-not $SkipToolCheck) {
    Ensure-Tool 'cmake'
    Ensure-Tool 'ninja'
}

function Resolve-Tool([string]$Name) {
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $cmd) { Write-Error "'$Name' not found on PATH. Please install it." }
    return $cmd.Source
}

function Convert-ToCMakePath([string]$Path) {
    return $Path -replace '\\', '/'
}

$CMake = Resolve-Tool 'cmake'
$Ninja = Resolve-Tool 'ninja'
Write-Host "[info] cmake : $CMake"
Write-Host "[info] ninja : $Ninja"

function Find-ClExe {
    $vswhereLocations = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe"
    )

    $vswhere = $null
    foreach ($p in $vswhereLocations) {
        if (Test-Path $p) { $vswhere = $p; break }
    }
    if (-not $vswhere) { Write-Error "vswhere.exe not found. Install Visual Studio with C++ workload." }

    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) { Write-Error "No VS installation with C++ VC tools found." }

    $msvcVersionFile = Join-Path $vsPath 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt'
    if (-not (Test-Path $msvcVersionFile)) { Write-Error "Cannot determine MSVC toolset version." }

    $msvcVer = (Get-Content $msvcVersionFile -Raw).Trim()
    $clPath = Join-Path $vsPath "VC\Tools\MSVC\$msvcVer\bin\Hostx64\x64\cl.exe"
    if (-not (Test-Path $clPath)) { Write-Error "cl.exe not found at $clPath" }

    return @{
        VsPath   = $vsPath
        MsvcVer  = $msvcVer
        ClExe    = $clPath
        MsvcBase = Join-Path $vsPath "VC\Tools\MSVC\$msvcVer"
    }
}

$msvc = Find-ClExe
Write-Host "[info] VS    : $($msvc.VsPath)"
Write-Host "[info] MSVC  : $($msvc.MsvcVer)"
Write-Host "[info] cl.exe: $($msvc.ClExe)"

$ClExeCMakePath = Convert-ToCMakePath $msvc.ClExe
$NinjaCMakePath = Convert-ToCMakePath $Ninja

function Enter-MsvcEnv([hashtable]$Msvc) {
    $vcvars = Join-Path $Msvc.VsPath 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $vcvars)) { Write-Error "vcvars64.bat not found at $vcvars" }

    $envBlock = cmd /c "`"$vcvars`" >nul 2>&1 && set" 2>&1
    foreach ($line in $envBlock) {
        if ($line -match '^([^=]+)=(.*)$') {
            [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
        }
    }

    Write-Host "[info] MSVC environment loaded from vcvars64.bat"
}

Enter-MsvcEnv $msvc

if ($Jobs -le 0) {
    $Jobs = [System.Environment]::ProcessorCount
}
Write-Host "[info] Parallel jobs: $Jobs"

$CommonArgs = @(
    "-G", "Ninja",
    "-DCMAKE_C_COMPILER=$ClExeCMakePath",
    "-DCMAKE_CXX_COMPILER=$ClExeCMakePath",
    "-DCMAKE_MAKE_PROGRAM=$NinjaCMakePath",
    "-DCMAKE_CXX_STANDARD=20",
    "-DCMAKE_CXX_STANDARD_REQUIRED=ON",
    "-DCMAKE_CXX_EXTENSIONS=OFF",
    "-DgRPC_BUILD_TESTS=OFF",
    "-DgRPC_BUILD_GRPCPP_OTEL_PLUGIN=OFF",
    "-Dprotobuf_BUILD_TESTS=OFF",
    "-DABSL_BUILD_TESTING=OFF",
    "-DABSL_BUILD_TEST_HELPERS=OFF"
)

function Invoke-GrpcBuild {
    param(
        [string]$BuildType,
        [string]$BuildDir,
        [string]$InstallDir
    )

    $buildPath   = Join-Path $GrpcRoot $BuildDir
    $installPath = Join-Path $GrpcRoot $InstallDir

    Write-Host "`n=============================================="
    Write-Host "  gRPC $BuildType build"
    Write-Host "  Build dir  : $buildPath"
    Write-Host "  Install dir: $installPath"
    Write-Host "==============================================`n"

    if ($Clean -and (Test-Path $buildPath)) {
        Write-Host "[clean] Removing $buildPath ..."
        Remove-Item -Recurse -Force $buildPath
    }

    $cachePath = Join-Path $buildPath 'CMakeCache.txt'
    if (Test-Path $cachePath) {
        $cacheCompilerLine = Select-String -Path $cachePath -Pattern '^CMAKE_C_COMPILER:FILEPATH=' -SimpleMatch:$false | Select-Object -First 1
        if ($cacheCompilerLine) {
            $cachedCompiler = $cacheCompilerLine.Line.Split('=', 2)[1]
            if ((Convert-ToCMakePath $cachedCompiler) -ne $ClExeCMakePath) {
                Write-Host "[clean] Cached compiler changed from $cachedCompiler to $ClExeCMakePath - resetting $buildPath ..."
                Remove-Item -Recurse -Force $buildPath
            }
        }
    }

    if (-not (Test-Path $buildPath)) { New-Item -ItemType Directory -Path $buildPath | Out-Null }

    Write-Host "[1/3] CMake configure ($BuildType) ..."
    $configArgs = $CommonArgs + @(
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-DCMAKE_INSTALL_PREFIX=$installPath",
        $GrpcRoot
    )
    & $CMake @configArgs -B $buildPath
    if ($LASTEXITCODE -ne 0) { Write-Error "CMake configure failed for $BuildType" }

    Write-Host "[2/3] Building ($BuildType) ..."
    & $CMake --build $buildPath --config $BuildType -j $Jobs
    if ($LASTEXITCODE -ne 0) { Write-Error "Build failed for $BuildType" }

    Write-Host "[3/3] Installing ($BuildType) -> $installPath ..."
    & $CMake --install $buildPath --prefix $installPath --config $BuildType
    if ($LASTEXITCODE -ne 0) { Write-Error "Install failed for $BuildType" }

    Write-Host "`n[done] $BuildType build installed to $installPath`n"
}

$sw = [System.Diagnostics.Stopwatch]::StartNew()

if ($BuildRelease) {
    Invoke-GrpcBuild -BuildType 'Release' `
                     -BuildDir  '.build_ninja_release' `
                     -InstallDir 'install_vs2026'
}

if ($BuildDebug) {
    Invoke-GrpcBuild -BuildType 'Debug' `
                     -BuildDir  '.build_ninja_debug' `
                     -InstallDir 'install_vs2026_dbg'
}

$sw.Stop()
$elapsed = $sw.Elapsed
Write-Host "=============================================="
Write-Host "  All done!  Total time: $($elapsed.Hours)h $($elapsed.Minutes)m $($elapsed.Seconds)s"
Write-Host "=============================================="
