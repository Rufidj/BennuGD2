# Instrucciones de Integración - Sectores Anidados

## Funciones Creadas

He creado las siguientes funciones para soporte de sectores anidados en el archivo `nested_sector_functions.c`:

1. **`init_region_hierarchy(WLD_Map *map)`** - Inicializa el array de jerarquías
2. **`is_region_inside_region(WLD_Map *map, int inner, int outer)`** - Detecta si un sector está dentro de otro
3. **`build_region_hierarchy(WLD_Map *map)`** - Construye la jerarquía completa
4. **`point_in_region_nested(float x, float y, WLD_Map *map)`** - Retorna el sector más anidado

## Dónde Insertar el Código

### Paso 1: Añadir las funciones

Abre `libmod_heightmap.c` y busca la función `point_in_region` (debería estar alrededor de la línea 4028-4058).

Inmediatamente **después** del cierre de `point_in_region` (después de la línea que dice `return inside;` y su `}`), inserta todo el contenido del archivo `nested_sector_functions.c`.

### Paso 2: Llamar a `build_region_hierarchy` al cargar el mapa

Busca la función `libmod_heightmap_load_wld` o donde se carga el mapa WLD (probablemente alrededor de la línea 3700).

Después de las líneas:
```c
wld_build_wall_ptrs(&wld_map); 
wld_assign_regions_simple(&wld_map, -1); 
```

Añade:
```c
build_region_hierarchy(&wld_map);
```

### Paso 3: Actualizar `scan_walls_from_region` (Opcional)

Para que el renderizado visite sectores hijos, en la función `scan_walls_from_region` (alrededor de la línea 4061), después del bucle que escanea las paredes de una región, añade:

```c
// Visitar sectores hijos si existen
for (int i = 0; i < region_hierarchy[current].num_children; i++) {
    int child = region_hierarchy[current].child_regions[i];
    if (child >= 0 && child < map->num_regions && num_to_visit < 256) {
        regions_to_visit[num_to_visit++] = child;
    }
}
```

## Uso

Una vez integrado, puedes usar:

- `point_in_region_nested(x, y, &wld_map)` para obtener el sector más específico que contiene un punto
- La jerarquía se construirá automáticamente al cargar el mapa WLD
- Los sectores anidados se detectarán automáticamente basándose en geometría

## Próximos Pasos

Después de integrar estas funciones, necesitarás:
1. Actualizar el renderizado para manejar clipping de sectores anidados
2. Probar con un mapa WLD que contenga sectores anidados (ej: una caja dentro de una habitación)
