@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0package-cpp-release.ps1" %*
if errorlevel 1 (
    echo Release packaging failed. See the error above.
    pause
    exit /b 1
)
pause
