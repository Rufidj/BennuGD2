# Guía Rápida de Uso del Módulo Raycasting

## 1. Preparar Texturas

Crea un FPG con texturas de 128x128 píxeles:

```bennugd
// crear_fpg.prg
import "libmod_gfx";
import "libmod_misc";

process main()
private
    int fpg_id;
begin
    set_mode(640, 480, 32);
    
    fpg_id = fpg_new();
    
    // Cargar texturas (debes tener los archivos PNG/BMP)
    map_load("wall1.png", fpg_id, 1);
    map_load("wall2.png", fpg_id, 2);
    map_load("wall3.png", fpg_id, 3);
    map_load("wall4.png", fpg_id, 4);
    map_load("floor.png", fpg_id, 5);
    map_load("ceiling.png", fpg_id, 6);
    map_load("column.png", fpg_id, 10);
    map_load("enemy1.png", fpg_id, 11);
    map_load("enemy2.png", fpg_id, 12);
    map_load("item1.png", fpg_id, 13);
    map_load("item2.png", fpg_id, 14);
    
    fpg_save(fpg_id, "textures.fpg");
    
    say("FPG creado: textures.fpg");
end
```

## 2. Crear Mapa

```bash
cd tools
gcc -o map_builder map_builder.c
./map_builder example/config.txt test.raymap
```

## 3. Ejecutar Programa de Prueba

```bash
bgdc test_ray.prg
./test_ray
```

## Controles

- **WASD**: Movimiento
- **Flechas**: Rotar cámara / Mirar arriba-abajo
- **Espacio**: Saltar
- **Enter**: Abrir/cerrar puerta
- **F**: Toggle niebla
- **M**: Toggle minimapa
- **ESC**: Salir

## Estructura de Archivos Necesaria

```
tu_proyecto/
├── test_ray.prg          # Programa principal
├── textures.fpg          # FPG con texturas
├── test.raymap           # Mapa generado
└── tools/
    ├── map_builder       # Herramienta compilada
    └── example/          # Archivos de ejemplo
        ├── config.txt
        ├── nivel0.txt
        ├── floor.txt
        └── ...
```

## Crear Tus Propios Mapas

1. Edita los archivos en `tools/example/`:
   - `nivel0.txt`: Grid de paredes (CSV)
   - `floor.txt`: Texturas de suelo
   - `ceiling.txt`: Texturas de techo
   - `sprites.txt`: Posiciones de sprites

2. Genera el mapa:
   ```bash
   ./map_builder example/config.txt mi_mapa.raymap
   ```

3. Usa en tu programa:
   ```bennugd
   RAY_LOAD_MAP("mi_mapa.raymap", fpg_textures);
   ```

## Valores de Paredes

En los archivos de grid (nivel0.txt, etc.):

- `0` = Vacío
- `1-999` = Paredes normales (ID de textura)
- `1001-1500` = Puertas verticales
- `1501+` = Puertas horizontales

## Posiciones de Sprites

En `sprites.txt`, formato: `x,y,z,texture_id,w,h,level,rot`

Para centrar en celda (X, Y):
```
x = X * 128 + 64
y = Y * 128 + 64
```

Ejemplo: sprite en celda (5, 10):
```
704,1344,0,11,128,128,0,0
```
