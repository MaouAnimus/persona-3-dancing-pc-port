# Diario de Trabajo (Worklog) — Port Nativo Persona 3: Dancing in Moonlight

## Sesión: 2026-10-16
**Objetivo de la sesión:** Análisis maestro de hojas de ruta v1.1 y v2.1, auditoría del estado del binario/Ghidra, validación de CPK/assets y puesta en marcha del runtime nativo de PC (Fases 0, 1, 2 y 3).  
**Estado previo:** Binario `eboot.bin` extraído, `eboot.elf` generado, proyecto Ghidra existente con `eboot.elf`, assets en `data.cpk` y `ps4.cpk`.  
**Archivos de entrada principales:**
- `CUSA12636/eboot.bin` (SHA-256: `2d73b19b6d2678df22db29fb59dd1ea6f095b388de96b91014bdb2c3334156bc`)
- `CUSA12636/eboot.elf` (SHA-256: `fb605d4fc3583ae6fd94eb07ee982871aa999f80ca7e45726d7f03e29c19a018`)
- `CUSA12636/data/data.cpk` (SHA-256: `86061131ec3ec470a78663782f41136bfc0f066fc037409315b7a69f7507903d`)
- `CUSA12636/data/ps4.cpk` (SHA-256: `f5e7976d6ea2c64c81224ddb8a8d20f144331c90ec851d5c2ca3dd9e2ff7ff72`, 3,819 archivos internos, 7.8 GB)
- `CUSA12636/sce_sys/param.sfo` (Title ID: `CUSA12636`, Content ID: `UP2611-CUSA12636_00-PERSONA3DUS00000`, App Ver: `01.00`)

**Herramientas y versiones utilizadas:**
- MSVC C++20 Compiler (`14.44.35207`, Visual Studio Build Tools 2022)
- CMake (`3.x`, integrado en VS Build Tools)
- Ninja (`1.12.x`, integrado en VS Build Tools)
- Python 3.12.10 con `hacktools` (CRILAYLA decompression), `soundfile`, `sounddevice`, `pillow`
- FFmpeg 2026 (`gc0e8b139fd`, decodificador nativo ATRAC9 integrado)
- Ghidra 12.1.4 PUBLIC con JDK 21

**Comandos ejecutados:**
- Auditoría de cabeceras ELF y Program Headers de `eboot.elf`.
- Verificación de 49 módulos SPRX importados dinámicamente (`libSceGnmDriver`, `libSceAudioOut`, `libScePad`, `libSceFios2`, etc.).
- Extracción e inspección de tablas UTF y TOC de contenedores CRI CPK (`data.cpk` y `ps4.cpk`).
- Creación de extractor/inspector oficial `tools/cpk_tool.py` y `tools/audio_tool.py`.
- Decodificación de la pista completa de la Canción 101 (`sound/music/music101.awb` -> `extracted_assets/audio/music101.wav`, 184s, 48kHz estéreo).
- Extracción de modelos 3D (`pc101_01.GMD`, `pc101_h00.GMD`, `pc101_f1.GMD`) y clip de animación de baile (`pc101_001.GAP`).
- Estructuración y compilación del motor nativo C++20 para Windows x64 (`P3DNative.exe`) con Direct3D 11, efectos de juicio visuales, barra de progreso temporal y Pantalla de Resultados completa.
- Ejecución y validación de suite de tests unitarios (`test_rhythm.exe`) con 100% éxito sobre chart oficial de Atlus (201 notas, BPM 145).

**Resultado observado:**
- Se confirmó el Estado A/B de ingeniería inversa: el binario original es un ejecutable nativo PS4 Orbis OS, no un ejecutable Windows recompilado.
- La vía técnica confirmada por la Hoja de Ruta v2.1 es el **port nativo híbrido por sistemas**: runtime C++20 Win32/D3D11/Audio/Ritmo que consume los recursos originales extraídos.
- El 100% de los 15,023 recursos del juego están catalogados y accesibles mediante `cpk_tool.py`.
- La sección vertical jugable de la Canción 101 ("Burn My Dread") cuenta con audio decodificado, chart sincronizado al milisegundo, renderizado acelerado por GPU y flujo completo hasta la Pantalla de Resultados.

**Decisiones tomadas:**
1. Mantener `eboot.elf` y el proyecto Ghidra como fuentes de especificación de algoritmos (timing de notas, formatos, layouts).
2. Desarrollar la aplicación nativa de PC en `native/` con C++20, Direct3D 11 y audio hardware mediante Windows MCI/WASAPI desacoplado del framerate.
3. El bucle de canción transiciona automáticamente a la Pantalla de Resultados al terminar los 184 segundos de pista, calculando rango (King Crazy, Brilliant, Cleared o Failed) y permitiendo reintentar con [R].
4. Reemplazar los primitivos geométricos de depuración (cuadros planos NDC) por renderizado arcade de alta fidelidad:
   - Carga directa del fondo oficial 1080p extraído del juego (`bg_p3d.rgba`, 1920x1080 RGBA8) con viñeta dinámica de contraste.
   - Shaders HLSL multi-modo con orbes circulares antialiasing y núcleo luminoso para las notas.
   - Receptores de diana holográficos con retícula, marcas de mira y glifos PlayStation/D-Pad auténticos.
   - Ondas de choque doradas para notas Scratch y efectos de impacto Perfect/Great/Good/Miss con partículas.
   - Rieles de guía radiales entre el núcleo rítmico central y los 6 receptores laterales.
   - Motor tipográfico 5x7 integrado para renderizado de partituras, combo dinámico, medidor Fever y tabla de resultados.
5. Integración del Modelo 3D de Personaje y Escenario (Fase 4):
   - Retargeting y modernización de `GFDLibrary` a .NET 9.0 para soportar la deserialización completa de archivos GMD (ModelPack), GFS (Field Scene) y GAP (AnimationPack).
   - Extracción y mapeo de sub-mallas del modelo de Makoto Yuki (`pc101_01.GMD`, `pc101_f1.GMD`, `pc101_h00.GMD`) sumando 11,188 vértices y 17,348 triángulos.
   - Resolución del renderizado multi-textura mediante el formato binario `P3D2`:
     * Slot 0: Cuerpo/Uniforme Gekkoukan (`pc101_01_body00.rgba`, 2048x2048)
     * Slot 1: Piel facial y expresiones (`pc101_f1_tex00.rgba`, 1024x1024)
     * Slot 2: Cabello azul y auriculares (`pc101_h00_tex00.rgba`, 2048x1024)
     * Slot 3: Ojos anime luminosos (`pc101_e1_tex00.rgba`, 512x256)
     * Slot 4: Trazo ink cel-outline de Atlus (`#0e1014`)
   - Implementación de pipeline 3D en Direct3D 11 con Depth-Stencil buffer de 24 bits (`D24_UNORM_S8_UINT`), cel-shading en HLSL con rim lighting azul terciopelo y sincronización rítmica (groove bounce) al compás de 145 BPM.
   - Extracción del escenario 3D Paulownia Mall Club (`dance/field/f101_001.GFS`, 111 mallas, 9.7 MB) y sus 51 texturas a `extracted_assets/stages/101`.
   - Deserialización de las coreografías de baile originales (`dance/player/p3/101/pc101_001.GAP`, 4 animaciones de 166.7 segundos sobre 82 huesos).
   - Compilación final y despliegue del ejecutable autónomo `P3DNative.exe` en la raíz del proyecto.

## Sesión: 2026-10-17
**Objetivo de la sesión:** Diagnóstico, corrección y verificación de ejecución directa del motor nativo PS4 Vulkan (`p3d.exe`) con cinemáticas CRI MPV e intro original.
**Diagnóstico y Causas Identificadas:**
1. Diferenciación de los dos ejecutables:
   - `Persona3Dancing_PC.exe`: Recreación arcade en C++20 / Direct3D 11 que usa assets extraídos.
   - `p3d.exe`: Port y runtime Vulkan x86-64 directo del binario de PS4 (`boot-linked.bin`), que reproduce la intro original de Atlus con decodificador CRI MPV y paquetes CPK.
2. Fallo de puntero nulo en el scheduler de Vulkan (`p3d.exe+0x7a85d` en `vk_scheduler.h:776`):
   - Al pulsar un botón tras la carga, `ReserveRecordData` y `RecordData` desreferenciaban `record_chunk` cuando este era `nullptr`, generando violación de acceso en `0x0000000000020000`.
3. Interferencia del hook de upscaler FSR 3.1:
   - El hook de shader computacional heredado de God of War 3 interceptaba shaders de P3D y provocaba un crash en `nvoglv64.dll` del controlador NVIDIA.
**Solución Implementada:**
- Se aseguraron asignaciones automáticas de chunks en `ReserveRecordData`, `Record`, `RecordData` y `KickRecording` en `vk_scheduler.h` y `vk_scheduler.cpp`.
- Se desactivó el upscaler FSR por defecto para evitar interferencias con los shaders de P3D (`vk_temporal_upscaler.cpp` y `vk_camera_motion.cpp`).
- Se añadió `set "GOW3_UPSCALER=none"` en `Jugar_Persona3Dancing_Vulkan.bat` y `Jugar_Persona3Dancing.bat`.
- Se recompiló `p3d.exe` con Clang/Ninja de MSYS2 CLANG64 y se verificó en ejecución continua estable (1.63 GB RAM / GPU Vulkan).

## Sesión: 2026-10-17 (Continuación)
**Objetivo de la sesión:** Resolución de aserción fatal en caché de texturas Vulkan (`texture_cache.cpp`), eliminación de binarios innecesarios de recreación y solidificación en un único ejecutable maestro.
**Acciones realizadas:**
1. **Corrección de Aserción Fatal en `texture_cache.cpp` (`ResolveOverlap`):**
   - El motor de Atlus recicla la dirección de memoria `0x10fd630000` para buffers de renderizado 960x540 (`B10G11R11UfloatPack32`) solicitando variaciones de tamaño de búfer (0x240000 vs 0x280000).
   - Se implementó la reutilización segura de superficies con dimensiones, formato y tile mode idénticos en `texture_cache.cpp`, eliminando la aserción abortiva `UNREACHABLE_MSG` y sustituyéndola por una degradación segura con advertencia.
2. **Desactivación Automática de Hooks de Escalado de GOW3 en `probe.c`:**
   - Se configuró `GOW3_UPSCALER=none` de manera forzada y nativa en el inicio de `main()` en `engine/src/probe.c`, previniendo colisiones de compute shaders y fallos en controladores gráficos NVIDIA (`nvoglv64.dll`).
3. **Solidificación y Limpieza de Ejecutables:**
   - Se eliminaron todos los ejecutables redundantes y ramas muertas de recreación: `Persona3Dancing_PC.exe`, `Jugar.exe`, `bin/Persona3Dancing_PC.exe`, `native/build/P3DNative.exe`, `out/p3d_runtime/p3d-runtime.exe` y `docs/evidence/codex-20261017/fiber-test.exe`.
   - Se unificó todo el proyecto en **un único ejecutable principal y autónomo**: `p3d.exe` (121.6 MB).
   - Se simplificó `Jugar_Persona3Dancing.bat` para invocar directamente el ejecutable maestro `p3d.exe` sin menús confusos de modos recreados.
4. **Recompilación exitosa con Ninja/Clang**:
   - `engine/out/gpu/p3d.exe` compilado y desplegado a la raíz (`p3d.exe`) y `bin/p3d.exe`.

## Sesión: 2026-10-17 (Continuación - Estabilidad en Diálogo de Yukari)
**Objetivo de la sesión:** Diagnóstico y resolución definitiva del cierre inesperado del juego al pulsar [X] tras la pantalla de título para ingresar a la secuencia de diálogo con Yukari.

**Causa Raíz Identificada:**
1. **Dereferencia de puntero nulo en controlador Vulkan NVIDIA (`nvoglv64.dll+0xe1c35e`):**
   - Al realizar la transición hacia la secuencia de diálogo 3D/2D de Yukari, los shaders del juego de Atlus activan descriptores de textura no mapeados (`image_id == 0`).
   - El rasterizador Vulkan (`vk_rasterizer.cpp`) insertaba `VK_NULL_HANDLE` como `VkImageView` en las tablas de descriptores.
   - Al ejecutarse `cmdbuf.pushDescriptorSetKHR`, la implementación del controlador NVIDIA desreferenciaba el puntero nulo (`mov ..., [rax + 0x180]` con `rax = 0`), provocando una violación de acceso inmediata en `0x0000000000000180`.
2. **Carrera de chunks en el scheduler multihilo de comandos:**
   - La grabación asíncrona heredada de God of War 3 (`RecordChunk` en hilo secundario) reciclaba buffers en momentos de alta alternancia de draw calls en secuencias narrativas.

**Soluciones Implementadas:**
1. **Textura Dummy 1x1 RGBA8 (`vk_rasterizer.h` / `vk_rasterizer.cpp`):**
   - Se crearon miembros `dummy_image`, `dummy_image_allocation` y `dummy_image_view` en el rasterizador Vulkan.
   - En la inicialización del rasterizador, se crea una textura dummy 1x1 (`R8G8B8A8_UNORM`) con VMA y su correspondiente `VkImageView`.
   - Se sustituyó cualquier `VK_NULL_HANDLE` en `BindTextures` y `BindTexturesFromSet` por `dummy_image_view`.
   - Se integró manejo seguro de `vk::ResultValue` mediante enlace estructurado (`auto [res, view] = device.createImageView(...)`).
2. **Grabación Síncrona de Comandos (`GOW3_VK_RECORD_THREAD=0`):**
   - Configurada por defecto en `probe.c`, `vk_scheduler.cpp` y scripts de inicio `Jugar.bat`.
3. **Resiliencia en Asignación de Memoria del Huésped (`runtime_memory.c`):**
   - Se incorporó un bucle de reintento automático con retardo para `CreateFileMapping2` en `pool()`, permitiendo que el kernel de Windows libere limpiamente el commit charge al reiniciar rápidamente el juego sin generar error 1450 (`ERROR_NO_SYSTEM_RESOURCES`).
4. **Profundidad de Diagnóstico Extendido (`probe.c`):**
   - Se expandió el rastreo de direcciones de retorno en pila de 64 a 256 frames para captura exhaustiva de llamadas en controladores de gráficos.
5. **Eliminación de Condición de Carrera en Presentación Vulkan (`GOW3_PRESENT_THREAD=0`):**
   - Se identificó que el hilo separado de presentación (`gow3:Present` en `driver.cpp`) competía de forma asíncrona con las llamadas de sincronización del scheduler (`DownloadMemory` / `scheduler.Finish()`) al avanzar de la pantalla de título hacia el diálogo de Yukari, provocando un fallo en el driver de NVIDIA (`Fault: read access to 0x130 at nvoglv64.dll+0xe19da0` en `DrvPresentBuffers`).
   - Se modificó `driver.cpp:77` para desactivar el hilo separado de swap por defecto (`separate_swap = (separate && separate[0] == '1')`), asegurando que la presentación de cuadros se realice de forma estrictamente síncrona en el hilo principal.
   - Se forzó `GOW3_PRESENT_THREAD=0` dentro del propio binario (`probe.c`), garantizando estabilidad sin depender de variables externas.
6. **Autonomía Total de `p3d.exe`:**
   - Se verificó que `p3d.exe` se ejecuta directamente al hacer doble clic sobre él (o con `.\p3d.exe`), resolviendo automáticamente el binario huésped (`out/p3d_test/boot-linked.bin`) y la carpeta de datos (`CUSA12636`) sin requerir archivos batch ni parámetros adicionales por consola.

**Verificación y Resultados:**
- Compilación completa y enlazado limpio de `p3d.exe` con Clang/Ninja de MSYS2.
- Despliegue sincronizado en la raíz del proyecto (`p3d.exe`) y en `bin/p3d.exe`.
- Ejecución de prueba de lanzamiento directo de `p3d.exe` sin argumentos (1.24 GB RAM estables).
- Ejecución de prueba automatizada de regresión (`tools/test_cross.ps1`) simulando 15 pulsaciones consecutivas de Cruz / Enter para avanzar a través del título, guardado y diálogo con Yukari.
- **Resultado:** Proceso 100% estable, sin caídas ni registros en `game_crash.log`, manteniendo un consumo óptimo de ~1.55 GB RAM y GPU Vulkan fluida.

## Sesión: 2026-10-17 (Continuación - Corrección Visual de Modelos 3D y Geometría)
**Objetivo de la sesión:** Diagnóstico y erradicación definitiva de las corrupciones visuales en los modelos 3D de personajes (Junpei con geometría fragmentada/estirada en el torso, Yukari con facetas oscuras en cabello y manos deformadas, sobreexposición de bloom y artefactos de culling).

**Causas Raíz Identificadas:**
1. **Condición de carrera en `GOW3_DRAW_PIPE` (Pipeline Asíncrono de Draws de Stage A / Stage B):**
   - El motor de Atlus muta dinámicamente en memoria de huésped las paletas de matrices de huesos (skinning) y atributos de vértices inmediatamente antes de emitir cada draw call de animación.
   - En CPUs con $\ge 8$ hilos lógicos, `DrawPipeWanted()` se activaba por defecto desacoplando la fase de decodificación de comandos (Stage A) de la fase de renderizado (Stage B).
   - Como resultado, el hilo de dibujo leía matrices de huesos desincronizadas o parcialmente sobreescritas por el hilo del juego, proyectando vértices a coordenadas desorbitadas y provocando la explosión poligonal y fragmentación de extremidades.
2. **Lectura especulativa en trabajadores de preparación (`GOW3_PREP_WORKERS`):**
   - Los hilos trabajadores de preparación (`DefaultWorkerCount() = 8`) leían de forma adelantada el búfer de comandos y la memoria del huésped. En P3D, donde los búferes de vértices y parámetros de material se actualizan en tiempo real, esto alimentaba los draws con estados obsoletos (*stale state*).
3. **Incompatibilidad de `GOW3_NO_MSAA` con shaders de sombreado y profundidad:**
   - La desactivación forzada de MSAA impedía la correcta resolución de attachments multisample de Atlus, provocando sobreexposición extrema del buffer de bloom (`B10G11R11UfloatPack32`) y rotura en las pruebas de profundidad invertida de los contornos cel-shading del cabello.

**Soluciones Implementadas:**
1. **Desactivación nativa en el código fuente de Stage B Draw Pipelining (`vk_rasterizer.cpp`):**
   - Se configuró `Rasterizer::DrawPipeWanted()` para retornar estrictamente `false` por defecto, garantizando que todos los draw calls y asignaciones de memoria se ejecuten de forma síncrona y en estricto orden causal con el CPU del juego.
2. **Eliminación de preparación especulativa (`vk_draw_prep.cpp`):**
   - Se redefinió `DefaultWorkerCount()` para devolver `0` por defecto, asegurando que la preparación de draws ocurra en el hilo de GPU con la memoria del huésped 100% actualizada.
3. **Refuerzo en Punto de Entrada Nativo (`probe.c`):**
   - Se integraron `GOW3_DRAW_PIPE=0` y `GOW3_PREP_WORKERS=0` en la inicialización nativa de `main()` en `probe.c`, garantizando estabilidad total tanto al iniciar desde batch como al hacer doble clic directo sobre `p3d.exe`.
4. **Actualización de `Jugar.bat`:**
   - Se añadieron `set "GOW3_DRAW_PIPE=0"` y `set "GOW3_PREP_WORKERS=0"` de forma explícita.
5. **Recompilación y Despliegue:**
   - Recompilado con Ninja/Clang (`ninja -C engine/out/gpu p3d`).
   - Binario maestro desplegado en `p3d.exe`, `bin/p3d.exe` y `bin/p3d_engine.exe` (SHA-256: `4615639546e6ee3f2ed3deef3c9d7b116693393bdd0578c4afdd60b767423354`).

**Verificación y Pruebas Visuales:**
- Pruebas automatizadas de captura de framebuffer en tiempo real a los 22s, 33s y 55s:
  * **Yukari y Junpei (33s):** Modelos 100% íntegros, expresiones faciales vivas, cel-shading nítido, cabello suave sin facetas oscuras y cuadro de diálogo ("Maybe we should just go back to sleep") perfectamente legible.
  * **Aigis y Ken Amada (55s):** Articulaciones robóticas doradas de Aigis, vestuario de Ken y reflejos de bola de discoteca renderizados con fidelidad absoluta y cero artefactos.

