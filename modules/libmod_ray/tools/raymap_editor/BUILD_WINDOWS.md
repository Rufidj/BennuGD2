# Compilar para Windows desde Linux

Este documento explica cómo compilar `raymap_editor` para Windows desde tu máquina Linux usando MXE (M cross environment).

## ⚠️ Advertencia

La **primera vez** que ejecutes el script, tardará **varias horas** (2-4 horas) porque tiene que:
1. Descargar y compilar MXE
2. Compilar Qt5 completo para Windows
3. Compilar todas las dependencias

Las compilaciones posteriores serán **mucho más rápidas** (solo minutos).

## 📋 Requisitos

- **Espacio en disco**: ~10 GB libres
- **RAM**: Mínimo 4 GB (recomendado 8 GB)
- **Tiempo**: 2-4 horas la primera vez, ~5 minutos después

## 🚀 Uso

### Compilación simple

```bash
cd /home/ruben/BennuGD2/modules/libmod_ray/tools/raymap_editor
./build-windows.sh
```

El script hará todo automáticamente:
- ✅ Instalar MXE si no existe
- ✅ Compilar Qt6 para Windows
- ✅ Compilar raymap_editor
- ✅ Copiar todas las DLLs necesarias
- ✅ Crear un ZIP listo para distribuir

### Resultado

Encontrarás:
- **ZIP**: `raymap_editor-windows-x64.zip` - Listo para distribuir
- **Carpeta**: `raymap_editor-windows/` - Contenido del paquete

## 🧪 Probar en Linux con Wine

```bash
# Instalar Wine si no lo tienes
sudo apt install wine64

# Ejecutar el programa
cd raymap_editor-windows
wine raymap_editor.exe
```

## 📦 Contenido del paquete

El paquete incluye:
```
raymap_editor-windows/
├── raymap_editor.exe          # Ejecutable principal
├── Qt5Core.dll                # Librerías Qt5
├── Qt5Gui.dll
├── Qt5Widgets.dll
├── zlib1.dll                  # Dependencias
├── libgcc_s_seh-1.dll
├── libstdc++-6.dll
├── libwinpthread-1.dll
├── platforms/                 # Plugins Qt
│   └── qwindows.dll
├── styles/
│   └── qwindowsvistastyle.dll
├── README.md
└── USAGE.md
```

## 🔧 Solución de problemas

### Error: "No se encuentra el toolchain"

```bash
cd ~/mxe
make MXE_TARGETS='x86_64-w64-mingw32.shared' qt5 zlib
```

### Error: "Falta espacio en disco"

MXE necesita ~10 GB. Libera espacio y vuelve a intentar.

### La compilación falla

1. Asegúrate de tener todas las dependencias:
```bash
sudo apt-get install -y \
    autoconf automake autopoint bash bison bzip2 flex g++ \
    g++-multilib gettext git gperf intltool libc6-dev-i386 \
    libgdk-pixbuf2.0-dev libltdl-dev libssl-dev libtool-bin \
    libxml-parser-perl lzip make openssl p7zip-full patch perl \
    python3 python3-mako python3-pkg-resources ruby sed unzip \
    wget xz-utils
```

2. Borra MXE y vuelve a empezar:
```bash
rm -rf ~/mxe
./build-windows.sh
```

## 🆚 Comparación con GitHub Actions

| Característica | MXE Local | GitHub Actions |
|----------------|-----------|----------------|
| Primera compilación | 2-4 horas | 15-20 minutos |
| Compilaciones posteriores | ~5 minutos | 15-20 minutos |
| Espacio en disco | ~10 GB | 0 GB (en la nube) |
| Control total | ✅ Sí | ⚠️ Limitado |
| Requiere internet | Solo primera vez | Siempre |
| Gratis | ✅ Sí | ✅ Sí (repos públicos) |

## 💡 Recomendación

- **Usa MXE local** si necesitas compilar frecuentemente y tienes espacio en disco
- **Usa GitHub Actions** si solo compilas ocasionalmente o no quieres esperar la instalación inicial

## 📝 Notas

- MXE se instala en `~/mxe` (puedes cambiarlo editando el script)
- El script usa `x86_64-w64-mingw32.shared` (Windows 64-bit con DLLs compartidas)
- Todas las DLLs necesarias se copian automáticamente al paquete
