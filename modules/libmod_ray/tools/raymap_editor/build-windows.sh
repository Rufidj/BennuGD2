#!/bin/bash
# Script para compilar raymap_editor para Windows desde Linux usando MXE
# Autor: Generado automáticamente
# Fecha: 2025-12-23

set -e  # Salir si hay algún error

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Compilador Windows desde Linux (MXE)${NC}"
echo -e "${GREEN}========================================${NC}"

# Directorio de trabajo
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MXE_DIR="$HOME/mxe"
BUILD_DIR="$SCRIPT_DIR/build-windows"
PACKAGE_DIR="$SCRIPT_DIR/raymap_editor-windows"

# Verificar si MXE está instalado
if [ ! -d "$MXE_DIR" ]; then
    echo -e "${YELLOW}MXE no está instalado. Instalando...${NC}"
    echo -e "${YELLOW}Esto puede tardar varias horas la primera vez.${NC}"
    read -p "¿Continuar? (s/n): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Ss]$ ]]; then
        echo -e "${RED}Instalación cancelada.${NC}"
        exit 1
    fi
    
    # Clonar MXE
    cd ~
    git clone https://github.com/mxe/mxe.git
    cd mxe
    
    # Instalar dependencias de MXE
    echo -e "${YELLOW}Instalando dependencias del sistema...${NC}"
    sudo apt-get update
    sudo apt-get install -y \
        autoconf automake autopoint bash bison bzip2 flex g++ \
        g++-multilib gettext git gperf intltool libc6-dev-i386 \
        libgdk-pixbuf2.0-dev libltdl-dev libssl-dev libtool-bin \
        libxml-parser-perl lzip make openssl p7zip-full patch perl \
        python3 python3-mako python3-pkg-resources ruby sed unzip \
        wget xz-utils
    
    # Compilar Qt5 y zlib para Windows
    echo -e "${YELLOW}Compilando Qt5 para Windows (esto tardará MUCHO tiempo)...${NC}"
    make MXE_TARGETS='x86_64-w64-mingw32.shared' qt5 zlib
    
    echo -e "${GREEN}MXE instalado correctamente en $MXE_DIR${NC}"
fi

# Configurar variables de entorno para MXE
export PATH="$MXE_DIR/usr/bin:$PATH"
MXE_TARGET="x86_64-w64-mingw32.shared"
CMAKE_TOOLCHAIN="$MXE_DIR/usr/$MXE_TARGET/share/cmake/mxe-conf.cmake"

# Verificar que el toolchain existe
if [ ! -f "$CMAKE_TOOLCHAIN" ]; then
    echo -e "${RED}Error: No se encuentra el toolchain de CMake de MXE${NC}"
    echo -e "${YELLOW}Intenta compilar Qt6 manualmente:${NC}"
    echo "cd $MXE_DIR && make MXE_TARGETS='$MXE_TARGET' qtbase qttools zlib"
    exit 1
fi

# Limpiar build anterior
echo -e "${YELLOW}Limpiando builds anteriores...${NC}"
rm -rf "$BUILD_DIR"
rm -rf "$PACKAGE_DIR"

# Crear directorio de build
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configurar con CMake
echo -e "${GREEN}Configurando proyecto con CMake...${NC}"
$MXE_TARGET-cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN" \
    ..

# Compilar
echo -e "${GREEN}Compilando...${NC}"
make -j$(nproc)

# Crear directorio de paquete
echo -e "${GREEN}Empaquetando...${NC}"
mkdir -p "$PACKAGE_DIR"

# Copiar ejecutable
cp raymap_editor.exe "$PACKAGE_DIR/"

# Copiar DLLs de Qt y dependencias
echo -e "${YELLOW}Copiando DLLs necesarias...${NC}"

# DLLs de Qt5
QT_BIN_DIR="$MXE_DIR/usr/$MXE_TARGET/qt5/bin"
QT_PLUGINS_DIR="$MXE_DIR/usr/$MXE_TARGET/qt5/plugins"

cp "$QT_BIN_DIR/Qt5Core.dll" "$PACKAGE_DIR/" || true
cp "$QT_BIN_DIR/Qt5Gui.dll" "$PACKAGE_DIR/" || true
cp "$QT_BIN_DIR/Qt5Widgets.dll" "$PACKAGE_DIR/" || true

# DLLs del sistema
MINGW_BIN_DIR="$MXE_DIR/usr/$MXE_TARGET/bin"
cp "$MINGW_BIN_DIR/zlib1.dll" "$PACKAGE_DIR/" || true
cp "$MINGW_BIN_DIR/libgcc_s_seh-1.dll" "$PACKAGE_DIR/" || true
cp "$MINGW_BIN_DIR/libstdc++-6.dll" "$PACKAGE_DIR/" || true
cp "$MINGW_BIN_DIR/libwinpthread-1.dll" "$PACKAGE_DIR/" || true

# Copiar plugins de Qt
mkdir -p "$PACKAGE_DIR/platforms"
cp "$QT_PLUGINS_DIR/platforms/qwindows.dll" "$PACKAGE_DIR/platforms/" || true

mkdir -p "$PACKAGE_DIR/styles"
cp "$QT_PLUGINS_DIR/styles/qwindowsvistastyle.dll" "$PACKAGE_DIR/styles/" || true

# Copiar documentación
cp "$SCRIPT_DIR/README.md" "$PACKAGE_DIR/" 2>/dev/null || true
cp "$SCRIPT_DIR/USAGE.md" "$PACKAGE_DIR/" 2>/dev/null || true

# Crear archivo ZIP
echo -e "${GREEN}Creando archivo ZIP...${NC}"
cd "$SCRIPT_DIR"
zip -r raymap_editor-windows-x64.zip raymap_editor-windows/

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  ¡Compilación completada!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "Paquete creado en: ${YELLOW}$SCRIPT_DIR/raymap_editor-windows-x64.zip${NC}"
echo -e "Carpeta: ${YELLOW}$PACKAGE_DIR${NC}"
echo ""
echo -e "${YELLOW}Puedes probar el ejecutable en Wine:${NC}"
echo -e "wine $PACKAGE_DIR/raymap_editor.exe"
