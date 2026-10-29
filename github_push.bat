@echo off
setlocal
echo Publish scope: F13 CAN, F24 Classical CAN and F24 CAN FD device Boot/App/Transfer.
echo Nine repositories only. Tool/Host, RH850, serial projects and websites are excluded.
echo Confirm the reviewed release scope and validation basis before publishing.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\push_github.ps1" -Execute %*
set "publish_exit=%errorlevel%"
if not "%publish_exit%"=="0" echo GitHub publication stopped. Review the error and local backup before retrying.
pause
exit /b %publish_exit%
