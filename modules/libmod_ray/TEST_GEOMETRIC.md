# Test Geometric Engine

## Descripción
Programa de prueba simple para el nuevo motor de sectores geométricos.

## Requisitos
1. **Mapa**: Necesitas crear un mapa `test.raymap` con el editor
2. **Texturas**: Necesitas un archivo `textures.fpg` con texturas

## Crear un mapa de prueba

### Opción 1: Usar el editor
```bash
cd /home/ruben/BennuGD2/modules/libmod_ray/tools/raymap_editor/build
./raymap_editor
```

1. File → Load FPG (cargar textures.fpg)
2. Modo: "Draw Sector"
3. Click para crear vértices de un polígono (mínimo 3)
4. Right-click para finalizar el sector
5. Crear otro sector adyacente
6. Toolbar → "Detect Portals"
7. File → Save As → test.raymap

### Opción 2: Crear mapa hardcodeado (temporal)
Si el editor no compila aún, puedes crear un mapa simple programáticamente.

## Controles

- **WASD**: Movimiento (adelante/atrás/izquierda/derecha)
- **Flechas**: Rotar cámara y mirar arriba/abajo
- **Espacio**: Saltar
- **F**: Toggle niebla
- **M**: Toggle minimapa
- **ESC**: Salir

## Ejecutar

```bash
cd /home/ruben/BennuGD2/modules/libmod_ray
bgdi test_geometric.prg
```

## Características probadas

- ✅ Inicialización del motor
- ✅ Carga de mapa v8 (sectores geométricos)
- ✅ Movimiento con colisiones
- ✅ Rotación de cámara
- ✅ Renderizado de sectores
- ✅ Portales entre sectores
- ✅ Fog system
- ✅ Minimapa

## Notas

- El programa espera `test.raymap` en el directorio actual
- Si no existe el mapa, mostrará un error y saldrá
- La posición inicial es (384, 384, 0) - ajusta según tu mapa
