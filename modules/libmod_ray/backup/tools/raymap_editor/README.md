# RayMap Editor - Editor Visual de Mapas para libmod_ray

Editor visual de mapas para el módulo de raycasting de BennuGD2.

## 📸 Capturas de Pantalla

### Interfaz Principal
![Interfaz Principal](screenshots/editor_main_interface_1766517212880.png)
*Vista general del editor mostrando el grid, paleta de texturas y herramientas*

### Sistema de Puertas
![Sistema de Puertas](screenshots/editor_doors_example_1766517231292.png)
*Puertas verticales (azul) y horizontales (naranja) con indicadores visuales*


### Spawn Flags
![Spawn Flags](screenshots/editor_spawn_flags_1766517261045.png)
*Colocación de puntos de spawn con numeración automática*

## 🎯 Características

### ✅ Edición de Mapas
- **Editor visual de grids** con 3 niveles (0, 1, 2)
- **Edición multi-nivel** de paredes, suelos y techos independientes
- **Pintar con mouse** (click izquierdo = pintar, click derecho = borrar)
- **Zoom** con rueda del mouse
- **Paleta de texturas** con vista previa

### 🚪 Sistema de Puertas
- **Puertas verticales** (ID 1001-1500) con indicador 🚪V y borde azul
- **Puertas horizontales** (ID 1501+) con indicador 🚪H y borde naranja
- **Animación de apertura/cierre** en el motor
- **Botones dedicados** con código de colores

### 🗺️ Sistema de Slopes (Rampas)
- **Creación visual de rampas** arrastrando rectángulos
- **Dos tipos de slopes**: West→East y North→South
- **Slopes invertidos** para techos inclinados
- **Configuración de alturas** inicial y final
- **Visualización en tiempo real** con preview

### 📍 Spawn Flags
- **Colocación de puntos de spawn** con click
- **Numeración automática** de flags
- **Visualización por nivel** con indicadores visuales
- **Eliminación con click derecho**

### 📦 Gestión de Archivos
- **Carga de texturas FPG** de BennuGD2 (con soporte gzip)
- **Formato .raymap versión 4** con soporte multi-nivel completo
- **Retrocompatibilidad** con versiones anteriores
- **Exportar a texto** (formato CSV)
- **Barra de progreso** para operaciones largas

## 📖 Guía de Uso

### 1. Cargar texturas

1. **Archivo → Cargar Texturas FPG...**
2. Seleccionar el archivo `textures.fpg`
3. Las texturas aparecerán en el panel derecho

### 2. Crear un nuevo mapa

1. **Archivo → Nuevo Mapa**
2. Especificar dimensiones (ej: 32x32)
3. Seleccionar textura de la paleta
4. Pintar en el grid con el mouse

### 3. Editar niveles

- Usar el selector **"Nivel"** (0, 1, 2) en la barra de herramientas
- Cada nivel tiene sus propios grids de paredes, suelo y techo

### 4. Modos de edición

- **Paredes**: Editar paredes del mapa
- **Suelo**: Editar texturas del suelo
- **Techo**: Editar texturas del techo
- **Slopes**: Crear rampas e inclinaciones
- **Spawn Flags**: Colocar puntos de spawn

### 5. Añadir puertas

#### Puertas Verticales (🚪V)
1. Click en **🚪 Puerta V** (botón azul)
2. El ID se ajusta automáticamente al rango 1001-1500
3. Pintar en el mapa como una pared normal
4. Aparecen con borde azul y etiqueta 🚪V

#### Puertas Horizontales (🚪H)
1. Click en **🚪 Puerta H** (botón naranja)
2. El ID se ajusta automáticamente al rango 1501+
3. Pintar en el mapa
4. Aparecen con borde naranja y etiqueta 🚪H

### 6. Crear Slopes (Rampas)

1. Seleccionar modo **Slopes**
2. Configurar:
   - **Tipo**: West→East o North→South
   - **Altura inicial** y **Altura final**
   - **Invertido**: Para techos inclinados
3. Click y arrastrar para crear el rectángulo de la rampa
4. Soltar para crear el slope

### 7. Colocar Spawn Flags

1. Seleccionar modo **Spawn Flags**
2. Click izquierdo para colocar una flag
3. Click derecho para eliminar una flag
4. Las flags se numeran automáticamente

### 8. Controles del mouse

- **Click izquierdo + arrastrar**: Pintar textura seleccionada
- **Click derecho + arrastrar**: Borrar (poner a 0)
- **Rueda del mouse**: Zoom in/out

### 9. Guardar el mapa

1. **Archivo → Guardar Como...**
2. Guardar como archivo `.raymap`

## 🎮 Integración con BennuGD2

```bennugd
import "mod_ray";

process main()
begin
    fpg_textures = load_fpg("textures.fpg");
    RAY_INIT(800, 600, 90, 2);
    RAY_LOAD_MAP("mi_mapa.raymap", fpg_textures);
    
    // Usar posición de cámara del mapa
    if (RAY_HAS_MAP_CAMERA())
        RAY_SET_CAMERA_FROM_MAP();
    end
    
    while (!key(_ESC))
        // Movimiento
        if (key(_W)) RAY_MOVE_FORWARD(5.0); end
        if (key(_S)) RAY_MOVE_BACKWARD(5.0); end
        if (key(_A)) RAY_STRAFE_LEFT(5.0); end
        if (key(_D)) RAY_STRAFE_RIGHT(5.0); end
        
        // Rotación con mouse
        RAY_MOUSE_LOOK(0.002);
        
        // Interacción con puertas
        if (key(_E))
            RAY_TOGGLE_DOOR();
        end
        
        // Cambiar de nivel
        if (key(_1)) RAY_SET_LEVEL(0); end
        if (key(_2)) RAY_SET_LEVEL(1); end
        if (key(_3)) RAY_SET_LEVEL(2); end
        
        RAY_RENDER();
        frame;
    end
    
    RAY_SHUTDOWN();
end
```

## ⌨️ Atajos de Teclado

### Archivo
- **Ctrl+N**: Nuevo mapa
- **Ctrl+O**: Abrir mapa
- **Ctrl+S**: Guardar
- **Ctrl+Shift+S**: Guardar como
- **Ctrl+E**: Exportar a texto
- **Ctrl+Q**: Salir

### Vista
- **Ctrl++**: Acercar zoom
- **Ctrl+-**: Alejar zoom
- **Ctrl+0**: Resetear zoom

### Edición
- **1**: Modo Paredes
- **2**: Modo Suelo
- **3**: Modo Techo
- **4**: Modo Slopes
- **5**: Modo Spawn Flags

## 📄 Formato de Archivo

### .raymap Versión 4

El formato actual incluye:

- **3 niveles completos** (0, 1, 2)
- **Grids independientes** por nivel:
  - Paredes (walls)
  - Suelo (floor)
  - Techo (ceiling)
  - Altura de suelo (floor height)
- **ThickWalls** (slopes/rampas)
- **ThinWalls** (paredes de slopes)
- **Spawn Flags** con nivel asociado
- **Posición de cámara**

### Retrocompatibilidad

El editor puede abrir mapas de versiones anteriores (1, 2 y 3) y los convierte automáticamente a versión 4 al guardar.

## 🔧 Consejos y Trucos

### Crear edificios de varios pisos

1. Diseña el nivel 0 (planta baja)
2. Cambia al nivel 1 y diseña el primer piso
3. Usa slopes para crear escaleras entre niveles
4. Coloca spawn flags en cada nivel para puntos de entrada

### Puertas automáticas

Las puertas se crean automáticamente al pintar con IDs especiales:
- **1001-1500**: Puertas que se deslizan verticalmente
- **1501+**: Puertas que se deslizan horizontalmente

El motor detecta automáticamente el tipo y anima la apertura/cierre.

### Rampas realistas

Para crear rampas que se vean bien:
1. Usa altura inicial 0.0 y final 128.0 para una rampa completa
2. Para medias rampas, usa 0.0 a 64.0
3. Invierte el slope para crear techos inclinados

### Organización de texturas

Organiza tus texturas en el FPG:
- **1-999**: Paredes normales
- **1001-1500**: Texturas para puertas verticales
- **1501+**: Texturas para puertas horizontales

## 📜 Licencia

Este editor es parte del proyecto BennuGD2 y se distribuye bajo la misma licencia.
