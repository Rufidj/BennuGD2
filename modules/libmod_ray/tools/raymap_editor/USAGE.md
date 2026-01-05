# Guía de Uso del Editor de Mapas

## Interfaz del Editor

### Barra de Herramientas

```
┌─────────────────────────────────────────────────────────────────┐
│ Nivel: [🔷 Nivel 0 ▼] │ Modo: [🧱 Paredes ▼] │ Tipo: [🧱 Pared] [🚪 Puerta V] [🚪 Puerta H] │ Textura ID: [1] │
└─────────────────────────────────────────────────────────────────┘
```

#### Controles Principales

**Nivel** (🔷🔶🔴)
- **Nivel 0** (🔷): Planta baja
- **Nivel 1** (🔶): Primer piso
- **Nivel 2** (🔴): Segundo piso

**Modo** (🧱⬛⬜)
- **Paredes** (🧱): Editar paredes y puertas
- **Suelo** (⬛): Editar texturas del suelo
- **Techo** (⬜): Editar texturas del techo

**Tipo de Tile**
- **🧱 Pared**: Pared normal (ID 1-999)
- **🚪 Puerta V**: Puerta vertical (ID 1001-1500)
- **🚪 Puerta H**: Puerta horizontal (ID 1501+)

## Tipos de Tiles

### Paredes Normales (1-999)
- **Uso**: Paredes sólidas que bloquean el movimiento
- **ID**: 1 a 999
- **Color sin textura**: Azul grisáceo
- **Ejemplo**: Paredes exteriores, muros interiores

### Puertas Verticales (1001-1500)
- **Uso**: Puertas que se abren verticalmente
- **ID**: 1001 a 1500
- **Color sin textura**: Azul
- **Indicador**: Borde azul + 🚪V
- **Ejemplo**: Puertas normales, entradas

### Puertas Horizontales (1501+)
- **Uso**: Puertas que se abren horizontalmente
- **ID**: 1501 en adelante
- **Color sin textura**: Naranja
- **Indicador**: Borde naranja + 🚪H
- **Ejemplo**: Puertas correderas, portones

## Flujo de Trabajo

### 1. Cargar Texturas

```
Archivo → Cargar Texturas FPG... → Seleccionar textures.fpg
```

Las texturas aparecerán en el panel derecho.

### 2. Crear Nuevo Mapa

```
Archivo → Nuevo Mapa
  ├─ Ancho: 16
  └─ Alto: 16
```

### 3. Pintar Paredes

1. Seleccionar **Nivel 0**
2. Seleccionar **Modo: Paredes**
3. Hacer clic en **🧱 Pared**
4. Seleccionar textura del panel derecho (o escribir ID)
5. Click izquierdo + arrastrar para pintar
6. Click derecho para borrar

### 4. Añadir Puertas

#### Puerta Vertical
1. Hacer clic en **🚪 Puerta V**
2. El ID se ajusta automáticamente (1001-1500)
3. Pintar en el mapa

#### Puerta Horizontal
1. Hacer clic en **🚪 Puerta H**
2. El ID se ajusta automáticamente (1501+)
3. Pintar en el mapa

### 5. Editar Suelos y Techos

1. Cambiar a **Modo: Suelo** o **Modo: Techo**
2. Seleccionar textura
3. Pintar como con las paredes

### 6. Trabajar con Múltiples Niveles

1. Cambiar entre **Nivel 0**, **Nivel 1**, **Nivel 2**
2. Cada nivel es independiente
3. Útil para edificios de varios pisos

### 7. Guardar el Mapa

```
Archivo → Guardar Como... → mi_mapa.raymap
```

El mapa se guarda en formato v2 con posición de cámara.

## Controles del Mouse

| Acción | Resultado |
|--------|-----------|
| **Click izquierdo** | Pintar textura seleccionada |
| **Click izquierdo + arrastrar** | Pintar múltiples celdas |
| **Click derecho** | Borrar (poner a 0) |
| **Click derecho + arrastrar** | Borrar múltiples celdas |
| **Rueda del mouse** | Zoom in/out |

## Atajos de Teclado

| Atajo | Acción |
|-------|--------|
| **Ctrl+N** | Nuevo mapa |
| **Ctrl+O** | Abrir mapa |
| **Ctrl+S** | Guardar |
| **Ctrl+Shift+S** | Guardar como |
| **Ctrl++** | Acercar zoom |
| **Ctrl+-** | Alejar zoom |
| **Ctrl+Q** | Salir |

## Ejemplos de Uso

### Crear una Habitación Simple

```
1. Nuevo mapa 16x16
2. Nivel 0, Modo: Paredes, Tipo: Pared
3. Pintar borde exterior con textura 1
4. Crear habitación interior (ej: 5,5 a 10,10)
5. Añadir puerta vertical en (7,5) con ID 1001
```

### Edificio de Dos Pisos

```
Nivel 0:
  - Paredes exteriores
  - Habitaciones planta baja
  - Escaleras (marcar con textura especial)

Nivel 1:
  - Paredes del primer piso
  - Habitaciones superiores
  - Mismo diseño de escaleras
```

### Mapa con Diferentes Tipos de Puertas

```
- Puertas de entrada: Vertical (1001)
- Puertas interiores: Vertical (1002)
- Portones: Horizontal (1501)
- Puertas secretas: Horizontal (1502)
```

## Indicadores Visuales

### En el Grid

- **Celda vacía**: Gris oscuro
- **Pared con textura**: Muestra la textura
- **Pared sin textura**: Azul grisáceo + ID
- **Puerta V con textura**: Textura + borde azul + 🚪V
- **Puerta V sin textura**: Azul + 🚪V + ID
- **Puerta H con textura**: Textura + borde naranja + 🚪H
- **Puerta H sin textura**: Naranja + 🚪H + ID

### En la Barra de Estado

- **Coordenadas**: `X: 5, Y: 10`
- **Mensajes**: Estado actual de la operación

## Consejos

1. **Organiza por niveles**: Usa Nivel 0 para planta baja, Nivel 1 para primer piso, etc.
2. **Usa IDs consistentes**: Agrupa texturas similares (ej: 1-10 paredes de piedra, 11-20 paredes de madera)
3. **Puertas verticales vs horizontales**: Usa verticales para puertas normales, horizontales para portones grandes
4. **Guarda frecuentemente**: Usa Ctrl+S regularmente
5. **Exporta a texto**: Útil para backup y edición manual

## Solución de Problemas

**Las texturas no se ven**
- Verifica que cargaste el archivo FPG correcto
- Asegúrate de que los IDs coincidan

**No puedo pintar**
- Verifica que seleccionaste una textura
- Comprueba que estás en el modo correcto (Paredes/Suelo/Techo)

**Las puertas no funcionan en el juego**
- Verifica que los IDs estén en el rango correcto
- Puertas V: 1001-1500
- Puertas H: 1501+
