@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_mfc.ps1" -Configuration Release -Platform x64
if errorlevel 1 exit /b %errorlevel%
endlocal
