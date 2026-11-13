@echo off
setlocal
if /I "%~1"=="tool" goto tool_only
echo Publish scope: fifteen CAN device repositories, three F24 I2C projects and Simple Renesas ISP Tool, nineteen repositories total.
echo Includes Tool source, EXE, docs and the selected F24 Host Boot/App sources and images.
echo Includes F13/F24 Classical CAN, F24 CAN FD, RH850 Classical CAN and RH850 CAN FD.
echo Includes F24 I2C Custom Transfer and ISP Boot/App. Other serial projects, private data and websites remain excluded.
echo Confirm the reviewed release scope and validation basis before publishing.
if /I "%~1"=="new" (
    if not "%~2"=="" (
        echo Usage: github_push.bat new
        echo Use push_github.ps1 for first-time setup with additional options.
        exit /b 2
    )
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\push_github.ps1" -IncludeRh850 -IncludeF24I2c -InitializeMissing -CreateRemote -Execute
) else (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\push_github.ps1" -IncludeRh850 -IncludeF24I2c -Execute %*
)
set "publish_exit=%errorlevel%"
if not "%publish_exit%"=="0" echo GitHub publication stopped. Review the error and local backup before retrying.
if not "%publish_exit%"=="0" if "%~1"=="" pause
exit /b %publish_exit%

:tool_only
rem PC UI maintenance only; coupled MCU publication keeps its original gates.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\github_publish.ps1" %2 %3 %4 %5 %6 %7 %8 %9
exit /b %errorlevel%
