# OBJ to MD3 Converter

Herramienta para convertir modelos Wavefront OBJ a formato Quake 3 MD3.

## Uso

```bash
python tools/obj_to_md3.py <input.obj> [output.md3] [scale]
```

## Parámetros

- `input.obj`: Archivo OBJ de entrada (requerido)
- `output.md3`: Archivo MD3 de salida (opcional, por defecto usa el mismo nombre que el input)
- `scale`: Factor de escala (opcional, por defecto 1.0)

## Ejemplos

### Conversión básica
```bash
python tools/obj_to_md3.py car.obj
```
Genera `car.md3` con escala 1.0

### Especificar archivo de salida
```bash
python tools/obj_to_md3.py car.obj my_car.md3
```

### Con escala personalizada
```bash
python tools/obj_to_md3.py car.obj car.md3 10.0
```
Genera el modelo 10x más grande (útil para modelos muy pequeños)

## Características

- ✅ Preserva coordenadas UV correctamente
- ✅ Triangula automáticamente polígonos convexos
- ✅ Soporta escala ajustable
- ✅ Genera bounding box correcto
- ✅ Compatible con formato MD3 estándar
- ✅ **Lee archivos MTL automáticamente**
- ✅ **Extrae y copia la textura al directorio de salida**

## Requisitos del modelo OBJ

- Debe tener caras trianguladas o polígonos convexos
- **Archivo MTL con referencia a textura (map_Kd)** - opcional pero recomendado
- Textura en formato PNG/JPG

### Sobre las coordenadas UV

**IMPORTANTE**: Esta herramienta maneja dos casos:

1. **OBJ con UVs** (`vt` en el archivo):
   - ✅ Preserva las UVs originales
   - ✅ Funciona con texturas complejas
   - ✅ Resultado profesional

2. **OBJ sin UVs** (solo geometría):
   - ⚠️ Genera UVs automáticos simples
   - ⚠️ Crea atlas de **colores sólidos** (no unwrap real)
   - ⚠️ Cada cara usa el color de su material
   - ℹ️ Resultado: modelo con colores planos, no textura detallada

**Recomendación**: Para mejores resultados, usa Blender para crear UVs:
1. Importa tu modelo en Blender
2. Selecciona el modelo → UV Editing
3. Selecciona todas las caras (A)
4. UV → Smart UV Project
5. Exporta como OBJ con "Include UVs" activado
6. Usa esta herramienta para convertir a MD3

## Workflow completo

1. Exporta tu modelo desde Blender/3D software como OBJ con MTL
2. Asegúrate de que el archivo MTL y la textura estén en el mismo directorio
3. Ejecuta el convertidor:
   ```bash
   python tools/obj_to_md3.py car.obj Models/md3/Car.md3 10.0
   ```
4. La herramienta copiará automáticamente la textura al directorio `Models/md3/`
5. Usa el modelo en tu juego con `RAY_LOAD_MD3` y `map_load` para la textura

## Notas

- El formato MD3 comprime vértices con factor 1/64, por lo que modelos muy pequeños necesitan escala mayor
- Las coordenadas V se invierten automáticamente para compatibilidad MD3
- Si tu modelo se ve muy pequeño en el motor, usa un factor de escala mayor (ej: 10.0 o 20.0)
