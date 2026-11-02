@echo off
setlocal
echo Publish scope: fifteen CAN device repositories plus Simple Renesas ISP Tool, sixteen repositories total.
echo Includes Tool source, EXE, docs and the selected F24 Host Boot/App sources and images.
echo Includes F13/F24 Classical CAN, F24 CAN FD, RH850 Classical CAN and RH850 CAN FD.
echo Serial MCU projects, unselected firmware/support assets, private data and websites remain excluded.
echo Confirm the reviewed release scope and validation basis before publishing.
if /I "%~1"=="new" (
    if not "%~2"=="" (
        echo Usage: github_push.bat new
        echo Use push_github.ps1 for first-time setup with additional options.
        pause
        exit /b 2
    )
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\push_github.ps1" -IncludeRh850 -InitializeMissing -CreateRemote -Execute
) else (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\push_github.ps1" -IncludeRh850 -Execute %*
)
set "publish_exit=%errorlevel%"
if not "%publish_exit%"=="0" echo GitHub publication stopped. Review the error and local backup before retrying.
pause
exit /b %publish_exit%
