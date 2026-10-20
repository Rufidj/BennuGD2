# Recovery Studio Qt / C++

Base nativa en `C++/Qt Widgets` para construir la herramienta propia de recuperacion a partir del prototipo Python.

Estado actual:

- Ventana principal nativa
- Tema oscuro
- Arbol de recuperacion con checkboxes e iconos
- Panel derecho con vista previa, resumen y detalles
- Acciones base en barra superior y menus
- Modelo C++ separado de la UI para seguir portando el motor
- Importacion de sesiones JSON y logs heredados

Compilacion:

```bash
cd /home/ruben/BennuGD2/tools/dmde_log_viewer_cpp
cmake -S . -B build
cmake --build build -j
./build/recovery_studio_qt
```

Siguiente objetivo recomendado:

1. Mejorar la importacion de sesiones y logs heredados
2. Portar el escaneo NTFS y la recuperacion por `runlist`
3. Sustituir progresivamente los importadores heredados por motor nativo completo

