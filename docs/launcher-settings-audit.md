# Auditoría del launcher — 19 de octubre de 2026

## Corrección posterior: velocidad original restaurada

El usuario confirmó que el desbloqueo aceleraba la intro. Se retiró el cambio global de reloj: `GOW3_VBLANK_HZ` queda en 60 y las solicitudes antiguas de 120/144/240 vuelven a 60 tanto en Python como en los ejecutables y el motor. `p3d.ini` se restauró a 60 FPS. El límite reducido de 30 sigue disponible.

La notificación de decodificación de vídeo existente solo sirve para reconocer imágenes de cinemáticas; no identifica de forma fiable el título o el inicio del gameplay. No se añadió una transición automática a FPS altos. Esa función requiere identificar la fase y desacoplar o corregir el paso de simulación para conservar el ritmo.

Verificación de la corrección: 15 pruebas del launcher y las comprobaciones C++ pasan. Una configuración antigua de 144 FPS se probó con el juego real: `VideoOut: vblank 60 Hz, frame limit 60 FPS`, con muestras de 60,7/60,0/60,0 FPS. Evidencia: `out/settings-audit/runtime-20261019-024322/results.json`. El estado de los 60 FPS se aplica ahora a toda la sesión, incluido gameplay; todavía no existe un desbloqueo exclusivo de partidas.

Las cifras y pruebas de FPS altos de las secciones siguientes son **históricas**, correspondientes a la versión experimental retirada; no describen una opción disponible ahora.

## Auditoría anterior, conservada como historial

El launcher, `Jugar.exe` y el motor compilado ya leen la misma configuración. Las pruebas comprobaron el archivo guardado, las opciones cargadas por el motor y las dimensiones reales de la ventana. No se modificaron las preferencias ni las partidas originales durante las pruebas de integración.

**Los FPS altos siguen siendo experimentales.** Se midieron aproximadamente 144 FPS y 240 FPS en secuencias de arranque, pero esto cambia la presentación/vblank; no implementa un parche del paso de simulación original. No se ha validado una canción completa ni que el ritmo, la velocidad y los juicios de notas permanezcan correctos por encima de 60 FPS. Las cifras de arranque tampoco garantizan ese rendimiento durante el baile.

## Discrepancias corregidas

| Ajuste o acción | Problema encontrado | Comportamiento actual |
|---|---|---|
| Límite de FPS | Guardaba 144 sin trasladarlo al reloj de vídeo; el parser tampoco aceptaba 144. | 30/60/120/144/240 se transmiten al limitador y a la frecuencia de vídeo. 120/144/240 están identificados como experimentales. |
| Ilimitado | No representaba una presentación realmente ilimitada. | Retirado del selector; las configuraciones antiguas sin límite se normalizan a 60 al guardar. |
| Resolución de salida | Se guardaba, pero la ventana arrancaba usando otras dimensiones. | La ventana usa el tamaño seleccionado. El backend acepta también 2560×1080 y 3440×1440. |
| Pantalla completa | Siempre pedía el modo del escritorio, aunque se hubiese elegido otro tamaño. | Busca un modo del monitor con las dimensiones solicitadas y refresco cercano al escritorio. Si no existe, registra que utiliza el escritorio. |
| Sin bordes | La resolución seleccionada parecía aplicarse aunque realmente se usaba el escritorio. | El selector de salida queda deshabilitado y explica que conserva la preferencia para ventana/pantalla completa. |
| Resolución interna | Las escalas 0,75×/1,25×/1,5×/2× no correspondían a un parche de Persona. | Campo deshabilitado y normalizado a nativa, 1920×1080. |
| FSR 1/3 | FSR 1 estaba desactivado por el motor; FSR 3 procedía de una integración no calibrada para Persona. | Reescalado deshabilitado; el motor de Persona aplica `upscaler=off` y `fsr1=0`. No se anuncian como funciones operativas. |
| RCAS | Compartía una opción con reescalado y el guardado podía volver a activarlo aunque se hubiese desactivado. | Es independiente. Solo queda habilitado para una salida de 1920×1080; el registro confirma si se aplicó al frame. |
| Anisotropía | El valor 0 se llamaba «Desactivado», aunque significa utilizar el comportamiento del juego. | Se llama «Predeterminado»; 2×/4×/8×/16× conservan su valor al guardar y arrancar. |
| VSync / Mostrar FPS | El área de clic y la propiedad de animación originales eran incorrectas. | Toda la superficie acepta clics; la animación mueve el deslizador. Se verificaron guardado y lectura por el motor. |
| Guardar | Sustituía el archivo completo, descartando ajustes del motor, y podía anunciar éxito tras un error de escritura. | Actualización atómica, conserva comentarios y opciones ajenas al formulario y muestra los errores. |
| Variables heredadas | Un entorno anterior podía imponerse a los valores del formulario. | El arranque fija los valores correspondientes a los ajustes seleccionados y elimina los overrides incompatibles. |
| JUGAR | Las notificaciones desde el hilo de ejecución podían dejar el botón bloqueado o impedir cerrar el launcher al iniciar. | Señales Qt en el hilo de interfaz; restaura el botón y comunica fallos. Comprueba los archivos necesarios antes de arrancar. |
| `Jugar.exe` | No reenviaba opciones adicionales junto a `--play`. | Admite opciones como `--user` conservando el arranque con la configuración guardada. |
| Motor sin launcher | Sin una variable de entorno, buscaba `gow3.ini` en vez del archivo de Persona. | Localiza `p3d.ini` junto al ejecutable o en su carpeta superior, incluso desde otro directorio de trabajo. |
| Registro | Mostraba datos de ejemplo si no había una ejecución. | Estado vacío real o las últimas 200 líneas del archivo existente. |
| Controles y capturas | Faltaban alias y cancelar en la guía; los errores al abrir la carpeta quedaban sin aviso. | Guía corregida y errores de carpeta comunicados en la interfaz. |

## Pruebas realizadas

- **14 pruebas automatizadas del launcher**, con interacciones Qt: selectores, interruptores, guardado, lectura, errores, pestañas, registro, capturas, falta de archivos, inicio y cierre. El test de persistencia recorre 120 combinaciones de modo, FPS, RCAS, VSync y contador.
- **34 casos contra el código C++ de configuración compilado**: todas las resoluciones y límites disponibles, modos, filtros, indicadores, compilación de shaders, cola de frames y fuerza de nitidez.
- Pruebas C++ existentes `graphics-settings-test` y `upscaler-support-test`: pasan. La segunda se adaptó a las funciones de entorno y temporales de Windows para poder ejecutarla en esta máquina.
- Búsqueda automática de `p3d.ini` desde un ejecutable situado en una subcarpeta, iniciado desde otro directorio: pasa.
- Pruebas con el juego real desde el botón JUGAR y desde `Jugar.exe`, con copias aisladas de los guardados. Comparación del archivo original y de las partidas antes/después: sin cambios.

| Prueba real | Salida confirmada | FPS observados en muestras de arranque |
|---|---|---|
| Ventana, límite 30, VSync desactivado | 1280×720 | 30,0 tras la primera muestra |
| Ventana, límite 144, VSync desactivado | 1920×1080 | 144,5; 140,0; 107,0 según la escena |
| Pantalla completa, límite 60, VSync activado | 1280×720 | 60,7; 60,0; 60,0 |
| Sin bordes, límite 60 | Escritorio: 1920×1080 | 60,7; 60,0; 59,0 |
| Ventana, límite 120 | 2560×1440 | 120,7 |
| Ventana, límite 240 | 3840×2160 | 239,8 |
| Ventana ultrapanorámica | 2560×1080 | 60,7 |
| Ventana ultrapanorámica | 3440×1440 | 60,7 |
| `Jugar.exe`, ajustes originales de 144 FPS | 1920×1080 | 138,8; 144,0 |

Las muestras de cinco segundos pueden quedar ligeramente por encima del límite por la contabilización del primer frame. Los registros también comprueban explícitamente VSync, RCAS aplicado/no aplicado, anisotropía, contador, shaders y frames en cola. La validación del filtro anisotrópico es de configuración/código; no es una comparación visual de cada textura.

## Evidencia y reproducción

- `python tools/test_p3d_launcher_settings.py`
- `python tools/audit_launcher_runtime.py`
- Matriz del backend: `out/settings-audit/backend-results.json`.
- Integración principal: `out/settings-audit/runtime-20261019-022738/results.json`.
- Resoluciones restantes: `out/settings-audit/runtime-20261019-023248/results.json`.
- 720p/30 y primera prueba de 144: registros en `out/settings-audit/runtime-20261019-022556/` y `docs/evidence/crash-20261017/fps*-audit-*`.
- Ruta de configuración sin launcher: `out/settings-audit/default-path-result.txt`.

Los binarios reconstruidos están en la raíz y en `bin/`. Las versiones anteriores se conservaron en `out/launcher-backups/` y `out/settings-audit/engine-backups/`.
