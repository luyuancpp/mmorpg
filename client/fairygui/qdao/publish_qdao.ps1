param(
    [string]$EditorPath  = "F:/work/mmorpg/tools/cache/fairygui-editor/FairyGUI-Editor/FairyGUI-Editor.exe",
    [string]$ProjectPath = "$PSScriptRoot/qdao.fairy",
    [string]$OutputPath  = "$PSScriptRoot/../../unity/Assets/Resources/UI/qdao"
)

# Publish the qdao FairyGUI package from the command line.
#
# IMPORTANT: FairyGUI Editor 6.1.4 free edition blocks command-line publish
# with the message "This feature is only available in professional FairyGUI
# Editor". If you don't have a Professional license, this script will
# exit non-zero and you must publish from the Editor GUI by hand:
#
#   1. Open qdao.fairy in FairyGUI Editor
#   2. Press F8 (or menu -> Publish)
#   3. Output is written to client/unity/Assets/Resources/UI/qdao/
#      (qdao_fui.bytes + qdao_atlas0.png)
#
# The default $EditorPath above points at the portable copy this repo
# carries under tools/cache/. Override with -EditorPath if your Editor
# lives elsewhere (e.g. a Pro install in Program Files).

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ProjectPath)) {
    throw "FairyGUI project file not found: $ProjectPath"
}
if (-not (Test-Path $EditorPath)) {
    throw "FairyGUI Editor not found at: $EditorPath. Override with -EditorPath."
}

New-Item -ItemType Directory -Force $OutputPath | Out-Null

Write-Host "Editor:  $EditorPath"
Write-Host "Project: $ProjectPath"
Write-Host "Output:  $OutputPath"
Write-Host ""

& $EditorPath -p $ProjectPath -b qdao -o $OutputPath

if ($LASTEXITCODE -ne 0) {
    Write-Warning ""
    Write-Warning "FairyGUI command-line publish failed with exit code $LASTEXITCODE."
    Write-Warning "If the editor log says 'This feature is only available in professional"
    Write-Warning "FairyGUI Editor', publish from the GUI: open qdao.fairy and press F8."
    exit $LASTEXITCODE
}

Write-Host "Published qdao package to: $OutputPath"
