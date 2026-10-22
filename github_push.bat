@echo off
setlocal
echo Publish Tool, F13 Classical Transfer/Boot/App, F24 Classical and CAN FD Transfer/Boot/App after validation and PUSH confirmation.
echo RH850 development paths remain local and are excluded from Tool publication.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\push_github.ps1" -Execute %*
set "publish_exit=%errorlevel%"
if not "%publish_exit%"=="0" echo GitHub publication stopped. Review the error and local backup before retrying.
pause
exit /b %publish_exit%
