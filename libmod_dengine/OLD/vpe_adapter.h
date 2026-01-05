#ifndef VPE_ADAPTER_H
#define VPE_ADAPTER_H

#include "libmod_wld.h"
#include "vpe_src/vpe.h"
#include "vpe_src/internal.h"

// Adapter entre WLD y VPE
typedef struct {
    struct View vpe_view;
    struct Level vpe_level;
    struct Object vpe_camera_object;
    struct Point vpe_camera_point;
    
    // Mapeo WLD → VPE
    WLD_Map *wld_map;
    CAMERA_3D *wld_camera;
    GRAPH *target_graph;
    
    // Arrays de conversión
    struct Region **vpe_regions;
    struct Wall **vpe_walls;
    struct Point **vpe_points;
    
    int initialized;
} VPEAdapter;

// Funciones del adapter
void vpe_adapter_init(VPEAdapter *adapter);
void vpe_adapter_free(VPEAdapter *adapter);

// Convertir WLD a VPE
int vpe_adapter_load_map(VPEAdapter *adapter, WLD_Map *map);

// Configurar cámara
void vpe_adapter_set_camera(VPEAdapter *adapter, CAMERA_3D *camera);

// Renderizar usando VPE
void vpe_adapter_render(VPEAdapter *adapter, GRAPH *target);

#endif
