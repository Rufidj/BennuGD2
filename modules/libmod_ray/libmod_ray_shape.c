/*
 * libmod_ray_shape.c - Geometry and Shape Functions
 * Port of shape.cpp from Andrew Lim's raycasting engine
 */

#include "libmod_ray.h"
#include <math.h>

/* ============================================================================
   LINE INTERSECTION
   ============================================================================ */

/* Port of Shape::linesIntersect()
 * Based on: http://paulbourke.net/geometry/pointlineplane/javascript.txt
 */
int ray_lines_intersect(float x1, float y1, float x2, float y2,
                        float x3, float y3, float x4, float y4,
                        float *ix, float *iy)
{
    /* Check if none of the lines are of length 0 */
    if ((x1 == x2 && y1 == y2) || (x3 == x4 && y3 == y4)) {
        return 0;
    }
    
    float denominator = ((y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1));
    
    /* Lines are parallel */
    if (denominator == 0) {
        return 0;
    }
    
    float ua = ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / denominator;
    float ub = ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / denominator;
    
    /* Is the intersection along the segments */
    if (ua < 0 || ua > 1 || ub < 0 || ub > 1) {
        return 0;
    }
    
    /* Return coordinates of the intersection */
    float x = x1 + ua * (x2 - x1);
    float y = y1 + ua * (y2 - y1);
    
    if (ix) {
        *ix = x;
    }
    if (iy) {
        *iy = y;
    }
    
    return 1;
}

/* ============================================================================
   POINT IN RECTANGLE
   ============================================================================ */

/* Port of Shape::pointInRect() */
int ray_point_in_rect(float ptx, float pty, float x, float y, float w, float h)
{
    return x <= ptx && ptx <= (x + w) &&
           y <= pty && pty <= (y + h);
}

/* ============================================================================
   POINT IN TRIANGLE
   ============================================================================ */

/* Port of Shape::sign() */
float ray_sign(const RAY_Point *p1, const RAY_Point *p2, const RAY_Point *p3)
{
    return (p1->x - p3->x) * (p2->y - p3->y) - (p2->x - p3->x) * (p1->y - p3->y);
}

/* Port of Shape::pointInTriangle()
 * Based on: https://stackoverflow.com/a/2049593/1645045
 */
int ray_point_in_triangle(const RAY_Point *pt, const RAY_Point *v1,
                          const RAY_Point *v2, const RAY_Point *v3)
{
    float d1, d2, d3;
    int has_neg, has_pos;
    
    d1 = ray_sign(pt, v1, v2);
    d2 = ray_sign(pt, v2, v3);
    d3 = ray_sign(pt, v3, v1);
    
    has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    
    return !(has_neg && has_pos);
}

/* ============================================================================
   POINT IN QUAD
   ============================================================================ */

/* Port of Shape::pointInQuad() */
int ray_point_in_quad(const RAY_Point *pt, const RAY_Point *v1,
                      const RAY_Point *v2, const RAY_Point *v3,
                      const RAY_Point *v4)
{
    return ray_point_in_triangle(pt, v1, v2, v3) ||
           ray_point_in_triangle(pt, v3, v4, v1);
}

/* ============================================================================
   THIN WALL FUNCTIONS
   ============================================================================ */

void ray_thin_wall_init(RAY_ThinWall *tw)
{
    tw->x1 = 0;
    tw->y1 = 0;
    tw->x2 = 0;
    tw->y2 = 0;
    tw->wallType = 0;
    tw->horizontal = 0;
    tw->height = 0;
    tw->z = 0;
    tw->hidden = 0;
    tw->thickWall = NULL;
    tw->slope = 0;
}

void ray_thin_wall_create(RAY_ThinWall *tw, float x1, float y1, float x2, float y2,
                          int wallType, RAY_ThickWall *thickWall, float wallHeight)
{
    tw->x1 = x1;
    tw->y1 = y1;
    tw->x2 = x2;
    tw->y2 = y2;
    tw->wallType = wallType;
    tw->horizontal = 0;
    tw->height = wallHeight;
    tw->z = 0;
    tw->hidden = 0;
    tw->thickWall = thickWall;
    tw->slope = 0;
}

float ray_thin_wall_distance_to_origin(RAY_ThinWall *tw, float ix, float iy)
{
    float dx = tw->x1 - ix;
    float dy = tw->y1 - iy;
    return sqrtf(dx * dx + dy * dy);
}

/* ============================================================================
   THICK WALL FUNCTIONS
   ============================================================================ */

void ray_thick_wall_init(RAY_ThickWall *tw)
{
    tw->type = 0;
    tw->slopeType = 0;
    tw->height = 0;
    tw->x = tw->y = tw->w = tw->h = tw->z = 0;
    tw->slope = 0;
    tw->ceilingTextureID = 0;
    tw->floorTextureID = 0;
    tw->startHeight = tw->endHeight = tw->tallerHeight = 0;
    tw->invertedSlope = 0;
    tw->points = NULL;
    tw->num_points = 0;
    tw->thinWalls = NULL;
    tw->num_thin_walls = 0;
    tw->thin_walls_capacity = 0;
}

void ray_thick_wall_free(RAY_ThickWall *tw)
{
    if (tw->points) {
        free(tw->points);
        tw->points = NULL;
    }
    if (tw->thinWalls) {
        free(tw->thinWalls);
        tw->thinWalls = NULL;
    }
}

/* Helper para añadir ThinWall a ThickWall */
static void thick_wall_add_thin_wall(RAY_ThickWall *tw, const RAY_ThinWall *thin)
{
    if (tw->num_thin_walls >= tw->thin_walls_capacity) {
        int new_capacity = tw->thin_walls_capacity == 0 ? 4 : tw->thin_walls_capacity * 2;
        RAY_ThinWall *new_array = (RAY_ThinWall*)realloc(tw->thinWalls, 
                                                          new_capacity * sizeof(RAY_ThinWall));
        if (!new_array) return;
        tw->thinWalls = new_array;
        tw->thin_walls_capacity = new_capacity;
    }
    
    tw->thinWalls[tw->num_thin_walls] = *thin;
    tw->num_thin_walls++;
}

void ray_thick_wall_create_rect(RAY_ThickWall *tw, float x, float y, float w, float h,
                                float z, float wallHeight)
{
    tw->type = RAY_THICK_WALL_TYPE_RECT;
    tw->x = x;
    tw->y = y;
    tw->w = w;
    tw->h = h;
    
    RAY_Point topLeft = {x, y};
    RAY_Point topRight = {x + w, y};
    RAY_Point bottomLeft = {x, y + h};
    RAY_Point bottomRight = {x + w, y + h};
    
    int wallType = 0;
    
    /* West */
    RAY_ThinWall west;
    ray_thin_wall_create(&west, topLeft.x, topLeft.y, bottomLeft.x, bottomLeft.y,
                        wallType, tw, wallHeight);
    thick_wall_add_thin_wall(tw, &west);
    
    /* East */
    RAY_ThinWall east;
    ray_thin_wall_create(&east, topRight.x, topRight.y, bottomRight.x, bottomRight.y,
                        wallType, tw, wallHeight);
    thick_wall_add_thin_wall(tw, &east);
    
    /* North */
    RAY_ThinWall north;
    ray_thin_wall_create(&north, topLeft.x, topLeft.y, topRight.x, topRight.y,
                        wallType, tw, wallHeight);
    north.horizontal = 1;
    thick_wall_add_thin_wall(tw, &north);
    
    /* South */
    RAY_ThinWall south;
    ray_thin_wall_create(&south, bottomLeft.x, bottomLeft.y, bottomRight.x, bottomRight.y,
                        wallType, tw, wallHeight);
    south.horizontal = 1;
    thick_wall_add_thin_wall(tw, &south);
    
    ray_thick_wall_set_height(tw, wallHeight);
    ray_thick_wall_set_z(tw, z);
}

void ray_thick_wall_create_triangle(RAY_ThickWall *tw, const RAY_Point *v1,
                                    const RAY_Point *v2, const RAY_Point *v3,
                                    float z, float wallHeight)
{
    tw->type = RAY_THICK_WALL_TYPE_TRIANGLE;
    tw->num_points = 3;
    tw->points = (RAY_Point*)malloc(3 * sizeof(RAY_Point));
    tw->points[0] = *v1;
    tw->points[1] = *v2;
    tw->points[2] = *v3;
    
    int wallType = 0;
    
    RAY_ThinWall tw1, tw2, tw3;
    ray_thin_wall_create(&tw1, v1->x, v1->y, v2->x, v2->y, wallType, tw, wallHeight);
    ray_thin_wall_create(&tw2, v2->x, v2->y, v3->x, v3->y, wallType, tw, wallHeight);
    ray_thin_wall_create(&tw3, v3->x, v3->y, v1->x, v1->y, wallType, tw, wallHeight);
    
    tw2.horizontal = 1;
    
    thick_wall_add_thin_wall(tw, &tw1);
    thick_wall_add_thin_wall(tw, &tw2);
    thick_wall_add_thin_wall(tw, &tw3);
    
    ray_thick_wall_set_height(tw, wallHeight);
    ray_thick_wall_set_z(tw, z);
}

void ray_thick_wall_create_quad(RAY_ThickWall *tw, const RAY_Point *v1,
                                const RAY_Point *v2, const RAY_Point *v3,
                                const RAY_Point *v4, float z, float wallHeight)
{
    tw->type = RAY_THICK_WALL_TYPE_QUAD;
    tw->num_points = 4;
    tw->points = (RAY_Point*)malloc(4 * sizeof(RAY_Point));
    tw->points[0] = *v1;
    tw->points[1] = *v2;
    tw->points[2] = *v3;
    tw->points[3] = *v4;
    
    int wallType = 0;
    
    RAY_ThinWall tw1, tw2, tw3, tw4;
    ray_thin_wall_create(&tw1, v1->x, v1->y, v2->x, v2->y, wallType, tw, wallHeight);
    ray_thin_wall_create(&tw2, v2->x, v2->y, v3->x, v3->y, wallType, tw, wallHeight);
    ray_thin_wall_create(&tw3, v3->x, v3->y, v4->x, v4->y, wallType, tw, wallHeight);
    ray_thin_wall_create(&tw4, v4->x, v4->y, v1->x, v1->y, wallType, tw, wallHeight);
    
    tw3.horizontal = 1;
    tw4.horizontal = 1;
    
    thick_wall_add_thin_wall(tw, &tw1);
    thick_wall_add_thin_wall(tw, &tw2);
    thick_wall_add_thin_wall(tw, &tw3);
    thick_wall_add_thin_wall(tw, &tw4);
    
    ray_thick_wall_set_height(tw, wallHeight);
    ray_thick_wall_set_z(tw, z);
}

void ray_thick_wall_set_z(RAY_ThickWall *tw, float z)
{
    tw->z = z;
    for (int i = 0; i < tw->num_thin_walls; i++) {
        tw->thinWalls[i].z = z;
    }
}

void ray_thick_wall_set_height(RAY_ThickWall *tw, float height)
{
    tw->height = height;
    for (int i = 0; i < tw->num_thin_walls; i++) {
        tw->thinWalls[i].height = height;
    }
}

void ray_thick_wall_set_thin_walls_type(RAY_ThickWall *tw, int wallType)
{
    for (int i = 0; i < tw->num_thin_walls; i++) {
        tw->thinWalls[i].wallType = wallType;
    }
}

int ray_thick_wall_contains_point(RAY_ThickWall *tw, float px, float py)
{
    if (tw->type == RAY_THICK_WALL_TYPE_RECT) {
        return ray_point_in_rect(px, py, tw->x, tw->y, tw->w, tw->h);
    }
    if (tw->type == RAY_THICK_WALL_TYPE_TRIANGLE) {
        return ray_point_in_triangle(&(RAY_Point){px, py},
                                     &tw->points[2], &tw->points[1], &tw->points[0]);
    }
    if (tw->type == RAY_THICK_WALL_TYPE_QUAD) {
        return ray_point_in_quad(&(RAY_Point){px, py},
                                &tw->points[0], &tw->points[1],
                                &tw->points[2], &tw->points[3]);
    }
    return 0;
}

/* TODO: Implementar slopes (createRectSlope, createRectInvertedSlope) */
void ray_thick_wall_create_rect_slope(RAY_ThickWall *tw, int slopeType,
                                      float x, float y, float w, float h, float z,
                                      float startHeight, float endHeight)
{
    /* Implementación completa pendiente - por ahora crear rect normal */
    ray_thick_wall_create_rect(tw, x, y, w, h, z, endHeight);
    tw->slopeType = slopeType;
    tw->startHeight = startHeight;
    tw->endHeight = endHeight;
}

void ray_thick_wall_create_rect_inverted_slope(RAY_ThickWall *tw, int slopeType,
                                               float x, float y, float w, float h, float z,
                                               float startHeight, float endHeight)
{
    /* Implementación completa pendiente - por ahora crear rect normal */
    ray_thick_wall_create_rect(tw, x, y, w, h, z, endHeight);
    tw->slopeType = slopeType;
    tw->startHeight = startHeight;
    tw->endHeight = endHeight;
    tw->invertedSlope = 1;
}
