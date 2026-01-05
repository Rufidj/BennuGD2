# Map Builder - Herramienta de Creación de Mapas

## Descripción

`map_builder` es una herramienta que genera archivos `.raymap` leyendo datos desde archivos de texto en formato CSV.

## Compilación

```bash
cd /home/ruben/BennuGD2/modules/libmod_ray/tools
gcc -o map_builder map_builder.c
```

## Uso

```bash
./map_builder config.txt output.raymap
```

## Formato de Archivos

### 1. Archivo de Configuración (config.txt)

```ini
# Comentarios empiezan con #
grid0=nivel0.txt       # Grid nivel 0 (REQUERIDO)
grid1=nivel1.txt       # Grid nivel 1 (opcional)
grid2=nivel2.txt       # Grid nivel 2 (opcional)
floor=floor.txt        # Mapa de suelos (opcional)
ceiling=ceiling.txt    # Mapa de techos (opcional)
sprites=sprites.txt    # Lista de sprites (opcional)
```

### 2. Archivos de Grid (CSV)

Formato: valores separados por comas, una fila por línea.

**Ejemplo (nivel0.txt):**
```
1,1,1,1,1,1,1,1
1,0,0,0,0,0,0,1
1,0,0,4,0,0,0,1
1,0,0,4,0,0,0,1
1,0,0,0,0,0,0,1
1,1,1,1,1,1,1,1
```

**Valores:**
- `0` = Espacio vacío
- `1-999` = Paredes (ID de textura en FPG)
- `1001-1500` = Puertas verticales
- `1501+` = Puertas horizontales

### 3. Archivo de Sprites (sprites.txt)

Formato: `x,y,z,texture_id,w,h,level,rot`

**Ejemplo:**
```
# Columna en celda (3,1)
448,192,0,10,128,128,0,0

# Enemigo en celda (12,1)
1600,192,0,11,128,128,0,0

# Item en celda (5,11)
640,1472,0,13,128,128,0,0
```

**Campos:**
- `x,y,z`: Posición en unidades de juego
- `texture_id`: ID de textura en el FPG
- `w,h`: Tamaño (típicamente 128)
- `level`: Nivel del grid (0, 1, 2)
- `rot`: Rotación en radianes

**Cálculo de posición:**
Para centrar un sprite en una celda (X, Y):
```
x = X * 128 + 64
y = Y * 128 + 64
```

## Ejemplo Completo

### 1. Crear archivos de datos

Usa los archivos de ejemplo en `example/`:
```bash
cd example
ls -la
# config.txt
# nivel0.txt
# nivel1.txt
# nivel2.txt
# floor.txt
# ceiling.txt
# sprites.txt
```

### 2. Generar mapa

```bash
cd ..
./map_builder example/config.txt mi_mapa.raymap
```

Salida:
```
Grid cargado: example/nivel0.txt (16x16)
Grid cargado: example/nivel1.txt (16x16)
Grid cargado: example/nivel2.txt (16x16)
Grid cargado: example/floor.txt (16x16)
Grid cargado: example/ceiling.txt (16x16)
Sprites cargados: 6

=== Mapa creado exitosamente ===
Archivo: mi_mapa.raymap
Dimensiones: 16x16
Niveles: 3
Sprites: 6
```

### 3. Usar en BennuGD2

```bennugd
import "mod_ray";

process main()
begin
    fpg_textures = load_fpg("textures.fpg");
    RAY_INIT(800, 600, 90, 2);
    RAY_LOAD_MAP("mi_mapa.raymap", fpg_textures);
    RAY_SET_CAMERA(3.0 * 128.0, 3.0 * 128.0, 0.0, 0.0, 0.0);
    
    // ... resto del código
end
```

## Consejos

### Crear mapas grandes

Puedes usar cualquier editor de texto o Excel/LibreOffice Calc:

1. Crear grid en Excel con números
2. Guardar como CSV
3. Reemplazar `;` por `,` si es necesario
4. Usar como archivo de grid

### Calcular posiciones de sprites

```python
# Script Python para calcular posiciones
celda_x = 5
celda_y = 10
x = celda_x * 128 + 64
y = celda_y * 128 + 64
print(f"{x},{y},0,11,128,128,0,0")
```

### Validar dimensiones

Todos los grids deben tener las mismas dimensiones. La herramienta lo verifica automáticamente.

## Errores Comunes

**Error: "Grid vacío"**
- Verifica que el archivo no esté vacío
- Asegúrate de que las líneas no estén todas comentadas

**Error: "tiene dimensiones diferentes"**
- Todos los grids deben ser del mismo tamaño
- Cuenta las filas y columnas en cada archivo

**Error: "No se puede abrir"**
- Verifica la ruta del archivo en config.txt
- Usa rutas relativas desde donde ejecutas map_builder

## Archivos de Ejemplo

Los archivos en `example/` crean un mapa de 16x16 con:
- Paredes exteriores
- Habitaciones internas
- Columnas decorativas
- Enemigos
- Items
- Suelos y techos texturizados
