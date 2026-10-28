@echo off
rem Upgrade Aim - abre o app (modo de pre-visualizacao segura; nada e enviado aos jogos).
rem Compila o app antes, se ele ainda nao tiver sido compilado.
setlocal
set "ROOT=%~dp0"
set "APPDIR=%ROOT%build\release\src\app"
set "EXE=%APPDIR%\UpgradeAim.exe"

if not exist "%EXE%" (
    echo UpgradeAim.exe nao encontrado. Compilando agora, isso leva alguns minutos...
    powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%scripts\build.ps1" -Deploy -NoTest
    if errorlevel 1 (
        echo.
        echo A compilacao falhou. Veja docs\BUILD.md e docs\TROUBLESHOOTING.md.
        pause
        exit /b 1
    )
)

start "Upgrade Aim" /D "%APPDIR%" "%EXE%"
endlocal
