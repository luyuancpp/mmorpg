@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "GO_LOG=bin\logs\go_services"
set "CPP_LOG=bin\logs\cpp_nodes"
set "PS=pwsh -NoProfile -ExecutionPolicy Bypass"

if "%~1"=="" goto :menu

set "CMD=%~1"
if /I "%CMD%"=="start"     goto :start
if /I "%CMD%"=="start-cpp" goto :start_cpp
if /I "%CMD%"=="start-go"  goto :start_go
if /I "%CMD%"=="stop"      goto :stop
if /I "%CMD%"=="status"    goto :status
if /I "%CMD%"=="restart"   goto :restart
if /I "%CMD%"=="logs"      goto :logs
if /I "%CMD%"=="robot"     goto :robot
if /I "%CMD%"=="ui"        goto :ui
if /I "%CMD%"=="ui-cpp"    goto :ui_cpp
if /I "%CMD%"=="ui-go"     goto :ui_go
if /I "%CMD%"=="help"      goto :help

echo Unknown command: %CMD%
echo.
goto :help

:: ================================================================
::  Double-click / no-args: interactive numbered menu
:: ================================================================
:menu
echo.
echo   ========================================
echo     MMORPG Dev Server
echo   ========================================
echo.
echo     1.  Start all   (build Go + launch C++ and Go)
echo     2.  Start C++   (gate + scene only)
echo     3.  Start Go    (build + launch Go services only)
echo     4.  Stop all
echo     5.  Status
echo     6.  Restart     (stop + build + start)
echo     7.  Logs        (pick a process to tail)
echo     8.  Logs all    (dump last 60 lines)
echo     9.  Robot       (build + run load-test client)
echo     10. UI          (mprocs TUI dashboard)
echo.
echo     0.  Exit
echo.
set /p "CHOICE=  Pick [0-10]: "

if "%CHOICE%"=="1" goto :start
if "%CHOICE%"=="2" goto :start_cpp
if "%CHOICE%"=="3" goto :start_go
if "%CHOICE%"=="4" goto :stop
if "%CHOICE%"=="5" goto :status
if "%CHOICE%"=="6" goto :restart
if "%CHOICE%"=="7" goto :logs
if "%CHOICE%"=="8" goto :logs_all
if "%CHOICE%"=="9" goto :robot
if "%CHOICE%"=="10" goto :ui
if "%CHOICE%"=="0" exit /b 0

echo   Invalid choice.
goto :menu

:: ================================================================
:start
echo [1/2] Building Go executables...
%PS% -File tools\scripts\dev_tools.ps1 -Command go-svc-build
if errorlevel 1 ( echo Build failed. & pause & exit /b 1 )
echo.
echo [2/2] Starting C++ nodes + Go services...
%PS% -File tools\scripts\dev_tools.ps1 -Command dev-start-exe -GateCount 2 -SceneCount 4
if errorlevel 1 ( echo Start failed. & pause & exit /b 1 )
echo.
echo Done. Run "dev status" to check.
exit /b 0

:: ================================================================
:start_cpp
echo Starting C++ nodes (2 gate + 4 scene)...
%PS% -File tools\scripts\dev_tools.ps1 -Command cpp-node-start -GateCount 2 -SceneCount 4
if errorlevel 1 ( echo Start C++ failed. & pause & exit /b 1 )
echo C++ nodes started.
exit /b 0

:: ================================================================
:start_go
echo [1/2] Building Go executables...
%PS% -File tools\scripts\dev_tools.ps1 -Command go-svc-build
if errorlevel 1 ( echo Build failed. & pause & exit /b 1 )
echo.
echo [2/2] Starting Go services (exe)...
%PS% -File tools\scripts\dev_tools.ps1 -Command go-svc-start-exe
if errorlevel 1 ( echo Start Go failed. & pause & exit /b 1 )
echo Go services started.
exit /b 0

:: ================================================================
:stop
echo Stopping all local services...
%PS% -File tools\scripts\dev_tools.ps1 -Command dev-stop
if errorlevel 1 ( echo Stop failed. & pause & exit /b 1 )
echo All services stopped.
exit /b 0

:: ================================================================
:status
%PS% -File tools\scripts\dev_tools.ps1 -Command dev-status
echo.
pause
exit /b 0

:: ================================================================
:restart
echo [1/2] Stopping...
%PS% -File tools\scripts\dev_tools.ps1 -Command dev-stop
if errorlevel 1 ( echo Stop failed. & pause & exit /b 1 )
echo.
echo [2/2] Building + Starting...
%PS% -File tools\scripts\dev_tools.ps1 -Command go-svc-build
if errorlevel 1 ( echo Build failed. & pause & exit /b 1 )
%PS% -File tools\scripts\dev_tools.ps1 -Command dev-start-exe -GateCount 2 -SceneCount 4
if errorlevel 1 ( echo Start failed. & pause & exit /b 1 )
echo Restarted. Run "dev status" to check.
exit /b 0

:: ================================================================
::  dev logs          -> interactive menu to pick a process
::  dev logs login    -> open new window tailing login's log
::  dev logs all      -> dump last 60 lines of every process
:: ================================================================
:logs
set "PROC=%~2"

:: --- "dev logs all": dump everything inline ---
if /I "%PROC%"=="all" goto :logs_all

:: --- "dev logs <name>": open new window tailing that process ---
if not "%PROC%"=="" goto :logs_tail

:: --- "dev logs": interactive menu ---
echo.
echo   Available processes:
echo.

set /a N=0

:: Go services
if exist "%GO_LOG%" (
    for %%F in ("%GO_LOG%\*.stderr.log") do (
        set "BASENAME=%%~nF"
        set "BASENAME=!BASENAME:.stderr=!"
        set /a N+=1
        set "P[!N!]=go:!BASENAME!"
        echo     !N!  !BASENAME!  [go]
    )
)

:: C++ nodes
if exist "%CPP_LOG%" (
    for %%F in ("%CPP_LOG%\*.stderr.log") do (
        set "BASENAME=%%~nF"
        set "BASENAME=!BASENAME:.stderr=!"
        set /a N+=1
        set "P[!N!]=cpp:!BASENAME!"
        echo     !N!  !BASENAME!  [cpp]
    )
)

if %N%==0 (
    echo   No log files found.
    pause & exit /b 1
)

echo.
echo     0  (all ^- dump last 60 lines inline)
echo.
set /p "PICK=Pick a number: "

if "%PICK%"=="0" goto :logs_all

set "SEL=!P[%PICK%]!"
if "%SEL%"=="" (
    echo Invalid selection.
    pause & exit /b 1
)

:: Parse "go:login" or "cpp:gate_1"
for /f "tokens=1,2 delims=:" %%A in ("%SEL%") do (
    set "KIND=%%A"
    set "PROC=%%B"
)
goto :logs_tail

:: --- Open a new cmd window tailing the process log ---
:logs_tail
:: Find the log file
set "LOGFILE="
if exist "%GO_LOG%\%PROC%.stderr.log"  set "LOGFILE=%GO_LOG%\%PROC%.stderr.log"
if exist "%CPP_LOG%\%PROC%.stderr.log" set "LOGFILE=%CPP_LOG%\%PROC%.stderr.log"

if "%LOGFILE%"=="" (
    echo No log found for "%PROC%".
    echo.
    echo Available: dir /b "%GO_LOG%\*.stderr.log" "%CPP_LOG%\*.stderr.log" 2^>nul
    pause & exit /b 1
)

echo Opening log window for: %PROC%
echo   %LOGFILE%
start "%PROC% log" cmd /k "title %PROC% log & powershell -NoProfile -Command \"Get-Content '%CD%\%LOGFILE%' -Tail 100 -Wait\""
exit /b 0

:: --- Dump all logs inline (original behavior) ---
:logs_all
echo ===== Go service logs =====
if exist "%GO_LOG%" (
    for %%F in ("%GO_LOG%\*.stderr.log") do (
        echo.
        echo --- %%~nxF ---
        powershell -NoProfile -Command "Get-Content '%%~fF' -Tail 60"
    )
) else ( echo No Go logs found. )

echo.
echo ===== C++ node logs =====
if exist "%CPP_LOG%" (
    for %%F in ("%CPP_LOG%\*.stderr.log") do (
        echo.
        echo --- %%~nxF ---
        powershell -NoProfile -Command "Get-Content '%%~fF' -Tail 60"
    )
) else ( echo No C++ logs found. )
echo.
pause
exit /b 0

:: ================================================================
:robot
set "ROBOT_EXE=robot\robot.exe"
set "ROBOT_CFG=%~2"
if "%ROBOT_CFG%"=="" set "ROBOT_CFG=robot\etc\robot.yaml"

echo Building robot...
pushd robot
go build -o robot.exe .
if errorlevel 1 ( echo Robot build failed. & popd & pause & exit /b 1 )
popd
echo.
echo Starting robot  [config: %ROBOT_CFG%]
start "robot" cmd /k "title robot ^& \"%CD%\%ROBOT_EXE%\" -c \"%CD%\%ROBOT_CFG%\""
echo Robot launched in new window.
exit /b 0

:: ================================================================
::  mprocs TUI dashboard — all processes in one terminal with UI
:: ================================================================
:ui
set "MPROCS_CFG=tools\dev\mprocs.2g4s.yaml"
if not "%~2"=="" set "MPROCS_CFG=%~2"
where mprocs >nul 2>&1
if errorlevel 1 (
    echo mprocs not found. Install with: winget install mprocs
    pause & exit /b 1
)
echo Launching mprocs TUI  [config: %MPROCS_CFG%]
mprocs -c "%MPROCS_CFG%"
exit /b 0

:ui_cpp
where mprocs >nul 2>&1
if errorlevel 1 ( echo mprocs not found. Install: winget install mprocs & pause & exit /b 1 )
mprocs -c tools\dev\mprocs.cpp-only.yaml
exit /b 0

:ui_go
where mprocs >nul 2>&1
if errorlevel 1 ( echo mprocs not found. Install: winget install mprocs & pause & exit /b 1 )
mprocs -c tools\dev\mprocs.go-only.yaml
exit /b 0

:: ================================================================
:help
echo.
echo   dev.bat - Local server management
echo.
echo   Double-click:   Interactive menu
echo   Command line:   dev ^<command^>
echo.
echo   Commands:
echo     start          Build Go exe + start all (2 gate, 4 scene)
echo     start-cpp      Start C++ nodes only (gate + scene)
echo     start-go       Build + start Go services only
echo     stop           Stop all C++ and Go processes
echo     status         Show running/gone status
echo     restart        Stop then build + start
echo     logs           Pick a process, open tail window
echo     logs ^<name^>    Tail a specific process (e.g. dev logs login)
echo     logs all       Dump last 60 lines of every process
echo     robot          Build + run robot load-test client
echo     robot ^<cfg^>    Use custom config (default: robot\etc\robot.yaml)
echo     ui             mprocs TUI dashboard (all processes)
echo     ui-cpp         mprocs TUI (C++ nodes only)
echo     ui-go          mprocs TUI (Go services only)
echo     help           Show this message
echo.
echo   Example:
echo     dev start
echo     dev robot
echo     dev ui
echo     dev logs login
echo.
pause
exit /b 0
