# GLB to MD3 Conversion

La herramienta ahora soporta archivos GLB (glTF Binary).

## Instalación de dependencias

```bash
pip install pygltflib numpy
```

## Uso con GLB

```bash
python tools/obj_to_md3.py modelo.glb salida.md3 10.0
```

## Características GLB

- ✅ Lee geometría (vértices, triángulos)
- ✅ Lee coordenadas UV
- ✅ Extrae materiales y colores base
- ✅ Genera atlas de textura UV-aware
- ✅ Convierte ejes automáticamente

## Notas

- GLB es un formato binario más eficiente que OBJ
- Soporta materiales PBR (usa baseColorFactor)
- Las animaciones se ignoran (MD3 es estático)
- Si el GLB tiene texturas embebidas, se extraerán automáticamente
