<#
.SYNOPSIS
    Build gRPC (Release + Debug) using Ninja or VS generator + MSVC cl.exe,
    then install to grpc/install_vs2026/ (Release) and grpc/install_vs2026_dbg/ (Debug).

.DESCRIPTION
    This script automates the full gRPC build for the MMORPG project.
    It locates grpc source at ./grpc (relative to this script), uses vswhere
    to locate the newest MSVC toolchain, then runs CMake with the Ninja
    generator (default) or Visual Studio generator (-UseVSGenerator).

    缺少或低于最低版本的 cmake / ninja 会明确失败；本脚本不安装或升级工具。
    Visual Studio with C++ Desktop workload must be installed.

.PARAMETER BuildRelease
    Build the Release configuration (default: true).
.PARAMETER BuildDebug
    Build the Debug configuration (default: true).
.PARAMETER SkipToolCheck
    跳过 cmake / ninja 版本检查，仍要求工具存在；不会安装或升级工具。
.PARAMETER Jobs
    构建并行数，固定为 1，避免共享工具链的并发编译。
.PARAMETER BuildRoot
    可选的仓库内产物根目录；相对路径以仓库根为基准。
    未指定时沿用 third_party/grpc；指定后 build 与 install 均放在该根目录下，
    安装子目录仍为 install_vs2026 / install_vs2026_dbg，便于验证后发布 SDK。
.PARAMETER ZlibInstallRoot
    可选的外部 zlib 安装目录；显式使用该目录的头文件和静态库，避免回退到内置旧版。
    Debug 需要 lib/zlibstaticd.lib，Release 需要 lib/zlibstatic.lib。
.PARAMETER Clean
    设置后，在构建前同时删除已有 build 与 install 目录。
    新版 gRPC/Abseil 的安装清单不会主动删除上游已移除的库，因此 install
    目录也必须清理，避免旧库残留掩盖依赖错误。
.PARAMETER UseVSGenerator
    Use Visual Studio generator instead of Ninja. Useful when CMake + Ninja
    has compatibility issues (e.g. CMake 4.2.0 recompact bug).
.EXAMPLE
    # Build both Release and Debug (default, Ninja)
    .\build_grpc.ps1

    # Build using Visual Studio generator (workaround for CMake 4.2.0 + Ninja bug)
    .\build_grpc.ps1 -UseVSGenerator

    # 仅构建 Release，串行执行
    .\build_grpc.ps1 -BuildDebug:$false -Jobs 1

    # Clean rebuild
    .\build_grpc.ps1 -Clean

    # 跳过 cmake / ninja 的版本检查
    .\build_grpc.ps1 -SkipToolCheck
#>
[CmdletBinding()]
param(
    [bool]$BuildRelease  = $true,
    [bool]$BuildDebug    = $true,
    [ValidateRange(1, 1)]
    [int]$Jobs = 1,
    [string]$BuildRoot = '',
    [string]$ZlibInstallRoot = '',
    [string]$PreferredMsvcVersion = '14.51',
    [switch]$Clean,
    [switch]$SkipToolCheck,
    [switch]$UseVSGenerator
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Resolve repository root from tools/scripts/third_party and locate grpc source.
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$GrpcRoot = Join-Path $RepoRoot 'third_party\grpc'
if (-not (Test-Path (Join-Path $GrpcRoot 'CMakeLists.txt'))) {
    Write-Error "Cannot find grpc CMakeLists.txt at $GrpcRoot."
}

$ExpectedGrpcSha = '3252a89f10d8e92997862167ca7d095ecda85973'
$ExpectedBoringSslSha = '2b44a3701a4788e1ef866ddc7f143060a3d196c9'
$BoringSslRoot = Join-Path $GrpcRoot 'third_party\boringssl-with-bazel'
$BoringSslPatch = Join-Path $RepoRoot 'tools\patches\grpc\v1.83.0-boringssl-windows-x509-name.patch'

function Get-GitHead([string]$RepositoryPath) {
    $git = Get-Command git -ErrorAction SilentlyContinue
    if (-not $git) {
        Write-Error "git is not available on PATH."
    }

    $output = & $git.Source -C $RepositoryPath rev-parse HEAD 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Cannot resolve git HEAD for ${RepositoryPath}: $($output -join [Environment]::NewLine)"
    }

    return ($output | Select-Object -Last 1).ToString().Trim()
}

function Assert-GrpcProtoSource {
    $git = (Get-Command git -ErrorAction Stop).Source
    $relativePath = 'third_party/grpc-proto'
    $protoRoot = Join-Path $GrpcRoot $relativePath
    $repair = 'git -C third_party/grpc submodule update --init --recursive'
    $entry = & $git -C $GrpcRoot ls-tree HEAD -- $relativePath
    if ($LASTEXITCODE -ne 0 -or $entry -notmatch '^160000 commit ([0-9a-f]{40})\s+third_party/grpc-proto$') {
        throw "gRPC 父版本缺少有效的 grpc-proto gitlink，拒绝配置。请核对源码版本并执行：$repair"
    }
    $expectedSha = $Matches[1]
    if (-not (Test-Path -LiteralPath (Join-Path $protoRoot '.git'))) {
        throw "grpc-proto 尚未初始化，父版本要求 $expectedSha。请执行：$repair"
    }
    $actualRoot = & $git -C $protoRoot rev-parse --show-toplevel
    if ($LASTEXITCODE -ne 0 -or -not [IO.Path]::GetFullPath($actualRoot).Equals([IO.Path]::GetFullPath($protoRoot), [StringComparison]::OrdinalIgnoreCase)) {
        throw "grpc-proto 不是独立的预期 Git 根目录，拒绝借用父仓库。请执行：$repair"
    }
    $actualSha = Get-GitHead $protoRoot
    if ($actualSha -ne $expectedSha) {
        throw "grpc-proto 提交不符，父版本要求 $expectedSha，实际 $actualSha。请执行：$repair"
    }
    foreach ($file in @('grpc/channelz/v1/channelz.proto', 'grpc/reflection/v1/reflection.proto', 'grpc/reflection/v1alpha/reflection.proto')) {
        if (-not (Test-Path -LiteralPath (Join-Path $protoRoot $file) -PathType Leaf)) {
            throw "grpc-proto 缺少 $file，拒绝配置。请执行：$repair"
        }
    }
    Write-Host "[info] grpc-proto 与父版本 gitlink 一致：$expectedSha"
}

function Initialize-BoringSslWindowsPatch {
    if (-not (Test-Path -LiteralPath $BoringSslRoot -PathType Container)) {
        Write-Error "BoringSSL submodule is missing: $BoringSslRoot"
    }
    if (-not (Test-Path -LiteralPath $BoringSslPatch -PathType Leaf)) {
        Write-Error "Required BoringSSL patch is missing: $BoringSslPatch"
    }

    $actualGrpcSha = Get-GitHead $GrpcRoot
    if ($actualGrpcSha -ne $ExpectedGrpcSha) {
        Write-Error "Unexpected gRPC revision. Expected $ExpectedGrpcSha, got $actualGrpcSha."
    }

    $actualBoringSslSha = Get-GitHead $BoringSslRoot
    if ($actualBoringSslSha -ne $ExpectedBoringSslSha) {
        Write-Error "Unexpected BoringSSL revision. Expected $ExpectedBoringSslSha, got $actualBoringSslSha."
    }

    $git = (Get-Command git -ErrorAction Stop).Source
    $null = & $git -C $BoringSslRoot apply --check $BoringSslPatch 2>$null
    $forwardCheckExitCode = $LASTEXITCODE
    if ($forwardCheckExitCode -eq 0) {
        & $git -C $BoringSslRoot apply $BoringSslPatch
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Failed to apply BoringSSL Windows X509_NAME patch: $BoringSslPatch"
        }
        Write-Host "[patch] Applied BoringSSL Windows X509_NAME compatibility patch."
        return
    }

    $null = & $git -C $BoringSslRoot apply --reverse --check $BoringSslPatch 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[patch] BoringSSL Windows X509_NAME compatibility patch is already applied."
        return
    }

    Write-Error "BoringSSL patch is neither applicable nor already applied. Refusing to build: $BoringSslPatch"
}

Initialize-BoringSslWindowsPatch
Assert-GrpcProtoSource

$ToolDefs = @{
    cmake = @{ MinVersion = [version]'3.20.0' }
    ninja = @{ MinVersion = [version]'1.11.0' }
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

    Write-Error "$Name 缺失、版本无法识别或低于 $($def.MinVersion)（当前：$currentVer）。请先准备受支持的工具并加入 PATH，再重新执行；本脚本不会安装或升级工具。"
}

if (-not $SkipToolCheck) {
    Ensure-Tool 'cmake'
    if (-not $UseVSGenerator) { Ensure-Tool 'ninja' }
}

function Resolve-Tool([string]$Name) {
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $cmd) { Write-Error "'$Name' not found on PATH. Please install it." }
    return $cmd.Source
}

function Resolve-OptionalTool([string]$Name) {
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $cmd) { return $null }
    return $cmd.Source
}

function Convert-ToCMakePath([string]$Path) {
    return $Path -replace '\\', '/'
}

# 产物目录必须留在当前仓库内；拒绝通过链接或联接跳到其他目录。
function Get-ArtifactPath([string]$Path) {
    $fullPath = [IO.Path]::TrimEndingDirectorySeparator([IO.Path]::GetFullPath($Path))
    $workspacePrefix = $RepoRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $fullPath.StartsWith($workspacePrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "产物路径必须位于仓库根目录内，且不能等于仓库根目录：$fullPath"
    }

    $currentPath = $fullPath
    while (-not $currentPath.Equals($RepoRoot, [StringComparison]::OrdinalIgnoreCase)) {
        if (Test-Path -LiteralPath $currentPath) {
            $item = Get-Item -LiteralPath $currentPath -Force -ErrorAction Stop
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "产物路径不能经过链接或目录联接：$currentPath"
            }
        }
        $currentPath = [IO.Directory]::GetParent($currentPath).FullName
    }
    return $fullPath
}

$ArtifactRoot = if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $GrpcRoot
} elseif ([IO.Path]::IsPathRooted($BuildRoot)) {
    Get-ArtifactPath $BuildRoot
} else {
    Get-ArtifactPath (Join-Path $RepoRoot $BuildRoot)
}

$ExternalZlibRoot = $null
if (-not [string]::IsNullOrWhiteSpace($ZlibInstallRoot)) {
    $ExternalZlibRoot = if ([IO.Path]::IsPathRooted($ZlibInstallRoot)) {
        Get-ArtifactPath $ZlibInstallRoot
    } else {
        Get-ArtifactPath (Join-Path $RepoRoot $ZlibInstallRoot)
    }
    $requiredZlibFiles = @('include/zlib.h', 'include/zconf.h')
    if ($BuildDebug) { $requiredZlibFiles += 'lib/zlibstaticd.lib' }
    if ($BuildRelease) { $requiredZlibFiles += 'lib/zlibstatic.lib' }
    foreach ($relativePath in $requiredZlibFiles) {
        $requiredPath = Join-Path $ExternalZlibRoot $relativePath
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
            throw "外部 zlib 安装不完整，拒绝回退到其他版本：$requiredPath"
        }
    }
    if (-not (Select-String -LiteralPath (Join-Path $ExternalZlibRoot 'include/zlib.h') -Pattern '^#define ZLIB_VERSION "1\.3\.2"' -Quiet)) {
        throw "外部 zlib 必须为 1.3.2：$ExternalZlibRoot"
    }
}

function Remove-DirectoryRobust([string]$Path) {
    $fullPath = Get-ArtifactPath $Path
    if ($fullPath.Equals($GrpcRoot, [StringComparison]::OrdinalIgnoreCase) -or
        $fullPath.Equals($ArtifactRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "拒绝删除源码根目录或产物根目录：$fullPath"
    }
    if (-not (Test-Path -LiteralPath $fullPath)) { return }
    if (-not (Test-Path -LiteralPath $fullPath -PathType Container)) {
        throw "待清理的产物路径不是目录：$fullPath"
    }

    $link = Get-ChildItem -LiteralPath $fullPath -Recurse -Force -ErrorAction Stop |
        Where-Object { ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 } |
        Select-Object -First 1
    if ($link) { throw "拒绝递归删除包含链接或目录联接的产物目录：$($link.FullName)" }
    Remove-Item -LiteralPath $fullPath -Recurse -Force -ErrorAction Stop
}

$CMake = Resolve-Tool 'cmake'
if ($UseVSGenerator) {
    $Ninja = $null
    Write-Host "[info] Using Visual Studio generator (Ninja skipped)"
} else {
    $Ninja = Resolve-Tool 'ninja'
    Write-Host "[info] ninja : $Ninja"
}
$Nasm  = Resolve-OptionalTool 'nasm'
Write-Host "[info] cmake : $CMake"
if ($Nasm) {
    Write-Host "[info] nasm  : $Nasm"
} else {
    Write-Host "[warn] nasm not found on PATH. Falling back to OPENSSL_NO_ASM=ON for BoringSSL."
}

function Find-ClExe([string]$PreferredVersionPrefix) {
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

    $msvcRoot = Join-Path $vsPath 'VC\Tools\MSVC'
    if (-not (Test-Path $msvcRoot)) { Write-Error "MSVC tools directory not found: $msvcRoot" }

    $allMsvcVersions = @(Get-ChildItem -Path $msvcRoot -Directory | Select-Object -ExpandProperty Name)
    if ($allMsvcVersions.Count -eq 0) { Write-Error "No MSVC toolset found under $msvcRoot" }

    $msvcVer = $null
    if (-not [string]::IsNullOrWhiteSpace($PreferredVersionPrefix)) {
        $msvcVer = $allMsvcVersions |
            Where-Object { $_ -like "$PreferredVersionPrefix*" } |
            Sort-Object { [version]$_ } -Descending |
            Select-Object -First 1
    }

    if (-not $msvcVer) {
        $msvcVer = $allMsvcVersions |
            Sort-Object { [version]$_ } -Descending |
            Select-Object -First 1
    }

    $clPath = Join-Path $vsPath "VC\Tools\MSVC\$msvcVer\bin\Hostx64\x64\cl.exe"
    if (-not (Test-Path $clPath)) { Write-Error "cl.exe not found at $clPath" }

    return @{
        VsPath   = $vsPath
        MsvcVer  = $msvcVer
        ClExe    = $clPath
        MsvcBase = Join-Path $vsPath "VC\Tools\MSVC\$msvcVer"
    }
}

$msvc = Find-ClExe -PreferredVersionPrefix $PreferredMsvcVersion
Write-Host "[info] VS    : $($msvc.VsPath)"
Write-Host "[info] MSVC  : $($msvc.MsvcVer)"
Write-Host "[info] cl.exe: $($msvc.ClExe)"

$GrpcExtraArgs = @()
if ($ExternalZlibRoot) {
    $zlibCMakePath = Convert-ToCMakePath $ExternalZlibRoot
    # 固定 FindZLIB 的所有输入；当前构建配置的库已经前置检查，不接受系统库或旧缓存兜底。
    $GrpcExtraArgs += @(
        '-DgRPC_ZLIB_PROVIDER=package',
        '-DCMAKE_FIND_PACKAGE_PREFER_CONFIG=OFF',
        '-UZLIB_LIBRARY',
        '-UZLIB_LIBRARIES',
        "-DZLIB_ROOT=$zlibCMakePath",
        '-DZLIB_USE_STATIC_LIBS=ON',
        "-DZLIB_INCLUDE_DIR=$zlibCMakePath/include",
        "-DZLIB_LIBRARY_DEBUG=$zlibCMakePath/lib/zlibstaticd.lib",
        "-DZLIB_LIBRARY_RELEASE=$zlibCMakePath/lib/zlibstatic.lib"
    )
}
if ($Nasm) {
    $nasmCMakePath = Convert-ToCMakePath $Nasm
    $GrpcExtraArgs += "-DCMAKE_ASM_NASM_COMPILER=$nasmCMakePath"
} elseif (-not $UseVSGenerator) {
    # Ninja generator does not auto-disable BoringSSL ASM when NASM is missing.
    $GrpcExtraArgs += "-DOPENSSL_NO_ASM=ON"
}

function Enter-MsvcEnv([hashtable]$Msvc) {
    $vcvars = Join-Path $Msvc.VsPath 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $vcvars)) { Write-Error "vcvars64.bat not found at $vcvars" }

    $versionParts = $Msvc.MsvcVer.Split('.')
    $vcvarsVer = if ($versionParts.Length -ge 2) { "$($versionParts[0]).$($versionParts[1])" } else { $Msvc.MsvcVer }
    $envBlock = cmd /c "`"$vcvars`" -vcvars_ver=$vcvarsVer >nul 2>&1 && set" 2>&1
    foreach ($line in $envBlock) {
        if ($line -match '^([^=]+)=(.*)$') {
            [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
        }
    }

    Write-Host "[info] MSVC environment loaded from vcvars64.bat (vcvars_ver=$vcvarsVer)"
}

Enter-MsvcEnv $msvc

# After vcvars64 loads, cl.exe is on PATH. Verify it is reachable.
$clOnPath = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $clOnPath) {
    Write-Error "cl.exe not on PATH after loading vcvars64. MSVC environment setup may have failed."
}
Write-Host "[info] cl.exe on PATH: $($clOnPath.Source)"

Write-Host "[info] Parallel jobs: $Jobs"

# Use short name 'cl' since vcvars64 has put cl.exe on PATH.
# This avoids CMake path-escaping issues with long paths containing spaces.
if ($UseVSGenerator) {
    $NinjaCMakePath = $null
    $CommonArgs = @(
        "-G", "Visual Studio 18 2026",
        "-A", "x64",
        "-DCMAKE_CXX_FLAGS=/std:c++20 /DGRPC_NO_FILTER_FUSION",
        "-DgRPC_BUILD_TESTS=OFF",
        "-DgRPC_BUILD_GRPCPP_OTEL_PLUGIN=OFF",
        "-Dprotobuf_BUILD_TESTS=OFF",
        "-DABSL_BUILD_TESTING=OFF",
        "-DABSL_BUILD_TEST_HELPERS=OFF",
        "-DABSL_PROPAGATE_CXX_STD=OFF"
    )
} else {
    $NinjaCMakePath = Convert-ToCMakePath $Ninja
    $CommonArgs = @(
        "-G", "Ninja",
        "-DCMAKE_C_COMPILER=cl",
        "-DCMAKE_CXX_COMPILER=cl",
        "-DCMAKE_MAKE_PROGRAM=$NinjaCMakePath",
        "-DCMAKE_CXX_FLAGS=/std:c++20 /DGRPC_NO_FILTER_FUSION",
        "-DgRPC_BUILD_TESTS=OFF",
        "-DgRPC_BUILD_GRPCPP_OTEL_PLUGIN=OFF",
        "-Dprotobuf_BUILD_TESTS=OFF",
        "-DABSL_BUILD_TESTING=OFF",
        "-DABSL_BUILD_TEST_HELPERS=OFF",
        "-DABSL_PROPAGATE_CXX_STD=OFF"
    )
}

function Invoke-GrpcBuild {
    param(
        [string]$BuildType,
        [string]$BuildDir,
        [string]$InstallDir
    )

    $buildPath = Get-ArtifactPath (Join-Path $ArtifactRoot $BuildDir)
    $installPath = Get-ArtifactPath (Join-Path $ArtifactRoot $InstallDir)

    Write-Host "`n=============================================="
    Write-Host "  gRPC $BuildType build"
    Write-Host "  Build dir  : $buildPath"
    Write-Host "  Install dir: $installPath"
    Write-Host "==============================================`n"

    if ($Clean) {
        foreach ($pathToRemove in @($buildPath, $installPath)) {
            if (Test-Path $pathToRemove) {
                Write-Host "[clean] Removing $pathToRemove ..."
                Remove-DirectoryRobust $pathToRemove
            }
        }
    }

    $cachePath = Join-Path $buildPath 'CMakeCache.txt'
    $expectedCxxFlags = '/std:c++20 /DGRPC_NO_FILTER_FUSION'
    if (Test-Path $cachePath) {
        $cacheCompilerLine = Select-String -Path $cachePath -Pattern '^CMAKE_C_COMPILER:FILEPATH=' -SimpleMatch:$false | Select-Object -First 1
        $cacheCxxFlagsLine = Select-String -Path $cachePath -Pattern '^CMAKE_CXX_FLAGS:STRING=' -SimpleMatch:$false | Select-Object -First 1
        if ($cacheCompilerLine) {
            $cachedCompiler = $cacheCompilerLine.Line.Split('=', 2)[1]
            $cachedName = [System.IO.Path]::GetFileNameWithoutExtension($cachedCompiler)
            # We now pass 'cl' to CMake; cache stores a resolved full path.
            # Reset only if the cached compiler is a completely different toolchain.
            if ($cachedName -ne 'cl') {
                Write-Host "[clean] Cached compiler is not cl ($cachedCompiler) - resetting $buildPath ..."
                Remove-DirectoryRobust $buildPath
            } elseif (-not $cachedCompiler.StartsWith($msvc.MsvcBase, [System.StringComparison]::OrdinalIgnoreCase)) {
                Write-Host "[clean] Cached compiler is from a different MSVC toolset ($cachedCompiler) - resetting $buildPath ..."
                Remove-DirectoryRobust $buildPath
            }
        }

        if ((Test-Path $buildPath) -and $cacheCxxFlagsLine) {
            $cachedCxxFlags = $cacheCxxFlagsLine.Line.Split('=', 2)[1]
            if ($cachedCxxFlags -ne $expectedCxxFlags) {
                Write-Host "[clean] Cached CMAKE_CXX_FLAGS is '$cachedCxxFlags' instead of '$expectedCxxFlags' - resetting $buildPath ..."
                Remove-DirectoryRobust $buildPath
            }
        }
    }

    if (-not (Test-Path $buildPath)) { New-Item -ItemType Directory -Path $buildPath | Out-Null }

    Write-Host "[1/3] CMake configure ($BuildType) ..."
    $configArgs = $CommonArgs + @(
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-DCMAKE_INSTALL_PREFIX=$installPath"
    ) + $GrpcExtraArgs + @(
        $GrpcRoot
    )

    & $CMake @configArgs -B $buildPath
    if ($LASTEXITCODE -ne 0) { Write-Error "CMake configure failed for $BuildType; build directory preserved: $buildPath" }

    Write-Host "[2/3] Building ($BuildType) ..."
    & $CMake --build $buildPath --config $BuildType -j $Jobs
    if ($LASTEXITCODE -ne 0) { Write-Error "Build failed for $BuildType" }

    Write-Host "[3/3] Installing ($BuildType) -> $installPath ..."
    & $CMake --install $buildPath --prefix $installPath --config $BuildType
    if ($LASTEXITCODE -ne 0) { Write-Error "Install failed for $BuildType" }

    Write-Host "`n[done] $BuildType build installed to $installPath`n"
}

$sw = [System.Diagnostics.Stopwatch]::StartNew()

$buildDirSuffix = if ($UseVSGenerator) { '_vs' } else { '_ninja' }

# 第三方 VS 工程不继承仓库的语言模式、/MP、额外包含目录与业务检查。
# 编译器探测也会启动 MSBuild，因此隔离必须覆盖配置、构建和安装，失败后仍恢复。
$savedImportProps = $env:ImportDirectoryBuildProps
$savedImportTargets = $env:ImportDirectoryBuildTargets
try {
    if ($UseVSGenerator) {
        $env:ImportDirectoryBuildProps = 'false'
        $env:ImportDirectoryBuildTargets = 'false'
    }

    if ($BuildRelease) {
        Invoke-GrpcBuild -BuildType 'Release' `
                         -BuildDir  ".build${buildDirSuffix}_release" `
                         -InstallDir 'install_vs2026'
    }

    if ($BuildDebug) {
        Invoke-GrpcBuild -BuildType 'Debug' `
                         -BuildDir  ".build${buildDirSuffix}_debug" `
                         -InstallDir 'install_vs2026_dbg'
    }
} finally {
    if ($UseVSGenerator) {
        $env:ImportDirectoryBuildProps = $savedImportProps
        $env:ImportDirectoryBuildTargets = $savedImportTargets
    }
}

$sw.Stop()
$elapsed = $sw.Elapsed
Write-Host "=============================================="
Write-Host "  All done!  Total time: $($elapsed.Hours)h $($elapsed.Minutes)m $($elapsed.Seconds)s"
Write-Host "=============================================="
