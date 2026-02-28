# Plan de Implementación: Motor de Sectores GPU "El Trabajo de mi Vida"

## Objetivo
Reescribir y optimizar el motor `libmod_ray` para BennuGD2, pasando de un enfoque pixel-a-pixel a un motor basado en geometría acelerada por hardware (GPU), al estilo de los motores Build/Doom modernos.

## Características Principales
1.  **Motor de Sectores Puro**: Eliminación definitiva de cualquier rastro del sistema de tiles antiguo.
2.  **Sectores Anidados (Islands)**: Soporte completo para sectores dentro de sectores con jerarquía padre/hijo.
3.  **Portales (Sectores Adyacentes)**: Renderizado recursivo a través de portales con recortes perfectos.
4.  **Aceleración por Hardware**: Uso intensivo de `sdl_gpu` para dibujo de polígonos, texturizado y MD3.
5.  **Físicas de Deslizamiento**: Implementación de colisiones suaves contra paredes y techos/suelos.
6.  **Soporte MD3**: Modelos animados integrados en el pipeline de la GPU.

## Fases de Trabajo

### Fase 1: Estructuras de Datos y API
*   Refactorizar `libmod_ray.h` para incluir jerarquías y flags avanzados.
*   Asegurar compatibilidad con mapas v23 (incluyendo sectores con flags y metadatos).

### Fase 2: Motor de Renderizado GPU (libmod_ray_render_gpu.c)
*   **Tesselación**: Función para triangular polígonos de sectores (suelos y techos).
*   **Pipeline de Portales**: Renderizado recursivo usando `GPU_SetClip` para recortes horizontales y verticales.
*   **Manejo de Profundidad**: Uso del Z-Buffer de `sdl_gpu` para evitar clipping entre modelos y arquitectura.

### Fase 3: Físicas y Colisiones (libmod_ray_physics.c)
*   Implementar `ray_move_entity` con detección de colisión deslizante.
*   Detección de altura de suelo/techo dinámica para sectores anidados.

### Fase 4: Integración MD3 GPU
*   Adaptar el renderizado de MD3 para usar `GPU_TriangleBatch`.
*   Interpolación de frames en la GPU para animaciones suaves.

### Fase 5: Refactorización del Cargador de Mapas
*   Asegurar que `libmod_ray_map.c` reconstruye la jerarquía correctamente al cargar.
*   Sincronización automática de IDs de portales.

## Consideraciones de Rendimiento
*   Minimizar cambios de estado en la GPU (Texture binds).
*   Uso de `GPU_TriangleBatch` para agrupar paredes de un mismo sector con la misma textura.
