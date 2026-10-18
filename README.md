# Persona 3: Dancing in Moonlight — PC Native Port

[![Platform](https://img.shields.io/badge/Platform-Windows%20x64-blue.svg)](https://microsoft.com/windows)
[![API](https://img.shields.io/badge/Graphics-Vulkan%201.3-red.svg)](https://www.vulkan.org/)
[![License](https://img.shields.io/badge/License-GPL--2.0--or--later-green.svg)](LICENSE)
[![Audio](https://img.shields.io/badge/Audio-WASAPI%20Exclusive-purple.svg)]()

Port nativo para PC de ***Persona 3: Dancing in Moonlight*** (`CUSA12636`) mediante **ejecución directa de código de máquina x86-64 en CPU host** y **traducción dinámica de comandos GPU GNM a Vulkan**, desacoplado de emuladores de consola tradicionales.

---

## 🚀 Características Principales

- **0% Emulación de CPU:** El binario original de PS4 corre como código de máquina x86-64 nativo directamente sobre la CPU del host (Intel / AMD), sin recompiladores JIT ni capas de virtualización pesadas.
- **Backend Vulkan Moderno:** Traductor de comandos de GPU Liverpool (GNM/PM4) a Vulkan 1.3 con soporte de renderizado dinámico (`VK_KHR_dynamic_rendering`).
- **Sincronización Estricta de Modelos 3D:** Renderizado cel-shading de alta fidelidad con mallas de personajes (Yukari, Junpei, Aigis, Ken, Makoto), contornos de tinta invertida (*inverted hull cel-outline*) y sincronización de matrices de huesos en tiempo real.
- **Audio de Ultra Baja Latencia:** Implementación nativa de `libSceAudioOut` sobre Windows WASAPI (48 kHz, punto flotante estéreo).
- **Cinemáticas y Video en Tiempo Real:** Decodificación fluida de flujos de video CRI Sofdec2 / MPV.
- **Soporte de Mandos y Teclado:** Compatibilidad plug-and-play con mandos de Xbox, PlayStation (XInput/DirectInput) y controles de teclado mapeados.

---

## 🎮 Controles

### Modo Diálogo / Menús / Historia
| Acción | Teclado | Mando (PlayStation / Xbox) |
| :--- | :--- | :--- |
| **Avanzar Diálogo / Aceptar** | <kbd>Enter</kbd> / <kbd>X</kbd> / <kbd>K</kbd> / <kbd>↓</kbd> | Cruz / <kbd>A</kbd> |
| **Cancelar / Volver** | <kbd>Backspace</kbd> / <kbd>Z</kbd> / <kbd>L</kbd> | Círculo / <kbd>B</kbd> |
| **Modo Automático** | <kbd>I</kbd> / <kbd>↑</kbd> | Triángulo / <kbd>Y</kbd> |
| **Saltar / Acelerar Diálogo** | <kbd>E</kbd> / <kbd>3</kbd> | <kbd>R1</kbd> |

### Modo Ritmo (Gameplay de Baile)
| Acción | Teclado | Mando (PlayStation / Xbox) |
| :--- | :--- | :--- |
| **Notas Izquierda (Arriba / Izq / Abajo)** | <kbd>W</kbd>, <kbd>A</kbd>, <kbd>S</kbd> | D-Pad (<kbd>↑</kbd>, <kbd>←</kbd>, <kbd>↓</kbd>) |
| **Notas Derecha (Arriba / Der / Abajo)** | <kbd>I</kbd>, <kbd>L</kbd>, <kbd>K</kbd> o Flechas | Triángulo, Círculo, Cruz (<kbd>Y</kbd>, <kbd>B</kbd>, <kbd>A</kbd>) |
| **Scratch / Anillo Fever** | <kbd>Espacio</kbd> / <kbd>Shift</kbd> | <kbd>L1</kbd> / <kbd>R1</kbd> o Sticks analógicos |
| **Pausa** | <kbd>Escape</kbd> / <kbd>P</kbd> | Options / Start |

---

## 📦 Instrucciones de Instalación y Uso (BYOG)

> [!NOTE]  
> Este repositorio **NO** contiene código con derechos de autor, imágenes de disco, binarios propietarios ni assets comerciales de Atlus o SEGA. Se requiere que el usuario aporte su propia copia legítima del juego de PS4 (*Bring Your Own Game*).

### 1. Clonar el Repositorio
```bash
git clone https://github.com/MaouAnimus/persona-3-dancing-pc-port.git
cd persona-3-dancing-pc-port
```

### 2. Aportar los Archivos del Juego
Coloca tu volcado de PS4 en la carpeta raíz del proyecto bajo el nombre `CUSA12636`:
```
persona-3-dancing-pc-port/
├── CUSA12636/
│   ├── eboot.bin
│   ├── data/
│   │   ├── data.cpk
│   │   └── ps4.cpk
│   └── sce_sys/
├── Jugar.bat
└── ...
```

### 3. Obtener el Motor Compilado
- **Opción A (Binarios precompilados):** Descarga el archivo de lanzamiento desde la sección de [Releases](../../releases) y extrae `p3d.exe` y `boot-linked.bin` en sus respectivas carpetas.
- **Opción B (Compilar desde código fuente):**
  Requiere [MSYS2 Clang64](https://www.msys2.org/) con Ninja y CMake:
  ```powershell
  $env:PATH = "C:\msys64\clang64\bin;$env:PATH"
  ninja -C engine/out/gpu p3d
  ```

### 4. Iniciar el Juego
Ejecuta [`Jugar.bat`](Jugar.bat) o haz doble clic directamente en `p3d.exe`.

---

## 📚 Documentación Técnica y Arquitectura

Para detalles de las sesiones de ingeniería inversa, diagnósticos de estabilidad y soluciones aplicadas:
- [Diario de Trabajo de Ingeniería Inversa (Worklog)](docs/worklog.md)

---

## ⚖️ Aviso Legal

Este proyecto es una reimplementación de interoperabilidad de código limpio (*clean-room interoperability*) con fines educativos y de investigación sobre la arquitectura x86-64 y la API Vulkan. *Persona*, *Persona 3: Dancing in Moonlight*, sus personajes y su banda sonora son marcas comerciales y propiedad intelectual registrada de **ATLUS / SEGA**.
