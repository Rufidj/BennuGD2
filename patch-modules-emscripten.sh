#!/bin/bash

# Script simple para parchear CMakeLists.txt de módulos para Emscripten
# Envuelve find_package(SDL2*) en if(NOT EMSCRIPTEN)...endif()

echo "Parcheando módulos para Emscripten..."

# Encontrar todos los CMakeLists.txt en modules (excepto backups)
find modules -name "CMakeLists.txt" -not -path "*/backup/*" | while read -r file; do
    # Verificar si ya está parcheado
    if grep -q "if(NOT EMSCRIPTEN)" "$file" 2>/dev/null; then
        echo "✓ $(dirname $file) ya parcheado"
        continue
    fi
    
    # Verificar si tiene find_package(SDL2
    if ! grep -q "find_package(SDL2" "$file" 2>/dev/null; then
        continue
    fi
    
    echo "→ Parcheando $(dirname $file)..."
    
    # Crear backup
    cp "$file" "$file.emscripten_backup"
    
    # Usar awk para un parche más preciso
    awk '
    BEGIN { in_sdl_block = 0; sdl_lines = "" }
    
    # Detectar inicio de bloque SDL
    /^find_package\(SDL2/ {
        if (!in_sdl_block) {
            in_sdl_block = 1
            print "if(NOT EMSCRIPTEN)"
        }
        print "    " $0
        sdl_lines = sdl_lines $0 "\n"
        next
    }
    
    # Continuar bloque SDL si es SDL2_mixer o SDL2_image
    in_sdl_block && /^find_package\(SDL2_/ {
        print "    " $0
        sdl_lines = sdl_lines $0 "\n"
        next
    }
    
    # Continuar bloque SDL si es if(USE_SDL2_GPU)
    in_sdl_block && /^if\(USE_SDL2_GPU\)/ {
        print "    " $0
        next
    }
    
    # Continuar bloque SDL si es find_package(SDL_GPU
    in_sdl_block && /find_package\(SDL_GPU/ {
        print "    " $0
        next
    }
    
    # Continuar bloque SDL si es endif() de SDL_GPU
    in_sdl_block && /^endif\(\)/ && sdl_lines ~ /SDL_GPU/ {
        print "    " $0
        sdl_lines = ""
        next
    }
    
    # Fin del bloque SDL
    in_sdl_block && !/^find_package\(SDL/ && !/^if\(USE_SDL2_GPU\)/ && !/^endif\(\)/ && NF > 0 {
        print "endif()"
        print ""
        in_sdl_block = 0
        sdl_lines = ""
    }
    
    # Línea normal
    { print }
    ' "$file.emscripten_backup" > "$file"
    
    echo "✓ $(dirname $file) parcheado"
done

echo ""
echo "✅ Parche completado!"
echo ""
echo "Los backups están en: *.emscripten_backup"
echo "Para revertir: find modules -name '*.emscripten_backup' -exec sh -c 'mv \"\$1\" \"\${1%.emscripten_backup}\"' _ {} \\;"
