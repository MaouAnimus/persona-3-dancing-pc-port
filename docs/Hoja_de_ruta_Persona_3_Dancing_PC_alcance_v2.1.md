# Persona 3: Dancing in Moonlight para PC
## Hoja de ruta maestra — alcance integrado v2.1

**Objetivo final:** jugar *Persona 3: Dancing in Moonlight* como una aplicación nativa de Windows, sin emulador de PS4 ni una consola ejecutándose en segundo plano.

**Punto de partida declarado:** existe un archivo `.BIN` y el usuario ya realizó análisis con Ghidra; indica también haber descompilado y recompilado el programa. Esta afirmación se tratará como una hipótesis que se debe documentar, no como prueba de que el juego completo ya se pueda compilar para Windows.

**Plataforma objetivo:** Windows x64.  
**Versión de referencia:** edición PS4 estadounidense, cuyo identificador publicado es CUSA-12636. Confirmar el identificador real de los archivos de trabajo antes de mezclar ejecutables, actualizaciones, DLC o recursos de otra región.  
**Hardware conocido:** NVIDIA GeForce GTX 3050.  
**Prioridad:** fidelidad funcional y audiovisual primero; mejoras gráficas opcionales después.
**Estado del documento:** plan de trabajo vivo; no implica que las fases o resultados estén completados.
**Actualización v2.1:** se integra el flujo de entrada PKG → contenido extraído → identificación de `eboot.bin` → auditoría Ghidra, la referencia de PKG Studio y el vídeo aportado por el usuario.

---

## 0. Resumen ejecutivo

El proyecto cambia de un plan que empezaba por la extracción de un paquete a un plan que parte de un ejecutable ya disponible y un análisis previo en Ghidra. La primera tarea, por tanto, no es volver a descompilarlo: es realizar una auditoría reproducible de los artefactos existentes y determinar qué puede reutilizarse.

Hay una distinción fundamental: Ghidra analiza binarios, desensambla instrucciones y produce pseudocódigo de alto nivel parecido a C. Ese pseudocódigo no constituye automáticamente el código fuente original ni una base que pueda compilarse sin reconstrucción manual. Si se generó una compilación mediante un complemento, exportador, traductor binario o flujo externo, ese componente y sus resultados deben identificarse y probarse por separado.

Un ejecutable de PS4 también depende del formato del ejecutable y de servicios del sistema Orbis. La arquitectura x86-64, por sí sola, no hace que el programa sea compatible con Windows. Un port nativo real debe sustituir o reimplementar las dependencias de plataforma, el sistema gráfico, audio, entrada, archivos, memoria, sincronización y otras interfaces usadas por el juego.

### Estrategia principal

Adoptar un **port híbrido orientado a recursos y sistemas**:

1. Preservar el BIN, el proyecto Ghidra y todos los artefactos actuales.
2. Auditar el ejecutable y el supuesto proceso de recompilación.
3. Recuperar y documentar los recursos del juego usando herramientas conocidas, sin acoplarlas todavía al runtime.
4. Construir una aplicación nativa de Windows capaz de visualizar un modelo real y reproducir una animación.
5. Implementar un reloj musical estable y el sistema de pulsaciones/puntuación.
6. Crear una sección vertical jugable que incluya selección o carga de pista, personaje animado, audio, notas, evaluación y pantalla de resultados.
7. Ampliar la cobertura hacia el juego completo, mantener fidelidad con pruebas y añadir mejoras gráficas optativas.

No se considerará viable el port completo hasta que un prototipo nativo muestre que los formatos, animaciones, audio y lógica esencial pueden reconstruirse o reutilizarse.

---

## 1. Definición de éxito: ¿qué significa «nativo»?

La versión final debe cumplir todo lo siguiente:

- Se inicia como un ejecutable de Windows x64 (`.exe`).
- No necesita que un emulador de PS4 esté ejecutándose ni delega en un proceso de emulación de la consola.
- Utiliza implementaciones de PC para ventana, renderizado, audio, entrada, archivos, configuración y guardado.
- Carga contenido del juego desde un directorio de datos del usuario, en lugar de distribuir los archivos comerciales dentro del repositorio.
- Permite jugar una pista completa desde el inicio hasta la pantalla de resultados, con sincronización musical, animaciones, puntuación y controles funcionales.
- La meta de producto completo añade las pistas y dificultades incluidas en el alcance, menús, eventos, desbloqueos y demás contenido que se decida admitir.
- Las mejoras de resolución, filtrado, antialiasing y presentación se pueden desactivar o configurar sin cambiar el comportamiento temporal del juego.

**No basta** con abrir el BIN en Ghidra, exportar pseudocódigo, compilar una muestra aislada, iniciar una ventana vacía o mostrar un modelo estático. Esos son hitos intermedios, no un port jugable.

### Fuera del alcance inicial

- Reproducir PS4/Orbis genéricamente o crear un emulador de la consola.
- Desarrollar un motor 3D universal para todos los juegos de Atlus.
- Remasterizar todas las texturas antes de alcanzar una sección jugable.
- Añadir contenido nuevo, canciones creadas por fans o modificaciones online antes de completar la base.
- Distribuir ejecutables, música, modelos, vídeos, claves o datos del juego que no tengamos derecho a redistribuir.

---

## 1.1 Flujo de entrada del proyecto: PKG, contenido extraído y BIN

El flujo de trabajo debe admitir dos estados de entrada: (A) ya existe un `BIN` y un proyecto Ghidra, o (B) todavía se parte del paquete PS4 `.pkg`. El proyecto que has descrito está en el estado A, pero conservaremos el flujo B como procedimiento de adquisición, repetición y auditoría.

**Importante: “convertir PKG a BIN” puede referirse a operaciones técnicamente diferentes.** Un `.pkg` de PS4 es un contenedor/paquete; `eboot.bin` suele ser el nombre del ejecutable de una aplicación dentro de su contenido instalado/extraído, con formato y dependencias específicos de PS4. No existe una conversión universal que convierta cualquier PKG completo en un BIN genérico que funcione en Windows. Cambiar una extensión no transforma el formato.

Flujo de trabajo que adoptaremos:

```text
PKG original (solo lectura; copia legal del usuario)
        │
        ▼
Identificar título, región, versión, tipo de paquete y hash
        │
        ▼
PKG Studio / herramienta de extracción compatible
        │
        ├── si solo personaliza metadatos y reempaqueta: no asumir que extrae el ejecutable
        └── si permite extraer/listar: conservar salida y registrar herramienta/opciones
        │
        ▼
Contenido extraído/instalado del juego
        │
        ├── sce_sys/param.sfo y metadatos disponibles
        ├── eboot.bin (si está presente)
        ├── módulos auxiliares y recursos
        └── actualizaciones/DLC, inventariados aparte
        │
        ▼
Identificar formato REAL de eboot.bin y calcular hash
        │
        ▼
Abrir copia de análisis en Ghidra con loader/arquitectura documentados
        │
        ▼
Auditar pseudocódigo, referencias, imports y cualquier supuesto resultado recompilado
```

Si el usuario ya dispone del `BIN`, no volver a procesar el paquete solo por seguir los pasos: primero identificar qué clase de BIN es y su procedencia. Si aún se conserva el PKG original, se registra como fuente inmutable y se usa solo cuando sea necesario comprobar versión, extraer otros recursos o reproducir el proceso.

### Reglas para versiones y entradas

- No mezclar base game, update y DLC hasta que el juego base esté identificado y el inventario tenga pruebas de compatibilidad.
- No combinar regiones ni versiones por nombre de archivo solamente: registrar title ID, versión y metadatos del paquete cuando estén disponibles.
- Mantener un directorio separado por origen y versión.
- La extracción no equivale a descompilación; la descompilación no equivale a recompilación; una recompilación del binario PS4 no equivale a un port Windows.
- No distribuir PKG, `eboot.bin`, claves, música, modelos, vídeos ni otros archivos comerciales del juego dentro del repositorio. El repositorio debe contener código propio, documentación y pruebas sintéticas.

### Referencia aportada por el usuario

Vídeo: [“Convierte esos juegos de formato PKG a formato BIN con PKG Studio”](https://www.youtube.com/watch?v=F1kdxkRXTdg&t=1s).

**Estado de verificación:** el enlace se registra como referencia aportada, pero el contenido del vídeo no pudo comprobarse automáticamente durante la preparación de esta versión. Antes de declarar que la herramienta realiza una conversión PKG→BIN válida para este proyecto, se debe registrar qué archivos entran, qué archivos salen, el nombre exacto de la herramienta/versión y qué programa consume la salida. Una salida llamada `BIN` no se considera correcta hasta que su formato y uso queden identificados.

---

## 2. Auditoría del punto de partida: BIN + Ghidra

**Esta es la primera fase obligatoria de auditoría una vez identificada la entrada.** Si se parte de un PKG, ejecutar antes el flujo de entrada de la sección 1.1; si ya existe el BIN, no repetir extracción sin motivo. No borrar, sustituir ni modificar los artefactos originales.

### 2.1 Crear una copia de trabajo

Estructura inicial sugerida:

```text
p3d-native-pc/
├── evidence/                 # Manifest, hashes y metadatos no modificables
│   ├── input_manifest.csv
│   └── notes.md
├── original/                 # Copias de trabajo de archivos propios
├── ghidra/
│   ├── project_backup/       # Copia del proyecto Ghidra
│   ├── exports/               # Etiquetas, cadenas, referencias y funciones
│   └── reports/
├── tools/                    # Scripts propios de análisis
├── asset-workbench/           # Pruebas de conversión y visualización
├── native/                    # Aplicación Windows
├── tests/
└── docs/
```

No subir archivos comerciales, ejecutables del juego ni recursos protegidos a repositorios públicos. Git puede contener código original escrito por el equipo, pruebas sintéticas y documentación sin material comercial.

### 2.2 Inventario mínimo

Por cada archivo anotar:

- Nombre, extensión, tamaño y ruta de origen.
- SHA-256.
- Si es el ejecutable principal, un módulo auxiliar, un archivo de recursos o una salida de una herramienta.
- Región, identificador de título, versión y actualización, si se pueden verificar desde metadatos legítimamente disponibles.
- Herramienta que lo generó y parámetros conocidos.
- Si se puede abrir, analizar y validar de forma repetible.

Para registrar hashes en PowerShell:

```powershell
Get-FileHash .\ruta\al\archivo.bin -Algorithm SHA256
```

Para crear un hash desde Linux/WSL:

```bash
sha256sum ./ruta/al/archivo.bin
file ./ruta/al/archivo.bin
```

La extensión no es evidencia suficiente del formato. En particular, `.BIN` puede nombrar cosas diferentes. Registrar cabecera, formato real y arquitectura mediante herramientas de identificación adecuadas.

### 2.3 Auditar el proyecto Ghidra

Exportar o documentar, cuando sea posible:

- Versión de Ghidra y versión/arquitectura detectada para el programa.
- Formato de entrada y cargador usado.
- Rangos de memoria y secciones.
- Tabla de imports/exports o dependencias detectadas.
- Funciones identificadas y funciones sin resolver.
- Cadenas de texto, rutas, nombres de recursos y mensajes de error.
- Referencias a funciones de lectura de archivos, carga de recursos, audio, animación, temporización, entrada y gráficos.
- Scripts, extensiones o loaders usados.
- Modificaciones manuales de etiquetas, prototipos y tipos.

### 2.4 Verificar qué significa «recompilado»

Recoger evidencia del paso descrito como recompilación:

- ¿Existe un proyecto C/C++ generado o solo pseudocódigo visible en la ventana Decompiler?
- ¿Se generó un conjunto de archivos fuente que compila fuera de Ghidra?
- ¿Se utilizó un traductor de binario a binario, lifting, un recompilador estático o un plugin externo?
- ¿Qué compilador y linker se usaron?
- ¿Qué archivo fue el resultado y para qué plataforma/ABI se produjo?
- ¿Qué pruebas demuestran que el resultado ejecuta código equivalente?

No se presupone que Ghidra pueda reconstruir automáticamente una compilación completa del juego a partir de su pseudocódigo.

### Entregables

1. `input_manifest.csv` con hashes y metadatos.
2. Copia de seguridad del proyecto Ghidra.
3. Informe breve de arquitectura del ejecutable.
4. Inventario de dependencias de PS4 identificadas.
5. Nota inequívoca describiendo qué artefacto fue supuestamente recompilado y cómo.
6. Decisión registrada: reutilizar el flujo previo, usar el análisis como documentación o tratar de reconstruir funciones selectas de forma manual.

### Criterio de aceptación

Otra persona debe poder seguir el inventario y abrir los mismos archivos de trabajo sin perder la relación entre BIN, proyecto Ghidra y resultados exportados. No se avanza hasta que el artefacto de entrada y el supuesto resultado de compilación estén identificados.

---

## 3. Arquitectura objetivo para Windows

La arquitectura debe permitir probar cada subsistema sin tener que ejecutar todo el juego.

```text
                    Datos propios del juego del usuario
           ┌───────────────────────────────────────────┐
           │ Modelos / materiales / texturas / audio    │
           │ Animaciones / charts / UI / configuración  │
           └──────────────────────┬────────────────────┘
                                  │
                         Asset Pipeline
              validadores / conversores / catálogo
                                  │
                                  ▼
┌───────────────────────────────────────────────────────────────┐
│                   P3D Native Runtime                          │
│                                                               │
│  Game State ── Rhythm/Scoring ── Timeline/Animation            │
│       │              │                    │                    │
│  Input/Mapping   Audio Clock          Scene/Character          │
│       └──────────────┬────────────────────┘                    │
│                      ▼                                         │
│             Resource & Asset Manager                           │
│                      ▼                                         │
│              Renderer backend                                  │
└──────────────────────┬────────────────────────────────────────┘
                       ▼
            Windows / Direct3D / audio / input
```

### Tecnología inicial recomendada

- **Lenguaje:** C++20.
- **Build:** CMake + Ninja.
- **Ventana y entrada:** SDL3 (o SDL2 si una dependencia concreta lo requiere; fijar versión en el proyecto).
- **Renderizador inicial:** Direct3D 11 para reducir la complejidad del primer prototipo de Windows. Mantener una interfaz propia para poder evaluar Direct3D 12 después.
- **Importación provisional:** glTF/GLB como formato puente para modelos y animaciones convertidos desde herramientas comunitarias; no como formato original del juego.
- **Audio:** backend separado con salida de baja latencia y reloj de reproducción medible; escoger la librería tras verificar los códecs reales de los archivos.
- **Herramientas de desarrollo:** Visual Studio o Build Tools, Git, CMake, Ninja, Python 3, Blender y Ghidra.
- **Depuración:** logs estructurados, capturas de pantalla de referencia, comprobaciones de recursos y pruebas automatizadas de lógica.

El primer prototipo no requiere Direct3D 12 ni shaders complejos. La arquitectura debe separar reloj musical, animación y renderizado para impedir que una variación de FPS altere el tiempo de las notas.

### Módulos de código

```text
native/
├── app/                 # Entrada del programa, configuración y ciclo de vida
├── platform/            # Ventana, reloj, rutas, controles y filesystem de Windows
├── renderer/            # Device, swap chain, materiales, shaders y cámara
├── assets/              # Catálogo, validadores, caché y cargadores
├── animation/           # Esqueleto, clips, blend y sincronización
├── audio/               # Decode, reproducción, reloj de canción y latencia
├── rhythm/              # Notas, ventanas de timing, juicio, combo y puntuación
├── scenes/              # Escenas, cámara, luces y personaje
├── ui/                  # Menús, HUD y resultados
├── game/                # Máquina de estados y flujo de juego
├── save/                # Guardado y configuración
└── diagnostics/         # Logs, métricas y overlays de depuración
```

La compatibilidad con el comportamiento original se documentará aparte en `docs/reverse-engineering/`. No se mezclará pseudocódigo de Ghidra con el runtime nuevo sin una conversión manual y una prueba de equivalencia.

---

## 4. Herramientas de recursos existentes

El proyecto aprovechará herramientas ya existentes para evitar escribir desde cero parsers de todos los formatos.

### GFD Studio

Repositorio: https://github.com/tge-was-taken/GFD-Studio

Su importador documenta soporte para recursos de Persona 3: Dancing in Moonlight y formatos como GMD, GFS y GAP, incluyendo modelos y datos de animación. Esto lo convierte en una herramienta de inspección prioritaria, pero no implica que todas las variantes del juego ni todos los recursos funcionen sin fallos.

### GMDTool

Repositorio: https://github.com/lemoncove/GMDTool

Permite convertir modelos GMD a COLLADA/DAE y declara compatibilidad con modelos de Persona 3: Dancing in Moonlight. Su documentación especifica que no convierte de vuelta a GMD. Por tanto, tratarlo inicialmente como una herramienta de exportación e investigación, no como una herramienta de reempaquetado del juego.

### Procedimiento de validación de recursos

1. Encontrar un archivo de modelo, textura o animación mediante las herramientas apropiadas y una copia legítima de los datos.
2. Registrar el archivo de entrada y su hash.
3. Ejecutar la herramienta en modo de solo lectura/exportación cuando sea posible.
4. Inspeccionar el resultado en Blender u otro visor.
5. Comprobar escala, orientación, materiales, jerarquía del esqueleto y cantidad de animaciones.
6. Guardar un informe por recurso, incluyendo los fallos.
7. Comparar el recurso en el visor con una captura de referencia del juego, si está disponible.

No modificar los recursos originales en esta fase. Un modelo exportado correctamente no demuestra que se pueda recuperar el juego entero ni que el formato acepte una recompilación inversa.

---

## 5. Plan por fases y puertas de decisión

Las duraciones son rangos de planificación aproximados para una persona con conocimientos básicos y no son garantías. La ingeniería inversa puede obligar a ampliar etapas. Cada fase debe entregar algo observable.

### Fase previa (-1) — Ingesta y validación del PKG (solo si el punto de partida sigue siendo un PKG)
**Esfuerzo orientativo:** 1–3 días si las herramientas leen el paquete y no hay errores de versión o formato.

Tareas:
- Registrar hash del PKG original, tamaño, nombre de origen y metadatos disponibles.
- Verificar si el PKG Studio instalado es la misma herramienta que el repositorio público [ababilkhoerulimam/pkg-studio](https://github.com/ababilkhoerulimam/pkg-studio), o una aplicación distinta que comparte nombre.
- Consultar las funciones reales de la versión instalada. La versión pública consultada documenta personalización de iconos/imagen de inicio, modificación de metadatos y reempaquetado mediante herramientas Orbis/GP4; esa descripción no demuestra por sí sola una conversión general a un BIN genérico.
- Si la herramienta permite listar o extraer, usar una copia de trabajo, guardar logs y no sobrescribir la fuente. Si solo permite modificar/reempaquetar, no usarla como extractor: identificar la herramienta apropiada para leer el contenido de una copia que poseas legítimamente.
- Localizar `eboot.bin` si está presente y distinguirlo de los demás archivos `.bin`. Registrar su hash, tamaño, cabecera/formato y relación con la versión fuente.
- Comprobar que el contenido extraído se puede inventariar y que `param.sfo` u otros metadatos concuerdan con la edición esperada.

**Entrega:** paquete de entrada documentado, contenido extraído/inventariado o informe de bloqueo técnico y `eboot.bin` identificado, si existe.

**Aceptación:** sabemos qué representa cada archivo de salida; no basta con que el archivo termine en `.bin`.

**Si el proyecto ya tiene un BIN válido y un proyecto Ghidra:** marcar esta fase como “no aplica / ya completada con evidencia” y no volver a realizarla solo para mantener la numeración.

### Fase 0 — Congelar el estado actual
**Esfuerzo orientativo:** 1–3 días.

Tareas:
- Respaldar BIN, proyecto de Ghidra, exports, scripts y archivos compilados.
- Calcular hashes y reunir metadatos.
- Escribir un registro de cómo se produjo cada archivo.
- Separar original, análisis y salida recompilada.

**Entrega:** manifest, copia de seguridad y diagrama de artefactos.

**Aceptación:** se puede identificar qué archivo se analizó y qué resultado se obtuvo sin sobrescribir originales.

### Fase 1 — Auditoría técnica del ejecutable y de Ghidra
**Esfuerzo:** 3–10 días, según el estado del proyecto.

Tareas:
- Identificar el tipo real del BIN y su arquitectura.
- Documentar módulos, imports y dependencias de sistema.
- Exportar listado de funciones y cadenas, conservando direcciones y nombres.
- Buscar posibles puntos de entrada, inicialización, carga de recursos, reloj/audio, entrada, renderizado, estados de escena y evaluación de ritmo.
- Verificar qué puede recuperarse de la supuesta recompilación.

**Entrega:** mapa de dependencias y lista de funciones priorizadas.

**Aceptación:** podemos responder qué debe sustituirse para que el código pueda funcionar en Windows y qué partes aún son desconocidas.

**Puerta de decisión 1:** si no existe una fuente compilable ni un traductor reproducible, no se presentará el juego como «recompilado». Continuar con la descompilación como referencia y reconstruir solo funciones que se entiendan y puedan probarse.

### Fase 2 — Pipeline de extracción e inventario de recursos
**Esfuerzo:** 1–3 semanas iniciales.

Tareas:
- Identificar formatos presentes y asociarlos con cargadores o herramientas.
- Probar GFD Studio y GMDTool con un modelo de referencia.
- Determinar cómo se almacenan texturas, materiales, rig, clips, música, charts, interfaz y vídeos.
- Crear un inventario: ruta/ID de recurso, tipo, herramienta, resultado y estado.
- Establecer que los conversores nunca sobrescriban las fuentes.

**Entrega:** inventario inicial y carpeta de recursos convertidos de prueba.

**Aceptación:** un modelo y sus texturas pueden abrirse en una herramienta visual; se conoce qué recursos no se han podido interpretar.

### Fase 3 — Visor nativo de Windows
**Esfuerzo:** 1–2 semanas.

Tareas:
- Crear un ejecutable C++20 con CMake.
- Crear ventana, dispositivo gráfico, cámara, controles de depuración y logs.
- Cargar un modelo puente glTF/GLB convertido previamente.
- Mostrar una textura y permitir mover la cámara, pausar y reiniciar.

**Entrega:** `P3DAssetViewer.exe` de desarrollo.

**Aceptación:** funciona en Windows sin iniciar un emulador ni invocar un proceso de PS4; carga recursos desde una ruta local del usuario.

### Fase 4 — Modelos y animaciones
**Esfuerzo:** 2–5 semanas iniciales, potencialmente más.

Tareas:
- Importar personaje con materiales y esqueleto.
- Validar nombres de huesos, pose inicial, transformaciones y skinning.
- Cargar al menos un clip de baile y reproducirlo en tiempo real.
- Verificar que el bucle, la duración y la interpolación sean correctos.
- Establecer un sistema de animación que permita escoger y sincronizar clips.

**Entrega:** personaje del juego animado dentro del visor nativo.

**Aceptación:** el personaje se mueve con un clip convertido y el error visual es documentado; no basta con que el modelo esté estático.

### Fase 5 — Audio y reloj musical determinista
**Esfuerzo:** 1–3 semanas iniciales.

Tareas:
- Identificar códec y contenedor del audio de prueba antes de escoger decoder.
- Implementar reproducción, pausa, reanudación y posición de reproducción consultable.
- Mantener un reloj monotónico y mapear el tiempo musical al reloj del backend de audio.
- Medir latencia de entrada/salida y permitir calibración de audio.
- Evitar usar FPS o `deltaTime` de renderizado como reloj maestro de la canción.

**Entrega:** reproductor de una pista de prueba con reloj y métricas.

**Aceptación:** tras pausa/reanudación, la posición del audio y del reloj lógico vuelve a alinearse; las mediciones tienen registros reproducibles.

### Fase 6 — Motor de ritmo aislado
**Esfuerzo:** 2–4 semanas iniciales.

Tareas:
- Crear estructuras para eventos de nota, tiempo objetivo, carril/botón, dificultad y resultado.
- Implementar ventanas de juicio configurables, combo, puntuación y precisión.
- Añadir entrada configurable de teclado y mando XInput/SDL.
- Escribir tests para pulsaciones adelantadas, perfectas, tardías, repetidas, omitidas y eventos al borde de canción.
- Separar cálculo de juicio del HUD y de la animación.

**Entrega:** prueba de ritmo con audio de prueba y chart sintético.

**Aceptación:** las mismas entradas y tiempos de referencia producen los mismos resultados en múltiples ejecuciones; el sistema no depende del framerate.

### Fase 7 — Primera sección vertical jugable
**Esfuerzo:** 3–8 semanas después de validar los recursos.

Una sección vertical pequeña debe incluir:
- Un personaje y una animación real.
- Una pista y sus datos de ritmo correctamente identificados.
- Música sincronizada con eventos, notas e input.
- Puntuación, combo, juicio y pantalla de resultados.
- Reinicio de la pista sin reiniciar toda la aplicación.
- Capturas y logs de referencia para comparar comportamiento y sincronización.

**Entrega:** primera canción completa en el programa nativo.

**Aceptación:** se puede iniciar, jugar de principio a fin y alcanzar resultados, sin emulador ni dependencias de PS4 en ejecución.

**Puerta de decisión 2:** este es el punto mínimo que demuestra que el enfoque puede producir un producto jugable. Si no se logra la sincronización o no se pueden interpretar los charts/animaciones, detener la expansión de contenido y resolver el bloqueo primero.

### Fase 8 — Reconstrucción selectiva del comportamiento original
**Se ejecuta en paralelo a partir de la Fase 1.**

No intentar recompilar todo el pseudocódigo de una vez. Priorizar las funciones que ayuden a explicar:
- Carga de recursos y nombres/IDs.
- Mapeo de archivos a escenas y personajes.
- Datos de canción, tiempos de nota y dificultades.
- Cambios de estado antes, durante y después de una pista.
- Animaciones y transiciones.
- Puntuación, ventanas de timing y evaluación.
- Menús y guardado.

Por cada función recuperada, crear una ficha: dirección original, prototipo inferido, entradas/salidas, dependencias, supuestos, test y nivel de confianza. Si se reimplementa en C++, comparar su comportamiento con datos observables y casos de prueba, no solo con la apariencia del pseudocódigo.

**Entrega:** mapa de subsistemas y una colección pequeña de funciones reconstruidas y probadas.

### Fase 9 — Ampliación al contenido base

Tareas:
- Automatizar el inventario de pistas, dificultades, personajes, vestuario, escenarios y clips.
- Registrar recursos faltantes o incompatibles.
- Añadir los estados de menú requeridos para seleccionar y jugar contenido.
- Priorizar una pista completa por cada clase de caso de uso antes de incorporar todo masivamente.
- Validar cada elemento mediante metadatos y pruebas automáticas de carga.

**Aceptación:** la lista de contenido admitido coincide con el inventario documentado; ningún recurso ausente se presenta como compatible.

### Fase 10 — Menús, progresión y persistencia

Tareas:
- Implementar flujos de navegación, selección y regreso.
- Añadir ajustes de audio, controles, pantalla y accesibilidad.
- Persistir preferencias y progresión en formato propio de PC.
- Probar casos de guardado nuevo, carga, actualización de configuración y recuperación de datos corruptos.
- Incorporar escenas/eventos y contenido adicional solo después del núcleo de ritmo.

**Aceptación:** se puede iniciar la aplicación, seleccionar contenido disponible, jugar, guardar los resultados admitidos y volver al menú de forma estable.

### Fase 11 — Mejoras gráficas opcionales

Solo después de que la sección vertical funcione.

Prioridad:
1. Renderizado correcto y relación de aspecto.
2. Resoluciones configurables, empezando por 1080p y probando 1440p.
3. Filtrado de texturas y anisotropía.
4. Antialiasing adecuado al backend.
5. Escalado de UI sin alterar posiciones/tiempos del juego.
6. Mejoras opcionales de materiales o texturas si la información original lo permite.

La GTX 3050 es suficiente para los prototipos y objetivos gráficos iniciales, pero el rendimiento final depende del renderizador, los formatos, el coste de CPU y la resolución. No fijar presupuesto de FPS sin medirlo.

**Aceptación:** mejoras activables/desactivables con comparación visual; ninguna opción gráfica afecta al reloj musical, a los juicios ni a la precisión de la entrada.

### Fase 12 — Pruebas de regresión y estabilidad

Por cada cambio, probar:
- Arranque, carga de recursos y cierre limpio.
- Una pista completa con dificultad seleccionada.
- Pausa, reanudación y reinicio.
- Entrada rápida, simultánea, adelantada y tardía.
- Cambios de resolución, alt-tab, pérdida/recuperación de foco y cambios de dispositivo de audio.
- Errores de recursos y rutas incorrectas.
- Rendimiento y carga de memoria en sesiones largas.
- Consistencia de puntuación y resultados entre ejecuciones de referencia.

Usar pruebas unitarias para lógica de ritmo, pruebas de recursos para cargadores y pruebas de integración para la pista completa.

### Fase 13 — Empaquetado y distribución

- Crear un instalador o carpeta portable con el ejecutable, bibliotecas permitidas, configuración de ejemplo y documentación.
- No incluir BIN, eboot, música, personajes, vídeos ni otros recursos comerciales que no tengamos derecho a redistribuir.
- El primer arranque debe explicar dónde colocar los recursos que el usuario obtenga legalmente por su cuenta.
- Añadir registro de versión, hashes esperados cuando sea apropiado, reporte de errores y notas de compatibilidad.
- Separar claramente los módulos experimentales de los componentes que pasan pruebas.

**Criterio final de producto:** una instalación limpia en Windows permite abrir la aplicación, localizar datos admitidos y completar la experiencia jugable declarada sin requerir una instalación o emulación de PS4.

---

## 6. Prioridad para el uso de Ghidra

La base de Ghidra debe servir para responder preguntas concretas del motor original. Evitar la meta irreal de convertir de golpe todas las funciones decompiladas a C++ compilable.

### Orden de investigación recomendado

1. Entrypoint, inicialización, imports y dependencias de sistema.
2. Archivos y filesystem: qué formatos abre el juego y cómo resuelve rutas/IDs.
3. Strings y referencias que nombren escenas, personajes, canciones, estados o formatos.
4. Reloj de canción, eventos de notas, evaluación de pulsaciones y puntuación.
5. Carga y evaluación de animaciones.
6. Audio y coordinación de escenas.
7. Dibujo, materiales, cámaras, sincronización vertical y presentación.
8. Menús, desbloqueos, guardado y opciones.

Una tabla de prioridad sugerida:

| Prioridad | Subsistema | Resultado que buscamos |
|---|---|---|
| P0 | Formato y dependencias | Saber qué ejecutable se tiene y qué necesita |
| P1 | Recursos y archivos | Relacionar ficheros con su consumidor |
| P1 | Audio/timeline/ritmo | Conseguir una pista sincronizada |
| P1 | Animación | Reproducir los movimientos adecuados a tiempo |
| P2 | Renderer y materiales | Reproducir escena y personajes |
| P2 | Estados/UI | Poder seleccionar y completar una pista |
| P3 | Progresión y extras | Ampliar hacia cobertura completa |

Cada subsistema debe tener una definición de terminado y una prueba. Un nombre de función asignado por intuición no se considerará evidencia suficiente.

---

## 7. Plan inicial de 30 días

El primer mes tiene que reducir incertidumbre, no prometer el port completo.

### Semana 1: preservar y verificar

- Hacer copias y calcular hashes.
- Identificar el BIN, la región y la versión.
- Respaldar el proyecto Ghidra y registrar la versión de herramienta.
- Determinar qué artefacto fue supuestamente recompilado y con qué flujo.
- Crear manifest y mapa inicial de dependencias.

**Resultado:** sabemos con precisión qué tenemos.

### Semana 2: investigar recursos

- Probar las herramientas de importación con un modelo conocido.
- Encontrar una textura relacionada y comprobar el material.
- Abrir el modelo en Blender/visor y guardar capturas.
- Documentar formatos todavía no reconocidos.

**Resultado:** primer paquete pequeño de recursos convertido con trazabilidad.

### Semana 3: visor de Windows

- Crear solución CMake/C++20.
- Inicializar SDL y Direct3D 11.
- Mostrar un modelo puente con textura.
- Añadir cámara, logs, tratamiento de errores y selector de ruta.

**Resultado:** aplicación nativa que no depende del emulador.

### Semana 4: animación y decisión

- Importar o convertir un clip de animación.
- Reproducirlo en el visor y revisar huesos, velocidad y bucle.
- Documentar problemas con deformaciones, morph targets y materiales.
- Escribir una evaluación: ¿podemos preservar el modelo/rig/animación y en qué condiciones?
- Crear especificación del siguiente prototipo: reloj de audio y rhythm engine.

**Resultado:** visor nativo con personaje animado o informe técnico concreto de qué bloquea ese resultado.

No se fija como compromiso que una canción completa estará lista en 30 días; ese hito depende de la calidad y cobertura de los recursos y del alcance real del binario analizado.

---

## 8. Gestión de riesgos y alternativas

### Riesgo A: «Ghidra recompiló el juego» no está respaldado por artefactos

**Mitigación:** auditar fuentes, scripts y logs. Si solo hay pseudocódigo, conservarlo como referencia y reconstruir funciones concretas.

### Riesgo B: el BIN no es un ELF/SELF útil o está empaquetado de otra manera

**Mitigación:** identificar formato real antes de escoger procedimientos. No deducirlo solo a partir del nombre ni renombrar archivos esperando que cambien de formato.

### Riesgo C: faltan servicios de PS4

**Mitigación:** catalogar imports y dependencias; decidir caso por caso si se implementa una capa de compatibilidad mínima o se reconstruye el subsistema en Windows.

### Riesgo D: recursos extraíbles, pero difíciles de volver a escribir

**Mitigación:** usar formatos de intercambio para el runtime nativo. No depender de volver a empaquetar recursos al formato original, salvo que sea necesario y esté técnicamente demostrado.

### Riesgo E: precisión musical inconsistente

**Mitigación:** reloj basado en el backend de audio, entrada con marcas temporales, calibración de latencia y tests deterministas. No basar el juicio en los FPS.

### Riesgo F: intentar cubrir todo el juego demasiado pronto

**Mitigación:** exigir una pista vertical completa antes de expandir el catálogo.

### Riesgo G: confundir mejora gráfica con port funcional

**Mitigación:** congelar trabajo de mejoras gráficas hasta que audio, animación, notas, puntuación y resultados funcionen de principio a fin.

### Riesgo H: mezclar versiones, regiones y DLC

**Mitigación:** registrar versión y título de cada fuente; comenzar con una única edición base. Añadir parches o DLC solo tras validar compatibilidad.

---

## 9. Reglas de trabajo del repositorio

- Mantener un registro de decisiones arquitectónicas (`docs/decisions/`).
- Cada función reconstruida debe incluir propósito, evidencias y test.
- Cada recurso convertido debe tener hash de entrada, herramienta y versión, opciones, ruta de salida y estado de validación.
- No modificar archivos originales en el sitio.
- No usar nombres hipotéticos de funciones como si fueran comprobados.
- No llamar «nativo» a un prototipo que delega la ejecución en un emulador.
- Mantener el reloj musical separado del renderizador.
- Separar contenido, lógica, presentación y capas de plataforma.
- Compilar y probar con frecuencia; trabajar en cambios pequeños y reversibles.

---

## 10. Fuentes y recursos técnicos

- Ghidra — repositorio oficial: https://github.com/NationalSecurityAgency/ghidra
- Introducción al decompilador de Ghidra: https://www.ghidradocs.com/10.2.3_PUBLIC/help/Decompiler/help/topics/DecompilePlugin/DecompilerIntro.html
- Especificación ELF de PS4 de OpenOrbis: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/wiki/PS4-ELF-Specification
- Documentación de `create-eboot`/SELF y herramientas de OpenOrbis: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain
- GFD Studio: https://github.com/tge-was-taken/GFD-Studio
- GMDTool: https://github.com/lemoncove/GMDTool
- Identificación de edición estadounidense PS4 CUSA-12636: https://gamefaqs.gamespot.com/ps4/214740-persona-3-dancing-in-moonlight/data

Las herramientas de modelos prueban que existen vías útiles para inspeccionar determinados recursos de Persona 3: Dancing in Moonlight; no prueban que todo el motor o todos los formatos del juego puedan convertirse automáticamente.

---

## 11. La próxima tarea concreta

No comenzar reescribiendo miles de funciones ni construyendo todo el renderizador.

**Siguiente hito: completar la Fase 0 y la Fase 1.** Preparar un pequeño dossier con:

1. Nombre, tamaño, SHA-256 y resultado de identificación del BIN.
2. Identificador de título/versión, si está disponible en la copia de trabajo.
3. Captura o informe de Ghidra con el formato, arquitectura y cargador.
4. Qué significa exactamente la «recompilación»: carpeta de fuentes, logs de compilación, ejecutable resultante o únicamente pseudocódigo exportado.
5. Listado de módulos/archivos adicionales que el ejecutable espera.

Con esa evidencia se puede decidir de forma fundamentada si conviene reutilizar un flujo de recompilación existente, reconstruir funciones específicas o usar Ghidra como documentación mientras los subsistemas se implementan nativamente en Windows.

**Principio rector del proyecto:** el éxito no es tener un BIN abierto en Ghidra; es completar el ciclo de juego —inicio, pista, sincronización, entrada, puntuación y resultados— en un programa de Windows que no dependa de la ejecución de PS4.


---

## 12. Registro de cambios y estado de trabajo

### Historial del documento

- **v1.0:** hoja de ruta inicial para un port nativo de Persona 3: Dancing in Moonlight.
- **v1.1:** integración del paso relacionado con PKG Studio y la referencia de conversión a BIN, pendiente de verificar técnicamente.
- **v2.0:** actualización del alcance para partir del BIN existente y el análisis previo en Ghidra.
- **v2.1 (esta versión):** integración en un único documento operativo del flujo de ingesta PKG → extracción/inventario → identificación del ejecutable → Ghidra → prototipos nativos; aclaración de la diferencia entre paquete PKG, `eboot.bin`, pseudocódigo, recompilación y port Windows; criterios de aceptación y seguimiento.

### Estado inicial según lo declarado por el usuario

| Elemento | Estado declarado | Verificación requerida |
|---|---|---|
| Archivo BIN | Disponible | Identificar formato real, tamaño, hash y procedencia |
| Proyecto Ghidra | Existe análisis | Respaldar proyecto, versión, loader y arquitectura |
| Descompilación | Realizada | Distinguir pseudocódigo de código fuente recuperado |
| Recompilación | Declarada | Registrar herramienta, fuentes, comando, artefacto y prueba de ejecución |
| PKG original | Puede ser relevante para reproducir el proceso | Confirmar si se conserva y registrar solo si se usa |
| PKG Studio | Herramienta que el usuario denomina propia | Confirmar nombre exacto, versión, repositorio y funciones reales |
| Vídeo tutorial | Enlace aportado | Verificar manualmente entrada, salida y pasos que muestra |
| PC objetivo | Windows x64, NVIDIA GTX 3050 | Registrar versión de Windows, CPU, RAM y drivers para pruebas de rendimiento |

No marcar como “hecho” un estado basándose solo en la conversación. Las columnas de evidencia y artefactos concretos deben completarse en el repositorio de trabajo.

### Log de ejecución recomendado

Mantener `docs/worklog.md` y registrar cada sesión con este formato:

```text
Fecha:
Objetivo de la sesión:
Estado previo / commit:
Archivos de entrada (rutas relativas + SHA-256):
Herramientas y versiones:
Comandos ejecutados (sin secretos ni claves):
Resultado observado:
Pruebas que pasaron/fallaron:
Capturas o logs generados:
Decisiones y dudas pendientes:
Próximo paso verificable:
```

### Tablero maestro de hitos

| ID | Hito | Criterio de salida | Estado inicial |
|---|---|---|---|
| M0 | Entradas respaldadas e identificadas | Manifest, hashes y copias de trabajo | Pendiente de evidencia |
| M1 | BIN y análisis Ghidra auditados | Formato/arquitectura y resultado de compilación descritos | Pendiente de evidencia |
| M2 | Recursos documentados | Al menos un recurso real identificado y convertido sin modificar el original | Pendiente |
| M3 | Visor Windows | Ejecutable propio abre una ventana y muestra un recurso puente | Pendiente |
| M4 | Personaje animado | Modelo, materiales, rig y clip reproducidos en el visor | Pendiente |
| M5 | Audio sincronizado | Posición musical estable, pausa/reanudación y métricas | Pendiente |
| M6 | Motor de ritmo probado | Tests deterministas para juicio, combo y puntuación | Pendiente |
| M7 | Primera canción nativa completa | Inicio → juego → resultados, sin emulador PS4 en ejecución | Pendiente |
| M8 | Cobertura de contenido acordada | Inventario y porcentaje de contenido admitido documentados | Pendiente |
| M9 | Versión de pruebas estable | Pruebas de regresión, errores conocidos y paquete reproducible | Pendiente |

---

## 13. Plan operativo: qué hacer ahora, en orden

Este es el camino corto para retomar el trabajo sin dispersarse. Cada punto debe cerrarse con un archivo o una prueba visible.

### Paso 1 — Congelar y documentar lo que ya existe

- Crear una copia de seguridad del BIN y del proyecto Ghidra.
- Calcular hashes; no renombrar para sugerir un formato distinto.
- Registrar si el archivo procede de un PKG, de una herramienta de extracción o de otra conversión.
- Guardar una captura/informe de Ghidra en el que se lean formato, arquitectura y loader.

**Salida:** `evidence/input_manifest.csv` y `ghidra/reports/initial-audit.md`.

### Paso 2 — Aclarar PKG Studio

- Registrar el nombre exacto, versión instalada y URL/repositorio de la herramienta.
- Revisar qué hace el vídeo del usuario: ¿lista/extracción de contenido, conversión de `eboot.bin`, conversión a otro contenedor o solo reempaquetado?
- Si se sigue el tutorial, documentar por separado archivo de entrada, salida exacta y herramienta que puede abrir la salida.
- No transformar la fuente original y no asumir que el `.bin` resultante sea un ejecutable Windows.

**Salida:** `docs/pkg-studio-validation.md`.

### Paso 3 — Resolver la afirmación de recompilación

Crear un inventario con uno de estos estados (elegir el que corresponda con pruebas):

- `A`: solo hay pseudocódigo/ensamblador dentro de Ghidra.
- `B`: se han exportado fuentes aproximadas que requieren corrección y reconstrucción manual.
- `C`: una herramienta adicional generó fuentes compilables.
- `D`: existe un traductor/recompilador y artefacto de salida reproducible.
- `E`: existe un ejecutable de salida probado, pero todavía no se conoce su cobertura funcional.

Para estados C–E, registrar repositorio de la herramienta, versión, comandos, dependencias, logs y prueba mínima. No declarar equivalencia funcional sin pruebas.

**Salida:** `docs/reverse-engineering/recompilation-status.md`.

### Paso 4 — Probar un solo recurso real

Usar GFD Studio/GMDTool para probar un modelo o animación compatible, si está disponible en la extracción. Si no se puede obtener un recurso, el primer entregable será un informe que documente el formato faltante y la barrera.

**Salida:** recurso de prueba bajo una ruta local ignorada por Git, captura y ficha técnica.

### Paso 5 — Crear la aplicación nativa mínima

- Visual Studio o Build Tools, CMake y Git.
- C++20 con SDL y Direct3D 11 para una primera ventana.
- Configuración de rutas y logs.
- Cargador de un modelo puente GLB/glTF; no programar todavía un cargador de cada formato original.

**Salida:** `P3DAssetViewer.exe` de desarrollo.

### Paso 6 — Construir la prueba musical aislada

Antes de integrar datos originales complejos, crear un chart sintético y audio de prueba. Verificar que el reloj proviene de la reproducción de audio, no de los FPS, y que los resultados se repiten con el mismo conjunto de entradas y marcas temporales.

**Salida:** tests automáticos de timing y una demo que se pueda reiniciar.

### Paso 7 — Integrar una canción completa

Solo cuando funcionen modelo/animación, audio, chart, controles y juicio por separado, crear la primera sección vertical. Este será el punto de validación más importante del port.

**Salida:** partida completa hasta la pantalla de resultados, con logs de sincronización y sin depender de un proceso de emulación de PS4.

---

## 14. Protocolo de aceptación para cualquier conversión PKG/BIN

Toda conversión o extracción debe quedar registrada como una operación reproducible, no como una suposición.

### Ficha de conversión

```text
ID de operación:
Fecha/hora:
Herramienta exacta y versión:
URL/repositorio (si se conoce):
Archivo fuente y tamaño:
SHA-256 fuente:
Metadatos del título/región/versión:
Opciones utilizadas:
Archivo o carpeta de salida:
SHA-256 de cada salida importante:
Formato detectado de la salida:
Programa que logra abrir la salida:
Errores/advertencias:
Resultado: PASS / FAIL / INCONCLUSO
Notas:
```

### Condiciones de PASS

- La herramienta se identificó y la operación puede repetirse.
- La fuente original permanece intacta.
- La salida tiene una función técnica definida, no solo una extensión.
- El tipo de archivo se validó con una herramienta adecuada y puede abrirse con el consumidor esperado.
- La versión/región de los recursos concuerda con el proyecto.

### Condiciones de FAIL o INCONCLUSO

- No se sabe qué representa el BIN de salida.
- Solo se cambió la extensión.
- El tutorial usa otra herramienta o versión no identificada.
- No hay evidencia de que la salida sea legible.
- Se mezclaron archivos de distintas versiones sin verificar compatibilidad.

---

## 15. Dependencias, decisiones y referencias que deben mantenerse actualizadas

- Ghidra: https://github.com/NationalSecurityAgency/ghidra
- Documentación del decompilador Ghidra: https://www.ghidradocs.com/10.2.3_PUBLIC/help/Decompiler/help/topics/DecompilePlugin/DecompilerIntro.html
- OpenOrbis PS4 Toolchain y formatos/herramientas ELF/OELF: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain
- OpenOrbis PS4 ELF Specification: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/wiki/PS4-ELF-Specification
- PKG Studio público localizado durante la actualización: https://github.com/ababilkhoerulimam/pkg-studio
- Vídeo aportado por el usuario: https://www.youtube.com/watch?v=F1kdxkRXTdg&t=1s
- GFD Studio: https://github.com/tge-was-taken/GFD-Studio
- GMDTool: https://github.com/lemoncove/GMDTool
- SDL: https://github.com/libsdl-org/SDL
- CMake: https://cmake.org/
- Blender: https://www.blender.org/

La documentación pública de OpenOrbis indica que su toolchain sirve para desarrollar aplicaciones/homebrew para PS4 y producir ejecutables compatibles con su cargador; eso no constituye una herramienta que traduzca un juego comercial de PS4 a un programa Windows. Véase el README de OpenOrbis para el alcance de su toolchain.

La versión pública de PKG Studio localizada durante esta actualización describe funciones de personalización visual/metadata y reempaquetado de paquetes. Puede no ser la misma aplicación/version que tengas instalada o la que muestra el vídeo; debe confirmarse en el entorno real antes de depender de ella.

---

## 16. Definición final del producto y condición de cierre

El proyecto se dará por completado únicamente cuando cumpla los siguientes puntos: 

- [ ] El repositorio compila desde un checkout limpio siguiendo documentación reproducible.
- [ ] La aplicación inicia en Windows x64 sin arrancar un emulador de PS4.
- [ ] Los recursos necesarios se localizan desde un directorio de datos del usuario y los errores se informan claramente.
- [ ] Se pueden seleccionar e iniciar las canciones incluidas en el alcance publicado.
- [ ] Música, eventos de ritmo, animaciones y evaluación mantienen sincronización verificable.
- [ ] Una canción se puede completar desde inicio hasta resultados y reiniciarse sin cerrar la aplicación.
- [ ] Puntuación, combo y resultados superan pruebas unitarias e integración.
- [ ] Las mejoras gráficas son configurables y no cambian el comportamiento temporal del juego.
- [ ] Se ha hecho una campaña de regresión con hardware objetivo (incluida GTX 3050) y limitaciones conocidas documentadas.
- [ ] Los archivos comerciales del juego no se distribuyen dentro del repositorio ni del paquete público.
- [ ] Existe un manifiesto de versiones compatibles, contenidos soportados y fallos conocidos.

**Principio rector:** tener un PKG, un `eboot.bin`, pseudocódigo en Ghidra o una conversión con extensión BIN son artefactos intermedios. El producto final es una aplicación de Windows que permite jugar de principio a fin y que no delega la ejecución en un emulador de PS4.
