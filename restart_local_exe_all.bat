@echo off
setlocal
set "ROOT=%~dp0"
call "%ROOT%tools\scripts\restart_local_exe_all.bat" %*
exit /b %errorlevel%
