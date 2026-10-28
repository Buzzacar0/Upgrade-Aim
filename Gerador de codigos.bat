@echo off
rem Abre o gerador de codigos de ativacao do Upgrade Aim (so para o dono; nunca distribua).
setlocal
set "KG=%~dp0dist\keygen\upgradeaim-keygen.exe"
if not exist "%KG%" (
    echo O gerador ainda nao foi montado. Rodando scripts\package.ps1, isso leva alguns minutos...
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\package.ps1" -NoTest
    if errorlevel 1 (
        echo Falhou. Veja docs\DISTRIBUTION.md.
        pause
        exit /b 1
    )
)
start "" "%KG%"
endlocal
