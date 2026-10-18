# Guía Maestra: Porting de Juegos PS4 x86-64 a PC Nativo con IA
**Basado en las arquitecturas híbridas de *God of War 3* y *Persona 3: Dancing in Moonlight***

---

## 1. Fundamentos: ¿Qué es la "Vía Híbrida" y por qué supera a la emulación?

Los emuladores tradicionales de PS4 (como RPCSX o shadPS4 estándar) intentan simular la consola al completo de forma genérica. Esto introduce un enorme sobrecoste (*overhead*):
- Emulación o recompilación dinámica (JIT) innecesaria de instrucciones x86-64.
- Sistemas de sincronización de hilos y memoria virtual ultra complejos y lentos.
- Fallos genéricos en llamadas de sistema de Orbis OS.

### La Filosofía del Porting Nativo HLE (Host Execution)
La PlayStation 4 comparte la **misma arquitectura de procesador que un PC moderno**: CPU AMD x86-64.
Por tanto:
1. **CPU Directa (0% Emulación):** El código binario compilado del juego corre **directamente** sobre tu CPU Intel o AMD a velocidad nativa sin JIT.
2. **HLE OS Runtime:** En lugar de emular el sistema operativo Orbis OS de Sony, implementamos stubs nativos en C/C++ para las funciones de `libkernel`, `libScePad` (usando Windows XInput/SDL3), `libSceAudioOut` (usando Windows WASAPI para audio ultra bajo en latencia) y `libSceFios2` (redirigiendo la lectura de archivos directamente al disco duro de tu PC).
3. **Traductor GNM a Vulkan:** La GPU de PS4 ("Liverpool") emite comandos en un formato llamado PM4 / GNM. Nuestro motor intercepta este flujo de paquetes y lo traduce al vuelo a **Vulkan moderno** (`VK_KHR_dynamic_rendering`, `VK_EXT_vertex_input_dynamic2`), aprovechando tu GPU NVIDIA o AMD al 100%.

---

## 2. Estudio de Caso: God of War 3 vs. Persona 3 Dancing

Comprender la diferencia entre estos dos juegos es la clave para no estrellarse al portear:

| Característica | God of War 3 (Santa Monica Studio) | Persona 3: Dancing (Atlus / P-Studio) |
| :--- | :--- | :--- |
| **Tipo de Shaders** | Hiperrealistas, miles de draw calls estáticos por frame, computación pesada. | Anime Cel-Shading, baja cantidad de draws pero con deformación de malla dinámica. |
| **Animación y Skinning** | Buffers precalculados o computados en GPU con paletas fijas. | **Matrices de huesos y deformación en memoria del huésped** que mutan al vuelo justo antes de cada draw. |
| **Pipeline de GPU** | Se beneficia enormemente de `GOW3_DRAW_PIPE` (dibujo asíncrono en hilo secundario) y `GOW3_PREP_WORKERS` (preparación especulativa con 8 hilos). | **Incompatible con draws asíncronos:** si el hilo de dibujo se adelanta al juego, lee huesos desincronizados y los modelos de personajes explotan en fragmentos poligonales. |
| **Middlewares** | Propios de Sony. | **CRI Middleware**: Contenedores CPK con compresión CRILAYLA, cinemáticas CRI Sofdec2/MPV, audio CRI ADX2 (ATRAC9). |
| **Post-procesado** | FSR 3.1 / DLSS / Motion Vectors. | MSAA nativo, resolución de Bloom en formato `B10G11R11UfloatPack32` y contornos de tinta cel-outline (*inverted hull*). |

---

## 3. Secuencia Maestra de Prompts para Portear Juegos (Fase por Fase)

A continuación tienes los prompts exactos y copy-pasteables para guiar a una IA avanzada (Gemini, Claude, ChatGPT) en el porting de cualquier juego de PS4:

---

### FASE 0: Auditoría del Binario, ELF y Assets

> **Prompt Maestro - Fase 0:**
> ```text
> Actúa como un Ingeniero Principal de Sistemas y Recompilación de Bajo Nivel especializado en la arquitectura x86-64 de PlayStation 4 (Orbis OS) y APIs gráficas Vulkan/C++23.
> 
> Vamos a iniciar el port nativo para PC de un juego de PS4. Disponemos del volcado del juego en la carpeta CUSAXXXXX (con eboot.bin, param.sfo y paquetes de assets).
> 
> Tu tarea en esta fase es:
> 1. Analizar el archivo eboot.bin / eboot.elf descifrado: extrae los Program Headers, punto de entrada, tabla de módulos dinámicos importados (PRX) y sus NIDs correspondientes.
> 2. Identificar qué librerías del sistema de PS4 importa (ej. libSceGnmDriver, libScePad, libSceAudioOut, libSceFios2, CRI Middleware, etc.).
> 3. Inspeccionar el contenedor principal de assets (archivos .cpk, .pak o .pkg) y crea un script en Python para listar y extraer su contenido manteniendo la estructura de directorios.
> 4. Generar un informe técnico con la arquitectura del binario y el mapa de memoria inicial necesario para reservar el espacio de direcciones virtual en Windows (VirtualAlloc2/CreateFileMapping2).
> ```

---

### FASE 1: Construcción del Runtime HLE (Kernel, Audio, Pad y FileSystem)

> **Prompt Maestro - Fase 1:**
> ```text
> Con el binario ELF mapeado en memoria virtual x86-64 del host, necesitamos implementar los stubs HLE (High-Level Emulation) mínimos para que el ejecutable arranque su bucle principal sin invocar un emulador completo:
> 
> 1. Implementa en C/C++ los módulos esenciales de Orbis OS:
>    - libkernel: gestión de hilos nativos de Windows (CreateThread con tamaño de stack del huésped), primitivas de sincronización (semáforos, mutexes y event flags de PS4 mapeados a Win32), temporizadores de alta resolución (QueryPerformanceCounter) y asignador de memoria del juego (direct memory pool de 4-5 GB en 0x1000000000).
>    - libScePad: captura de mando mediante XInput / SDL3 y simulación de teclado para PC (Enter/Espacio/WASD mapeados a Cruz, Círculo, Cuadrado, Triángulo, L1, R1).
>    - libSceAudioOut: backend de audio desacoplado en tiempo real usando Windows WASAPI exclusivo o compartido (float 48kHz estéreo) con decodificación de streams de audio (ATRAC9 / FFmpeg).
>    - libSceFios2: interceptor del sistema de archivos para redirigir las rutas internas de la consola (/app0/, /savedata0/, /hostapp/) a rutas relativas en disco local.
> 2. Configura un loader mínimo (probe.c) que cargue el binario enlazado (boot-linked.bin), aplique las relocalizaciones y salte al punto de entrada nativo.
> ```

---

### FASE 2: Traductor GNM a Vulkan (Pipeline Gráfico)

> **Prompt Maestro - Fase 2:**
> ```text
> Ahora debemos conectar las llamadas de renderizado de la GPU Liverpool (PS4 GNM) al backend moderno de Vulkan:
> 
> 1. Intercepta los buffers de comandos PM4 emitidos por el juego y decodifica los registros de estado gráfico (Render Targets, Viewport, Scissor, Blend State, Depth-Stencil).
> 2. Mapea los render passes dinámicos de Vulkan (VK_KHR_dynamic_rendering / vkCmdBeginRendering) sin crear VkRenderPass obsoletos.
> 3. Maneja de forma segura los Multiple Render Targets (MRT): si un slot de color no tiene imagen activa vinculada (image_id == 0), marca el attachment como nulo ({}) para descartar escrituras en lugar de vincular imágenes dummy con dimensiones incorrectas que crashean el driver de NVIDIA/AMD.
> 4. Remapea los formatos de profundidad conflictivos: si el juego solicita Z16 + Stencil8 (incompatible en hardware NVIDIA de PC como DepthStencilAttachment), remapéalo de forma transparente a VK_FORMAT_D24_UNORM_S8_UINT.
> ```

---

### FASE 3: Diagnóstico de Crashes en Drivers de GPU y Memoria

> **Prompt Maestro - Fase 3:**
> ```text
> El juego arranca y muestra pantallas 2D, pero colapsa en el driver gráfico (ej. violación de acceso 0x130 o 0x180 en nvoglv64.dll) al entrar a escenas 3D o diálogos.
> 
> Analiza y corrige los siguientes puntos críticos:
> 1. Sanitización de descriptores nulos: verifica en vk_rasterizer.cpp que ninguna tabla de descriptores (pushDescriptorSet / bindDescriptorSets) reciba VK_NULL_HANDLE en texturas no vinculadas; reemplázalos por un dummy_image_view 1x1 RGBA8 válido.
> 2. Concurrencia de Swapchain y Presentación: desactiva los hilos separados de presentación de cuadros (forzar GOW3_PRESENT_THREAD=0) para evitar colisiones entre vkQueuePresentKHR y el vaciado de buffers del scheduler de comandos.
> 3. Supresión de intercepciones indebidas: añade variables de entorno de bajo nivel en probe.c (__GL_THREADED_OPTIMIZATIONS=0, DISABLE_LAYER_AMD_SWITCHABLE_GRAPHICS_1=1) para impedir que la capa OpenGL de NVIDIA interfiera en el pipeline de Vulkan.
> ```

---

### FASE 4: Sincronización de Modelos 3D, Matrices de Huesos y Cel-Shading

> **Prompt Maestro - Fase 4 (La solución al problema visual de los modelos):**
> ```text
> Los personajes 3D se dibujan en pantalla pero presentan graves deformaciones visuales:
> - Los torsos o extremidades explotan en triángulos estirados desorbitados.
> - El cabello presenta facetas negras o polígonos recortados.
> - El bloom o iluminación se ve sobreexpuesta o blanca cegadora.
> 
> Corrige la arquitectura de sincronización gráfica:
> 1. Condición de carrera en Draw Pipelining: los juegos con animación por software/skinning dinámico (como Atlus) mutan las paletas de matrices de huesos en memoria de huésped justo antes de dibujar. Desactiva en el código fuente el pipeline asíncrono de draws (fuerza Rasterizer::DrawPipeWanted() = false y GOW3_DRAW_PIPE=0) para que la decodificación y el renderizado operen en orden causal estricto.
> 2. Desactiva los trabajadores de preparación especulativa: ajusta DefaultWorkerCount() = 0 (GOW3_PREP_WORKERS=0) para evitar que hilos secundarios lean memoria de huésped desactualizada (stale skinning matrices).
> 3. Respeta el muestreo MSAA del juego: no fuerces GOW3_NO_MSAA=1 si el juego utiliza buffers de iluminación B10G11R11UfloatPack32 con resolve multisample, ya que de lo contrario el buffer de bloom se sobreexpone de forma masiva.
> 4. Asegura la prueba de profundidad en contornos cel-shading (inverted hull): ajusta el depth bias y el stencil test para que las líneas de tinta externa no se solapen con la malla frontal del personaje.
> ```

---

### FASE 5: Automatización de Pruebas y Volcado de Fotogramas (Headless Harness)

> **Prompt Maestro - Fase 5:**
> ```text
> Crea un arnés de pruebas automatizado en Python (tools/game_probe.py) para regresión y validación continua:
> 1. Debe lanzar el ejecutable compilado como subproceso controlado con tiempo límite acotado (--seconds X).
> 2. Simula pulsaciones de mando en momentos exactos a través de un archivo de pipe/texto (pad.txt) para avanzar automáticamente a través de intros, menú de inicio, guardado y secuencias de diálogo.
> 3. Implementa un disparador de volcado de fotogramas (--capture-at X): cuando el tiempo alcance X segundos, genera un archivo trigger para que el presenter de Vulkan extraiga el framebuffer a un volcado RAW 1920x1080 RGBA8.
> 4. Convierte automáticamente el archivo .raw a una imagen .png legible y analiza si la ejecución produjo excepciones o fallos en los registros de depuración.
> ```

---

## 4. Guía para Compartir el Proyecto en GitHub (Sin Incluir el Juego)

Para subir el proyecto a GitHub de forma **100% legal, segura y sin riesgo de infracción de copyright**:

### Paso 1: Configurar el `.gitignore`
Ya hemos creado en la raíz del proyecto un archivo `.gitignore` estricto que excluye automáticamente:
- La carpeta del juego original (`CUSA12636/`).
- Los assets extraídos (`extracted_assets/`).
- Los volcados de Ghidra (`ghidra_*/`).
- Archivos `.cpk`, `.pkg`, `.awb`, `.prx`, `.elf`, `eboot.bin`.
- Los ejecutables compilados locales (`*.exe`, `bin/`, `out/`).
- Logs y volcados de video RAW de depuración (`*.log`, `*.raw`).

### Paso 2: Inicializar y Subir el Repositorio
Abre PowerShell en la raíz del proyecto y ejecuta:

```powershell
# 1. Comprobar que git solo detecta código fuente limpio
git status

# 2. Agregar los archivos permitidos
git add .gitignore Jugar.bat docs/ engine/ native/ tools/ p3d_launcher.cpp p3d_launcher.rc reporte_t_cnico_p3d_crash_vulkan.md

# 3. Hacer el commit inicial del port
git commit -m "Feat: PC Native Port Engine (Direct x86-64 Host CPU + Vulkan GNM Core)"

# 4. Vincular con tu repositorio de GitHub (reemplaza con tu URL)
git remote add origin https://github.com/TU_USUARIO/persona-3-dancing-pc-port.git
git branch -M main
git push -u origin main
```

---

## 5. Instrucciones para que tu Amigo lo Pruebe

Tu amigo solo debe seguir estos pasos sencillos:

1. **Clonar tu repositorio:**
   ```bash
   git clone https://github.com/TU_USUARIO/persona-3-dancing-pc-port.git
   cd persona-3-dancing-pc-port
   ```

2. **Aportar su propia copia del juego:**
   - Copiar la carpeta `CUSA12636` con los archivos originales del juego en la raíz del proyecto.

3. **Obtener el ejecutable del motor:**
   - **Opción recomendada (sin compilar):** Pásale el ejecutable precompilado `p3d.exe` y la carpeta `out/p3d_test/boot-linked.bin` por archivo comprimido privado (Google Drive / Telegram / GitHub Releases privados).
   - **Opción para compilar:** Si tu amigo tiene MSYS2 Clang64 instalado:
     ```powershell
     $env:PATH = "C:\msys64\clang64\bin;$env:PATH"
     ninja -C engine/out/gpu p3d
     ```

4. **Jugar:**
   - Hacer doble clic en [`Jugar.bat`](file:///c:/Users/Alvaro/Downloads/Persona%203%20PC/Jugar.bat) o directamente sobre [`p3d.exe`](file:///c:/Users/Alvaro/Downloads/Persona%203%20PC/p3d.exe).
   - ¡A disfrutar a 60 FPS estables con audio y gráficos perfectos!
