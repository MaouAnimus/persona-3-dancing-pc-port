# Hoja de ruta maestra — Persona 3: Dancing in Moonlight para PC

**Versión del documento:** 1.1  
**Fecha:** 14 de octubre de 2026  
**Objetivo:** port nativo para Windows, con fidelidad al juego de PS4 y mejoras gráficas opcionales.  
**Plataforma objetivo inicial:** Windows x64  
**Hardware conocido:** NVIDIA GeForce GTX 3050  
**Experiencia del desarrollador:** conocimientos básicos  
**Región de referencia inicial:** edición estadounidense; verificar el identificador real de la copia antes de empezar.

> Esta hoja de ruta define una estrategia de investigación y prototipado. No afirma que exista ya un port, una reconstrucción completa del código ni una herramienta que permita convertir el juego automáticamente. El objetivo es demostrar la viabilidad etapa por etapa.

> **Nota añadida en v1.1:** el flujo de trabajo ahora incluye validar PKG Studio y el destino “BIN”. No se presume que el PKG del juego pueda convertirse íntegramente a BIN: primero se debe definir qué tipo de BIN se necesita y comprobar que la herramienta lo admite.

---

## 1. Visión del proyecto

Desarrollar una aplicación de Windows que reproduzca la experiencia de *Persona 3: Dancing in Moonlight* sin necesitar un emulador de PS4 en tiempo de ejecución. La versión inicial debe priorizar la fidelidad de la jugabilidad, el ritmo musical, la presentación audiovisual, la interfaz y las animaciones. Las mejoras visuales serán opcionales y no deben cambiar la identidad artística.

### 1.1 Alcance deseado

- Ejecutable nativo de Windows x64.
- Reproducción de canciones sincronizada con el sistema de notas.
- Evaluación de entradas, puntuación, combo, multiplicadores, scratch/inputs especiales y estados de Fever, según el comportamiento observado en el juego original.
- Reproducción de modelos, esqueletos, animaciones, materiales, texturas, escenarios y cámaras.
- Menús, selección de canción, selección de personaje y vestuario en las etapas posteriores.
- Audio, diálogos, escenas y pantallas de resultados.
- Guardado local, configuración y remapeo de controles.
- Resolución 1080p como objetivo inicial; resoluciones superiores y mejoras gráficas como opciones.
- Recursos originales conservados localmente y separados del código del proyecto.

### 1.2 Fuera del alcance inicial

- Implementar de inmediato todas las canciones, escenas, accesorios y DLC.
- Crear herramientas completas para modificar y reempaquetar el PKG original.
- Ofrecer compatibilidad con todas las regiones desde el primer prototipo.
- Replicar PS4 OS o escribir un emulador de PS4.
- Añadir multijugador, funciones en línea u otras características no necesarias para la experiencia principal.

### 1.3 Definición de «port nativo»

El resultado final debe ser un programa de PC que implemente directamente los sistemas necesarios y no dependa de lanzar el binario de PS4 dentro de un emulador. No basta con abrir recursos extraídos ni con mostrar un modelo en Blender.

Dado que no tenemos el código fuente original, el camino probable es una **reimplementación por sistemas basada en análisis estático, observación del comportamiento y recursos compatibles**, reutilizando herramientas comunitarias de formatos. La posibilidad de reutilizar partes del ejecutable se decidirá después de examinar legalmente accesibles los binarios y sus dependencias; no se presupone que puedan recompilarse como C++ de Windows.

---

## 2. Herramientas existentes y para qué sirve cada una

### 2.1 PKG Studio y el paso solicitado «PKG → BIN»

**No debe asumirse que PKG Studio convierte un juego PS4 completo de `.pkg` a `.bin`.** La herramienta pública identificable como [ababilkhoerulimam/pkg-studio](https://github.com/ababilkhoerulimam/pkg-studio) se describe como personalizador de paquetes: extrae metadatos y recursos visuales, permite modificar iconos/fondos y vuelve a empaquetar el proyecto como `.pkg`. Su documentación no declara una conversión general de juegos completos a `.bin`. El enlace de YouTube aportado para esta hoja de ruta no pudo verificarse directamente; hasta confirmar la herramienta y el contenido concreto del vídeo, no daremos esa función por existente.

Además, «BIN» no identifica un formato único. Antes de convertir, hay que definir qué salida se necesita:

- **`eboot.bin`:** nombre habitual del ejecutable de entrada dentro de una estructura de juego PS4. Extraer o encontrar ese archivo no convierte el PKG entero a BIN ni garantiza que sea ejecutable en Windows; puede depender de formato, firma, cifrado y bibliotecas específicas de la consola.
- **Imagen de disco BIN/CUE:** formato de imagen óptica empleado en algunos flujos de CD/DVD y consolas antiguas. Un paquete PS4 no se vuelve una imagen BIN/CUE válida simplemente cambiando la extensión.
- **Archivo binario genérico (`.bin`):** extensión sin una especificación universal. Solo puede generarse si el programa consumidor define exactamente la estructura que espera.
- **Formato de imagen/carpeta para un emulador concreto:** utilizar el formato y la estructura documentados por ese emulador. No transformar ni renombrar archivos a ciegas.

### Procedimiento seguro para este proyecto

1. Preservar el PKG original sin modificarlo y trabajar sobre una copia local de una versión que se posea legítimamente.
2. Registrar nombre exacto, autor, versión y URL de la aplicación denominada «PKG Studio»; confirmar sus botones, formatos de entrada/salida y documentación.
3. Inspeccionar metadatos y listar los archivos que la herramienta pueda mostrar mediante funciones documentadas. Si la herramienta solo personaliza/reempaqueta PKG, usarla para esa función, no como conversor BIN.
4. Definir por escrito el consumidor del archivo de salida y su especificación: nombre, estructura, cabecera/magic, alineación, contenido esperado y validador.
5. Si el objetivo es investigar los recursos para el port, generar un inventario y trabajar con los recursos accesibles a través de herramientas documentadas; no es necesario crear un BIN ficticio.
6. Registrar hashes SHA-256 antes/después, errores, versión de la herramienta y resultado de la prueba. Nunca sobrescribir la única copia.

Herramientas de referencia para gestión e inspección de PKG: [PS4PKGTool](https://github.com/pearlxcore/PS4PKGTool) y [PS4-PKG-Workbench](https://github.com/bigjokker/PS4-PKG-Workbench). Sus funciones concretas deben comprobarse en la versión instalada. El segundo documenta extracción/listado y administración de paquetes, no una conversión universal de juegos a BIN.

No se deben descargar paquetes de procedencia dudosa ni intentar sortear protecciones de cifrado/DRM. Trabajar con una copia propia y con métodos de extracción permitidos. No incluir PKG, ejecutables propietarios ni recursos del juego en repositorios públicos.

### 2.2 REA — Reverse Engineer Anything

Repositorio oficial: https://github.com/morluto/rea

REA integra un agente con herramientas de análisis y puede ayudar a inspeccionar binarios, cadenas, referencias, llamadas y pseudocódigo; trabaja con motores como Ghidra, Hopper o IDA, según la configuración y el tipo de análisis. REA no recupera el código fuente original de manera garantizada, no es por sí solo un extractor de PKG y sus conclusiones deben verificarse con evidencia.

Instalación documentada por el proyecto (revisar requisitos actuales antes de ejecutar):

```powershell
node --version
npm --version
npx rea-agents setup
```

Se requiere una versión compatible de Node.js indicada por el README actual de REA. Tras la configuración, revisar cada cambio que proponga el instalador, aprobarlo manualmente y reiniciar el agente integrado.

Antes de apuntar REA a un binario, probar capacidades y proveedor disponibles:

```powershell
npx -y rea-agents@latest doctor
npx -y rea-agents@latest --help
```

REA cambia con frecuencia; consultar la documentación actual y comprobar específicamente el soporte del proveedor de análisis elegido en Windows. Una alternativa es usar Ghidra en un entorno compatible documentado por sus autores y mantener el entorno de análisis separado del juego y del repositorio de código.

### 2.3 GFD Studio

Repositorio: https://github.com/tge-was-taken/GFD-Studio

Prioridad alta para analizar modelos Persona. El proyecto se describe como visor/editor de modelos GMD/GFS, con vista renderizada, exportación y reemplazo de texturas, edición de materiales e importación/exportación de modelos mediante Assimp. El importador asociado menciona explícitamente P3D/P5D.

Uso previsto:

1. Abrir una muestra de modelo obtenida legalmente.
2. Confirmar si el archivo y la variante se reconocen correctamente.
3. Documentar materiales, texturas, jerarquía y animaciones visibles.
4. Exportar una copia de prueba a un formato de intercambio para el prototipo propio.
5. Comparar la exportación con el modelo mostrado por la herramienta.

No asumir que la exportación vuelve a generar un recurso que el juego original pueda cargar. El primer uso será de lectura y conversión para nuestro propio runtime.

### 2.4 GMDTool

Repositorio: https://github.com/lemoncove/GMDTool

GMDTool convierte modelos `.GMD` a Collada `.DAE`, con opciones relacionadas con blend shapes y compatibilidad con Blender. Su README incluye Persona 3: Dancing in Moonlight. No convierte modelos editados desde DAE de vuelta a GMD.

Debe tratarse como herramienta de extracción/conversión, no como un pipeline completo de ida y vuelta.

### 2.5 BlenderToolsForGFS

Repositorio: https://github.com/Pherakki/BlenderToolsForGFS

Alternativa para investigar formatos GFS desde Blender. La tabla del proyecto indica soporte parcial para algunas variantes de Persona 3 Dancing, así que la precisión de las importaciones/exportaciones debe validarse con recursos concretos. No asumir soporte completo de todas las animaciones o subformatos.

### 2.6 Entorno de desarrollo

- Git y Git LFS solo si el proyecto necesita versionar archivos grandes propios; nunca usar LFS para publicar recursos propietarios del juego.
- Visual Studio 2022 con el workload de C++ para escritorio, o un toolchain C++ moderno equivalente.
- CMake para builds reproducibles.
- Python 3 para inventario de archivos, hashes y automatización de recursos.
- Ghidra como herramienta de ingeniería inversa, según su licencia y requisitos.
- Blender para inspeccionar mallas y animaciones exportadas.
- RenderDoc para investigar el renderizador propio y depurar cuadros de GPU del prototipo, no para saltarse protecciones.
- Catch2 o doctest para pruebas unitarias.
- GitHub Actions u otro CI para compilar y ejecutar las pruebas del código propio sin recursos propietarios.

---

## 3. Arquitectura técnica objetivo

### 3.1 Principio general

Separar el **material de referencia y análisis** del **programa nativo que vamos a escribir**. El primer repositorio contendrá código propio, documentación y herramientas; las copias y extracciones del juego estarán en una carpeta local excluida de Git.

### 3.2 Diagrama de alto nivel

```text
Copia legítima de PS4 / archivos de investigación locales
             |
             v
Inventario + metadatos + hashes + capturas de referencia
             |
             +-----------------------------+
             |                             |
             v                             v
REA + Ghidra (binario)          GFD Studio / GMDTool (recursos)
             |                             |
             v                             v
Notas de comportamiento          Conversiones de mallas, texturas,
Pseudocódigo y mapas             materiales y animaciones
             |                             |
             +--------------+--------------+
                            v
                 Documentación de formatos
                 y especificación observable
                            |
                            v
                Xenosaga no; proyecto P3D PC
           Runtime nativo C++20 para Windows x64
                            |
           +----------------+-------------------+
           |                |                   |
           v                v                   v
       Render              Audio             Ritmo/juego
           |                |                   |
           +----------------+-------------------+
                            v
                Menús, resultados y guardado
                            |
                            v
             Ejecutable nativo y recursos locales
```

### 3.3 Stack sugerido

| Subsistema | Elección inicial | Motivo |
|---|---|---|
| Lenguaje | C++20 | Buen control del rendimiento y amplia compatibilidad con bibliotecas gráficas. |
| Build | CMake + Ninja o generador de Visual Studio | Builds repetibles y estructura clara. |
| Ventana/entrada | SDL3 | Ventana, teclado, mando y eventos multiplataforma. |
| Renderizador | Vulkan o Direct3D 12 detrás de una interfaz propia | GPU moderna; escoger solo una API para el primer prototipo. |
| Herramientas | Dear ImGui (solo para herramientas internas) | Depuración de recursos, reloj musical, cámara y estado de animación. |
| Audio | Biblioteca de audio con reloj de reproducción consultable (por ejemplo, miniaudio) | Sincronizar el sistema de ritmo con el tiempo efectivo del audio. |
| Importación | GFD Studio/GMDTool para análisis; conversor propio a un formato runtime | Aprovechar los importadores existentes sin hacer que el juego dependa de Blender. |
| Formato runtime | GLB/glTF para pruebas iniciales; formato interno documentado cuando sea necesario | Evitar inventar de entrada un contenedor binario complejo. |
| Pruebas | Catch2/doctest + pruebas de integración | Validar sincronización, puntuación y carga de recursos. |
| Registro | spdlog o logging sencillo propio | Diagnóstico de errores reproducible. |

**Decisión gráfica recomendada:** en Windows con una GTX 3050, Direct3D 12 es razonable si el objetivo es solo Windows; Vulkan es razonable si se desea mantener abierta la posibilidad de otros sistemas. El primer prototipo debe usar una sola API. No es necesario explotar funciones avanzadas de GPU para demostrar que el modelo y el ritmo funcionan.

### 3.4 Módulos del runtime

- `platform/`: ventana, rutas, entrada, configuración y servicios del sistema operativo.
- `render/`: GPU, swapchain, texturas, buffers, materiales, cámara, luces y postprocesado opcional.
- `assets/`: catálogo de recursos, lectores, validadores y conversor a formato runtime.
- `animation/`: esqueletos, jerarquías, clips, blend trees sencillos, morph targets y reloj de animación.
- `audio/`: carga, reproducción, volumen, latencia estimada y reloj maestro de canción.
- `rhythm/`: chart de notas, ventanas de juicio, entrada, score, combo, Fever y eventos especiales.
- `game/`: estados de menú, canción, pausa, resultados y selección de personaje.
- `ui/`: elementos de interfaz, tipografía, HUD, navegación y localización.
- `save/`: opciones y progreso local en formato versionado.
- `tools/`: inspección de recursos, editor de chart de prueba y captura de estado.
- `tests/`: pruebas unitarias e integrales.

### 3.5 Fuentes de verdad

- El juego original y las capturas de referencia sirven para medir el comportamiento observable.
- Cada deducción de REA/Ghidra debe tener dirección/función, captura o evidencia asociada y nivel de confianza.
- El código propio define el comportamiento del port una vez documentado.
- Los recursos extraídos no deben editarse de forma destructiva: preservar el original y transformar copias.

---

## 4. Organización de carpetas

```text
p3dancing-pc/
├── README.md
├── LICENSES.md
├── CMakeLists.txt
├── CMakePresets.json
├── .gitignore
├── docs/
│   ├── vision.md
│   ├── target-build.md
│   ├── pkg-tool-validation.md
│   ├── package-inventory.md
│   ├── asset-formats.md
│   ├── reverse-engineering-notes/
│   ├── gameplay-spec/
│   ├── audio-timing.md
│   ├── rendering-parity.md
│   └── decision-records/
├── src/
│   ├── platform/
│   ├── render/
│   ├── assets/
│   ├── animation/
│   ├── audio/
│   ├── rhythm/
│   ├── game/
│   ├── ui/
│   └── save/
├── tools/
│   ├── asset-inventory/
│   ├── asset-converter/
│   └── chart-debugger/
├── tests/
│   ├── unit/
│   ├── integration/
│   └── fixtures/              # solo archivos de prueba propios o redistribuibles
├── third_party/
│   └── README.md              # lista de dependencias, no recursos del juego
└── local_data/                 # local, excluida de Git
    ├── package_input/
    ├── extracted_reference/
    ├── manifests/
    ├── converted_assets/
    ├── screenshots/
    └── audio_reference/
```

Añadir al `.gitignore` carpetas de `local_data/`, binarios del juego, PKG, dumps, recursos propietarios, bases de datos de descompilación que incluyan datos del juego y archivos grandes de referencia. El repositorio público no debe contener la copia del juego ni sus canciones/modelos extraídos.

---

## 5. Hoja de ruta por fases

## Fase 0 — Definir exactamente la versión objetivo

**Objetivo:** evitar mezclar regiones, parches, demos o paquetes de DLC.

Tareas:

1. Identificar el producto real en la metadata (`param.sfo` si está accesible) y registrar Title ID, versión, región, tipo de paquete y presencia de actualizaciones/DLC.
2. Tomar la edición estadounidense como referencia inicial solo si los metadatos de la copia lo confirman. El identificador conocido para la edición US es CUSA-12636; existen otros Title IDs de otras regiones o ediciones.
3. Documentar versión del juego, nombre exacto del PKG Studio usado y método legal mediante el cual se obtuvo la copia local.
4. Guardar un manifiesto de archivos con tamaños y SHA-256.
5. Hacer copias de seguridad y trabajar sobre una copia de análisis no destructiva.

**Entregable:** `docs/target-build.md`, `docs/pkg-tool-validation.md` (tras la Fase 0B) y `local_data/manifests/package_inventory.csv`.

**Criterio de aceptación:** el objetivo se identifica sin ambigüedad y los archivos fuente están preservados. Si el paquete no se puede leer con la herramienta elegida, registrar el fallo en vez de asumir que el juego está dañado o saltarse protecciones.

## Fase 0B — Incorporar PKG Studio sin asumir una conversión inexistente

**Objetivo:** incorporar el paso de gestión del paquete al flujo de trabajo y decidir con evidencia si hay una conversión útil a BIN.

Tareas:

1. Identificar la herramienta exacta a partir de su URL de repositorio, versión o ventana «Acerca de». El vídeo puede servir como referencia visual, pero no sustituye a la documentación de la aplicación.
2. Crear `docs/pkg-tool-validation.md` con nombre, versión, autor, sistema operativo, entradas aceptadas, salidas aceptadas y enlaces de documentación.
3. Con una copia de trabajo del PKG propio, inspeccionar únicamente funciones documentadas como información del paquete, inventario y extracción accesible. No alterar ni sobrescribir el original.
4. Registrar qué significa «BIN» para nuestro caso: `eboot.bin`, imagen BIN/CUE, archivo binario definido por el usuario o formato exigido por otro programa.
5. Si PKG Studio no ofrece esa salida, detener ese subpaso y seleccionar una herramienta documentada para la operación necesaria, o redefinir el entregable como inventario/carpeta de recursos. No crear un archivo inválido cambiando la extensión.
6. Comparar hashes del original y de la copia, y verificar que cualquier salida tenga estructura reconocible y se pueda abrir con el programa consumidor esperado.
7. Mantener PKG Studio como herramienta auxiliar de administración/reempaquetado solo para las funciones que realmente soporte.

**Entregable:** `docs/pkg-tool-validation.md`, manifiesto de entradas/salidas y decisión escrita sobre el formato de destino.

**Criterio de aceptación:** el equipo puede explicar qué contiene el archivo de salida y con qué herramienta se valida. No se considera completada una «conversión a BIN» solo porque el archivo tenga extensión `.bin`.

**Puerta de decisión:** si no existe un objetivo BIN concreto o la herramienta no lo soporta, el proyecto continúa directamente al inventario de recursos y al análisis de formatos. La conversión PKG → BIN no es un requisito técnico del port por sí misma.

---

## Fase 1 — Instalación del entorno y control de versiones

**Objetivo:** tener un proyecto limpio que compile antes de tocar recursos complicados.

Tareas:

1. Instalar Git, Visual Studio con C++, CMake y Python 3.
2. Crear el repositorio del proyecto propio.
3. Añadir un ejecutable de prueba que abra una ventana, limpie la pantalla y cierre correctamente.
4. Añadir logging, una configuración mínima y una prueba automática.
5. Añadir un README que explique que los recursos del juego no se incluyen.

**Entregable:** `p3dancing-pc` compila en una máquina limpia y abre su ventana de prueba.

**Criterio de aceptación:** build reproducible en modo Debug y Release, salida de logs y al menos una prueba automatizada ejecutada correctamente.

## Fase 2 — Inventario de archivos y análisis inicial

**Objetivo:** averiguar qué contiene la versión concreta antes de diseñar parsers.

Tareas:

1. Exportar/listar la estructura del contenido mediante la herramienta que funcione con la copia legal.
2. Registrar extensión, ruta, tamaño, magic/header cuando sea observable y SHA-256.
3. Agrupar por familia de recurso: ejecutable, bibliotecas, datos, modelos, animaciones, texturas, archivos de audio, vídeos, localización, configuración o desconocido.
4. Crear un informe por grupos de extensiones y tamaños.
5. Elegir 3–5 recursos pequeños y no críticos como muestras de prueba.
6. Marcar qué recursos están en claro y cuáles no resultan interpretables. No intentar sortear cifrado/DRM; documentar la limitación.

**Entregable:** `docs/package-inventory.md`, CSV/JSON de inventario, hashes y un conjunto de muestras locales.

**Criterio de aceptación:** podemos reproducir el inventario y distinguir entre “formato conocido”, “candidato a formato conocido” y “desconocido”.

## Fase 3 — Catálogo de formatos de recursos

**Objetivo:** validar los formatos más prometedores con herramientas existentes.

Tareas:

1. Probar muestras de modelos con GFD Studio.
2. Probar conversión GMD → DAE con GMDTool si el modelo está reconocido como GMD.
3. Probar recursos GFS con GFD Studio y, cuando tenga sentido, BlenderToolsForGFS. Considerar la cobertura parcial.
4. Verificar las exportaciones en Blender: malla, esqueleto, UV, materiales, textura, nombres de huesos, escala y orientación.
5. Documentar resultados positivos y negativos con capturas, hash de entrada, versión de la herramienta y ajustes usados.
6. Repetir con una muestra de personaje y otra de escenario/objeto, si están disponibles.

**Entregable:** `docs/asset-formats.md` y una tabla de compatibilidad por clase de archivo.

**Criterio de aceptación:** al menos un modelo real exportado y visualizado fuera de la herramienta original, con errores conocidos anotados.

## Fase 4 — Ingeniería inversa guiada por hipótesis

**Objetivo:** entender la estructura del programa y el comportamiento del juego sin suponer que el decompilador conoce su intención.

Tareas:

1. Verificar que el ejecutable accesible corresponde a la versión objetivo y registrar hash/tamaño/metadata.
2. Configurar REA con un proveedor de análisis compatible; comprobar las funciones disponibles mediante la documentación actual.
3. Empezar por símbolos, cadenas, nombres de bibliotecas importadas y estructura general, siempre que el binario accesible admita ese análisis.
4. Construir hipótesis sobre subsistemas: audio, recursos, animación, ritmo, entrada, guardado, escenas y menús.
5. Crear un cuaderno de investigación por hipótesis: pregunta, evidencia, conclusión, contraejemplos y confianza.
6. Priorizar funciones que ayuden a reconstruir formatos o reglas del ritmo; dejar ofuscación y detalles no imprescindibles para después.
7. No tratar el pseudocódigo de REA como código fuente correcto; verificar tipos, límites, llamadas y valores devueltos.

**Entregable:** `docs/reverse-engineering-notes/` con fichas breves y evidencias.

**Criterio de aceptación:** al menos un comportamiento crítico del juego queda documentado con evidencia verificable, y una segunda persona podría entender la hipótesis sin leer el razonamiento completo del agente.

## Fase 5 — Especificación del ritmo y de la jugabilidad

**Objetivo:** diseñar una reproducción del sistema musical verificable sin modelos complejos.

Tareas:

1. Registrar una canción objetivo: duración, BPM si se conoce o puede medirse, compases, patrón de notas y eventos visibles.
2. Grabar referencias de timing o anotar tiempos relativos desde el inicio del audio.
3. Documentar las entradas posibles: botones direccionales, botones de acción, scratch u otros eventos observados en el juego.
4. Medir las ventanas de juicio mediante pruebas controladas y documentar incertidumbre cuando no se conozcan los valores exactos.
5. Diseñar un formato propio de chart de pruebas en JSON o TOML, con tiempo en segundos o ticks de alta precisión.
6. Implementar un reloj musical basado en la posición de reproducción de audio, no únicamente en delta-time de los fotogramas.
7. Diseñar estados: menú, precarga, cuenta atrás, reproducción, pausa, fallo/fin de canción y resultados.

**Entregable:** una especificación pequeña de chart y un test de una canción sintética creada para el proyecto.

**Criterio de aceptación:** la secuencia de notas y el resultado de las entradas son deterministas en pruebas automatizadas. La canción sintética no se debe confundir con el contenido original.

## Fase 6 — Primer prototipo jugable sin recursos propietarios

**Objetivo:** probar el mayor riesgo técnico temprano: sincronización, evaluación y puntuación.

Construir una escena simple con formas 2D y audio de prueba redistribuible o creado por el equipo.

Módulos mínimos:

- reloj musical;
- generador/cargador de chart;
- registro de inputs con timestamp;
- hit windows configurable;
- judgement y puntuación;
- combo y estados de fallo/fin;
- interfaz de depuración que muestre tiempo de la canción, siguiente nota y error de timing;
- reproducción a 60 FPS y límites de frame rate opcionales.

**Entregable:** prototipo que pueda completar una canción de prueba y generar un reporte de resultados.

**Criterio de aceptación:** con una secuencia de entradas grabada, el resultado debe ser idéntico en ejecuciones repetidas; el sistema no debe perder el tempo al cambiar la tasa de fotogramas.

## Fase 7 — Pipeline de modelos, texturas y materiales

**Objetivo:** mostrar un modelo real del juego en el runtime propio, sin exigir todavía animación completa.

Tareas:

1. Convertir un modelo conocido a un formato de intercambio comprobado.
2. Crear un conversor batch con registro de errores y manifest de conversión.
3. Establecer sistema de coordenadas, unidades, orientación de cámara, tangentes, formato de textura y nombres de materiales.
4. Implementar la carga runtime de malla, índices, UV, normales, materiales y texturas.
5. Crear un modo de inspección: cámara orbital, vistas de normales/UV, wireframe y lista de materiales.
6. Comparar visualmente con la herramienta de origen y corregir escala, orientación y normales antes de avanzar.
7. Preservar un hash del original y registrar los parámetros de conversión para poder reproducirla.

**Entregable:** ventana nativa que muestra al menos un modelo extraído de forma legítima con las texturas que pudieron convertirse.

**Criterio de aceptación:** no hay corrupción de geometría, UV manifiestamente desplazadas ni texturas perdidas sin diagnóstico. Las diferencias restantes están documentadas.

## Fase 8 — Esqueleto y animación

**Objetivo:** reproducir una animación corta y comprobar que la malla, los huesos y el reloj de animación están integrados.

Tareas:

1. Identificar cómo la herramienta disponible interpreta los esqueletos y clips de la muestra.
2. Verificar jerarquía ósea, transformaciones locales/globales, pesos de skinning y convención de rotaciones.
3. Implementar interpolación de traslación/rotación/escala según los datos disponibles.
4. Reproducir un clip de prueba en bucle, sin depender de eventos de la canción.
5. Añadir una línea de tiempo para inspeccionar el frame y el tiempo actual del clip.
6. Investigar blending entre clips después de demostrar que un clip individual funciona.

**Entregable:** personaje animado en el runtime propio, usando recursos permitidos de la copia local.

**Criterio de aceptación:** la pose no cambia de forma errática entre frames, el esqueleto mantiene continuidad y el clip se reproduce con duración/velocidad documentadas.

## Fase 9 — Audio real y sincronización audiovisual

**Objetivo:** sincronizar canciones, beats, animaciones y efectos.

Tareas:

1. Identificar formatos de audio reales por los encabezados y la documentación disponible; no asumir códec o contenedor antes de inspeccionarlos.
2. Mantener un módulo de audio independiente del sistema de juicio.
3. Consultar la posición real del dispositivo de audio cuando sea posible, estimar latencia y ofrecer ajuste manual de offset.
4. Programar cambios de escena, animaciones y efectos por eventos temporales de la canción.
5. Asegurar que pausa, reanudación, cambio de dispositivo y pérdida de foco no rompan la sincronización.
6. Comparar una canción de prueba con una referencia del original utilizando eventos medibles.

**Entregable:** audio reproducido con un reloj fiable que gobierna notas y animaciones en el prototipo propio.

**Criterio de aceptación:** desfase medido registrado; el offset puede calibrarse; el sistema no depende de acumular delta-time por frame.

## Fase 10 — Primera vertical slice con recursos del juego

**Objetivo:** unir todos los subsistemas en una demostración pequeña y medible.

Alcance sugerido:

- una canción seleccionada;
- un personaje y un vestuario;
- una escena de fondo;
- un conjunto limitado de animaciones;
- chart y notas representadas en pantalla;
- evaluación y combo;
- audio sincronizado;
- pantalla de resultado sencilla.

No incluir todavía todos los menús, todos los personajes, DLC o escenas sociales.

**Entregable:** una demo ejecutada desde un `.exe` propio de Windows y sin emulador de PS4 en tiempo de ejecución.

**Criterio de aceptación:** puede arrancarse desde una carpeta limpia del proyecto, completar la sección y mostrar resultados; se documentan las diferencias con la versión de referencia.

## Fase 11 — Interfaz, navegación, selección y progresión

**Objetivo:** integrar las pantallas del juego gradualmente.

Orden recomendado:

1. Menú principal propio provisional.
2. Selección de canción.
3. Selección de personaje y vestuario.
4. Precarga y transición a la canción.
5. Pausa, reintento y resultados.
6. Guardado de opciones/progreso.
7. Diálogos y escenas sociales cuando se haya entendido su formato y flujo.

Cada pantalla debe ser un estado explícito de una máquina de estados. Evitar condicionales dispersos dentro del renderizador.

**Criterio de aceptación:** las transiciones no pierden recursos ni dejan estados bloqueados; las opciones persisten tras reiniciar y el guardado se valida contra versiones de esquema.

## Fase 12 — Mejoras gráficas opcionales

**Objetivo:** elevar la calidad en PC sin cambiar la presentación base.

Implementar únicamente después de conseguir una vertical slice fiel:

- resolución y escalado configurables;
- filtrado de textura adecuado;
- antialiasing opcional;
- sincronización vertical y límite de FPS;
- mejores opciones de anisotropía;
- presentación de interfaz escalable;
- sombras, iluminación o postprocesado adicional solo si la escena lo tolera y existe comparación visual.

No forzar iluminación físicamente basada o sombras nuevas donde alteren materiales originales; preservar una opción “Original” que desactive las mejoras.

**Objetivo de rendimiento inicial:** 1080p estable a 60 FPS en la GTX 3050, considerado una meta de ingeniería a medir, no una garantía previa. Registrar GPU, CPU, resolución, VRAM y tiempos de frame.

**Criterio de aceptación:** opciones nuevas son conmutables, no rompen la temporización musical y el modo original mantiene la apariencia acordada.

## Fase 13 — Validación, compatibilidad y empaquetado

**Objetivo:** convertir la demo en una aplicación mantenible.

Tareas:

1. Probar distintas relaciones de aspecto y resoluciones.
2. Probar teclado y mandos; permitir remapeo de controles.
3. Probar desconexión/reconexión de mando, cambio de ventana, alt-tab y pausa.
4. Probar carga repetida de canciones y liberar recursos correctamente.
5. Probar datos ausentes o corruptos con mensajes útiles.
6. Añadir telemetría local de diagnóstico sin recopilar datos personales.
7. Crear empaquetado del ejecutable propio y sus dependencias redistribuibles.
8. Verificar licencias de bibliotecas, codecs y herramientas auxiliares.
9. Mantener los recursos propietarios fuera del instalador público salvo permiso expreso; diseñar el programa para cargar recursos del usuario localmente.

**Entregable:** build de prueba etiquetada, pruebas de regresión y guía de instalación.

**Criterio de aceptación:** arranque limpio, logs útiles, no se distribuyen recursos no autorizados y los defectos conocidos aparecen en notas de versión.

---

## 6. Primeros 30 días: plan de ejecución detallado

Este calendario asume trabajo parcial y aprendizaje durante el proceso. Si surgen obstáculos de formatos, la duración se ajustará; los objetivos son entregables, no promesas de terminar una fase en una fecha exacta.

### Semana 1 — Inventario y entorno

**Día 1–2**
- Registrar nombre, versión y URL oficial de PKG Studio.
- Confirmar que la copia propia corresponde a la edición que se quiere usar.
- Crear `docs/target-build.md`.
- No editar ni sobrescribir el paquete original.

**Día 3–4**
- Instalar Git, Visual Studio C++, CMake y Python.
- Crear repo `p3dancing-pc`.
- Configurar `.gitignore` con `local_data/` y todos los binarios/recursos del juego.
- Crear un programa que abra una ventana.

**Día 5–7**
- Exportar o producir el inventario de archivos disponible a partir de la copia local.
- Calcular hashes para que el inventario pueda repetirse.
- Clasificar archivos por extensión, tamaño, firma y confianza.

**Salida de la semana:** entorno que compila y un inventario inicial de recursos.

### Semana 2 — Validar herramientas de modelos

**Día 8–9**
- Instalar/abrir GFD Studio según las instrucciones de su repositorio.
- Probar una muestra de modelo en modo lectura.

**Día 10–11**
- Probar GMDTool si la muestra es GMD compatible.
- Exportar a DAE y abrir en Blender.

**Día 12–14**
- Anotar qué se conservó: geometría, UV, materiales, textura, esqueleto, animación.
- Probar otra muestra de otra categoría si está disponible.

**Salida de la semana:** al menos un recurso representado fuera de la herramienta de origen y una lista clara de limitaciones.

### Semana 3 — Prototipo de ritmo

**Día 15–17**
- Crear el formato JSON propio de chart.
- Implementar reloj musical independiente de FPS.
- Añadir nota básica, entrada y ventanas de juicio configurables.

**Día 18–20**
- Implementar combo, score y final de canción.
- Escribir tests para límites de juicio, inputs repetidos, pausa y reanudación.

**Día 21**
- Crear una canción de prueba con audio propio/redistribuible y una secuencia pequeña de eventos.

**Salida de la semana:** juego rítmico de prueba sin recursos propietarios y con resultado reproducible.

### Semana 4 — Primer renderizador con un recurso real

**Día 22–24**
- Crear un cargador para el formato de intercambio elegido.
- Renderizar malla estática, textura y cámara orbital.

**Día 25–27**
- Añadir inspección de materiales y registro de errores.
- Confirmar que la carga de recursos no depende de rutas absolutas fuera de `local_data/`.

**Día 28–30**
- Integrar una pantalla de prototipo que pueda mostrar el modelo junto al sistema de ritmo de prueba, aunque todavía sean componentes separados.
- Escribir informe: qué funciona, qué falta, qué formatos son desconocidos y cuál es el siguiente mayor riesgo.

**Salida del mes:** dos pruebas técnicamente útiles —sistema rítmico funcional y visor nativo de un recurso real—, no un port completo.

---

## 7. Backlog técnico inicial

Priorizar en este orden:

1. `P0`: identificar la versión y preservar la copia de análisis.
2. `P0`: compilar una ventana vacía nativa.
3. `P0`: inventariar archivos y hashes.
4. `P0`: abrir/exportar una muestra de modelo con GFD Studio/GMDTool.
5. `P0`: chart sintético y reloj sincronizado a la reproducción de audio.
6. `P1`: renderizar una malla real con texturas en el runtime propio.
7. `P1`: animar un clip de modelo.
8. `P1`: integrar nota, audio, animación y cámara.
9. `P2`: investigar la estructura interna del ejecutable y las dependencias críticas.
10. `P2`: implementar navegación y menús.
11. `P2`: pantalla de resultado, opciones y guardado.
12. `P3`: canción completa y contenido adicional.
13. `P3`: mejoras gráficas opcionales.
14. `P4`: empaquetado, regresión, rendimiento y documentación.

La investigación del ejecutable debe avanzar en paralelo, pero no bloquear las pruebas del pipeline de recursos y del sistema rítmico.

---

## 8. Pruebas que deben existir desde el principio

### 8.1 Tests de ritmo

- Una entrada en el centro de la ventana obtiene el juicio esperado.
- Entradas justo antes/después de cada límite se evalúan consistentemente.
- Un input no puede puntuar dos veces por la misma nota.
- La pausa detiene la evaluación; reanudar no genera notas retroactivas.
- La posición de audio es la referencia temporal y se mantiene aunque fluctúe FPS.

### 8.2 Tests de recursos

- El cargador detecta formato inválido y evita lecturas fuera de límites.
- Una textura ausente genera un fallback y un mensaje claro.
- El importador registra escala y orientación aplicadas.
- Los hashes y el origen de la conversión quedan anotados.

### 8.3 Tests de animación

- El clip llega a la pose esperada en tiempos de referencia.
- El tiempo del clip puede buscarse hacia adelante y atrás.
- La malla conserva pesos/jerarquía sin saltos obvios.
- El cambio de clip no deja matrices de huesos sin inicializar.

### 8.4 Tests de integración

- Arranque → selección de canción → reproducción → resultados.
- Alt-tab durante canción.
- Mando desconectado/reconectado.
- Archivo de configuración ausente, incompleto o con versión antigua.
- Carga y descarga repetida sin fuga evidente de memoria.

---

## 9. Riesgos principales y mitigaciones

| Riesgo | Impacto | Mitigación |
|---|---|---|
| PKG Studio no admite el formato/versión concreta | Alto | Identificar el producto exacto, probar metadatos primero y documentar limitaciones; no saltarse protecciones. |
| El ejecutable no es analizable con la configuración actual | Alto | Validar proveedor REA/Ghidra/IDA y el estado del binario; continuar la ingeniería del comportamiento y los recursos que sí estén disponibles. |
| Recursos de modelo solo parcialmente soportados | Alto | Probar GFD Studio, GMDTool y BlenderToolsForGFS; crear conversores propios pequeños solo para los huecos demostrados. |
| No se conocen formatos de canciones/chart | Muy alto | Priorizar prototipo musical sintético y elaborar formatos propios; investigar una sola muestra de cada tipo. |
| Pérdida de sincronía entre sonido y notas | Muy alto | Reloj maestro basado en audio, compensación de latencia y tests automatizados. |
| Animaciones complejas o blending no exportado | Alto | Validar primero un clip, documentar los tipos que faltan y expandir de forma incremental. |
| La reimplementación completa es más grande de lo esperado | Muy alto | Mantener vertical slices pequeñas, revisar alcance por hitos y evaluar viabilidad antes de cubrir todo el contenido. |
| Cambiar gráficos altera la estética | Medio | Modo de presentación original, mejoras optativas y comparaciones de referencia. |
| Recursos propietarios llegan al repo público | Muy alto | `local_data/` en `.gitignore`, revisión de commits y publicación solo de código, herramientas y datos permitidos. |
| Conclusiones erróneas de la IA | Alto | Evidencias, hipótesis falsables, revisión manual y tests reproducibles; nunca confiar únicamente en el pseudocódigo. |

---

## 10. Señales para continuar o revisar el plan

### Continuar

- Se identifica el paquete con precisión.
- La herramienta existente reconoce al menos una muestra de modelo.
- El prototipo rítmico usa un reloj estable y pasa sus tests.
- El runtime propio dibuja un modelo exportado.
- Existe un plan plausible para cargar animación y audio.

### Revisar la estrategia

- No se puede acceder legalmente a recursos necesarios para estudiar el juego.
- Los formatos dominantes son desconocidos y no hay especificaciones ni herramientas aplicables.
- El sistema rítmico no puede sincronizarse con suficiente consistencia después de medir y corregir latencia.
- El binario depende de subsistemas cuya reproducción requiere recursos desproporcionados frente al objetivo.

Si sucede esto, no se debe fingir avance ni abandonar todo automáticamente: reducir el alcance a una demo técnica, un reproductor de canción con modelo animado o un port parcial, y documentar la limitación antes de seguir.

---

## 11. Estimación de escala

No existe base suficiente para fijar una fecha fiable de finalización antes de completar las primeras pruebas.

- **Primer mes:** entorno reproducible, inventario, prueba de importación y prototipo rítmico sintético.
- **Meses siguientes:** modelo animado, audio sincronizado, primer vertical slice con recursos reales y definición de los formatos pendientes.
- **Etapa de port completo:** dependerá de la cantidad de sistemas no documentados, la complejidad de recursos, las cinemáticas, las pantallas sociales y los requisitos de fidelidad. Puede convertirse en un proyecto de muchos meses o más; la estimación debe revisarse con evidencia real de los prototipos.

No estimar el proyecto como si fuera solo una traducción de C++ a C++. El trabajo incluye análisis, formatos de recursos, runtime, herramientas, gráficos, audio, UI, validación y mantenimiento.

---

## 12. Lista de verificación inicial

- [ ] Confirmé el nombre, versión y enlace oficial de PKG Studio.
- [ ] Identifiqué el Title ID y versión exactos de mi copia local.
- [ ] Preservé el original y calculé hashes de los archivos de trabajo.
- [ ] Creé el repositorio del port y excluí recursos propietarios de Git.
- [ ] Compilé una ventana nativa vacía.
- [ ] Abrí o convertí un modelo de prueba con una herramienta existente.
- [ ] Escribí una primera especificación de chart sintético.
- [ ] El sistema de ritmo pasa tests de timing independientes de FPS.
- [ ] El runtime propio dibuja un recurso real exportado.
- [ ] Documenté limitaciones, errores y la siguiente prueba de mayor valor.

---

## 13. Fuentes y referencias

- REA — Reverse Engineer Anything: https://github.com/morluto/rea
- GFD Studio: https://github.com/tge-was-taken/GFD-Studio
- GMDTool: https://github.com/lemoncove/GMDTool
- BlenderToolsForGFS: https://github.com/Pherakki/BlenderToolsForGFS
- PS4 PKG Tool (Windows): https://github.com/pearlxcore/PS4PKGTool
- Datos de referencia para el identificador CUSA-12636: https://serialstation.com/titles/CUSA/12636

Las capacidades y dependencias de los repositorios pueden cambiar. Antes de instalar, compilar o automatizar una herramienta, consultar su README, releases, licencia y requisitos actuales.
