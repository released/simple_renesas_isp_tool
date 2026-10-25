@echo off
setlocal
echo Bulk firmware publication is on hold pending real-board validation and a reviewed release scope.
echo All unvalidated MCU firmware-related content remains non-public, including Tool-side copies.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\push_github.ps1" -Execute %*
set "publish_exit=%errorlevel%"
if not "%publish_exit%"=="0" echo GitHub publication stopped. Review the error and local backup before retrying.
pause
exit /b %publish_exit%
