# Roadmap: BennuGD2 Build Engine Module (Fase 2)

## Estado Actual (Hitos Completados)
- [x] **Carga de Mapas y Texturas**: Soporte para formatos .MAP (Build) y tiles .ART.
- [x] **Renderizado de Sectores**: Renderizado recursivo a través de portales (scansector).
- [x] **Geometría de Paredes**: Proyección 1:1 corregida (escala Z / 128).
- [x] **Clipping**: Sistema de *portal clipping* (Upper/Lower/Hole) funcionado correctamente.
- [x] **Suelos y Techos**: Renderizado texturizado básico con corrección de perspectiva.
- [x] **Debug**: Sistema de detección de texturas faltantes y gestión de tiles vacíos.

---

## Plan de Implementación Futuro

### 1. Renderizado de Sprites (Prioridad Alta)
Actualmente el mundo está vacío de objetos.
- [ ] **Carga de Sprites**: Leer la lista de sprites (`numsprites`) del archivo .MAP.
- [ ] **Ordenamiento**: Implementar algoritmo de ordenamiento (Painter's Algorithm o Z-buffer parcial) para dibujar sprites de atrás hacia delante.
- [ ] **Proyección**: Adaptar la lógica de `project_height` para sprites (billboarding).
- [ ] **Clipping**: Asegurar que los sprites se recortan correctamente contra las paredes y portales (usando `dmost`/`umost` o buffer de profundidad 1D).

### 2. Iluminación y Sombreado (Prioridad Alta)
Todo se ve con iluminación plana ("full bright").
- [ ] **Sector Shade**: Implementar el parámetro `shade` de los sectores (oscurecer colores basado en el valor del sector).
- [ ] **Distance Fog**: Implementar oscurecimiento por distancia (z-shading) típico del Build Engine.
- [ ] **Palette Lookups**: Implementar carga de `PALETTE.DAT` real (tablas de *shade* y *translucency*) para no depender de `SDL_MapRGB` por píxel (mejora de rendimiento masiva).

### 3. Geometría Avanzada
- [ ] **Slopes (Pendientes)**: Implementar soporte para `heinum` en techos y suelos (planos inclinados). Actualmente todo es plano.
- [ ] **Masked Walls**: Renderizar paredes con transparencia (rejas, ventanas) usando el bit `cstat` correspondiente (bit 0 del wall render).
- [ ] **Parallax Skies**: Implementar correctamente el scroll de texturas de cielo basado en el ángulo de cámara.

### 4. Físicas y Colisiones
- [ ] **Gravedad y Altura Player**: Eliminar el hack de altura fija (`camera->z`) e implementar física real de jugador sobre el suelo `floorz`.
- [ ] **ClipMove**: Implementar detección de colisiones contra paredes y límites de sectores (`clipmove`).
- [ ] **Transición de Sectores**: Actualizar `cursectnum` dinámicamente al moverse el jugador entre portales.

### 5. Optimización
- [ ] **Lookup Tables**: Reemplazar cálculos trigonométricos y divisiones costosas por tablas pre-calculadas (como el motor original).
- [ ] **Direct Pixel Access**: Migrar de `gr_put_pixel` a punteros directos (`uint32_t *pixels`) para el dibujado de columnas y spans verticales.

---

## Siguiente Sesión Sugerida
Recomendamos empezar por **2. Iluminación y Sombreado** para dar profundidad visual, o **1. Sprites** para poblar el mapa.
