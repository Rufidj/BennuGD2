#!/bin/bash

# Script de prueba para compilar BennuGD2 con Emscripten
# Este script verifica que todo esté configurado correctamente

set -e

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  BennuGD2 - Test de Compilación para Emscripten       ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"
echo ""

# 1. Verificar que Emscripten esté instalado
echo -e "${YELLOW}[1/5]${NC} Verificando Emscripten..."
if ! command -v emcc &> /dev/null; then
    echo -e "${RED}✗ Error: emcc no encontrado${NC}"
    echo ""
    echo "Emscripten no está activado. Por favor ejecuta:"
    echo -e "${GREEN}source /ruta/a/emsdk/emsdk_env.sh${NC}"
    echo ""
    echo "Si no tienes Emscripten instalado:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk"
    echo "  ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

EMCC_VERSION=$(emcc --version | head -n1)
echo -e "${GREEN}✓${NC} Emscripten encontrado: $EMCC_VERSION"

# 2. Verificar variable EMSCRIPTEN
echo -e "${YELLOW}[2/5]${NC} Verificando variable \$EMSCRIPTEN..."
if [ -z "$EMSCRIPTEN" ]; then
    echo -e "${YELLOW}⚠${NC} Variable \$EMSCRIPTEN no está definida"
    echo "  Intentando detectar automáticamente..."
    export EMSCRIPTEN=$(dirname $(which emcc))
    echo -e "${GREEN}✓${NC} \$EMSCRIPTEN establecido a: $EMSCRIPTEN"
else
    echo -e "${GREEN}✓${NC} \$EMSCRIPTEN = $EMSCRIPTEN"
fi

# 3. Limpiar build anterior
echo -e "${YELLOW}[3/5]${NC} Limpiando build anterior..."
if [ -d "build/emscripten" ]; then
    rm -rf build/emscripten
    echo -e "${GREEN}✓${NC} Build anterior eliminado"
else
    echo -e "${GREEN}✓${NC} No hay build anterior"
fi

# 4. Compilar BennuGD2
echo -e "${YELLOW}[4/5]${NC} Compilando BennuGD2 para Emscripten..."
echo ""
./build.sh emscripten

# 5. Verificar resultado
echo ""
echo -e "${YELLOW}[5/5]${NC} Verificando archivos generados..."

if [ -f "build/emscripten/bin/bgdi.html" ]; then
    echo -e "${GREEN}✓${NC} bgdi.html generado"
else
    echo -e "${RED}✗${NC} bgdi.html NO encontrado"
fi

if [ -f "build/emscripten/bin/bgdi.js" ]; then
    echo -e "${GREEN}✓${NC} bgdi.js generado"
else
    echo -e "${RED}✗${NC} bgdi.js NO encontrado"
fi

if [ -f "build/emscripten/bin/bgdi.wasm" ]; then
    echo -e "${GREEN}✓${NC} bgdi.wasm generado"
    WASM_SIZE=$(du -h build/emscripten/bin/bgdi.wasm | cut -f1)
    echo -e "  Tamaño: $WASM_SIZE"
else
    echo -e "${RED}✗${NC} bgdi.wasm NO encontrado"
fi

echo ""
echo -e "${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  Compilación completada exitosamente                  ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${GREEN}Próximos pasos:${NC}"
echo ""
echo "1. Compila un juego BennuGD (.prg) a .dcb:"
echo -e "   ${BLUE}build/linux-gnu/bin/bgdc examples/01-helloworld/01-helloworld.prg${NC}"
echo ""
echo "2. Empaqueta el juego para web:"
echo -e "   ${BLUE}./package-web-game.sh 01-helloworld.dcb helloworld${NC}"
echo ""
echo "3. Ejecuta un servidor HTTP:"
echo -e "   ${BLUE}cd build/emscripten/games/helloworld${NC}"
echo -e "   ${BLUE}python3 -m http.server 8000${NC}"
echo ""
echo "4. Abre en tu navegador:"
echo -e "   ${BLUE}http://localhost:8000/helloworld.html${NC}"
echo ""
