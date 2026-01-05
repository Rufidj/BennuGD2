# Abrir Proyecto en Qt Creator

## Opción 1: Usar archivo .pro (RECOMENDADO para Qt Creator)

El archivo `.pro` es el formato nativo de Qt Creator y organiza mejor los archivos.

### Pasos:

1. **Abrir Qt Creator**

2. **File → Open File or Project...**

3. **Navegar a:**
   ```
   /home/ruben/BennuGD2/modules/libmod_ray/tools/raymap_editor/
   ```

4. **Seleccionar:** `raymap_editor.pro` (NO el CMakeLists.txt)

5. **Click "Open"**

6. **Seleccionar Kit:** Desktop Qt 5.x.x GCC

7. **Click "Configure Project"**

### Resultado Esperado:

Deberías ver la estructura organizada así:

```
raymap_editor
├── Headers
│   ├── mainwindow.h
│   ├── fpgloader.h
│   ├── grideditor.h
│   ├── texturepalette.h
│   ├── spriteeditor.h
│   ├── cameramarker.h
│   ├── raymapformat.h
│   └── mapdata.h
├── Sources
│   ├── main.cpp
│   ├── mainwindow.cpp
│   ├── fpgloader.cpp
│   ├── grideditor.cpp
│   ├── texturepalette.cpp
│   ├── spriteeditor.cpp
│   ├── cameramarker.cpp
│   └── raymapformat.cpp
├── Forms
│   └── mainwindow.ui
└── raymap_editor.pro
```

---

## Opción 2: Usar CMakeLists.txt

Si prefieres CMake (para integración con otros sistemas):

1. **File → Open File or Project...**
2. Seleccionar `CMakeLists.txt`
3. Configurar proyecto

**Nota:** CMake puede no organizar los archivos en carpetas visuales en Qt Creator, pero funciona igual para compilar.

---

## Compilar

Una vez abierto con `.pro`:

1. **Click en el martillo** (Build) o `Ctrl+B`
2. **Click en el triángulo verde** (Run) o `Ctrl+R`

---

## Solución de Problemas

### "No se encuentra qmake"

```bash
sudo apt install qt5-qmake qtchooser
```

### "Kit no configurado"

1. **Tools → Options → Kits**
2. Verificar que existe un kit con Qt 5
3. Si no, **Add** y configurar:
   - Compiler: GCC
   - Qt version: Qt 5.x.x

### Archivos no organizados en carpetas

- Asegúrate de abrir el archivo `.pro`, NO el `CMakeLists.txt`
- Cierra y vuelve a abrir el proyecto
- Borra el archivo `.pro.user` y vuelve a abrir

---

## Diferencias .pro vs CMakeLists.txt

| Característica | .pro | CMakeLists.txt |
|----------------|------|----------------|
| Organización visual | ✅ Excelente | ⚠️ Limitada |
| Nativo Qt Creator | ✅ Sí | ❌ No |
| Multiplataforma | ✅ Sí | ✅ Sí |
| Integración CI/CD | ⚠️ Limitada | ✅ Excelente |

**Recomendación:** Usa `.pro` para desarrollo en Qt Creator, mantén `CMakeLists.txt` para compilación en servidores.
