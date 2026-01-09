$Plugin = 'D:/game/bin/plugin1/Debug/NoMemberPointerPlugin.dll'
$ClangTidy = 'D:/game/llvm/bin/clang-tidy.exe'
$ProjectDir = 'D:/game/luyuan/mmorpg1/cpp/nodes/centre'

# 单独写每个 include 和宏
$ArgsCommon = @(
    "--load", $Plugin,
    "-checks=NoMemberRawPointer",
    "-quiet",
    "-I", "D:/game/luyuan/mmorpg1/cpp",
    "-I", "D:/game/llvm/include",
    "-D", "_WIN32"
)

Get-ChildItem -Path $ProjectDir -Recurse -Include *.cpp,*.h | ForEach-Object {
    Write-Host "Scanning $($_.FullName)"
    & $ClangTidy $_.FullName $ArgsCommon
}
