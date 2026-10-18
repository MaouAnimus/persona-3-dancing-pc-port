# Diagnóstico del Crash en vkCmdEndRendering — Persona 3: Dancing in Moonlight PC
**Proyecto**: Port Nativo PC de *Persona 3: Dancing in Moonlight* (`CUSA12636`)  
**Arquitectura**: Reubicación y ejecución directa de código nativo PS4 x86-64 en CPU host Windows con runtime propio HLE (`libkernel`, `libScePad`, `libSceAudioOut`, CRI Middleware) + Traducción de comandos de GPU GNM a Vulkan (`video_core`).  
**Entorno de Compilación**: MSYS2 Clang64 (Clang 19+ / C11 & C++23) + CMake + Ninja (`ninja -C engine/out/gpu p3d`).  
**Binarios**: [`bin/p3d_engine.exe`](file:///c:/Users/Alvaro/Downloads/Persona%203%20PC/bin/p3d_engine.exe) y [`p3d.exe`](file:///c:/Users/Alvaro/Downloads/Persona%203%20PC/p3d.exe) (Launcher en raíz).  
**Código Fuente Limpio**: [`persona3_dancing_pc_source.zip`](file:///c:/Users/Alvaro/Downloads/Persona%203%20PC/persona3_dancing_pc_source.zip) (12.22 MB).

---

## 1. Estado de Ejecución y Progresión de Estabilidad

- **Pantalla de título (2D + Video CRI Sofdec2)**: 60 FPS estables, audio WASAPI sincronizado, navegación de menús operativa.
- **Hitos Superados**:
  1. **Crash `0x134` en `vkCmdBeginRendering` (`nvoglv64.dll+0xee8504`)**: **100% Solucionado** mediante la sanitización de attachments nulos y soporte de extensiones dinámicas.
  2. **Crash `0x130` inicial por incompatibilidad de silicio (`D16UnormS8Uint`)**: **100% Solucionado** remapeando `Z16 + Stencil8` a `VK_FORMAT_D24_UNORM_S8_UINT` en `liverpool_to_vk.cpp`. La advertencia de hardware desapareció de los registros.
- **Salto Masivo de Rendimiento**:
  - En la prueba previa se completaron **34,163 pases dinámicos de renderizado 3D** de forma consecutiva (registrados en `rendering_debug.log`, 7.6 MB de actividad).
  - El motor logró dibujar el shadow pass de 2048x2048 y pases a resolución nativa 1080p.

---

## 2. Diagnóstico del Crash en `vkCmdEndRendering`

### 2.1. Rectificación del Stack Trace
- Los frames profundos referenciando `runtime_memory_write_backing` correspondían a restos de un escaneo de pila por heurística de direcciones.
- La **cadena sólida y verificada** en el código es:
  $$\text{DrawRecord} \longrightarrow \text{FlushBarriers} \longrightarrow \text{Scheduler::EndRendering} \longrightarrow \text{vkCmdEndRendering}$$
- Excepción: `read access to 0000000000000130 at nvoglv64.dll+0xe19da0` con `%rcx = 0x0` (desreferencia nula dentro del driver NVIDIA al cerrar un pase de renderizado).

### 2.2. Mecánica del Fallo
En `Rasterizer::DrawRecord`:
1. `Rasterizer::BeginRendering(pipeline)` calcula el `RenderState` del siguiente draw (o reutiliza memo), invocando `FindImage`, `UpdateImage`, `SetBackingSamples`, `FindRenderTarget` y encolando `Transit`.
2. Si se detecta necesidad de barrera (`needs_barrier = true`), la línea 1114 de `vk_rasterizer.cpp` invoca:
   ```cpp
   if (needs_barrier) {
       runtime.FlushBarriers();
   }
   ```
3. `Runtime::FlushBarriers()` (en `vk_runtime.cpp:885`) necesita someter una barrera `vkCmdPipelineBarrier2`. Como Vulkan prohíbe emitir una barrera global dentro de un render pass dinámico activo, invoca `scheduler.EndRendering()`.
4. `EndRendering()` ejecuta `current_cmdbuf.endRendering()`.
5. **Causa del Colapso**: `vkCmdEndRendering()` está cerrando el pase del draw *anterior*, el cual contiene referencias que el driver NVIDIA no puede validar o resolver al hacer el store/finish.

---

## 3. Hipótesis Principales

### Hipótesis A: El Dummy View de 1x1
- Cuando un slot de Multiple Render Targets (MRT) no tiene imagen activa, el motor asigna `dummy_image_view` (imagen 1x1 R8G8B8A8).
- Ese slot omite el ajuste de dimensiones, dejando `renderArea` en `1920x1080` mientras el attachment mide `1x1`, violando la regla de Vulkan de que el área de renderizado debe caber en todos los attachments.
- El dummy view se declara con `imageLayout = eColorAttachmentOptimal` sin haber recibido una transición previa desde `eUndefined`.
- Si el driver NVIDIA intenta procesar el attachment de 1x1 al cerrar el pase, desreferencia el puntero de descriptor interno.

### Hipótesis B: Ciclo de Vida de Imágenes y Vistas
- Entre que se abre un pase de renderizado y se cierra con `EndRendering()`, la preparación del siguiente draw en `BeginRenderingFull` puede recrear backings (`SetBackingSamples`, `FindImage` con `needs_rebind`).
- Si un `VkImageView` o `VkImage` del pase activo es destruido o recreado antes de cerrarse el pase, `vkCmdEndRendering()` colapsa en el driver.

---

## 4. Instrumentación Implementada y Desplegada en el Build Actual

Se agregaron mecanismos de diagnóstico sin alterar el comportamiento por defecto:

### 4.1. Detección de Dummy Attachments en `vk_rasterizer.cpp`
Se instrumentó `log_dummy` para registrar en `rendering_debug.log` si `dummy_image_view` entra en algún pase:
```cpp
auto log_dummy = [](u32 slot, const char* reason) {
    if (FILE* f = std::fopen("rendering_debug.log", "a")) {
        std::fprintf(f, "DUMMY_ATTACHMENT slot=%u reason=%s\n", slot, reason);
        std::fclose(f);
    }
};
```
Se registra el handle inicial (`INIT: dummy_image_view handle=0x...`) y cualquier invocación por `no_image` o `bad_view`.

### 4.2. Flag de Prueba de Ciclo de Vida (`P3D_EXP_ENDRENDER_EARLY`)
En `Rasterizer::BeginRendering()`:
```cpp
static const bool early_end = [] {
    const char* v = std::getenv("P3D_EXP_ENDRENDER_EARLY");
    return v && v[0] == '1';
}();
if (early_end) {
    scheduler.EndRendering();
}
```
Si se define `$env:P3D_EXP_ENDRENDER_EARLY="1"`, el pase anterior se cierra de inmediato antes de que el motor toque el texture cache o modifique descriptores.

---

## 5. Instrucciones de Verificación

### Prueba 1: Test de Ciclo de Vida Anticipado
En PowerShell:
```powershell
$env:P3D_EXP_ENDRENDER_EARLY = "1"
.\p3d.exe
```
- **Si el juego avanza sin crash tras el título**: Confirma la Hipótesis B (ciclo de vida / recreación de views durante el pase abierto).
- **Si crashea de la misma forma**: Se descarta el ciclo de vida y se inspecciona el log del dummy.

### Prueba 2: Inspección de Dummy en `rendering_debug.log`
Revisar si en `rendering_debug.log` aparecen líneas `DUMMY_ATTACHMENT slot=... reason=...` antes del crash.

### Prueba 3: Vulkan Validation Layers
El entorno cuenta con `VkLayer_khronos_validation.dll` en `C:\msys64\clang64\bin\`. Para capturar el VUID exacto:
```powershell
$env:VK_LAYER_PATH = "C:\msys64\clang64\bin"
$env:VK_INSTANCE_LAYERS = "VK_LAYER_KHRONOS_validation"
$env:GOW3_VK_VALIDATION = "1"
$env:GOW3_VK_VALIDATION_SYNC = "1"
.\p3d.exe
```
El primer error reportado en consola o en `game_last_run.log` identificará el objeto y la regla de especificación vulnerada.