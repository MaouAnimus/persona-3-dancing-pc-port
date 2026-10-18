# Persona 3: Dancing in Moonlight - Estructura del Proyecto PC

Este documento detalla la estructura reorganizada, limpia y 100% dedicada a **Persona 3: Dancing in Moonlight (CUSA12636)**.

---

## 🎮 Ejecución Rápida

- **Lanzador 1-Click**: [`Jugar_Persona3Dancing.bat`](file:///C:/Users/Alvaro/Downloads/Persona%203%20PC/Jugar_Persona3Dancing.bat)
- **Binario Nativo Windows**: [`p3d.exe`](file:///C:/Users/Alvaro/Downloads/Persona%203%20PC/p3d.exe) (compilado con Clang/Ninja, icono oficial de Persona 3 Dancing y manifiesto DPI-Aware de Windows).
- **Copia en bin/**: [`bin/p3d.exe`](file:///C:/Users/Alvaro/Downloads/Persona%203%20PC/bin/p3d.exe)

---

## 📁 Organización de Carpetas

```
Persona 3 PC/
├── bin/                       # Binarios compilados listos para distribución
│   └── p3d.exe               # Ejecutable nativo Windows para P3D
│
├── CUSA12636/                 # Volcado original de PS4 (USA)
│   ├── eboot.bin              # Binario del juego original (ELF x86-64)
│   ├── data.cpk               # Archivo de recursos de Persona (música, modelos, core)
│   ├── sce_sys/               # Iconos, metadatos y param.sfo
│   └── ...
│
├── docs/                      # Documentación y hojas de ruta técnicas
│   ├── ESTRUCTURA_PROYECTO.md # Este documento de organización
│   ├── Hoja_de_ruta_*.md      # Planes de ingeniería inversa y alcance
│   ├── ps2_xenosaga_*.md      # Blueprint de referencia técnica
│   └── evidence/              # Reportes de análisis, trazas y llamadas
│
├── engine/                    # Motor de ejecución nativo y capa de traducción
│   ├── src/                   # HLE Runtime: Pad, Audio, VideoDec, Hilos, Memoria
│   ├── gpu/                   # Shim Vulkan y retranspilador de shaders Spir-V
│   ├── launcher/              # Iconos (p3d.ico, p3d.png) y recursos visuales
│   ├── packaging/windows/     # Manifiesto y recurso p3d.rc
│   └── third_party/           # Vulkan SDK, LibAtrac9, Sirit, etc.
│
├── extracted_assets/          # Recursos extraídos del juego
│   ├── audio/                 # Canciones y efectos de sonido en formato PC
│   ├── mainmenu_archive/      # Assets de la interfaz del menú principal
│   └── textures/              # Texturas convertidas a RGBA
│
├── native/                    # Módulos limpios C++ nativos en desarrollo
│   ├── app/                   # Loop principal de la aplicación PC
│   ├── audio/                 # Decodificadores y mezcladores de sonido
│   ├── platform/              # Entrada (DirectInput/XInput) y ventana
│   ├── renderer/              # Capa gráfica dedicada
│   └── rhythm/                # Lógica del motor de ritmo y beats
│
├── out/                       # Salidas de análisis y binarios enlazados
│   ├── dump/                  # Capturas de frames renderizados
│   └── p3d_test/              # boot-linked.bin (ELF enlazado listo para ejecución)
│
├── tools/                     # Herramientas de extracción y parsing
│   ├── PKG-Studio.exe         # Extractor de paquetes PS4
│   ├── GFD-Studio/            # Visor y extractor de modelos Atlus GFD
│   ├── GMDTool/               # Parser de modelos y animaciones GMD
│   ├── cpk_tool.py            # Desempaquetador de archivos CPK (CRI Middleware)
│   └── p3d_diagnose.py        # Diagnóstico de imports, stubs y rendimiento
│
├── user/                      # Partidas guardadas y configuraciones de usuario
│
├── Jugar_Persona3Dancing.bat  # Script lanzador principal
└── p3d.exe                    # Ejecutable principal en la raíz
```

---

## 🧹 Limpieza Realizada

1. **Eliminación de archivos residuales y scratch**:
   - Eliminados scripts y binarios obsoletos (`scratch_code.bin`, `scratch_disasm.exe`, `scratch_threads*.txt`, `missing_135.txt`, `lib_files.json`, `launch_probe.ps1`, `Probar_P3D_Original.bat`, etc.).
   - Eliminado el archivo comprimido pesado `ghidra_*.zip` (570 MB) y volcados `.raw` temporales.
   - Eliminada la carpeta antigua y confusa `tools/gow3_ref`, migrando y refactorizando el motor nativo a [`engine/`](file:///C:/Users/Alvaro/Downloads/Persona%203%20PC/engine/).
2. **Re-branding completo a Persona 3: Dancing in Moonlight**:
   - Compilación del nuevo binario nativo [`p3d.exe`](file:///C:/Users/Alvaro/Downloads/Persona%203%20PC/p3d.exe) (121.6 MB).
   - Título de ventana en tiempo de ejecución: `Persona 3: Dancing in Moonlight`.
   - Nombre de hilo de render: `p3d:window`.
   - Icono nativo en el `.exe` y en la ventana de Windows: [`p3d.ico`](file:///C:/Users/Alvaro/Downloads/Persona%203%20PC/engine/launcher/p3d.ico).
   - Banner de consola y logs de arranque ajustados para P3D (CUSA12636).
3. **Mapeo de Controles**:
   - Tecla **Enter** / **Espacio** / **X**: Botón Cruz (Cross / Confirmar).
   - Tecla **Z**: Botón Círculo (Circle / Cancelar).
   - Flechas direccionales: D-Pad.
