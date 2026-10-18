# Persona 3: Dancing in Moonlight - Native PC Engine

Motor de ejecución nativo x86-64 y capa de traducción gráfica Vulkan para **Persona 3: Dancing in Moonlight** (PS4 CUSA12636).

## Arquitectura

- **Carga de Binarios y Relocalizaciones**: Carga el ejecutable ELF desencriptado (`boot-linked.bin`) directamente en la memoria virtual del host Windows x86-64.
- **Traductor de Shaders y GPU**: Capa Vulkan moderna basada en spir-v para reproducir los command buffers y draw calls de GNM/GCN nativamente en tarjetas gráficas modernas.
- **HLE Runtime de PlayStation 4**:
  - `runtime_pad`: Sistema de entrada mapeado a mando (DirectInput / XInput / RawInput) y teclado con perfiles optimizados para juegos de ritmo.
  - `runtime_audio` / `atrac9`: Decodificación y playback de streams de audio AT9 y MP3 con baja latencia.
  - `runtime_videodec`: Reproducción de secuencias y clips de fondo.
  - `runtime_savedata`: Gestión de partidas guardadas en formato de PC (`user/savedata`).
  - `runtime_thread` / `runtime_sync`: Planificación de hilos y sincronización con primitivas nativas de Windows (`ntdll` / `onecore`).

## Compilación del Ejecutable `p3d.exe`

El binario ejecutable principal se genera con Clang/Ninja de MSYS2:

```bash
cd engine/gpu
cmake -B out -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C out p3d
```

El ejecutable resultante se despliega en la raíz del proyecto como `p3d.exe` y `bin/p3d.exe`.
