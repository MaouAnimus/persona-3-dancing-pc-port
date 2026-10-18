# Validación Técnica de PKG Studio

> [!NOTE]
> Este documento cumple el **Paso 2** de la Hoja de Ruta Maestra v2.1: evaluar las capacidades reales de la herramienta `PKG-Studio.exe` presente en el espacio de trabajo.

---

## 1. Identificación del Archivo

- **Archivo:** `PKG-Studio.exe` (raíz del espacio de trabajo)
- **Tamaño:** `5,926,400` bytes (5.65 MB)
- **SHA-256:** `0359e36e503122a8808a14bb140e6b0fb639951ea7d942b7dc73c05d48896ac0`
- **Tipo de archivo:** Windows PE32+ (x64) ejecutable (.NET / WPF GUI application).

---

## 2. Análisis Funcional

1. **Propósito real de la herramienta:**
   - PKG Studio es una utilidad para la inspección visual, personalización de metadatos (`param.sfo`), modificación de iconos (`icon0.png`), imágenes de fondo (`pic0.png`, `pic1.png`) y reempaquetado de paquetes PKG de PS4.
2. **Aclaración sobre la conversión "PKG a BIN":**
   - La herramienta **no** realiza una traducción binaria mágica de un juego completo de PS4 a un binario ejecutable de Windows.
   - La referencia comunitaria de "convertir a BIN" hace alusión a extraer el archivo ejecutable interno `eboot.bin` contenido dentro del paquete.
   - En nuestro entorno, el ejecutable `eboot.bin` ya ha sido extraído y desempacado a `eboot.elf` en `CUSA12636/eboot.elf`.
3. **Conclusión y estado:**
   - **PASS**: El origen del ejecutable está clarificado. No es necesario utilizar `PKG-Studio.exe` para continuar el desarrollo del port nativo, puesto que todos los contenedores (`data.cpk`, `ps4.cpk`, `eboot.elf`, `param.sfo`) ya están disponibles y desempacados.
