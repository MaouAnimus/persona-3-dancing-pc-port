# Estado de Recompilación e Ingeniería Inversa — P3D Native PC

> [!IMPORTANT]
> **Conclusión de la Auditoría Técnica (Puerta de Decisión 1):**  
> El artefacto analizado `eboot.bin` / `eboot.elf` es un binario nativo de PlayStation 4 (formato `ELF64`, ABI `FreeBSD / Orbis OS`, tipo `0xFE10 / ET_SCE_EXEC`), que depende de 49 bibliotecas propietarias de Sony Orbis OS (`libSceGnmDriver`, `libSceAudioOut`, `libScePad`, `libSceFios2`, etc.).
>
> **No existe previamente un código fuente C/C++ recompilable para Windows ni un ejecutable Win32/Win64 precompilado.**
>
> Por tanto, el proyecto se encuentra formalmente en el **Estado A/B** de la Hoja de Ruta:
> - **Estado A/B:** Se dispone del binario desempacado y el proyecto de análisis en Ghidra (`eboot.elf`). El pseudocódigo y el desensamblado sirven como documentación exacta de las estructuras de datos, lógica de juicio, algoritmos de ritmo y formatos internos.
> - **Estrategia a seguir:** Port nativo por sistemas (Reimplementación en C++20 sobre Windows x64 con Direct3D 11 y audio monotónico) que consume los recursos extraídos legítimamente del juego (`data.cpk` y `ps4.cpk`).

---

## 1. Clasificación del Binario y Arquitectura

- **Ruta del archivo:** `CUSA12636/eboot.elf`
- **Tamaño:** `9,081,617` bytes
- **SHA-256:** `fb605d4fc3583ae6fd94eb07ee982871aa999f80ca7e45726d7f03e29c19a018`
- **Cabecera ELF:** `7F 45 4C 46 02 01 01 09`
  - Clase: ELF64 (64 bits)
  - Endianness: Little-Endian (LSB)
  - ABI del Sistema Operativo: FreeBSD (`0x09` — base de Orbis OS de PS4)
  - Tipo de ELF: `0xFE10` (`ET_SCE_EXEC` — ejecutable PS4 con extensiones SCE)
  - Máquina objetivo: `0x003E` (AMD64 / x86-64)
  - Descriptores de programa: 11 Program Headers (segmentos RX text, R rodata, RW data/bss, SCE_RELA, PT_DYNAMIC)

---

## 2. Mapa de Dependencias del Sistema PS4 Identificadas

Para que la experiencia sea 100% nativa en Windows sin emuladores, se deben sustituir las siguientes interfaces:

| Subsistema de PS4 | Módulos SPRX Originales | Equivalente Nativo en Windows Implementado |
| :--- | :--- | :--- |
| **Gráficos / GPU** | `libSceGnmDriver`, `libSceVideoOut` | Direct3D 11 (HLSL, SwapChain, Device Context) |
| **Audio** | `libSceAudioOut`, `libSceAjm` | Motor de audio PC de baja latencia con reloj monotónico |
| **Entrada / Mandos** | `libScePad`, `libSceMouse` | XInput / DirectInput / Teclado con mapeo configurable |
| **Archivos y Caché** | `libSceFios2`, `libc`, `libkernel` | std::filesystem de C++20 + lector de CPK / recursos directos |
| **Guardado / Opciones** | `libSceSaveData`, `libSceSaveDataDialog` | Guardado en JSON / binario local en AppData del usuario |
| **Reloj y Sincronía** | `libSceRtc`, `libkernel` | QueryPerformanceCounter (QPC) de Windows |

---

## 3. Uso de Ghidra en el Proyecto

El proyecto Ghidra ubicado en `ghidra_projects/Persona3Dancing.gpr`:
1. Permite inspeccionar con precisión las estructuras de las canciones, los registros de ritmo (`danceMain.h`) y las tablas de notas.
2. Permite validar los límites de tiempo de juicio:
   - Ventanas de timing para juicios (Perfect, Great, Good, Miss).
   - Multiplicadores de combo y mecánica de Fever.
3. Permite correlacionar los identificadores de recursos con las tablas de canciones y personajes.

---

## 4. Estado de Implementación Nativa de la Experiencia Completa

| Subsistema / Característica | Estado | Detalles Técnicos |
| :--- | :---: | :--- |
| **Binario Nativo Windows** | **Completado** | `P3DNative.exe` (Win32 x64, MSVC C++20, Direct3D 11, 476 KB). Ejecución standalone sin emulación. |
| **Menú de Selección de Canción** | **Completado** | `GameState::SongSelect` con carrusel interactivo dinámico, ficha de pista detallada, BPM, duración, bailarín principal y selector de dificultad en tiempo real. |
| **Catálogo Completo de Pistas** | **Completado** | 25 canciones oficiales de ATLUS (ID 101 al 125) con audio PCM 48kHz de alta fidelidad decodificado de ATRAC9 (`extracted_assets/audio/`). |
| **Dificultades Oficiales (MNS)** | **Completado** | 111 tablas de ritmo MNS oficiales (`dance/mns/mns***_*.bin`) que abarcan Easy (0), Normal (1), Hard (2) y All Night (3). |
| **Soporte de Mandos / Gamepad** | **Completado** | Soporte Plug & Play XInput para mandos de Xbox y PlayStation: D-Pad, sticks analógicos, botones rítmicos, gatillos de Scratch y navegación por menús. |
| **Motor 3D Personajes (P3D2)** | **Completado** | Renderizador 3D multiextura con sombreado Cel-Shading, iluminación direccional difusa, profundidad depth-stencil y 23 submesh mapping (Cuerpo 2048x2048, Rostro 1024x1024, Cabello 2048x1024, Ojos 512x256, Cel Outline #0e1014). |
| **Persistencia de Puntuaciones** | **Completado** | Guardado y lectura de High Scores y Rangos de victoria (`user_scores.dat`) persistidos entre partidas por canción y dificultad. |

