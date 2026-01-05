#!/bin/bash
# Script para verificar el contenido de un archivo .raymap

if [ -z "$1" ]; then
    echo "Uso: $0 <archivo.raymap>"
    exit 1
fi

echo "Analizando mapa: $1"
echo "================================"

# Usar hexdump para ver los primeros bytes del header
echo "Header (primeros 64 bytes):"
hexdump -C "$1" | head -n 4

# Calcular el tamaño del archivo
SIZE=$(stat -f%z "$1" 2>/dev/null || stat -c%s "$1" 2>/dev/null)
echo ""
echo "Tamaño del archivo: $SIZE bytes"

echo ""
echo "Para ver el contenido completo en hexadecimal:"
echo "  hexdump -C $1 | less"
