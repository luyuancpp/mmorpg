<#
.SYNOPSIS
    为本轮升级单独构建 gRPC 固定版本的 GoogleTest 1.17.0 四个静态库。
.PARAMETER PreferredMsvcVersion
    指定已安装的 MSVC 版本前缀，默认 14.51；缺少时失败，不切换到其他版本。
.PARAMETER BuildRoot
    全新的仓库内 ignored 目录；构建与安装分别放在 build、install 子目录。
.NOTES
    使用已安装的指定 MSVC、Ninja Debug /MDd，串行构建；不安装工具、不运行测试、不发布到正式目录。
#>
[CmdletBinding()]
param(
    [string]$BuildRoot = 'build/third_party/googletest-v1.17.0-dbg',
    [ValidatePattern('^\d+\.\d+(?:\.\d+)?$')]
    [string]$PreferredMsvcVersion = '14.51'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$GrpcRoot = Join-Path $RepoRoot 'third_party/grpc'
$SourceRoot = Join-Path $GrpcRoot 'third_party/googletest'
$ExpectedSourceSha = '52eb8108c5bdec04579160ae17225d66034bd723'
$Git = (Get-Command git -ErrorAction Stop).Source

if (-not (Test-Path -LiteralPath (Join-Path $SourceRoot 'CMakeLists.txt') -PathType Leaf)) {
    throw "缺少 GoogleTest 源码；请先初始化 gRPC 固定的子模块：$SourceRoot"
}
$parentEntry = & $Git -C $GrpcRoot ls-tree HEAD -- third_party/googletest
if ($LASTEXITCODE -ne 0 -or $parentEntry -notmatch '^160000 commit ([0-9a-f]{40})\s+third_party/googletest$' -or
    $Matches[1] -ne $ExpectedSourceSha) {
    throw "gRPC 的 GoogleTest gitlink 不是本轮固定提交：$parentEntry"
}
$actualRoot = & $Git -C $SourceRoot rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0 -or -not $actualRoot -or
    -not [IO.Path]::GetFullPath($actualRoot.Trim()).Equals($SourceRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "GoogleTest 不是已初始化的独立子模块：$SourceRoot"
}
$actualSha = & $Git -C $SourceRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actualSha.Trim() -ne $ExpectedSourceSha) {
    throw "GoogleTest 源码不是钉住的 1.17.0 提交，拒绝构建：$actualSha"
}
$sourceChanges = & $Git -C $SourceRoot status --porcelain --untracked-files=no
if ($LASTEXITCODE -ne 0 -or $sourceChanges) {
    throw "GoogleTest 已跟踪源码存在本地改动，拒绝混入本轮产物：$sourceChanges"
}
if (-not (Select-String -LiteralPath (Join-Path $SourceRoot 'CMakeLists.txt') -Pattern '^set\(GOOGLETEST_VERSION 1\.17\.0\)$' -Quiet)) {
    throw 'GoogleTest CMake 版本不是 1.17.0。'
}

$ArtifactRoot = [IO.Path]::TrimEndingDirectorySeparator([IO.Path]::GetFullPath(
    $(if ([IO.Path]::IsPathRooted($BuildRoot)) { $BuildRoot } else { Join-Path $RepoRoot $BuildRoot })))
$workspacePrefix = $RepoRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
if (-not $ArtifactRoot.StartsWith($workspacePrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "产物目录必须位于当前仓库内，且不能等于仓库根目录：$ArtifactRoot"
}
$currentPath = $ArtifactRoot
while (-not $currentPath.Equals($RepoRoot, [StringComparison]::OrdinalIgnoreCase)) {
    if (Test-Path -LiteralPath $currentPath) {
        $pathItem = Get-Item -LiteralPath $currentPath -Force
        if (($pathItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "产物目录不能经过链接或目录联接：$currentPath"
        }
    }
    $currentPath = [IO.Directory]::GetParent($currentPath).FullName
}
if (Test-Path -LiteralPath $ArtifactRoot) {
    throw "产物目录已存在；为保留现场，请指定全新的 BuildRoot：$ArtifactRoot"
}
& $Git -C $RepoRoot check-ignore --quiet -- $ArtifactRoot
if ($LASTEXITCODE -ne 0) { throw "产物目录必须被当前仓库忽略：$ArtifactRoot" }
$BuildPath = Join-Path $ArtifactRoot 'build'
$InstallPath = Join-Path $ArtifactRoot 'install'

# 只使用已安装的 VS / MSVC，缺少指定版本即失败；不安装工具或回退到其他工具集。
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) { throw "缺少 vswhere：$vswhere" }
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $vsPath) { throw '没有可用的 Visual Studio C++ 工具链。' }
$toolsets = @(Get-ChildItem -LiteralPath (Join-Path $vsPath 'VC/Tools/MSVC') -Directory |
    Sort-Object { [version]$_.Name } -Descending)
$selectedToolset = $toolsets | Where-Object {
    $_.Name -eq $PreferredMsvcVersion -or $_.Name.StartsWith($PreferredMsvcVersion + '.', [StringComparison]::Ordinal)
} | Select-Object -First 1
if (-not $selectedToolset) { throw "缺少已安装的 MSVC $PreferredMsvcVersion 工具集：$vsPath" }
$versionParts = $selectedToolset.Name.Split('.')
$vcvarsVersion = "$($versionParts[0]).$($versionParts[1])"
$vcvars = Join-Path $vsPath 'VC/Auxiliary/Build/vcvars64.bat'
if (-not (Test-Path -LiteralPath $vcvars -PathType Leaf)) { throw "缺少 VS 环境脚本：$vcvars" }
$environmentLines = & cmd.exe /d /c "`"$vcvars`" -vcvars_ver=$vcvarsVersion >nul && set"
if ($LASTEXITCODE -ne 0) { throw "加载 MSVC $vcvarsVersion 环境失败。" }
foreach ($line in $environmentLines) {
    if ($line -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
    }
}
if ($env:VCToolsVersion.TrimEnd('\', '/') -ne $selectedToolset.Name) {
    throw "MSVC 实际版本与已选择版本不符：实际 $env:VCToolsVersion，期望 $($selectedToolset.Name)"
}
$CMake = (Get-Command cmake -ErrorAction Stop).Source
$Ninja = (Get-Command ninja -ErrorAction Stop).Source
$null = Get-Command cl -ErrorAction Stop

$configureArgs = @(
    '-S', $SourceRoot, '-B', $BuildPath, '-G', 'Ninja',
    '-DCMAKE_C_COMPILER=cl', '-DCMAKE_CXX_COMPILER=cl',
    "-DCMAKE_MAKE_PROGRAM=$($Ninja.Replace('\', '/'))",
    '-DCMAKE_BUILD_TYPE=Debug',
    '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebugDLL',
    # 静态 gtest 库也必须使用 /MDd，避免与所有调用工程的 Debug CRT 不一致。
    '-Dgtest_force_shared_crt=ON', '-DBUILD_SHARED_LIBS=OFF',
    '-DBUILD_GMOCK=ON', '-DINSTALL_GTEST=ON', '-DGTEST_HAS_ABSL=OFF',
    '-Dgtest_build_tests=OFF', '-Dgmock_build_tests=OFF',
    # 直接保留现有工程使用的四个库名；Debug 由构建类型和 CRT 选项保证。
    '-DCMAKE_DEBUG_POSTFIX:STRING=',
    "-DCMAKE_INSTALL_PREFIX=$($InstallPath.Replace('\', '/'))",
    '-DCMAKE_INSTALL_LIBDIR=lib', '-DCMAKE_INSTALL_BINDIR=bin'
)
$savedBuildParallelLevel = $env:CMAKE_BUILD_PARALLEL_LEVEL
$env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
try {
    Write-Host "[配置] GoogleTest 1.17.0，MSVC $env:VCToolsVersion，Ninja Debug /MDd：$BuildPath"
    Write-Host "[工具] CMake=$CMake；Ninja=$Ninja；源码=$ExpectedSourceSha"
    & $CMake @configureArgs
    $nativeExit = $LASTEXITCODE
    if ($nativeExit -ne 0) {
        Write-Error "GoogleTest 配置失败，退出码 $nativeExit；保留现场：$BuildPath" -ErrorAction Continue
        exit $nativeExit
    }
    & $CMake --build $BuildPath --config Debug --target gtest gtest_main gmock gmock_main --parallel 1
    $nativeExit = $LASTEXITCODE
    if ($nativeExit -ne 0) {
        Write-Error "GoogleTest 构建失败，退出码 $nativeExit；保留现场：$BuildPath" -ErrorAction Continue
        exit $nativeExit
    }
    foreach ($name in @('gtest', 'gtest_main', 'gmock', 'gmock_main')) {
        $requiredPath = Join-Path $BuildPath "lib/$name.lib"
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
            throw "构建缺少预期静态库：$requiredPath"
        }
    }
    & $CMake --install $BuildPath --config Debug --prefix $InstallPath
    $nativeExit = $LASTEXITCODE
    if ($nativeExit -ne 0) {
        Write-Error "GoogleTest 安装失败，退出码 $nativeExit；保留现场：$InstallPath" -ErrorAction Continue
        exit $nativeExit
    }
} finally {
    $env:CMAKE_BUILD_PARALLEL_LEVEL = $savedBuildParallelLevel
}
foreach ($relativePath in @('lib/gtest.lib', 'lib/gtest_main.lib', 'lib/gmock.lib', 'lib/gmock_main.lib',
    'include/gtest/gtest.h', 'include/gmock/gmock.h', 'lib/cmake/GTest/GTestConfig.cmake')) {
    $requiredPath = Join-Path $InstallPath $relativePath
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "安装缺少预期产物：$requiredPath"
    }
}
Write-Host "[完成] GoogleTest 1.17.0 四个 Debug /MDd 静态库已安装：$InstallPath；未运行测试、未发布。"
