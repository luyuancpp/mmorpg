#requires -Version 5
[CmdletBinding()]
param(
  [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
)

$ErrorActionPreference = 'Stop'

$editor = Join-Path $RepoRoot 'tools\cache\fairygui-editor\FairyGUI-Editor\FairyGUI-Editor\FairyGUI-Editor.exe'
$project = Join-Path $PSScriptRoot 'qdao.fairy'

if (-not (Test-Path $editor)) { throw "FairyGUI Editor not found: $editor" }
if (-not (Test-Path $project)) { throw "qdao project not found: $project" }

Get-Process FairyGUI-Editor -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Process -FilePath $editor -ArgumentList @($project)