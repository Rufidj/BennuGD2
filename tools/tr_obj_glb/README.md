# TR OBJ to GLB

Convierte un `OBJ` exportado desde `fexinspect` a:

- un unico `GLB`
- varios `GLB` por sectores X/Z para streaming

Conserva materiales base y texturas definidas en el `MTL`.

## Uso

```bash
python3 tools/tr_obj_glb/tr_obj_to_glb.py nivel.obj --output /tmp/nivel.glb
```

```bash
python3 tools/tr_obj_glb/tr_obj_to_glb.py nivel.obj \
  --output-dir /tmp/nivel_sectores \
  --sector-size 2048
```

## Flujo recomendado con FexInspect

1. Abre el nivel `.TRC` o `.TR4`.
2. Exporta `level geometry` a `OBJ`.
3. Exporta las texturas.
4. Ejecuta este conversor sobre el `OBJ` generado.

## Salida por sectores

Cuando usas `--sector-size`, la herramienta genera:

- un `GLB` por sector
- `manifest.json` con bounds y nombre de archivo de cada sector

La asignacion de triangulos a sectores se hace por el centroide del triangulo en X/Z.
