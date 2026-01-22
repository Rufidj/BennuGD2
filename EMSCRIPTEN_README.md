# BennuGD2 - Compilación para Emscripten/WebAssembly

Este documento explica cómo compilar BennuGD2 para ejecutarse en navegadores web usando Emscripten.

## Requisitos Previos

1. **Emscripten SDK instalado y activado**
   ```bash
   # Si no tienes Emscripten instalado:
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ./emsdk install latest
   ./emsdk activate latest
   source ./emsdk_env.sh
   ```

2. **CMake** (versión 3.5 o superior)

3. **Make** o **Ninja**

## Compilación

### Paso 1: Activar Emscripten

Antes de compilar, asegúrate de que Emscripten esté activado en tu terminal:

```bash
source /ruta/a/emsdk/emsdk_env.sh
```

### Paso 2: Compilar BennuGD2 para Emscripten

Desde el directorio raíz de BennuGD2:

```bash
./build.sh emscripten
```

Esto compilará todo el proyecto, incluyendo el intérprete `bgdi` como WebAssembly.

### Paso 3: Preparar tu juego

1. Compila tu juego BennuGD (.prg) a formato DCB usando el compilador bgdc:
   ```bash
   bgdc tu_juego.prg
   ```

2. Renombra el archivo compilado a `game.dcb`:
   ```bash
   mv tu_juego.dcb game.dcb
   ```

3. Copia `game.dcb` al directorio de salida de Emscripten:
   ```bash
   cp game.dcb build/emscripten/bin/
   ```

### Paso 4: Empaquetar el juego con el intérprete

Necesitas volver a compilar el intérprete para incluir tu `game.dcb`:

```bash
cd build/emscripten/core/bgdi
emcc -o bgdi.html \
    CMakeFiles/bgdi.dir/main.c.o \
    -L../../../../bin \
    -lbgdrtm \
    [... otras librerías ...] \
    --preload-file ../../../../build/emscripten/bin/game.dcb@game.dcb \
    --shell-file ../../../../core/bgdi/shell.html \
    -s WASM=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s USE_SDL=2 \
    -s USE_SDL_IMAGE=2 \
    -s USE_SDL_MIXER=2 \
    -s ASYNCIFY=1
```

O más simple, modifica el CMakeLists.txt para descomentar la línea de `--preload-file` y vuelve a compilar:

```bash
cd build/emscripten
make
```

## Ejecutar el juego

### Opción 1: Servidor HTTP local con Python

```bash
cd build/emscripten/bin
python3 -m http.server 8000
```

Luego abre tu navegador en: `http://localhost:8000/bgdi.html`

### Opción 2: Servidor HTTP local con Node.js

```bash
cd build/emscripten/bin
npx http-server -p 8000
```

Luego abre tu navegador en: `http://localhost:8000/bgdi.html`

### Opción 3: Emrun (incluido con Emscripten)

```bash
cd build/emscripten/bin
emrun --browser firefox bgdi.html
```

## Archivos Generados

Después de la compilación, encontrarás en `build/emscripten/bin/`:

- **bgdi.html** - Página web principal (usa el template de shell.html)
- **bgdi.js** - Código JavaScript que carga el WebAssembly
- **bgdi.wasm** - El binario WebAssembly compilado
- **bgdi.data** - Archivo de datos empaquetado (si usaste --preload-file)

## Solución de Problemas

### Error: "emcc not found"

Asegúrate de haber activado Emscripten:
```bash
source /ruta/a/emsdk/emsdk_env.sh
```

### El juego no carga

1. Verifica que estés usando un servidor HTTP (no file://)
2. Abre la consola del navegador (F12) para ver errores
3. Asegúrate de que `game.dcb` esté correctamente empaquetado

### Problemas de memoria

Si el juego se queda sin memoria, aumenta `TOTAL_MEMORY` en el CMakeLists.txt:
```cmake
set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -s TOTAL_MEMORY=134217728")
```

### El juego va lento

1. Compila en modo Release (por defecto)
2. Considera deshabilitar ASSERTIONS en producción
3. Optimiza tu código BennuGD

## Personalización

### Modificar la interfaz web

Edita `core/bgdi/shell.html` para personalizar:
- Estilos CSS
- Mensajes de carga
- Controles de usuario
- Información del juego

### Flags de compilación adicionales

Puedes añadir flags adicionales en `core/bgdi/CMakeLists.txt` en la sección de Emscripten.

## Limitaciones Conocidas

1. **Sistema de archivos**: El acceso a archivos está limitado al sistema de archivos virtual de Emscripten
2. **Rendimiento**: Puede ser más lento que la versión nativa
3. **Navegadores**: Requiere un navegador moderno con soporte para WebAssembly
4. **Audio**: Puede haber latencia en algunos navegadores

## Recursos Adicionales

- [Documentación de Emscripten](https://emscripten.org/docs/)
- [BennuGD2 en GitHub](https://github.com/SplinterGU/BennuGD2)
- [WebAssembly](https://webassembly.org/)

## Licencia

BennuGD2 mantiene su licencia original. Consulta LICENSE.md en el directorio raíz.
