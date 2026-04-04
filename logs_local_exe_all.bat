@echo off
setlocal
set "ROOT=%~dp0"
call "%ROOT%tools\scripts\logs_local_exe_all.bat" %*
exit /b %errorlevel%
