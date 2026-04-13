param()

$pf86 = [Environment]::GetFolderPath("ProgramFilesX86")
$vswhere = Join-Path $pf86 "Microsoft Visual Studio/Installer/vswhere.exe"
if (!(Test-Path $vswhere)) {
    throw "vswhere.exe not found: $vswhere"
}

$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild/**/Bin/MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) {
    throw "MSBuild.exe not found via vswhere"
}

& $msbuild "cpp/generated/grpc_client/grpc_client.vcxproj" /m /p:Configuration=Debug /p:Platform=x64 /nologo /clp:ErrorsOnly
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
