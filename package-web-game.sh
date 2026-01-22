#!/bin/bash

# Configuración por defecto
LIBS_DIR="build/emscripten/bin"
BGDI_BC="build/emscripten/core/bgdi/CMakeFiles/bgdi.dir/main.c.o"
OUTPUT_DIR="web-release"

# Función para mostrar menú de selección de juegos
select_game() {
    echo "=============================================="
    echo "      BennuGD2 Web Packager - Menu"
    echo "=============================================="
    
    OPTIONS=()
    
    # Buscar carpetas en 'games' si existe
    if [ -d "games" ]; then
        for d in games/*; do
            if [ -d "$d" ]; then
                OPTIONS+=("$d")
            fi
        done
    fi
    # Buscar carpetas en el directorio actual que parezcan juegos (tengan .dcb o .fpg)
    # (Opcional, puede ensuciar el menú)
    
    OPTIONS+=("Manual Entry (Escribir ruta)")
    OPTIONS+=("Salir")
    
    PS3="Selecciona un juego (número): "
    
    select opt in "${OPTIONS[@]}"; do
        if [[ "$opt" == "Salir" ]]; then
            echo "Adios."
            exit 0
        elif [[ "$opt" == "Manual Entry (Escribir ruta)" ]]; then
            read -p "Introduce la ruta absoluta o relativa del juego: " GAME_DIR
            break
        elif [[ -n "$opt" ]]; then
            GAME_DIR="$opt"
            break
        else
            echo "Opción inválida."
        fi
    done
}

# Lógica principal

if [ -z "$1" ]; then
    # Modo Interactivo
    select_game
    
    # Preguntar por directorio de salida
    read -p "Directorio de salida [default: web-release]: " OUT_INPUT
    if [ -n "$OUT_INPUT" ]; then
        OUTPUT_DIR="$OUT_INPUT"
        # Si el usuario escribe una ruta relativa, asegurar que sea relativa a donde estamos
        # Pero mkdir -p maneja eso.
    fi
else
    # Modo Argumentos (Batch)
    GAME_DIR="$1"
    if [ -n "$2" ]; then
        OUTPUT_DIR="$2"
    fi
fi

# Validar GAME_DIR
if [ -z "$GAME_DIR" ]; then
    echo "Error: No se ha especificado un directorio de juego."
    exit 1
fi

if [ ! -d "$GAME_DIR" ]; then
    echo "Error: El directorio '$GAME_DIR' no existe."
    exit 1
fi

echo ">> Empaquetando juego desde: $GAME_DIR"
echo ">> Destino: $OUTPUT_DIR"
echo "----------------------------------------------"

# Crear directorio de salida
mkdir -p "$OUTPUT_DIR"

# Directorio temporal para preparar el paquete
PKG_DIR="web-pkg-temp"
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR"

echo "Preparando archivos en $PKG_DIR..."
# Copiar todo el contenido del juego al directorio temporal
cp -r "$GAME_DIR"/* "$PKG_DIR"/

# Verificar si existe game.dcb, si no, buscar otro .dcb y renombrarlo
if [ ! -f "$PKG_DIR/game.dcb" ]; then
    # Activar nullglob para expandir correctamente
    shopt -s nullglob
    DCB_FILES=("$PKG_DIR"/*.dcb)
    shopt -u nullglob
    
    NUM_DCB=${#DCB_FILES[@]}
    
    if [ $NUM_DCB -eq 1 ]; then
        # Solo hay un .dcb, así que asumimos que es el principal
        echo "Renombrando $(basename "${DCB_FILES[0]}") a game.dcb para compatibilidad con Emscripten..."
        mv "${DCB_FILES[0]}" "$PKG_DIR/game.dcb"
    elif [ $NUM_DCB -gt 1 ]; then
      # Múltiples dcbs, intentamos buscar 'main.dcb' específicamente
      if [ -f "$PKG_DIR/main.dcb" ]; then
          echo "Renombrando main.dcb a game.dcb..."
          mv "$PKG_DIR/main.dcb" "$PKG_DIR/game.dcb"
      else
          echo "ADVERTENCIA: Hay múltiples archivos .dcb y ninguno se llama game.dcb o main.dcb."
          echo "El runtime web busca 'game.dcb'. Puede que el juego no arranque automáticamente."
          # No fallamos, quizás el usuario sabe lo que hace
      fi
    elif [ $NUM_DCB -eq 0 ]; then
        echo "ADVERTENCIA: No se encontró ningún archivo .dcb en $GAME_DIR."
    fi
fi

echo "Compilando webassembly..."

# Obtener lista de librerías estáticas generadas
LIBS="-L${LIBS_DIR} -lbgdrtm -lbggfx -lbginput -lbgload -lbgsound -lmod_ads -lmod_debug -lmod_gfx -lmod_iap -lmod_input -lmod_misc -lmod_net -lmod_ray -lmod_sound -lsdlhandler -lz"

# Flags de Emscripten usando array para evitar problemas de comillas
EM_FLAGS=(
    "-s" "USE_SDL=2"
    "-s" "USE_SDL_IMAGE=2"
    "-s" "USE_SDL_MIXER=2"
    "-s" "SDL2_IMAGE_FORMATS=[\"bmp\",\"png\",\"jpg\"]"
    "-s" "USE_OGG=1"
    "-s" "USE_VORBIS=1"
    "-s" "USE_ZLIB=1"
    "-s" "USE_LIBPNG=1"
    "-s" "ALLOW_MEMORY_GROWTH=1"
    "-s" "INITIAL_MEMORY=67108864"
    "-s" "EXIT_RUNTIME=1"
    "-s" "ASYNCIFY"
    "-s" "EXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
    "-O3"
)
# Nota: he añadido -O3 para release

echo "Generando index.html..."

emcc "$BGDI_BC" $LIBS "${EM_FLAGS[@]}" \
    --preload-file "$PKG_DIR"@/ \
    -o "$OUTPUT_DIR/index.html"

if [ $? -eq 0 ]; then
    echo "--------------------------------------------------------"
    echo "¡Juego empaquetado con éxito!"
    echo "Carpeta: $OUTPUT_DIR"
    echo "--------------------------------------------------------"
    echo "Para probarlo:"
    echo "1. cd $OUTPUT_DIR"
    echo "2. python3 -m http.server"
    echo "3. http://localhost:8000"
    
    # Limpiar directorio temporal
    rm -rf "$PKG_DIR"
else
    echo "Error al empaquetar."
    exit 1
fi
