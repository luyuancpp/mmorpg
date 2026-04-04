@echo off
setlocal
set "ROOT=%~dp0"
call "%ROOT%tools\scripts\status_local_exe_all.bat" %*
exit /b %errorlevel%
