@echo off
title Persona 3: Dancing in Moonlight
cd /d "%~dp0"
set "PATH=C:\msys64\clang64\bin;%PATH%"
rem === Optimizaciones de estabilidad Vulkan para Persona 3 Dancing ===
set "GOW3_UPSCALER=none"
set "GOW3_VK_RECORD_THREAD=0"
set "GOW3_PRESENT_THREAD=0"
set "GOW3_DRAW_PIPE=0"
set "GOW3_PREP_WORKERS=0"
set "DISABLE_LAYER_AMD_SWITCHABLE_GRAPHICS_1=1"
set "VK_LOADER_DISABLE_INST_EXT_FILTER=0"
rem ===================================================================

echo.
echo  ===================================================================
echo    PERSONA 3: DANCING IN MOONLIGHT - PC
echo  ===================================================================
echo.
echo    [CONTROLES DE DIALOGO / HISTORIA]
echo      Avanzar dialogo : Enter  o  X  o  K  o  Flecha Abajo (Cruz/A)
echo      Modo Automatico : I  o  Flecha Arriba (Triangulo/Y)
echo      Acelerar / Skip : E  o  3 (R1)
echo      Cancelar / Atras: Backspace  o  Z  o  L (Circulo/B)
echo.
echo    [CONTROLES DE RITMO]
echo      Notas Izquierda : W, A, S, D
echo      Notas Derecha   : I, J, K, L  o  Flechas direccionales
echo      Scratch / Fever : Espacio  o  Shift (L1/R1)
echo      Pausa           : Escape  o  P (Options/Start)
echo.
echo  ===================================================================
echo.
echo  Iniciando juego...
echo.

"%~dp0bin\p3d_engine.exe" "%~dp0out\p3d_test\boot-linked.bin" --app0 "%~dp0CUSA12636" %* > "%~dp0game_last_run.log" 2>&1
set EXIT_CODE=%ERRORLEVEL%

if %EXIT_CODE% NEQ 0 (
    echo.
    echo ===================================================================
    echo  [AVISO] El juego se cerro inesperadamente (Codigo de salida: %EXIT_CODE%).
    echo ===================================================================
    echo.
    if exist "%~dp0game_crash.log" (
        echo  Detalle del crash:
        type "%~dp0game_crash.log"
        echo.
    )
    echo  Ultimas lineas del registro (game_last_run.log):
    powershell -NoProfile -Command "Get-Content '%~dp0game_last_run.log' -Tail 25 -ErrorAction SilentlyContinue"
    echo.
    echo Presiona cualquier tecla para cerrar...
    pause >nul
)
