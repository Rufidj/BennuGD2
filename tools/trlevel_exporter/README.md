# trlevel_exporter

Aplicacion nativa en C++ para exportar niveles a `GLB`.

Estado actual:

- entrada `OBJ/MTL`
- entrada nativa `TR1 PC` (`.PHD/.TUB`)
- entrada nativa `TR2 PC` (`.TR2`)
- salida `GLB` unico
- salida `GLB` por sectores X/Z para streaming
- texturas embebidas en `PNG` o `JPEG`

Pensado para usarlo con:

- `fexinspect` para `TR4/TR5`
- `TRMOD` como futura base para `TR1/TR2`

## Compilar

```bash
cmake -S . -B build/native -DBUILD_TARGET=tools
cmake --build build/native --target trlevel_exporter -j
```

## Uso

```bash
./build/bin/trlevel_exporter obj2glb nivel.obj --output /tmp/nivel.glb
```

```bash
./build/bin/trlevel_exporter obj2glb nivel.obj \
  --output-dir /tmp/nivel_sectores \
  --sector-size 2048
```

```bash
./build/bin/trlevel_exporter level2glb LEVEL1.PHD --output /tmp/level1.glb
```

```bash
./build/bin/trlevel_exporter level2glb WALL.TR2 \
  --output-dir /tmp/wall_sectores \
  --sector-size 2048
```

## Salida por sectores

Con `--sector-size` genera:

- un `GLB` por sector
- `manifest.json` con bounds y archivo de cada sector
