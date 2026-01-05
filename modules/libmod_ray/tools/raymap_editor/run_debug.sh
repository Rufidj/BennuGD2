#!/bin/bash

# Script para ejecutar el editor con debug output

cd /home/ruben/BennuGD2/modules/libmod_ray/tools/raymap_editor

# Si existe el ejecutable en build
if [ -f "build/Desktop-Debug/raymap_editor" ]; then
    echo "Ejecutando desde build/Desktop-Debug..."
    ./build/Desktop-Debug/raymap_editor 2>&1 | tee editor_debug.log
elif [ -f "raymap_editor" ]; then
    echo "Ejecutando desde directorio actual..."
    ./raymap_editor 2>&1 | tee editor_debug.log
else
    echo "Error: No se encontró el ejecutable raymap_editor"
    echo "Compila primero el proyecto en Qt Creator"
    exit 1
fi
