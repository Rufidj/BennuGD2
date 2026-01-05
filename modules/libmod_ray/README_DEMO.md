# Christmas Raycasting Demo

Demo navideña que muestra las capacidades del motor de raycasting `libmod_ray` de BennuGD2.

## Características

### Motor de Raycasting
- Renderizado 3D en tiempo real
- Múltiples niveles (suelo, techo)
- Puertas animadas que se abren/cierran
- Sistema de fog (niebla) configurable
- Minimapa en tiempo real

### Sistema Billboard
- 6 Santas que se mueven aleatoriamente por el mapa
- Sprites siempre miran a la cámara (billboard)
- 12 direcciones de animación
- Colisiones con paredes
- Detección de proximidad con el jugador

### Audio
- Música de fondo navideña (OGG)
- Sonidos de proximidad cuando te acercas a los Santas (WAV)

### Minimapa
- Muestra todo el mapa en tiempo real
- Punto blanco: posición de la cámara
- Puntos cyan: Santas
- Puertas en magenta
- Paredes en rosa/azul

## Controles

- **W/S** - Adelante/Atrás
- **A/D** - Izquierda/Derecha (strafe)
- **Flechas** - Rotar cámara
- **E** - Abrir/Cerrar puertas
- **ESC** - Salir

## Archivos

- `test_billboard.prg` - Código fuente de la demo
- `Maps/M1N1.raymap` - Mapa del nivel
- `Textures/` - Texturas del mapa
- `santa.fpg` - Gráficos de los Santas (12 direcciones × 4 frames)
- `Music/christmas-happy-background.ogg` - Música de fondo
- `Sounds/santa-claus-EN.wav` - Sonidos de los Santas

## Ejecución

```bash
bgdc test_billboard.prg
bgdi test_billboard.dcb
```

## Créditos

Motor de raycasting desarrollado para BennuGD2.
