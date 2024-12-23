@echo off
REM 关闭指定的进程列表

setlocal enabledelayedexpansion

REM 设置进程列表，确保每个进程名称之间有空格
set "processes=gate.exe game.exe centre.exe"

REM 循环遍历每个进程
for %%i in (%processes%) do (
    taskkill /f /im %%i >nul 2>&1
    if !errorlevel! == 0 (
        echo %%i closed
    ) else (
        echo %%i could not find
    )
)

REM 完成
echo 完成!
pause
