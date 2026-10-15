@echo off
setlocal
set "BUILD_DIR=%~dp0..\tmp\internal\build"

if not exist "%BUILD_DIR%" (
  mkdir "%BUILD_DIR%"
  if errorlevel 1 exit /b %errorlevel%
)

for %%D in (obj tests verify verify_obj) do (
  if exist "%BUILD_DIR%\%%D" rmdir /s /q "%BUILD_DIR%\%%D"
)
for %%E in (obj pch idb pdb ilk res exp lib iobj ipdb tlog lastbuildstate) do (
  del /s /f /q "%BUILD_DIR%\*.%%E" >nul 2>nul
)

echo Clean complete. Removed compiler intermediates only.
echo Preserved executables, INI files, DLLs, data, logs, and other delivery files.
endlocal
