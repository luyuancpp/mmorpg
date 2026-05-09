param(
    [string]$EditorPath = "FairyGUI-Editor",
    [string]$ProjectPath = "$PSScriptRoot/qdao.fairy",
    [string]$OutputPath = "$PSScriptRoot/../../unity/Assets/Resources/UI/qdao"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ProjectPath)) {
    throw "FairyGUI project file not found: $ProjectPath"
}

New-Item -ItemType Directory -Force $OutputPath | Out-Null

Write-Warning "FairyGUI Editor 6.1.4 free edition blocks command-line publishing. This script requires a professional FairyGUI Editor license. Free-edition workflow: open qdao.fairy in the Editor and publish package 'qdao' from the GUI to $OutputPath."

& $EditorPath -p $ProjectPath -b qdao -o $OutputPath

if ($LASTEXITCODE -ne 0) {
    throw "FairyGUI command-line publish failed with exit code $LASTEXITCODE. If the log says 'This feature is only avaliable in professional FairyGUI-Editor', publish from the Editor GUI or use a professional license."
}

Write-Host "Published qdao FairyGUI package to: $OutputPath"