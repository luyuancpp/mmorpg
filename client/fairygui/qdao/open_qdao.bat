@echo off
setlocal
cd /d "%~dp0\..\..\.."
pwsh -NoProfile -ExecutionPolicy Bypass -File "client\fairygui\qdao\open_qdao.ps1"
endlocal