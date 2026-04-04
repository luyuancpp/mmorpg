@echo off
setlocal
cd /d "%~dp0"

if "%~1"=="" goto :help

set "CMD=%~1"
if /I "%CMD%"=="start"   goto :start
if /I "%CMD%"=="stop"    goto :stop
if /I "%CMD%"=="status"  goto :status
if /I "%CMD%"=="restart" goto :restart
if /I "%CMD%"=="logs"    goto :logs
if /I "%CMD%"=="help"    goto :help

echo Unknown command: %CMD%
echo.
goto :help

:: ----------------------------------------------------------------
:start
echo [1/2] Building Go executables...
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\scripts\dev_tools.ps1 -Command go-svc-build
if errorlevel 1 (
    echo Build failed.
    pause & exit /b 1
)
echo.
echo [2/2] Starting C++ nodes + Go services...
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\scripts\dev_tools.ps1 -Command dev-start-exe -GateCount 2 -SceneCount 4
if errorlevel 1 (
    echo Start failed.
    pause & exit /b 1
)
echo.
echo Done. Run "dev status" to check.
exit /b 0

:: ----------------------------------------------------------------
:stop
echo Stopping all local services...
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\scripts\dev_tools.ps1 -Command dev-stop
if errorlevel 1 (
    echo Stop failed.
    pause & exit /b 1
)
echo.
echo All services stopped.
exit /b 0

:: ----------------------------------------------------------------
:status
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\scripts\dev_tools.ps1 -Command dev-status
echo.
pause
exit /b 0

:: ----------------------------------------------------------------
:restart
echo [1/2] Stopping...
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\scripts\dev_tools.ps1 -Command dev-stop
if errorlevel 1 (
    echo Stop failed.
    pause & exit /b 1
)
echo.
echo [2/2] Building + Starting...
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\scripts\dev_tools.ps1 -Command go-svc-build
if errorlevel 1 (
    echo Build failed.
    pause & exit /b 1
)
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\scripts\dev_tools.ps1 -Command dev-start-exe -GateCount 2 -SceneCount 4
if errorlevel 1 (
    echo Start failed.
    pause & exit /b 1
)
echo.
echo Restarted. Run "dev status" to check.
exit /b 0

:: ----------------------------------------------------------------
:logs
set "GO_LOG=bin\logs\go_services"
set "CPP_LOG=bin\logs\cpp_nodes"

echo ===== Go service logs =====
if exist "%GO_LOG%" (
    for %%F in ("%GO_LOG%\*.stderr.log") do (
        echo.
        echo --- %%~nxF ---
        powershell -NoProfile -Command "Get-Content '%%~fF' -Tail 60"
    )
) else (
    echo No Go logs found.
)

echo.
echo ===== C++ node logs =====
if exist "%CPP_LOG%" (
    for %%F in ("%CPP_LOG%\*.stderr.log") do (
        echo.
        echo --- %%~nxF ---
        powershell -NoProfile -Command "Get-Content '%%~fF' -Tail 60"
    )
) else (
    echo No C++ logs found.
)
echo.
pause
exit /b 0

:: ----------------------------------------------------------------
:help
echo.
echo   dev.bat - Local server management
echo.
echo   Usage:  dev ^<command^>
echo.
echo   Commands:
echo     start     Build Go exe + start all (2 gate, 4 scene)
echo     stop      Stop all C++ and Go processes
echo     status    Show running/gone status
echo     restart   Stop then build + start
echo     logs      Tail latest stderr logs
echo     help      Show this message
echo.
pause
exit /b 0
