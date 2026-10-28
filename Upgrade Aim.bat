@echo off
rem Upgrade Aim - starts StickyAim Lab (safe preview mode; nothing is sent to games).
rem Builds the app first if it has not been built yet.
setlocal
set "ROOT=%~dp0"
set "APPDIR=%ROOT%build\release\src\app"
set "EXE=%APPDIR%\StickyAimLab.exe"

if not exist "%EXE%" (
    echo StickyAimLab.exe not found. Building it now, this takes a few minutes...
    powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%scripts\build.ps1" -Deploy -NoTest
    if errorlevel 1 (
        echo.
        echo Build failed. See docs\BUILD.md and docs\TROUBLESHOOTING.md.
        pause
        exit /b 1
    )
)

start "Upgrade Aim" /D "%APPDIR%" "%EXE%"
endlocal
