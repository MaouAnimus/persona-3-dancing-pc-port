@echo off
title Persona 3 Dancing Launcher
cd /d "%~dp0"

where pythonw >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    start "" pythonw "%~dp0p3d_launcher.pyw"
    exit /b 0
)

where python >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    start "" python "%~dp0p3d_launcher.py"
    exit /b 0
)

echo.
echo [AVISO] No se encontro Python en el sistema.
echo Ejecutando el juego directamente mediante el motor nativo...
call "%~dp0Jugar.bat"
