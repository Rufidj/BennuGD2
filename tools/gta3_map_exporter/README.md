# Exportador de mapas GTA a glTF / GLB

Esta herramienta sigue siendo una sola GUI y un solo script principal. Por dentro ya puede enrutar por juego con `--game`. A día de hoy el convertidor completo implementado es el de GTA 3: lee una instalación local, parsea `gta3.dat`, `IDE` e `IPL`, abre los modelos desde `models/gta3.img` y genera una escena completa en formato `glTF` o `GLB`.

Estado actual:

- `gta3`: implementado
- `gta4`: parser interno + exportación preview de zonas WPL

## Qué hace

- Reconstruye las instancias del mapa leyendo los `IPL`
- Respeta posición, rotación y escala
- Convierte mallas `DFF` estáticas a `glTF 2.0`
- Conserva múltiples materiales por modelo cuando el `DFF` los trae
- Si le pasas una carpeta con `PNG` extraídos desde `TXD`, intenta asignar una textura real por modelo
- Puede exportar recursos sueltos a `GLB` por categorías: vehículos, peds, armas y props
- Puede copiar `anim/ped.ifp` como animación cruda aparte
- Genera:
  - `gta3_map.gltf`
  - `gta3_map.bin`
  - o `gta3_map.glb`
  - `export_summary.json`

## Limitaciones actuales

- No exporta texturas `TXD` todavía
- Omite modelos que no se puedan convertir
- Por defecto excluye entradas `LOD*` para evitar duplicados visuales

## Uso

```bash
python3 tools/gta3_map_exporter/export_gta3_map.py \
  --game gta3 \
  --game-root "/home/ruben/.wine/drive_c/Program Files (x86)/Grand Theft Auto III" \
  --output-dir "/home/ruben/BennuGD2/tmp/gta3_export" \
  --extracted-root "/home/ruben/Descargas/GTA3/ARCHIVOS"
```

Para `GLB` único:

```bash
python3 tools/gta3_map_exporter/export_gta3_map.py \
  --game gta3 \
  --game-root "/home/ruben/.wine/drive_c/Program Files (x86)/Grand Theft Auto III" \
  --output-dir "/home/ruben/BennuGD2/tmp/gta3_glb" \
  --extracted-root "/home/ruben/Descargas/GTA3/ARCHIVOS" \
  --format glb
```

Para exportar por sectores con nombres compatibles con el motor:

```bash
python3 tools/gta3_map_exporter/export_gta3_map.py \
  --game gta3 \
  --game-root "/home/ruben/.wine/drive_c/Program Files (x86)/Grand Theft Auto III" \
  --output-dir "/home/ruben/BennuGD2/tmp/gta3_sectorized" \
  --extracted-root "/home/ruben/Descargas/GTA3/ARCHIVOS" \
  --format glb \
  --sector-size 512
```

Eso genera:

```text
assets/sectors/sec_<X>_<Z>.glb
```

Ejemplos:

```text
assets/sectors/sec_0_0.glb
assets/sectors/sec_-1_2.glb
```

Cada sector conserva coordenadas de mundo dentro del propio `GLB`, así que tu motor puede instanciarlo en `(0,0,0)` y la geometría ya cae en su sitio.

Para exportar también recursos sueltos:

```bash
python3 tools/gta3_map_exporter/export_gta3_map.py \
  --game gta3 \
  --game-root "/home/ruben/.wine/drive_c/Program Files (x86)/Grand Theft Auto III" \
  --output-dir "/home/ruben/BennuGD2/tmp/gta3_assets" \
  --extracted-root "/home/ruben/Descargas/GTA3/ARCHIVOS" \
  --format glb \
  --export-category vehicles \
  --export-category peds \
  --export-category weapons \
  --export-category props \
  --copy-ped-anims
```

Eso genera, según exista cada recurso:

```text
assets/vehicles/*.glb
assets/peds/*.glb
assets/weapons/*.glb
assets/props/*.glb
assets/animations_raw/ped.ifp
```

Si no quieres exportar el mapa y solo quieres assets:

```bash
python3 tools/gta3_map_exporter/export_gta3_map.py \
  --game gta3 \
  --game-root "/home/ruben/.wine/drive_c/Program Files (x86)/Grand Theft Auto III" \
  --output-dir "/home/ruben/BennuGD2/tmp/gta3_only_assets" \
  --extracted-root "/home/ruben/Descargas/GTA3/ARCHIVOS" \
  --format glb \
  --skip-map \
  --export-category vehicles \
  --export-category peds \
  --export-category weapons \
  --export-category props \
  --copy-ped-anims
```

Notas:

- GTA 3 no trae motos como categoría jugable normal, así que no verás una carpeta separada de motos.
- `ped.ifp` se copia en bruto; no se convierte todavía a glTF animado.
- Los `GLB` de categorías salen en origen `(0,0,0)` porque son assets sueltos, no instancias del mapa.

Para incluir LOD:

```bash
python3 tools/gta3_map_exporter/export_gta3_map.py \
  --game gta3 \
  --game-root "/home/ruben/.wine/drive_c/Program Files (x86)/Grand Theft Auto III" \
  --output-dir "/home/ruben/BennuGD2/tmp/gta3_export_lod" \
  --include-lod
```

Para probar solo un `IPL`:

```bash
python3 tools/gta3_map_exporter/export_gta3_map.py \
  --game gta3 \
  --game-root "/home/ruben/.wine/drive_c/Program Files (x86)/Grand Theft Auto III" \
  --output-dir "/home/ruben/BennuGD2/tmp/gta3_comNtop" \
  --ipl "data/maps/comNtop.ipl"
```

## Nota

Si tus `PNG` ya están en `/home/ruben/Descargas/GTA3/ARCHIVOS/txd`, el script los intentará enlazar automáticamente usando `txd_name + texture_name`.

## GUI

También tienes una interfaz gráfica local:

```bash
python3 tools/gta3_map_exporter/gta3_map_exporter_gui.py
```

Desde ahí puedes seleccionar:
- juego (`gta3` o `gta4`)
- ruta del juego
- carpeta con archivos extraídos
- carpeta de texturas PNG
- extracción automática de texturas del juego a `dds`, `png` o ambos
- carpeta de salida
- formato `gltf` o `glb`
- tamaño de sector para salida `assets/sectors/sec_X_Z.glb`
- `IPL` concreto opcional
- activar o desactivar la exportación del mapa
- exportación extra de vehículos, peds, armas y props
- copia opcional de `anim/ped.ifp`

La idea es mantener una sola herramienta:

- una sola GUI
- un solo comando principal
- un selector de juego
- lógica interna separada por juego dentro del mismo script

## GTA4

La misma GUI y el mismo script ya pueden trabajar con `gta4`, pero el soporte todavía está en fase intermedia.

Ahora mismo la parte de GTA4 sí hace trabajo real:

- lee `common/data/gta.dat`
- indexa `IDE`, `IPL`, `WPL` e `IMGLIST`
- genera `gta4_world_manifest.json`
- genera metadatos por sectores desde placements de `IPL` texto
- parsea contenedores `RSC5` dentro de archivos `.img`
- extrae recursos individuales a carpetas propias
- intenta reconstruir texturas `WTD` simples a `DDS`
- puede extraer texturas del juego a `DDS`, `PNG` o ambos desde la GUI o CLI

Limitaciones actuales en GTA4:

- la exportación `GLB` actual es una aproximación heurística basada en recursos enlazados desde `WPL`
- todavía no hay parser completo de `WDR/WDD`, así que no sale todo el mapa ni todas las mallas
- algunas zonas exportan solo una parte de los placements porque no todos los recursos encuentran preview válido

Ejemplo mínimo para extraer recursos de vehículos de GTA4:

```bash
python3 tools/gta3_map_exporter/export_gta3_map.py \
  --game gta4 \
  --game-root "/media/ruben/4ED0B68AD0B677B3/Grand Theft Auto IV Complete Edition/GTAIV" \
  --output-dir "/home/ruben/BennuGD2/tmp/gta4_export" \
  --skip-map \
  --export-category vehicles
```

Eso crea, entre otras cosas:

```text
assets/vehicles/vehicles_catalog.json
assets/vehicles/parsed_resources/<recurso>/manifest.json
assets/vehicles/parsed_resources/<recurso>/resource.bin
assets/vehicles/parsed_resources/<recurso>/textures/*.dds
```

Ejemplo para exportar una zona GTA4 a `GLB` preview usando su `WPL`:

```bash
python3 tools/gta3_map_exporter/export_gta3_map.py \
  --game gta4 \
  --game-root "/media/ruben/4ED0B68AD0B677B3/Grand Theft Auto IV Complete Edition/GTAIV" \
  --output-dir "/home/ruben/BennuGD2/tmp/gta4_export_current" \
  --ipl "platform:/data/maps/east/bronx_e.ipl" \
  --format glb \
  --skip-map
```

Salida principal actual:

```text
gta4_world_manifest.json
export_summary.json
wpl_decoded/<zona>.json
zone_resource_indexes/<zona>.json
wpl_linked_resources/<zona>.json
assets/zones/<zona>_preview.glb
```
