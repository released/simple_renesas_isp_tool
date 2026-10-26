@echo off
setlocal
if not "%~2"=="" goto invalid
if /I "%~1"=="/?" goto usage
if /I "%~1"=="--help" goto usage
if /I "%~1"=="--preview" goto preview
if not "%~1"=="" goto invalid

powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0clean_prerequisite_tests.ps1" -Apply
set "cleanup_exit=%errorlevel%"
echo.
pause
exit /b %cleanup_exit%

:preview
powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0clean_prerequisite_tests.ps1"
exit /b %errorlevel%

:usage
echo Usage: %~nx0 [--preview]
echo Default: remove only reviewed prerequisite fixture compiler outputs.
echo --preview: show the plan without deleting anything.
echo INI, screenshots, logs and unlisted files are retained.
exit /b 0

:invalid
echo Unsupported argument. Use --preview or --help.
exit /b 2
