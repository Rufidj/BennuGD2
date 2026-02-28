/*
 * libmod_ray_render_gpu.c - GPU Renderer for BennuGD2 (SDL_gpu)
 *
 * Uses OpenGL depth buffer for correct wall ordering.
 * - Floor/Ceiling: Z-bands with screen-space depth values
 * - Walls: Perspective-projected quads with Z depth
 * - Portals: Recursive rendering using Depth-Buffer Hole Punching
 * - Children: Build Engine nested sector support
 *
 */

#include "libmod_ray.h"
#include "libmod_ray_gltf.h"
#include "libmod_ray_md3.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* OpenGL for depth buffer control */
#ifdef _WIN32
#include <GL/glew.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#include "SDL_gpu.h"

extern RAY_Engine g_engine;
extern GPU_Target *gRenderer;

/* Forward declarations */
void ray_render_scene_gpu(GPU_Target *target, int current_sector);
static void render_sprite_gpu(GPU_Target *target, RAY_Sprite *sprite);

/* ============================================================================
   CONSTANTS
   ============================================================================
 */

#define MAX_PORTAL_DEPTH 16
#define MAX_SECTOR_VERTS 256
#define NEAR_PLANE 0.1f
#define FAR_PLANE 10000.0f

/* Global fog (determined per-frame from camera sector) */
static float s_fog_r = 0.5f, s_fog_g = 0.5f, s_fog_b = 0.5f;
static float s_fog_density = 0.0f;
static float s_fog_start = 100.0f;
static float s_fog_end = 1000.0f;

/* (Moved lower down to be after camera variables) */

/* ============================================================================
   VISITED BITSET
   ============================================================================
 */

static uint32_t s_visited[RAY_MAX_SECTORS / 32 + 1];

static inline void visited_clear(void) {
  memset(s_visited, 0, sizeof(s_visited));
}

static inline int visited_test(int sector_id) {
  if (sector_id < 0 || sector_id >= RAY_MAX_SECTORS)
    return 1;
  return (s_visited[sector_id / 32] >> (sector_id % 32)) & 1;
}

static inline void visited_set(int sector_id) {
  if (sector_id < 0 || sector_id >= RAY_MAX_SECTORS)
    return;
  s_visited[sector_id / 32] |= (1u << (sector_id % 32));
}

/* ============================================================================
   CLIP RECT HELPERS
   ============================================================================
 */

typedef struct {
  float x1, y1, x2, y2;
} ClipRect;

static inline ClipRect clip_intersect(ClipRect a, ClipRect b) {
  ClipRect r;
  r.x1 = (a.x1 > b.x1) ? a.x1 : b.x1;
  r.y1 = (a.y1 > b.y1) ? a.y1 : b.y1;
  r.x2 = (a.x2 < b.x2) ? a.x2 : b.x2;
  r.y2 = (a.y2 < b.y2) ? a.y2 : b.y2;
  return r;
}

static inline int clip_valid(ClipRect c) {
  return (c.x2 > c.x1) && (c.y2 > c.y1);
}

/* ============================================================================
   HELPER: Get GPU texture from FPG ID
   ============================================================================
 */

static GPU_Image *get_gpu_texture(int file_id, int texture_id) {
  if (texture_id <= 0)
    return NULL;

  int fid = (file_id > 0) ? file_id : g_engine.fpg_id;
  GRAPH *gr = bitmap_get((int64_t)fid, (int64_t)texture_id);
  if (!gr || !gr->tex)
    return NULL;
  return (GPU_Image *)gr->tex;
}

/* ============================================================================
   HELPER: Find sector by ID
   ============================================================================
 */

static RAY_Sector *find_sector_by_id(int sector_id) {
  for (int i = 0; i < g_engine.num_sectors; i++) {
    if (g_engine.sectors[i].sector_id == sector_id)
      return &g_engine.sectors[i];
  }
  return NULL;
}

/* ============================================================================
   HELPER: Get other sector through portal
   ============================================================================
 */

static int portal_get_other_sector(int portal_id, int current_sector_id) {
  for (int i = 0; i < g_engine.num_portals; i++) {
    RAY_Portal *p = &g_engine.portals[i];
    if (p->portal_id == portal_id) {
      if (p->sector_a == current_sector_id)
        return p->sector_b;
      if (p->sector_b == current_sector_id)
        return p->sector_a;
      return -1;
    }
  }
  return -1;
}

/* ============================================================================
   CAMERA STATE (cached per frame)
   ============================================================================
 */

static float s_cam_x, s_cam_y, s_cam_z;
static float s_cos_ang, s_sin_ang;
static float s_focal;
static int s_half_w, s_half_h, s_screen_w, s_screen_h;
static int s_horizon;

/* ============================================================================
   ISLAND SECTOR CACHE (pre-built each frame for sprite occlusion)
   ============================================================================
 */
#define MAX_ISLAND_SECTORS 256
static RAY_Sector *s_island_sectors[MAX_ISLAND_SECTORS];
static float s_island_floor[MAX_ISLAND_SECTORS];
static float s_island_ceil[MAX_ISLAND_SECTORS];
static int s_num_islands = 0;

/* ============================================================================
   NORMAL MAPPING SHADER
   ============================================================================
 */

static uint32_t s_normal_shader = 0;
static GPU_ShaderBlock s_normal_block;
static int s_u_tex = -1;
static int s_u_normalMap = -1;
static int s_u_useNormalMap = -1;
static int s_u_lightPosArr = -1;
static int s_u_lightColorArr = -1;
static int s_u_lightIntensityArr = -1;
static int s_u_numLights = -1;
static int s_u_tangent = -1;
static int s_u_bitangent = -1;
static int s_u_normal = -1;
static int s_u_focal = -1;
static int s_u_halfW = -1;
static int s_u_horizon = -1;
static int s_u_time = -1;
static int s_u_sectorFlags = -1;
static int s_u_liquidIntensity = -1;
static int s_u_liquidSpeed = -1;
static int s_u_fogColor = -1;
static int s_u_fogDensity = -1;
static int s_u_fogStart = -1;
static int s_u_fogEnd = -1;

static const char *vertex_shader_source =
    "#version 120\n"
    "attribute vec3 bgd_Vertex;\n"
    "attribute vec2 bgd_TexCoord;\n"
    "attribute vec4 bgd_Color;\n"
    "varying vec2 uv;\n"
    "varying vec3 fragPos;\n"
    "varying vec4 colorVarying;\n"
    "uniform mat4 bgd_ModelViewProjectionMatrix;\n"
    "void main() {\n"
    "    uv = bgd_TexCoord;\n"
    "    fragPos = bgd_Vertex;\n"
    "    colorVarying = bgd_Color;\n"
    "    gl_Position = bgd_ModelViewProjectionMatrix * vec4(bgd_Vertex, 1.0);\n"
    "}\n";

static const char *fragment_shader_source =
    "#version 120\n"
    "varying vec2 uv;\n"
    "varying vec3 fragPos;\n"
    "varying vec4 colorVarying;\n"
    "uniform sampler2D tex;\n"
    "uniform sampler2D normalMap;\n"
    "uniform int useNormalMap;\n"
    "uniform vec3 u_tangent;\n"
    "uniform vec3 u_bitangent;\n"
    "uniform vec3 u_normal;\n"
    "uniform vec3 u_lightPos[16];\n"
    "uniform vec3 u_lightColor[16];\n"
    "uniform float u_lightIntensity[16];\n"
    "uniform int u_numLights;\n"
    "uniform float u_focal;\n"
    "uniform float u_halfW;\n"
    "uniform float u_horizon;\n"
    "uniform float u_time;\n"
    "uniform int u_sectorFlags;\n"
    "uniform float u_liquidIntensity;\n"
    "uniform float u_liquidSpeed;\n"
    "uniform vec3 u_fogColor;\n"
    "uniform float u_fogDensity;\n"
    "uniform float u_fogStart;\n"
    "uniform float u_fogEnd;\n"
    "void main() {\n"
    "    vec2 finalUV = uv;\n"
    "    float fFlags = float(u_sectorFlags) + 0.1;\n"
    "    bool isWater = mod(fFlags, 2.0) >= 1.0;\n"
    "    bool isLava  = mod(floor(fFlags / 2.0), 2.0) >= 1.0;\n"
    "    bool isAcid  = mod(floor(fFlags / 4.0), 2.0) >= 1.0;\n"
    "    bool scrollX = mod(floor(fFlags / 8.0), 2.0) >= 1.0;\n"
    "    bool scrollY = mod(floor(fFlags / 16.0), 2.0) >= 1.0;\n"
    "    bool ripples = mod(floor(fFlags / 256.0), 2.0) >= 1.0;\n"
    "\n"
    "    float effectiveTime = u_time * u_liquidSpeed;\n"
    "    float freqMul = (abs(u_normal.y) > 0.5) ? 0.6 : 1.0;\n"
    "    if (ripples) {\n"
    "        if (isWater || (!isLava && !isAcid)) {\n"
    "            finalUV.x += sin(uv.y * 6.0 * freqMul + effectiveTime * 4.0) "
    "* 0.20 * max(u_liquidIntensity, 0.2);\n"
    "            finalUV.y += cos(uv.x * 6.0 * freqMul + effectiveTime * 3.5) "
    "* 0.20 * max(u_liquidIntensity, 0.2);\n"
    "        } else if (isLava) {\n"
    "            finalUV.x += sin(uv.y * 3.0 * freqMul + effectiveTime * 2.0) "
    "* 0.25 * max(u_liquidIntensity, 0.2);\n"
    "            finalUV.y += cos(uv.x * 3.0 * freqMul + effectiveTime * 1.5) "
    "* 0.25 * max(u_liquidIntensity, 0.2);\n"
    "        } else if (isAcid) {\n"
    "            finalUV.x += sin(uv.y * 12.0 * freqMul + effectiveTime * 8.0) "
    "* 0.10 * max(u_liquidIntensity, 0.2);\n"
    "            finalUV.y += cos(uv.x * 12.0 * freqMul + effectiveTime * 8.0) "
    "* 0.10 * max(u_liquidIntensity, 0.2);\n"
    "        }\n"
    "    }\n"
    "    if (scrollX) finalUV.x += effectiveTime * 1.0;\n"
    "    if (scrollY) finalUV.y += effectiveTime * 1.0;\n"
    "\n"
    "    vec4 texColor = texture2D(tex, finalUV) * colorVarying;\n"
    "    if (isWater || isLava || isAcid) texColor.a *= 0.5;\n"
    "    if (u_time < 0.001) texColor.rgb *= 0.1; // Visual Debug: if time "
    "stalls, darken\n"
    "    if (texColor.a < 0.01) discard;\n"
    "\n"
    "    // Volumetric Fog - Silent Hill style\n"
    "    vec3 finalRGB = texColor.rgb;\n"
    "    if (u_fogDensity > 0.001) {\n"
    "        // Reconstruct world distance from depth Z\n"
    "        float t = (100.0 - fragPos.z) / 200.0;\n"
    "        float worldDist = exp(mix(log(0.1), log(10000.0), clamp(t, 0.0, "
    "1.0)));\n"
    "        \n"
    "        // Exponential squared fog (like real atmospheric scattering)\n"
    "        float fogAmount = u_fogDensity / 100.0;\n"
    "        float expFog = 1.0 - exp(-pow(worldDist * fogAmount * 0.003, "
    "2.0));\n"
    "        \n"
    "        // Procedural noise for volumetric swirling effect\n"
    "        float wx = (fragPos.x - u_halfW) * worldDist / u_focal;\n"
    "        float wy = worldDist;\n"
    "        float noiseScale = 0.008;\n"
    "        float t1 = u_time * 0.15;\n"
    "        float n1 = sin(wx * noiseScale + t1) * cos(wy * noiseScale * 0.7 "
    "+ t1 * 0.8);\n"
    "        float n2 = sin(wx * noiseScale * 2.3 + t1 * 1.3 + 1.7) * cos(wy * "
    "noiseScale * 1.9 - t1 * 0.6);\n"
    "        float n3 = sin(wx * noiseScale * 4.7 - t1 * 0.9 + 3.1) * cos(wy * "
    "noiseScale * 3.8 + t1 * 1.1);\n"
    "        float noise = (n1 + n2 * 0.5 + n3 * 0.25) / 1.75;\n"
    "        \n"
    "        // Modulate fog with noise for volumetric patches\n"
    "        float volumetric = expFog * (1.0 + noise * 0.4 * expFog);\n"
    "        volumetric = clamp(volumetric, 0.0, 1.0);\n"
    "        \n"
    "        // Distance-based fade in (no fog before fogStart)\n"
    "        float startFade = clamp((worldDist - u_fogStart) / max(u_fogStart "
    "* 0.5, 10.0), 0.0, 1.0);\n"
    "        volumetric *= startFade;\n"
    "        \n"
    "        finalRGB = mix(finalRGB, u_fogColor, volumetric);\n"
    "    }\n"
    "\n"
    "    if (u_numLights <= 0) {\n"
    "        gl_FragColor = vec4(finalRGB, texColor.a);\n"
    "        return;\n"
    "    }\n"
    "    float t_z = (100.0 - fragPos.z) / 200.0;\n"
    "    float tz = exp(t_z * 11.512925 - 2.302585);\n"
    "    float tx = (fragPos.x - u_halfW) * tz / u_focal;\n"
    "    float ty = (u_horizon - fragPos.y) * tz / u_focal;\n"
    "    vec3 vPos = vec3(tx, ty, tz);\n"
    "    vec3 worldNormal = normalize(u_normal);\n"
    "    if (useNormalMap != 0) {\n"
    "        vec3 nm = texture2D(normalMap, uv).rgb * 2.0 - 1.0;\n"
    "        worldNormal = normalize(nm.x * u_tangent + nm.y * u_bitangent + "
    "nm.z * u_normal);\n"
    "    }\n"
    "    vec3 finalLight = vec3(0.2, 0.2, 0.2);\n"
    "    for(int i = 0; i < 16; i++) {\n"
    "        if (i >= u_numLights) break;\n"
    "        vec3 lightVector = u_lightPos[i] - vPos;\n"
    "        float distSq = dot(lightVector, lightVector);\n"
    "        float radSq = u_lightIntensity[i] * u_lightIntensity[i];\n"
    "        float attenuation = radSq / (radSq + distSq + 1.0);\n"
    "        vec3 L = normalize(lightVector);\n"
    "        float diff = max(dot(worldNormal, L), 0.0);\n"
    "        finalLight += u_lightColor[i] * (0.5 + 0.5 * diff) * attenuation "
    "* 2.0;\n"
    "    }\n"
    "    vec3 litColor = finalRGB * finalLight;\n"
    "    gl_FragColor = vec4(litColor, texColor.a);\n"
    "}\n";

static void init_normal_shader(void) {
  if (s_normal_shader != 0)
    return;

  uint32_t v = GPU_CompileShader(GPU_VERTEX_SHADER, vertex_shader_source);
  uint32_t f = GPU_CompileShader(GPU_FRAGMENT_SHADER, fragment_shader_source);
  s_normal_shader = GPU_LinkShaders(v, f);

  if (s_normal_shader == 0) {
    printf("RAY: Shader Link Error: %s\n", GPU_GetShaderMessage());
    return;
  }

  s_normal_block =
      GPU_LoadShaderBlock(s_normal_shader, "bgd_Vertex", "bgd_TexCoord",
                          "bgd_Color", "bgd_ModelViewProjectionMatrix");

  s_u_tex = GPU_GetUniformLocation(s_normal_shader, "tex");
  s_u_normalMap = GPU_GetUniformLocation(s_normal_shader, "normalMap");
  s_u_useNormalMap = GPU_GetUniformLocation(s_normal_shader, "useNormalMap");
  s_u_lightPosArr = GPU_GetUniformLocation(s_normal_shader, "u_lightPos");
  s_u_lightColorArr = GPU_GetUniformLocation(s_normal_shader, "u_lightColor");
  s_u_lightIntensityArr =
      GPU_GetUniformLocation(s_normal_shader, "u_lightIntensity");
  s_u_numLights = GPU_GetUniformLocation(s_normal_shader, "u_numLights");
  s_u_tangent = GPU_GetUniformLocation(s_normal_shader, "u_tangent");
  s_u_bitangent = GPU_GetUniformLocation(s_normal_shader, "u_bitangent");
  s_u_normal = GPU_GetUniformLocation(s_normal_shader, "u_normal");
  s_u_focal = GPU_GetUniformLocation(s_normal_shader, "u_focal");
  s_u_halfW = GPU_GetUniformLocation(s_normal_shader, "u_halfW");
  s_u_horizon = GPU_GetUniformLocation(s_normal_shader, "u_horizon");
  s_u_time = GPU_GetUniformLocation(s_normal_shader, "u_time");
  s_u_sectorFlags = GPU_GetUniformLocation(s_normal_shader, "u_sectorFlags");
  s_u_liquidIntensity =
      GPU_GetUniformLocation(s_normal_shader, "u_liquidIntensity");
  s_u_liquidSpeed = GPU_GetUniformLocation(s_normal_shader, "u_liquidSpeed");
  s_u_fogColor = GPU_GetUniformLocation(s_normal_shader, "u_fogColor");
  s_u_fogDensity = GPU_GetUniformLocation(s_normal_shader, "u_fogDensity");
  s_u_fogStart = GPU_GetUniformLocation(s_normal_shader, "u_fogStart");
  s_u_fogEnd = GPU_GetUniformLocation(s_normal_shader, "u_fogEnd");
}

static void activate_normal_shader(GPU_Image *normalMap, float tx, float ty,
                                   float tz, float bx, float by, float bz,
                                   float nx, float ny, float nz,
                                   int sectorFlags, float liquidIntensity,
                                   float liquidSpeed, float fogR, float fogG,
                                   float fogB, float fogDensity, float fogStart,
                                   float fogEnd) {
  init_normal_shader();
  if (s_normal_shader == 0)
    return;
  GPU_ActivateShaderProgram(s_normal_shader, &s_normal_block);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  GPU_SetUniformi(s_u_tex, 0);       /* Texture unit 0 */
  GPU_SetUniformi(s_u_normalMap, 1); /* Texture unit 1 */

  if (normalMap) {
    GPU_SetUniformi(s_u_useNormalMap, 1);
    /* Bind normal map to unit 1 using SDL_gpu */
    GPU_SetShaderImage(normalMap, s_u_normalMap, 1);
    GPU_SetBlendMode(normalMap, GPU_BLEND_NORMAL);
  } else {
    GPU_SetUniformi(s_u_useNormalMap, 0);
  }

  GPU_SetUniformi(s_u_numLights, g_engine.num_lights);
  if (g_engine.num_lights > 0) {
    float lp[16 * 3];
    float lc[16 * 3];
    float li[16];
    for (int i = 0; i < g_engine.num_lights && i < 16; i++) {
      /* Transform light pos to View Space for the shader */
      float dx = g_engine.lights[i].x - s_cam_x;
      float dy = g_engine.lights[i].y - s_cam_y;

      /* Camera space: X=Right, Z=Forward, Y=Up (relative to camera) */
      lp[i * 3 + 0] = dx * s_sin_ang - dy * s_cos_ang; /* Right */
      lp[i * 3 + 1] = g_engine.lights[i].z - s_cam_z;  /* Up */
      lp[i * 3 + 2] = dx * s_cos_ang + dy * s_sin_ang; /* Forward */

      /* Pass color directly: R,G,B are already correct in the engine */
      lc[i * 3 + 0] = g_engine.lights[i].r;
      lc[i * 3 + 1] = g_engine.lights[i].g;
      lc[i * 3 + 2] = g_engine.lights[i].b;
      li[i] = g_engine.lights[i].intensity;
    }
    GPU_SetUniformfv(s_u_lightPosArr, 3, g_engine.num_lights, lp);
    GPU_SetUniformfv(s_u_lightColorArr, 3, g_engine.num_lights, lc);
    GPU_SetUniformfv(s_u_lightIntensityArr, 1, g_engine.num_lights, li);
  }

  GPU_SetUniformf(s_u_focal, s_focal);
  GPU_SetUniformf(s_u_halfW, (float)s_half_w);
  GPU_SetUniformf(s_u_horizon, (float)s_horizon);
  GPU_SetUniformf(s_u_time, g_engine.time);
  GPU_SetUniformi(s_u_sectorFlags, sectorFlags);
  GPU_SetUniformf(s_u_liquidIntensity, liquidIntensity);
  GPU_SetUniformf(s_u_liquidSpeed, liquidSpeed);
  float fogCol[3] = {fogR, fogG, fogB};
  GPU_SetUniformfv(s_u_fogColor, 3, 1, fogCol);
  GPU_SetUniformf(s_u_fogDensity, fogDensity);
  GPU_SetUniformf(s_u_fogStart, fogStart);
  GPU_SetUniformf(s_u_fogEnd, fogEnd);

  /* Transform TBN Matrix to View Space */
  /* World vectors (nx, ny, tx, ty, bx, by) -> View space (X=Right, Z=Forward)
   */
  /* Y_view (Up) maps directly from Z_world */

  float vtx = tx * s_sin_ang - ty * s_cos_ang;
  float vty = tz;
  float vtz = tx * s_cos_ang + ty * s_sin_ang;

  float vbx = bx * s_sin_ang - by * s_cos_ang;
  float vby = bz;
  float vbz = bx * s_cos_ang + by * s_sin_ang;

  float vnx = nx * s_sin_ang - ny * s_cos_ang;
  float vny = nz;
  float vnz = nx * s_cos_ang + ny * s_sin_ang;

  float t[3] = {vtx, vty, vtz};
  GPU_SetUniformfv(s_u_tangent, 3, 1, t);
  float b[3] = {vbx, vby, vbz};
  GPU_SetUniformfv(s_u_bitangent, 3, 1, b);
  float n[3] = {vnx, vny, vnz};
  GPU_SetUniformfv(s_u_normal, 3, 1, n);
}

static void deactivate_normal_shader(void) {
  GPU_ActivateShaderProgram(0, NULL);
}

/* ============================================================================
   DEPTH CONVERSION: camera-space tz -> normalized 0..1 for GL depth buffer
   Using linear mapping for simple hardware integration.
   ============================================================================
 */

static inline float depth_from_tz(float tz) {
  /* SDL_gpu ortho projection: z_near=-100, z_far=100.
     The ortho matrix maps Z=+100 → depth=0.0 (near in GL),
                           Z=-100 → depth=1.0 (far in GL).
     With GL_LESS, smaller depth wins. So NEAR objects need Z=+100
     (maps to depth=0.0) and FAR objects need Z=-100 (maps to depth=1.0).
     This is INVERTED from the naive mapping! */
  if (tz <= NEAR_PLANE)
    return 100.0f; /* Nearest → highest Z → depth=0.0 (wins) */
  const float max_dist = 10000.0f;
  float log_near = logf(NEAR_PLANE);
  float log_far = logf(max_dist);
  float t = (logf(tz) - log_near) / (log_far - log_near); /* 0..1 */
  t = (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
  return 100.0f - t * 200.0f; /* Near→+100(depth=0), Far→-100(depth=1) */
}

/* ============================================================================
   RENDER SECTOR POLYGON (Floor or Ceiling) with depth values
   ============================================================================
 */

typedef struct {
  float tx0, tz0, tx1, tz1;
} XFormWall;

static void render_sector_plane(GPU_Target *target, RAY_Sector *sector,
                                float plane_z, GPU_Image *tex,
                                GPU_Image *normal_tex, XFormWall *xf,
                                int num_xf, int depth, int is_island,
                                ClipRect clip, int num_cutouts,
                                XFormWall *cutout_xf) {
  if (!tex || !sector)
    return;

  /* For floor/ceiling planes:
     Normal = (0, 0, 1) for floor (h < 0), (0, 0, -1) for ceiling (h > 0)
     Tangent = (1, 0, 0)
     Bitangent = (0, 1, 0) for floor, (0, -1, 0) for ceiling to keep it
     right-handed */
  float h = plane_z - s_cam_z;
  if (fabsf(h) < 0.001f)
    return;

  float nx_p = 0, ny_p = 0, nz_p = (h > 0) ? -1.0f : 1.0f;
  float bx_p = 0, by_p = (h > 0) ? -1.0f : 1.0f, bz_p = 0;

  int activeFlags = sector->flags & (24 | 256); /* Scroll bits & Ripples bit */
  if (h > 0) {                                  // Ceiling
    if (sector->flags & 64)
      activeFlags |= (sector->flags & 7);
  } else { // Floor
    if (sector->flags & 32)
      activeFlags |= (sector->flags & 7);
  }

  activate_normal_shader(normal_tex, 1.0f, 0.0f, 0.0f, bx_p, by_p, bz_p, nx_p,
                         ny_p, nz_p, activeFlags, sector->liquid_intensity,
                         sector->liquid_speed, s_fog_r, s_fog_g, s_fog_b,
                         s_fog_density, s_fog_start, s_fog_end);

  if (depth > 0) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f, -2.0f);
  }

/* RECURSION SAFE: Strictly local buffers to avoid corruption in nested calls */
#define SCAN_LIMIT 128
  float v_local[SCAN_LIMIT * 20];
  unsigned short i_local[SCAN_LIMIT * 6];
  int v_idx = 0, i_idx = 0;

  float intercepts[MAX_SECTOR_VERTS * 4];
  float horizon_f = (float)s_horizon;

  for (int y = (int)clip.y1; y <= (int)clip.y2; y++) {
    float dy = (float)y - horizon_f;
    if (fabsf(dy) < 0.1f)
      continue;

    /* Symmetric visibility: rz is valid if it projects in front of camera */
    float rz = -(h * s_focal) / dy;
    if (rz < NEAR_PLANE || rz > FAR_PLANE)
      continue;

    int hits = 0;
    if (hits < MAX_SECTOR_VERTS * 4 - 2) {
      if (xf && num_xf > 0 && is_island) {
        /* ISLANDS ONLY: Use wall intersections to clip floor/ceiling
           to the sector polygon shape. */
        for (int i = 0; i < num_xf; i++) {
          float z0 = xf[i].tz0, z1 = xf[i].tz1;
          if ((z0 <= rz && z1 >= rz) || (z1 <= rz && z0 >= rz)) {
            if (fabsf(z1 - z0) > 1e-4f) {
              float t = (rz - z0) / (z1 - z0);
              intercepts[hits++] =
                  s_half_w +
                  ((xf[i].tx0 + t * (xf[i].tx1 - xf[i].tx0)) * s_focal / rz);
            } else {
              intercepts[hits++] = s_half_w + (xf[i].tx0 * s_focal / rz);
              intercepts[hits++] = s_half_w + (xf[i].tx1 * s_focal / rz);
            }
          }
        }
      } else {
        /* Regular sectors: fill full clip width, subtract cutouts.
           The walls render on top and visually bound the floor. */
        intercepts[hits++] = clip.x1;
        intercepts[hits++] = clip.x2;

        /* Punch holes for child sectors (cutouts) */
        if (cutout_xf && num_cutouts > 0) {
          for (int i = 0; i < num_cutouts; i++) {
            float z0 = cutout_xf[i].tz0, z1 = cutout_xf[i].tz1;
            if ((z0 <= rz && z1 >= rz) || (z1 <= rz && z0 >= rz)) {
              if (fabsf(z1 - z0) > 1e-4f) {
                float t = (rz - z0) / (z1 - z0);
                intercepts[hits++] =
                    s_half_w + ((cutout_xf[i].tx0 +
                                 t * (cutout_xf[i].tx1 - cutout_xf[i].tx0)) *
                                s_focal / rz);
              } else {
                intercepts[hits++] =
                    s_half_w + (cutout_xf[i].tx0 * s_focal / rz);
                intercepts[hits++] =
                    s_half_w + (cutout_xf[i].tx1 * s_focal / rz);
              }
            }
          }
        }
      }
    }

    if (hits < 2)
      continue;

    /* Sort scanline intercepts for correct filling */
    for (int i = 0; i < hits - 1; i++) {
      for (int j = 0; j < hits - i - 1; j++) {
        if (intercepts[j] > intercepts[j + 1]) {
          float tmp = intercepts[j];
          intercepts[j] = intercepts[j + 1];
          intercepts[j + 1] = tmp;
        }
      }
    }

    float sz = depth_from_tz(rz);
    for (int i = 0; i < hits - 1; i += 2) {
      float x0 = intercepts[i], x1 = intercepts[i + 1];
      if (x0 < clip.x1)
        x0 = clip.x1;
      if (x1 > clip.x2)
        x1 = clip.x2;
      if (x0 >= x1)
        continue;

      /* FIXED WORLD UVs: Prevents texture sliding (wx, wy anchored to world)
       */
      float l_rel = (x0 - s_half_w) * rz / s_focal;
      float r_rel = (x1 - s_half_w) * rz / s_focal;
      float wx0 = s_cam_x + (rz * s_cos_ang - l_rel * s_sin_ang);
      float wy0 = s_cam_y + (rz * s_sin_ang + l_rel * s_cos_ang);
      float wx1 = s_cam_x + (rz * s_cos_ang - r_rel * s_sin_ang);
      float wy1 = s_cam_y + (rz * s_sin_ang + r_rel * s_cos_ang);

      int bv = v_idx, bi = (v_idx / 5);
      v_local[bv + 0] = x0;
      v_local[bv + 1] = (float)y;
      v_local[bv + 2] = sz;
      v_local[bv + 3] = wx0 / 64.0f;
      v_local[bv + 4] = wy0 / 64.0f;
      v_local[bv + 5] = x1;
      v_local[bv + 6] = (float)y;
      v_local[bv + 7] = sz;
      v_local[bv + 8] = wx1 / 64.0f;
      v_local[bv + 9] = wy1 / 64.0f;
      v_local[bv + 10] = x1;
      v_local[bv + 11] = (float)y + 1.0f;
      v_local[bv + 12] = sz;
      v_local[bv + 13] = wx1 / 64.0f;
      v_local[bv + 14] = wy1 / 64.0f;
      v_local[bv + 15] = x0;
      v_local[bv + 16] = (float)y + 1.0f;
      v_local[bv + 17] = sz;
      v_local[bv + 18] = wx0 / 64.0f;
      v_local[bv + 19] = wy0 / 64.0f;
      i_local[i_idx + 0] = bi;
      i_local[i_idx + 1] = bi + 1;
      i_local[i_idx + 2] = bi + 2;
      i_local[i_idx + 3] = bi;
      i_local[i_idx + 4] = bi + 2;
      i_local[i_idx + 5] = bi + 3;
      v_idx += 20;
      i_idx += 6;

      if (v_idx >= SCAN_LIMIT * 20 - 20) {
        GPU_SetWrapMode(tex, GPU_WRAP_REPEAT, GPU_WRAP_REPEAT);
        GPU_SetImageFilter(tex, GPU_FILTER_LINEAR);
        GPU_TriangleBatch(tex, target, v_idx / 5, v_local, i_idx, i_local,
                          GPU_BATCH_XYZ_ST);
        v_idx = 0;
        i_idx = 0;
      }
    }
  }

  if (v_idx > 0) {
    GPU_SetWrapMode(tex, GPU_WRAP_REPEAT, GPU_WRAP_REPEAT);
    GPU_SetImageFilter(tex, GPU_FILTER_LINEAR);
    GPU_TriangleBatch(tex, target, v_idx / 5, v_local, i_idx, i_local,
                      GPU_BATCH_XYZ_ST);
    GPU_FlushBlitBuffer();
  }

  deactivate_normal_shader();

  if (depth > 0)
    glDisable(GL_POLYGON_OFFSET_FILL);
}

/* ============================================================================
   RECURSIVE SECTOR RENDERER
   ============================================================================
 */

/* Render a solid sector lid (top or bottom face) as a projected 3D polygon.
   Uses depth buffer so the closer face automatically wins. */
static void render_island_lid(GPU_Target *target, RAY_Sector *sector,
                              float plane_z, GPU_Image *tex,
                              GPU_Image *normal_tex, XFormWall *xf,
                              int num_walls, ClipRect clip) {
  if (!tex || num_walls < 3 || num_walls > MAX_SECTOR_VERTS)
    return;

  float h = plane_z - s_cam_z;
  if (fabsf(h) < 0.001f)
    return;

  /* Normal mapping for lids:
     Floor (mapping bottom face): Normal=(0,0,1), Tangent=(1,0,0),
     Bitangent=(0,1,0) Ceiling (mapping top face): Normal=(0,0,-1),
     Tangent=(1,0,0), Bitangent=(0,-1,0)
  */
  float nx = 0, ny = 0, nz = (h > 0) ? -1.0f : 1.0f;
  float bx = 0, by = (h > 0) ? -1.0f : 1.0f, bz = 0;

  int activeFlags = (sector->flags & (8 | 16 | 256)); // Scroll & Ripples
  if (h > 0) {                                        // Ceiling face
    if (sector->flags & 64)
      activeFlags |= (sector->flags & 7);
  } else { // Floor face
    if (sector->flags & 32)
      activeFlags |= (sector->flags & 7);
  }

  activate_normal_shader(normal_tex, 1.0f, 0.0f, 0.0f, bx, by, bz, nx, ny, nz,
                         activeFlags, sector->liquid_intensity,
                         sector->liquid_speed, s_fog_r, s_fog_g, s_fog_b,
                         s_fog_density, s_fog_start, s_fog_end);

  /* Calculate bounding box of the sector for fixed texture mapping (0..1) */
  float min_x = sector->walls[0].x1, max_x = sector->walls[0].x1;
  float min_y = sector->walls[0].y1, max_y = sector->walls[0].y1;
  for (int i = 1; i < num_walls; i++) {
    if (sector->walls[i].x1 < min_x)
      min_x = sector->walls[i].x1;
    if (sector->walls[i].x1 > max_x)
      max_x = sector->walls[i].x1;
    if (sector->walls[i].y1 < min_y)
      min_y = sector->walls[i].y1;
    if (sector->walls[i].y1 > max_y)
      max_y = sector->walls[i].y1;
  }
  float dx = max_x - min_x;
  float dy = max_y - min_y;
  if (dx < 1.0f)
    dx = 1.0f;
  if (dy < 1.0f)
    dy = 1.0f;

  /* Project each wall start-vertex at the given plane height */
  float sx[MAX_SECTOR_VERTS], sy[MAX_SECTOR_VERTS], sz_arr[MAX_SECTOR_VERTS],
      wu[MAX_SECTOR_VERTS], wv[MAX_SECTOR_VERTS];
  int valid[MAX_SECTOR_VERTS];
  int num_valid = 0;

  for (int i = 0; i < num_walls; i++) {
    float tx = xf[i].tx0;
    float tz = xf[i].tz0;

    if (tz < NEAR_PLANE) {
      valid[i] = 0;
      continue;
    }
    valid[i] = 1;
    num_valid++;

    float sc = s_focal / tz;
    sx[i] = s_half_w + tx * sc;
    sy[i] = (float)s_horizon - h * sc;
    /* Use 3D planar distance for depth — consistent with walls and floors */
    sz_arr[i] = depth_from_tz(tz);

    /* Normalized UV coords for fixed texture mapping */
    wu[i] = (sector->walls[i].x1 - min_x) / dx;
    wv[i] = (sector->walls[i].y1 - min_y) / dy;
  }

  if (num_valid < 3) {
    deactivate_normal_shader();
    return;
  }

  /* Tessellate each fan triangle (subdivide 4x4=16) to reduce parallax/affine
     warping. By using many small triangles, the linear interpolation error of
     the GPU is minimized, resulting in a stable, non-deformed texture. */
  /* Use actual wall count instead of MAX to avoid stack overflow in recursion
   */
  float *t_vb = (float *)alloca(num_walls * 16 * 3 * 5 * sizeof(float));
  int t_nv = 0;

  for (int i = 1; i < num_walls - 1; i++) {
    int i0 = 0, i1 = i, i2 = i + 1;
    if (!valid[i0] || !valid[i1] || !valid[i2])
      continue;

    /* Subdivide triangle (v0, vi, vi+1) into 16 smaller ones (4x4) */
    for (int r = 0; r < 4; r++) {       /* rows */
      for (int c = 0; c < 4 - r; c++) { /* columns in row */
        /* Barycentric weights for 3 vertices of a sub-triangle */
        float bw[3][3];

        /* Triangle 1 (points down) */
        bw[0][1] = (float)c / 4.0f;
        bw[0][2] = (float)r / 4.0f;
        bw[0][0] = 1.0f - bw[0][1] - bw[0][2];
        bw[1][1] = (float)(c + 1) / 4.0f;
        bw[1][2] = (float)r / 4.0f;
        bw[1][0] = 1.0f - bw[1][1] - bw[1][2];
        bw[2][1] = (float)c / 4.0f;
        bw[2][2] = (float)(r + 1) / 4.0f;
        bw[2][0] = 1.0f - bw[2][1] - bw[2][2];

        for (int v = 0; v < 3; v++) {
          /* Interpolate world-space/projected values for the sub-vertex */
          float v_tx = bw[v][0] * xf[i0].tx0 + bw[v][1] * xf[i1].tx0 +
                       bw[v][2] * xf[i2].tx0;
          float v_tz = bw[v][0] * xf[i0].tz0 + bw[v][1] * xf[i1].tz0 +
                       bw[v][2] * xf[i2].tz0;

          float sc = s_focal / (v_tz < NEAR_PLANE ? NEAR_PLANE : v_tz);
          t_vb[t_nv * 5 + 0] = s_half_w + v_tx * sc;
          t_vb[t_nv * 5 + 1] = (float)s_horizon - h * sc;
          t_vb[t_nv * 5 + 2] = depth_from_tz(v_tz);
          t_vb[t_nv * 5 + 3] =
              bw[v][0] * wu[i0] + bw[v][1] * wu[i1] + bw[v][2] * wu[i2];
          t_vb[t_nv * 5 + 4] =
              bw[v][0] * wv[i0] + bw[v][1] * wv[i1] + bw[v][2] * wv[i2];
          t_nv++;
        }

        /* Triangle 2 (points up, only if not on the diagonal edge) */
        if (c + r < 3) {
          bw[0][1] = (float)(c + 1) / 4.0f;
          bw[0][2] = (float)r / 4.0f;
          bw[0][0] = 1.0f - bw[0][1] - bw[0][2];
          bw[1][1] = (float)(c + 1) / 4.0f;
          bw[1][2] = (float)(r + 1) / 4.0f;
          bw[1][0] = 1.0f - bw[1][1] - bw[1][2];
          bw[2][1] = (float)c / 4.0f;
          bw[2][2] = (float)(r + 1) / 4.0f;
          bw[2][0] = 1.0f - bw[2][1] - bw[2][2];

          for (int v = 0; v < 3; v++) {
            float v_tx = bw[v][0] * xf[i0].tx0 + bw[v][1] * xf[i1].tx0 +
                         bw[v][2] * xf[i2].tx0;
            float v_tz = bw[v][0] * xf[i0].tz0 + bw[v][1] * xf[i1].tz0 +
                         bw[v][2] * xf[i2].tz0;

            float sc = s_focal / (v_tz < NEAR_PLANE ? NEAR_PLANE : v_tz);
            t_vb[t_nv * 5 + 0] = s_half_w + v_tx * sc;
            t_vb[t_nv * 5 + 1] = (float)s_horizon - h * sc;
            t_vb[t_nv * 5 + 2] = depth_from_tz(v_tz);
            t_vb[t_nv * 5 + 3] =
                bw[v][0] * wu[i0] + bw[v][1] * wu[i1] + bw[v][2] * wu[i2];
            t_vb[t_nv * 5 + 4] =
                bw[v][0] * wv[i0] + bw[v][1] * wv[i1] + bw[v][2] * wv[i2];
            t_nv++;
          }
        }
      }
    }
  }

  if (t_nv < 3) {
    deactivate_normal_shader();
    return;
  }

  GPU_SetWrapMode(tex, GPU_WRAP_NONE, GPU_WRAP_NONE);
  GPU_SetImageFilter(tex, GPU_FILTER_LINEAR);

  GPU_FlushBlitBuffer();
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);

  /* Scissor test to match SDL_gpu clip rect (OpenGL Y is flipped) */
  glEnable(GL_SCISSOR_TEST);
  glScissor((int)clip.x1, s_screen_h - (int)clip.y2, (int)(clip.x2 - clip.x1),
            (int)(clip.y2 - clip.y1));

  GPU_TriangleBatch(tex, target, t_nv, t_vb, 0, NULL, GPU_BATCH_XYZ_ST);
  GPU_FlushBlitBuffer();

  deactivate_normal_shader();

  glDisable(GL_SCISSOR_TEST);
  /* Depth test disabled globally if needed, but we keep it on for the pass */
}

static void render_sector_gpu(GPU_Target *target, int sector_id, ClipRect clip,
                              int depth, int is_island, float parent_floor_z,
                              float parent_ceil_z, int transparent_pass) {
  if (depth > MAX_PORTAL_DEPTH)
    return;
  if (visited_test(sector_id))
    return;
  visited_set(sector_id);
  if (!clip_valid(clip))
    return;

  RAY_Sector *sector = find_sector_by_id(sector_id);
  if (!sector)
    return;

  GPU_SetClip(target, (Sint16)clip.x1, (Sint16)clip.y1,
              (Uint16)(clip.x2 - clip.x1), (Uint16)(clip.y2 - clip.y1));

  /* Heights */
  float fz = sector->floor_z;
  float cz = sector->ceiling_z;

  /* For holes (non-island children), extend walls to parent heights to fill
   * "rim" gaps */
  if (!is_island && sector->parent_sector_id != -1) {
    RAY_Sector *parent = find_sector_by_id(sector->parent_sector_id);
    if (parent) {
      if (parent->floor_z > cz)
        cz = parent->floor_z;
      if (parent->ceiling_z < fz)
        fz = parent->ceiling_z;
    }
  }

  float floor_h = fz - s_cam_z;
  float ceil_h = cz - s_cam_z;

  /* ---- PRE-XFORM WALLS FOR PRECISION CLIPPING ---- */
  XFormWall *xf_walls =
      (XFormWall *)alloca(sector->num_walls * sizeof(XFormWall));
  for (int w = 0; w < sector->num_walls; w++) {
    RAY_Wall *wall = &sector->walls[w];
    float x0 = wall->x1 - s_cam_x, y0 = wall->y1 - s_cam_y;
    float x1 = wall->x2 - s_cam_x, y1 = wall->y2 - s_cam_y;
    xf_walls[w].tz0 = x0 * s_cos_ang + y0 * s_sin_ang;
    xf_walls[w].tx0 = y0 * s_cos_ang - x0 * s_sin_ang;
    xf_walls[w].tz1 = x1 * s_cos_ang + y1 * s_sin_ang;
    xf_walls[w].tx1 = y1 * s_cos_ang - x1 * s_sin_ang;
  }

  /* ---- FLOOR & CEILING ---- */
  /* 2. RENDER PLANES (Floor & Ceiling) */
  /* ---- CUTOUTS FOR CHILD SECTORS (Holes/Pits) ---- */
  int max_cutout_walls = 0;
  for (int c = 0; c < sector->num_children; c++) {
    RAY_Sector *child = find_sector_by_id(sector->child_sector_ids[c]);
    if (child)
      max_cutout_walls += child->num_walls;
  }

  XFormWall *floor_cutouts = NULL;
  int num_floor_cutouts = 0;
  XFormWall *ceil_cutouts = NULL;
  int num_ceil_cutouts = 0;

  if (max_cutout_walls > 0) {
    floor_cutouts = (XFormWall *)alloca(max_cutout_walls * sizeof(XFormWall));
    ceil_cutouts = (XFormWall *)alloca(max_cutout_walls * sizeof(XFormWall));

    for (int c = 0; c < sector->num_children; c++) {
      RAY_Sector *child = find_sector_by_id(sector->child_sector_ids[c]);
      if (!child)
        continue;

      /* If child ceiling is at our floor height, it punches a hole in the floor
         (PIT). Check against the child's REAL rim height (which might have been
         extended). */
      float child_rim_z = child->ceiling_z;
      if (child->parent_sector_id != -1) {
        if (sector->floor_z > child_rim_z)
          child_rim_z = sector->floor_z;
      }

      if (fabsf(child_rim_z - sector->floor_z) < 0.1f ||
          child_rim_z < sector->floor_z) {
        for (int w = 0; w < child->num_walls; w++) {
          RAY_Wall *cw = &child->walls[w];
          float x0 = cw->x1 - s_cam_x, y0 = cw->y1 - s_cam_y;
          float x1 = cw->x2 - s_cam_x, y1 = cw->y2 - s_cam_y;
          floor_cutouts[num_floor_cutouts].tz0 =
              x0 * s_cos_ang + y0 * s_sin_ang;
          floor_cutouts[num_floor_cutouts].tx0 =
              y0 * s_cos_ang - x0 * s_sin_ang;
          floor_cutouts[num_floor_cutouts].tz1 =
              x1 * s_cos_ang + y1 * s_sin_ang;
          floor_cutouts[num_floor_cutouts].tx1 =
              y1 * s_cos_ang - x1 * s_sin_ang;
          num_floor_cutouts++;
        }
      }
      /* If child floor is at our ceiling height, it punches a hole in the
       * ceiling (ELEVATION) */
      if (fabsf(child->floor_z - sector->ceiling_z) < 0.1f ||
          child->floor_z > sector->ceiling_z) {
        for (int w = 0; w < child->num_walls; w++) {
          RAY_Wall *cw = &child->walls[w];
          float x0 = cw->x1 - s_cam_x, y0 = cw->y1 - s_cam_y;
          float x1 = cw->x2 - s_cam_x, y1 = cw->y2 - s_cam_y;
          ceil_cutouts[num_ceil_cutouts].tz0 = x0 * s_cos_ang + y0 * s_sin_ang;
          ceil_cutouts[num_ceil_cutouts].tx0 = y0 * s_cos_ang - x0 * s_sin_ang;
          ceil_cutouts[num_ceil_cutouts].tz1 = x1 * s_cos_ang + y1 * s_sin_ang;
          ceil_cutouts[num_ceil_cutouts].tx1 = y1 * s_cos_ang - x1 * s_sin_ang;
          num_ceil_cutouts++;
        }
      }
    }
  }

  if (g_engine.drawTexturedFloor) {
    GPU_Image *floor_tex = get_gpu_texture(0, sector->floor_texture_id);
    GPU_Image *floor_normal_tex = get_gpu_texture(0, sector->floor_normal_id);
    if (floor_tex) {
      int is_fluid =
          (sector->flags & 32) != 0; /* RAY_SECTOR_FLAG_LIQUID_FLOOR */
      if ((transparent_pass && is_fluid) || (!transparent_pass && !is_fluid)) {
        if (transparent_pass) {
          glDepthMask(GL_FALSE);
          GPU_SetBlendMode(floor_tex, GPU_BLEND_NORMAL);
        }
        render_sector_plane(target, sector, sector->floor_z, floor_tex,
                            floor_normal_tex, xf_walls, sector->num_walls,
                            depth, is_island, clip, num_floor_cutouts,
                            floor_cutouts);
        if (transparent_pass)
          glDepthMask(GL_TRUE);
      }
    }
  }

  if (g_engine.drawCeiling) {
    GPU_Image *ceil_tex = get_gpu_texture(0, sector->ceiling_texture_id);
    GPU_Image *ceil_normal_tex = get_gpu_texture(0, sector->ceiling_normal_id);
    if (ceil_tex) {
      int is_fluid =
          (sector->flags & 64) != 0; /* RAY_SECTOR_FLAG_LIQUID_CEILING */
      if ((transparent_pass && is_fluid) || (!transparent_pass && !is_fluid)) {
        if (transparent_pass) {
          glDepthMask(GL_FALSE);
          GPU_SetBlendMode(ceil_tex, GPU_BLEND_NORMAL);
        }
        render_sector_plane(target, sector, sector->ceiling_z, ceil_tex,
                            ceil_normal_tex, xf_walls, sector->num_walls, depth,
                            is_island, clip, num_ceil_cutouts, ceil_cutouts);
        if (transparent_pass)
          glDepthMask(GL_TRUE);
      }
    }
  }
  /* ---- CHILD SECTORS (Room-over-Room / Nested) ---- */
  /* We'll render these at the end, after portals, to use the Z-buffer
     correctly and ensure the visited system handles portal-connected children
     first. */

  /* ---- WALLS ---- */
  /* Sort walls from furthest to nearest (Painter's Algorithm) */
  typedef struct {
    int index;
    float dist;
  } WallOrder;

  WallOrder *order = (WallOrder *)alloca(sector->num_walls * sizeof(WallOrder));
  for (int w = 0; w < sector->num_walls; w++) {
    RAY_Wall *wall = &sector->walls[w];
    /* Use midpoint distance for sorting */
    float mx = (wall->x1 + wall->x2) * 0.5f - s_cam_x;
    float my = (wall->y1 + wall->y2) * 0.5f - s_cam_y;
    order[w].index = w;
    order[w].dist = mx * mx + my * my;
  }

  /* Simple bubble sort or qsort for wall order */
  for (int i = 0; i < sector->num_walls - 1; i++) {
    for (int j = 0; j < sector->num_walls - i - 1; j++) {
      if (order[j].dist < order[j + 1].dist) {
        WallOrder temp = order[j];
        order[j] = order[j + 1];
        order[j + 1] = temp;
      }
    }
  }

  for (int wi = 0; wi < sector->num_walls; wi++) {
    int w = order[wi].index;
    RAY_Wall *wall = &sector->walls[w];

    float x0 = wall->x1 - s_cam_x;
    float y0 = wall->y1 - s_cam_y;
    float x1 = wall->x2 - s_cam_x;
    float y1 = wall->y2 - s_cam_y;

    float tz0 = x0 * s_cos_ang + y0 * s_sin_ang;
    float tx0 = y0 * s_cos_ang - x0 * s_sin_ang;
    float tz1 = x1 * s_cos_ang + y1 * s_sin_ang;
    float tx1 = y1 * s_cos_ang - x1 * s_sin_ang;

    if (tz0 <= NEAR_PLANE && tz1 <= NEAR_PLANE)
      continue;

    int v0_clipped = 0, v1_clipped = 0;
    if (tz0 <= NEAR_PLANE || tz1 <= NEAR_PLANE) {
      float t = (NEAR_PLANE - tz0) / (tz1 - tz0);
      float ix = tx0 + t * (tx1 - tx0);
      if (tz0 <= NEAR_PLANE) {
        tx0 = ix;
        tz0 = NEAR_PLANE;
        v0_clipped = 1;
      } else {
        tx1 = ix;
        tz1 = NEAR_PLANE;
        v1_clipped = 1;
      }
    }

    float sx0 = s_half_w + (tx0 * s_focal / tz0);
    float sx1 = s_half_w + (tx1 * s_focal / tz1);
    float y0_ceil = (float)s_horizon - (ceil_h * s_focal / tz0);
    float y0_floor = (float)s_horizon - (floor_h * s_focal / tz0);
    float y1_ceil = (float)s_horizon - (ceil_h * s_focal / tz1);
    float y1_floor = (float)s_horizon - (floor_h * s_focal / tz1);

    /* ---- PORTAL WALL ---- */
    if (wall->portal_id != -1) {
      int other_sector_id = portal_get_other_sector(wall->portal_id, sector_id);
      if (other_sector_id >= 0 && !visited_test(other_sector_id)) {
        float portal_left = (sx0 < sx1) ? sx0 : sx1;
        float portal_right = (sx0 > sx1) ? sx0 : sx1;
        float portal_top = (y0_ceil < y1_ceil) ? y0_ceil : y1_ceil;
        float portal_bot = (y0_floor > y1_floor) ? y0_floor : y1_floor;

        /* Determine which side is left/right for interpolation */
        float left_sx = sx0, right_sx = sx1;
        float left_ceil = y0_ceil, right_ceil = y1_ceil;
        float left_floor = y0_floor, right_floor = y1_floor;
        if (sx0 > sx1) {
          left_sx = sx1;
          right_sx = sx0;
          left_ceil = y1_ceil;
          right_ceil = y0_ceil;
          left_floor = y1_floor;
          right_floor = y0_floor;
        }

        if (v0_clipped) {
          if (sx0 <= sx1)
            portal_left = clip.x1;
          else
            portal_right = clip.x2;
          portal_top = clip.y1;
          portal_bot = clip.y2;
        }
        if (v1_clipped) {
          if (sx1 <= sx0)
            portal_left = clip.x1;
          else
            portal_right = clip.x2;
          portal_top = clip.y1;
          portal_bot = clip.y2;
        }

        ClipRect portal_clip = {portal_left, portal_top, portal_right,
                                portal_bot};
        ClipRect new_clip = clip_intersect(clip, portal_clip);

        if (clip_valid(new_clip)) {
          /* Render adjacent sector clipped to the portal opening rectangle */
          render_sector_gpu(target, other_sector_id, new_clip, depth + 1, 0,
                            sector->floor_z, sector->ceiling_z,
                            transparent_pass);

          /* Repair: the portal opening is a trapezoid but new_clip is a
             rectangle. Paint the parent floor/ceiling in the excess corners.
             Optimization: group consecutive columns that share the same
             integer ceil_y / floor_y into a single GPU_SetClip call each.
             The trapezoid edges are linear so the number of unique Y values
             equals the vertical span of each edge — much less than width. */
          GPU_Image *pf_tex = get_gpu_texture(0, sector->floor_texture_id);
          GPU_Image *pc_tex = get_gpu_texture(0, sector->ceiling_texture_id);

          float wall_span = right_sx - left_sx;
          int col_start = (int)new_clip.x1;
          int col_end = (int)new_clip.x2;

#define INTERP_Y(base_, delta_, cx_)                                           \
  ({                                                                           \
    float _t =                                                                 \
        (wall_span > 0.0f) ? ((float)(cx_) - left_sx) / wall_span : 0.5f;      \
    if (_t < 0.0f)                                                             \
      _t = 0.0f;                                                               \
    if (_t > 1.0f)                                                             \
      _t = 1.0f;                                                               \
    float _y = (base_) + _t * (delta_);                                        \
    if (_y < clip.y1)                                                          \
      _y = clip.y1;                                                            \
    if (_y > clip.y2)                                                          \
      _y = clip.y2;                                                            \
    _y;                                                                        \
  })

          /* OPTIMIZATION: Instead of repairing column by column (which caused
             hundreds of redundant render_sector_plane calls), we use a fast
             dual-rectangle approximation. This covers >90% of the excess area
             with only 2 calls instead of hundreds. */
          float top_repair_y =
              (left_ceil > right_ceil) ? left_ceil : right_ceil;
          float bot_repair_y =
              (left_floor < right_floor) ? left_floor : right_floor;

          /* Upper excess repair */
          if (top_repair_y > clip.y1 && pc_tex) {
            GPU_SetClip(target, (Sint16)new_clip.x1, (Sint16)clip.y1,
                        (Uint16)(new_clip.x2 - new_clip.x1),
                        (Uint16)(top_repair_y - clip.y1));
            render_sector_plane(target, sector, sector->ceiling_z, pc_tex, NULL,
                                xf_walls, sector->num_walls, depth, 0, clip,
                                num_ceil_cutouts, ceil_cutouts);
          }
          /* Lower excess repair */
          if (bot_repair_y < clip.y2 && pf_tex) {
            GPU_SetClip(target, (Sint16)new_clip.x1, (Sint16)bot_repair_y,
                        (Uint16)(new_clip.x2 - new_clip.x1),
                        (Uint16)(clip.y2 - bot_repair_y));
            render_sector_plane(target, sector, sector->floor_z, pf_tex, NULL,
                                xf_walls, sector->num_walls, depth, 0, clip,
                                num_floor_cutouts, floor_cutouts);
          }

          GPU_SetClip(target, (Sint16)clip.x1, (Sint16)clip.y1,
                      (Uint16)(clip.x2 - clip.x1), (Uint16)(clip.y2 - clip.y1));

          /* ---- UPPER / LOWER PORTAL WALL TEXTURES ---- */
          RAY_Sector *other_sector = find_sector_by_id(other_sector_id);
          if (other_sector) {
            float other_ceil_h = other_sector->ceiling_z - s_cam_z;
            float other_floor_h = other_sector->floor_z - s_cam_z;

            /* Reuse the same column subdivision as solid walls */
            float wall_dx2 = tx1 - tx0;
            float wall_dz2 = tz1 - tz0;
            float wall_len2 =
                sqrtf((wall->x2 - wall->x1) * (wall->x2 - wall->x1) +
                      (wall->y2 - wall->y1) * (wall->y2 - wall->y1));
            if (wall_len2 < 0.001f)
              wall_len2 = 1.0f;

            /* World-anchored U coords (same near-plane clip recovery) */
            float pu0, pu1;
            if (v0_clipped) {
              float ox0 = wall->x1 - s_cam_x, oy0 = wall->y1 - s_cam_y;
              float otz0 = ox0 * s_cos_ang + oy0 * s_sin_ang;
              float ox1 = wall->x2 - wall->x1, oy1 = wall->y2 - wall->y1;
              float otz1 = ox1 * s_cos_ang + oy1 * s_sin_ang;
              float t_clip = (NEAR_PLANE - otz0) / (otz1 - otz0);
              pu0 = t_clip; /* fraction along wall */
            } else {
              pu0 = 0.0f;
            }
            if (v1_clipped) {
              float ox0 = wall->x1 - s_cam_x, oy0 = wall->y1 - s_cam_y;
              float ox1 = wall->x2 - wall->x1, oy1 = wall->y2 - wall->y1;
              float otz0 = ox0 * s_cos_ang + oy0 * s_sin_ang;
              float otz1 = ox1 * s_cos_ang + oy1 * s_sin_ang;
              float t_clip = (NEAR_PLANE - otz0) / (otz1 - otz0);
              pu1 = t_clip;
            } else {
              pu1 = 1.0f;
            }

            int pcol0 = (int)floorf(sx0 < sx1 ? sx0 : sx1);
            int pcol1 = (int)ceilf(sx0 < sx1 ? sx1 : sx0);
            if (pcol0 < (int)clip.x1)
              pcol0 = (int)clip.x1;
            if (pcol1 > (int)clip.x2)
              pcol1 = (int)clip.x2;
            int pnum_cols = pcol1 - pcol0 + 1;
            if (pnum_cols < 2)
              pnum_cols = 2;

            /* UPPER texture: between parent ceiling and child ceiling */
            if (other_ceil_h < ceil_h && wall->texture_id_upper > 0) {
              GPU_Image *upper_tex = get_gpu_texture(0, wall->texture_id_upper);
              if (upper_tex) {
                GPU_SetWrapMode(upper_tex, GPU_WRAP_REPEAT, GPU_WRAP_REPEAT);
                GPU_SetImageFilter(upper_tex, GPU_FILTER_LINEAR);
                float *vu = (float *)alloca(pnum_cols * 2 * 5 * sizeof(float));
                unsigned short *iu = (unsigned short *)alloca(
                    (pnum_cols - 1) * 6 * sizeof(unsigned short));
                int nvu = 0, niu = 0;
                for (int ci = 0; ci < pnum_cols; ci++) {
                  float sx = (float)(pcol0 + ci);
                  float rx = (sx - (float)s_half_w) / s_focal;
                  float denom = rx * wall_dz2 - wall_dx2;
                  float t_w, sub_tz;
                  if (fabsf(denom) > 1e-6f) {
                    t_w = (tx0 - rx * tz0) / denom;
                    if (t_w < 0.f)
                      t_w = 0.f;
                    if (t_w > 1.f)
                      t_w = 1.f;
                    sub_tz = tz0 + t_w * wall_dz2;
                  } else {
                    t_w = (float)ci / (float)(pnum_cols - 1);
                    sub_tz = tz0 + t_w * wall_dz2;
                  }
                  if (sub_tz < NEAR_PLANE)
                    sub_tz = NEAR_PLANE;
                  float u_c = pu0 + t_w * (pu1 - pu0);
                  /* V: 0=parent ceiling, 1=child ceiling (top-anchored) */
                  float y_parent_ceil =
                      (float)s_horizon - (ceil_h * s_focal / sub_tz);
                  float y_child_ceil =
                      (float)s_horizon - (other_ceil_h * s_focal / sub_tz);
                  float sub_z = depth_from_tz(sub_tz);
                  /* top vertex: parent ceiling, v=0 */
                  vu[nvu * 5 + 0] = sx;
                  vu[nvu * 5 + 1] = y_parent_ceil;
                  vu[nvu * 5 + 2] = sub_z;
                  vu[nvu * 5 + 3] = u_c;
                  vu[nvu * 5 + 4] = 0.0f;
                  nvu++;
                  /* bottom vertex: child ceiling, v=1 */
                  vu[nvu * 5 + 0] = sx;
                  vu[nvu * 5 + 1] = y_child_ceil;
                  vu[nvu * 5 + 2] = sub_z;
                  vu[nvu * 5 + 3] = u_c;
                  vu[nvu * 5 + 4] = 1.0f;
                  nvu++;
                }
                for (int s = 0; s < pnum_cols - 1; s++) {
                  int tl = s * 2, bl = s * 2 + 1, tr = (s + 1) * 2,
                      br = (s + 1) * 2 + 1;
                  iu[niu++] = tl;
                  iu[niu++] = tr;
                  iu[niu++] = br;
                  iu[niu++] = tl;
                  iu[niu++] = br;
                  iu[niu++] = bl;
                }
                GPU_Image *upper_normal_tex =
                    get_gpu_texture(0, wall->texture_id_upper_normal);
                /* TBN for Upper wall */
                float wall_dx_u = wall->x2 - wall->x1;
                float wall_dy_u = wall->y2 - wall->y1;
                float wall_len_u =
                    sqrtf(wall_dx_u * wall_dx_u + wall_dy_u * wall_dy_u);
                if (wall_len_u < 0.001f)
                  wall_len_u = 1.0f;
                float wall_tx_u = wall_dx_u / wall_len_u;
                float wall_ty_u = wall_dy_u / wall_len_u;
                float wall_nx_u = (wall->y1 - wall->y2) / wall_len_u;
                float wall_ny_u = (wall->x2 - wall->x1) / wall_len_u;

                int wallActiveFlags = (sector->flags & (8 | 16 | 256));
                if (sector->flags & 128)
                  wallActiveFlags |= (sector->flags & 7);
                activate_normal_shader(
                    upper_normal_tex, wall_tx_u, wall_ty_u, 0.0f, 0.0f, 0.0f,
                    1.0f, wall_nx_u, wall_ny_u, 0.0f, wallActiveFlags,
                    sector->liquid_intensity, sector->liquid_speed, s_fog_r,
                    s_fog_g, s_fog_b, s_fog_density, s_fog_start, s_fog_end);

                GPU_TriangleBatch(upper_tex, target, nvu, vu, niu, iu,
                                  GPU_BATCH_XYZ_ST);
                GPU_FlushBlitBuffer();
                deactivate_normal_shader();
              }
            }

            /* LOWER texture: between child floor and parent floor */
            if (other_floor_h > floor_h && wall->texture_id_lower > 0) {
              GPU_Image *lower_tex = get_gpu_texture(0, wall->texture_id_lower);
              if (lower_tex) {
                GPU_SetWrapMode(lower_tex, GPU_WRAP_REPEAT, GPU_WRAP_REPEAT);
                GPU_SetImageFilter(lower_tex, GPU_FILTER_LINEAR);
                float *vl = (float *)alloca(pnum_cols * 2 * 5 * sizeof(float));
                unsigned short *il = (unsigned short *)alloca(
                    (pnum_cols - 1) * 6 * sizeof(unsigned short));
                int nvl = 0, nil = 0;
                for (int ci = 0; ci < pnum_cols; ci++) {
                  float sx = (float)(pcol0 + ci);
                  float rx = (sx - (float)s_half_w) / s_focal;
                  float denom = rx * wall_dz2 - wall_dx2;
                  float t_w, sub_tz;
                  if (fabsf(denom) > 1e-6f) {
                    t_w = (tx0 - rx * tz0) / denom;
                    if (t_w < 0.f)
                      t_w = 0.f;
                    if (t_w > 1.f)
                      t_w = 1.f;
                    sub_tz = tz0 + t_w * wall_dz2;
                  } else {
                    t_w = (float)ci / (float)(pnum_cols - 1);
                    sub_tz = tz0 + t_w * wall_dz2;
                  }
                  if (sub_tz < NEAR_PLANE)
                    sub_tz = NEAR_PLANE;
                  float u_c = pu0 + t_w * (pu1 - pu0);
                  /* V: 0=child floor (top), 1=parent floor (bottom) */
                  float y_child_floor =
                      (float)s_horizon - (other_floor_h * s_focal / sub_tz);
                  float y_parent_floor =
                      (float)s_horizon - (floor_h * s_focal / sub_tz);
                  float sub_z = depth_from_tz(sub_tz);
                  vl[nvl * 5 + 0] = sx;
                  vl[nvl * 5 + 1] = y_child_floor;
                  vl[nvl * 5 + 2] = sub_z;
                  vl[nvl * 5 + 3] = u_c;
                  vl[nvl * 5 + 4] = 0.0f;
                  nvl++;
                  vl[nvl * 5 + 0] = sx;
                  vl[nvl * 5 + 1] = y_parent_floor;
                  vl[nvl * 5 + 2] = sub_z;
                  vl[nvl * 5 + 3] = u_c;
                  vl[nvl * 5 + 4] = 1.0f;
                  nvl++;
                }
                for (int s = 0; s < pnum_cols - 1; s++) {
                  int tl = s * 2, bl = s * 2 + 1, tr = (s + 1) * 2,
                      br = (s + 1) * 2 + 1;
                  il[nil++] = tl;
                  il[nil++] = tr;
                  il[nil++] = br;
                  il[nil++] = tl;
                  il[nil++] = br;
                  il[nil++] = bl;
                }
                GPU_Image *lower_normal_tex =
                    get_gpu_texture(0, wall->texture_id_lower_normal);

                /* TBN for Lower wall */
                float wall_dx_l = wall->x2 - wall->x1;
                float wall_dy_l = wall->y2 - wall->y1;
                float wall_len_l =
                    sqrtf(wall_dx_l * wall_dx_l + wall_dy_l * wall_dy_l);
                if (wall_len_l < 0.001f)
                  wall_len_l = 1.0f;
                float wall_tx_l = wall_dx_l / wall_len_l;
                float wall_ty_l = wall_dy_l / wall_len_l;
                float wall_nx_l = (wall->y1 - wall->y2) / wall_len_l;
                float wall_ny_l = (wall->x2 - wall->x1) / wall_len_l;

                int wallActiveFlags = (sector->flags & (8 | 16 | 256));
                if (sector->flags & 128)
                  wallActiveFlags |= (sector->flags & 7);
                activate_normal_shader(
                    lower_normal_tex, wall_tx_l, wall_ty_l, 0.0f, 0.0f, 0.0f,
                    1.0f, wall_nx_l, wall_ny_l, 0.0f, wallActiveFlags,
                    sector->liquid_intensity, sector->liquid_speed, s_fog_r,
                    s_fog_g, s_fog_b, s_fog_density, s_fog_start, s_fog_end);

                GPU_TriangleBatch(lower_tex, target, nvl, vl, nil, il,
                                  GPU_BATCH_XYZ_ST);
                GPU_FlushBlitBuffer();
                deactivate_normal_shader();
              }
            }
          }
        }
      }
      continue;
    }

    /* ---- SOLID WALL (Segmented Triple-Texture Support) ---- */
    /* Use ORIGINAL sector heights for wall textures (fluid zone only).
       Extended heights (fz/cz) are used ONLY for rim extensions below. */
    float z_floor = sector->floor_z;
    float z_ceil = sector->ceiling_z;
    float split_low = wall->texture_split_z_lower;
    float split_up = wall->texture_split_z_upper;

    // Clamp splits to ORIGINAL sector range (not extended)
    if (split_low < z_floor)
      split_low = z_floor;
    if (split_low > z_ceil)
      split_low = z_ceil;
    if (split_up < split_low)
      split_up = split_low;
    if (split_up > z_ceil)
      split_up = z_ceil;

    // Mid segment boundaries (if low/up textures are not set, mid expands to
    // covers those parts)
    float mid_z_bot = (wall->texture_id_lower > 0) ? split_low : z_floor;
    float mid_z_top = (wall->texture_id_upper > 0) ? split_up : z_ceil;

    /* TBN calculation common for all segments */
    float wall_dx_m = wall->x2 - wall->x1;
    float wall_dy_m = wall->y2 - wall->y1;
    float wall_len_m = sqrtf(wall_dx_m * wall_dx_m + wall_dy_m * wall_dy_m);
    if (wall_len_m < 0.001f)
      wall_len_m = 1.0f;
    float wall_tx_m = wall_dx_m / wall_len_m;
    float wall_ty_m = wall_dy_m / wall_len_m;
    float wall_nx_m = (wall->y1 - wall->y2) / wall_len_m;
    float wall_ny_m = (wall->x2 - wall->x1) / wall_len_m;

    /* Projection math (Screen-space subdivision) */
    float wall_dx = tx1 - tx0;
    float wall_dz = tz1 - tz0;

    int col0 = (int)floorf(sx0 < sx1 ? sx0 : sx1);
    int col1 = (int)ceilf(sx0 < sx1 ? sx1 : sx0);
    if (col0 < (int)clip.x1)
      col0 = (int)clip.x1;
    if (col1 > (int)clip.x2)
      col1 = (int)clip.x2;
    int num_cols = col1 - col0 + 1;
    if (num_cols < 2)
      num_cols = 2;

    /* UV endpoints */
    float u0_world, u1_world;
    if (v0_clipped) {
      float ox0 = wall->x1 - s_cam_x, oy0 = wall->y1 - s_cam_y;
      float otz0 = ox0 * s_cos_ang + oy0 * s_sin_ang;
      float ox_seg = wall->x2 - wall->x1, oy_seg = wall->y2 - wall->y1;
      float otz_seg = ox_seg * s_cos_ang + oy_seg * s_sin_ang;
      u0_world = (NEAR_PLANE - otz0) / (otz_seg - otz0);
    } else
      u0_world = 0.0f;
    if (v1_clipped) {
      float ox0 = wall->x1 - s_cam_x, oy0 = wall->y1 - s_cam_y;
      float otz0 = ox0 * s_cos_ang + oy0 * s_sin_ang;
      float ox_seg = wall->x2 - wall->x1, oy_seg = wall->y2 - wall->y1;
      float otz_seg = ox_seg * s_cos_ang + oy_seg * s_sin_ang;
      u1_world = (NEAR_PLANE - otz0) / (otz_seg - otz0);
    } else
      u1_world = 1.0f;

    /* Precalculate column tz and world UVs to avoid repeating math in each
     * segment */
    float *tz_vals = (float *)alloca(num_cols * sizeof(float));
    float *u_vals = (float *)alloca(num_cols * sizeof(float));
    for (int ci = 0; ci < num_cols; ci++) {
      float sx = (float)(col0 + ci);
      float rx = (sx - (float)s_half_w) / s_focal;
      float denom = rx * wall_dz - wall_dx;
      float t_wall, sub_tz;
      if (fabsf(denom) > 1e-6f) {
        t_wall = (tx0 - rx * tz0) / denom;
        if (t_wall < 0.0f)
          t_wall = 0.0f;
        if (t_wall > 1.0f)
          t_wall = 1.0f;
        sub_tz = tz0 + t_wall * wall_dz;
      } else {
        t_wall = (float)ci / (float)(num_cols - 1);
        sub_tz = tz0 + t_wall * wall_dz;
      }
      if (sub_tz < NEAR_PLANE)
        sub_tz = NEAR_PLANE;
      tz_vals[ci] = sub_tz;
      u_vals[ci] = u0_world + t_wall * (u1_world - u0_world);
    }

    /* Common Index buffer for the 2-lane strips */
    float *v_w = (float *)alloca(num_cols * 2 * 5 * sizeof(float));
    unsigned short *i_w =
        (unsigned short *)alloca((num_cols - 1) * 6 * sizeof(unsigned short));
    int ni = 0;
    for (int s = 0; s < num_cols - 1; s++) {
      int tl = s * 2, bl = s * 2 + 1, tr = (s + 1) * 2, br = (s + 1) * 2 + 1;
      i_w[ni++] = tl;
      i_w[ni++] = tr;
      i_w[ni++] = br;
      i_w[ni++] = tl;
      i_w[ni++] = br;
      i_w[ni++] = bl;
    }

    /* Segment rendering macro */
#define RENDER_SOLID_SEGMENT(tex_id, normal_id, z_bot_abs, z_top_abs,          \
                             force_no_fluid)                                   \
  do {                                                                         \
    if ((tex_id) > 0 && (z_top_abs) > (z_bot_abs)) {                           \
      GPU_Image *_tex = get_gpu_texture(0, tex_id);                            \
      if (_tex) {                                                              \
        GPU_SetImageFilter(_tex, GPU_FILTER_LINEAR);                           \
        GPU_SetWrapMode(_tex, GPU_WRAP_REPEAT, GPU_WRAP_REPEAT);               \
        GPU_Image *_norm = get_gpu_texture(0, normal_id);                      \
        int is_fluid = (force_no_fluid) ? 0 : ((sector->flags & 128) != 0);    \
        if ((transparent_pass && is_fluid) ||                                  \
            (!transparent_pass && !is_fluid)) {                                \
          if (transparent_pass) {                                              \
            glDepthMask(GL_FALSE);                                             \
            GPU_SetBlendMode(_tex, GPU_BLEND_NORMAL);                          \
          }                                                                    \
          int activeFlags =                                                    \
              (sector->flags & (24 | 256)); /* Scroll & Ripples */             \
          if (is_fluid)                                                        \
            activeFlags |= (sector->flags & 7);                                \
          else if (force_no_fluid)                                             \
            activeFlags = 0; /* No effects on rim */                           \
          activate_normal_shader(                                              \
              _norm, wall_tx_m, wall_ty_m, 0.0f, 0.0f, 0.0f, 1.0f, wall_nx_m,  \
              wall_ny_m, 0.0f, activeFlags, sector->liquid_intensity,          \
              sector->liquid_speed, s_fog_r, s_fog_g, s_fog_b, s_fog_density,  \
              s_fog_start, s_fog_end);                                         \
          int _nvCount = 0;                                                    \
          float _h_bot = (z_bot_abs) - s_cam_z;                                \
          float _h_top = (z_top_abs) - s_cam_z;                                \
          float _v_top = 0.0f;                                                 \
          float _v_bot = 1.0f;                                                 \
          for (int ci = 0; ci < num_cols; ci++) {                              \
            float _sx = (float)(col0 + ci);                                    \
            float _tz = tz_vals[ci];                                           \
            float _u = u_vals[ci];                                             \
            float _sz = depth_from_tz(_tz);                                    \
            float _y_top = (float)s_horizon - (_h_top * s_focal / _tz);        \
            float _y_bot = (float)s_horizon - (_h_bot * s_focal / _tz);        \
            v_w[_nvCount * 5 + 0] = _sx;                                       \
            v_w[_nvCount * 5 + 1] = _y_top;                                    \
            v_w[_nvCount * 5 + 2] = _sz;                                       \
            v_w[_nvCount * 5 + 3] = _u;                                        \
            v_w[_nvCount * 5 + 4] = _v_top;                                    \
            _nvCount++;                                                        \
            v_w[_nvCount * 5 + 0] = _sx;                                       \
            v_w[_nvCount * 5 + 1] = _y_bot;                                    \
            v_w[_nvCount * 5 + 2] = _sz;                                       \
            v_w[_nvCount * 5 + 3] = _u;                                        \
            v_w[_nvCount * 5 + 4] = _v_bot;                                    \
            _nvCount++;                                                        \
          }                                                                    \
          GPU_TriangleBatch(_tex, target, _nvCount, v_w, ni, i_w,              \
                            GPU_BATCH_XYZ_ST);                                 \
          GPU_FlushBlitBuffer();                                               \
          deactivate_normal_shader();                                          \
          if (transparent_pass)                                                \
            glDepthMask(GL_TRUE);                                              \
        }                                                                      \
      }                                                                        \
    }                                                                          \
  } while (0)

    /* Determine original (non-extended) sector heights for fluid boundary */
    float orig_floor = sector->floor_z;
    float orig_ceil = sector->ceiling_z;

    // 1. Lower Segment (Floor to SplitLow) — submerged, with fluid
    RENDER_SOLID_SEGMENT(wall->texture_id_lower, wall->texture_id_lower_normal,
                         z_floor, split_low, 0);
    // 2. Middle Segment (SplitLow to SplitUp) — submerged, with fluid
    RENDER_SOLID_SEGMENT(wall->texture_id_middle,
                         wall->texture_id_middle_normal, mid_z_bot, mid_z_top,
                         0);
    // 3. Upper Segment (SplitUp to original Ceiling) — submerged, with fluid
    RENDER_SOLID_SEGMENT(wall->texture_id_upper, wall->texture_id_upper_normal,
                         split_up, orig_ceil, 0);

    /* ---- RIM EXTENSIONS (above/below liquid) — NO fluid distortion ---- */
    /* Upper rim: from original ceiling up to extended ceiling (parent floor) */
    if (cz > orig_ceil + 0.1f) {
      int rim_tex = wall->texture_id_middle;
      int rim_norm = wall->texture_id_middle_normal;
      if (rim_tex <= 0) {
        rim_tex = wall->texture_id_upper;
        rim_norm = wall->texture_id_upper_normal;
      }
      if (rim_tex <= 0) {
        rim_tex = wall->texture_id_lower;
        rim_norm = wall->texture_id_lower_normal;
      }
      RENDER_SOLID_SEGMENT(rim_tex, rim_norm, orig_ceil, cz, 1);
    }
    /* Lower rim: from extended floor up to original floor (parent ceiling) */
    if (fz < orig_floor - 0.1f) {
      int rim_tex = wall->texture_id_middle;
      int rim_norm = wall->texture_id_middle_normal;
      if (rim_tex <= 0) {
        rim_tex = wall->texture_id_lower;
        rim_norm = wall->texture_id_lower_normal;
      }
      if (rim_tex <= 0) {
        rim_tex = wall->texture_id_upper;
        rim_norm = wall->texture_id_upper_normal;
      }
      RENDER_SOLID_SEGMENT(rim_tex, rim_norm, fz, orig_floor, 1);
    }

    /* ---- POOL/PIT RIM: fill gap between sector and parent heights ----
       When is_island=1, fz/cz are NOT extended to parent heights, so the
       existing rim checks above are always false. Use parent_floor_z and
       parent_ceil_z (function parameters) to fill the visual gap. */
    if (depth > 0 && is_island) {
      /* Pool edge: sector ceiling below parent floor */
      if (parent_floor_z > orig_ceil + 0.1f) {
        int pit_tex = wall->texture_id_middle;
        int pit_norm = wall->texture_id_middle_normal;
        if (pit_tex <= 0) {
          pit_tex = wall->texture_id_upper;
          pit_norm = wall->texture_id_upper_normal;
        }
        if (pit_tex <= 0) {
          pit_tex = wall->texture_id_lower;
          pit_norm = wall->texture_id_lower_normal;
        }
        RENDER_SOLID_SEGMENT(pit_tex, pit_norm, orig_ceil, parent_floor_z, 1);
      }
      /* Elevation edge: sector floor above parent ceiling */
      if (parent_ceil_z < orig_floor - 0.1f) {
        int elev_tex = wall->texture_id_middle;
        int elev_norm = wall->texture_id_middle_normal;
        if (elev_tex <= 0) {
          elev_tex = wall->texture_id_lower;
          elev_norm = wall->texture_id_lower_normal;
        }
        if (elev_tex <= 0) {
          elev_tex = wall->texture_id_upper;
          elev_norm = wall->texture_id_upper_normal;
        }
        RENDER_SOLID_SEGMENT(elev_tex, elev_norm, parent_ceil_z, orig_floor, 1);
      }
    }

#undef RENDER_SOLID_SEGMENT
  } /* end wall loop */

  /* ---- ISLAND LIDS (rendered after walls so walls occlude them) ---- */
  if (is_island) {
    /* Texture fallback */
    GPU_Image *floor_tex = get_gpu_texture(0, sector->floor_texture_id);
    GPU_Image *ceil_tex = get_gpu_texture(0, sector->ceiling_texture_id);
    if (!floor_tex && ceil_tex)
      floor_tex = ceil_tex;
    if (!ceil_tex && floor_tex)
      ceil_tex = floor_tex;

    /* Select which face(s) to render based on camera Z vs sector Z.
       cam_z < floor_z  → camera BELOW box → render floor (bottom face)
       cam_z > ceil_z   → camera ABOVE box → render ceiling (top face)
       otherwise        → camera inside box → render both */
    int render_floor = (s_cam_z <= sector->floor_z);
    int render_ceil = (s_cam_z >= sector->ceiling_z);

    /* Pit detection: If our ceiling is at parent's floor height, it's an
     * opening (PIT). Don't render the top lid. */
    if (depth > 0 && fabsf(sector->ceiling_z - parent_floor_z) < 0.1f) {
      render_ceil = 0;
    }
    /* Elevation detection: If our floor is at parent's ceiling height, it's
     * an opening. Don't render the bottom lid. */
    if (depth > 0 && fabsf(sector->floor_z - parent_ceil_z) < 0.1f) {
      render_floor = 0;
    }

    if (!render_floor && !render_ceil && depth == 0) {
      render_floor = 1; /* inside box: render both */
      render_ceil = 1;
    }

    /* Disable backface culling — winding reverses when viewed from below */
    glDisable(GL_CULL_FACE);

    /* Check if sector polygon is convex.
       Fan tessellation only works for convex polygons.
       For concave sectors (C-shape, rings), render_sector_plane
       already handles them correctly via scanline clipping. */
    int is_convex = 1;
    if (sector->num_walls >= 3) {
      int sign = 0;
      for (int w = 0; w < sector->num_walls; w++) {
        int w1 = (w + 1) % sector->num_walls;
        int w2 = (w + 2) % sector->num_walls;
        float ax = sector->walls[w1].x1 - sector->walls[w].x1;
        float ay = sector->walls[w1].y1 - sector->walls[w].y1;
        float bx = sector->walls[w2].x1 - sector->walls[w1].x1;
        float by = sector->walls[w2].y1 - sector->walls[w1].y1;
        float cross = ax * by - ay * bx;
        if (cross > 0.01f) {
          if (sign < 0) {
            is_convex = 0;
            break;
          }
          sign = 1;
        } else if (cross < -0.01f) {
          if (sign > 0) {
            is_convex = 0;
            break;
          }
          sign = -1;
        }
      }
    }

    if (is_convex) {
      if (render_floor && floor_tex) {
        GPU_Image *floor_normal_tex =
            get_gpu_texture(0, sector->floor_normal_id);
        render_island_lid(target, sector, sector->floor_z, floor_tex,
                          floor_normal_tex, xf_walls, sector->num_walls, clip);
      }
      if (render_ceil && ceil_tex) {
        GPU_Image *ceil_normal_tex =
            get_gpu_texture(0, sector->ceiling_normal_id);
        render_island_lid(target, sector, sector->ceiling_z, ceil_tex,
                          ceil_normal_tex, xf_walls, sector->num_walls, clip);
      }
    }
    /* Concave sectors are already rendered by render_sector_plane above */
  }

  /* ---- CHILD SECTORS (Room-over-Room / Nested) ---- */
  for (int c = 0; c < sector->num_children; c++) {
    int child_id = sector->child_sector_ids[c];
    if (child_id >= 0 && !visited_test(child_id)) {
      RAY_Sector *c_sect = find_sector_by_id(child_id);
      if (!c_sect)
        continue;

      /* All child sectors use wall-intersection clipping (island=1).
         This limits floor/ceiling to the sector's polygon for both
         islands (solid boxes) and pits/pools (depressions).
         Full-clip mode caused pools to fill the entire screen. */
      int child_is_island = 1;

      render_sector_gpu(target, child_id, clip, depth + 1, child_is_island,
                        sector->floor_z, sector->ceiling_z, transparent_pass);
      GPU_SetClip(target, (Sint16)clip.x1, (Sint16)clip.y1,
                  (Uint16)(clip.x2 - clip.x1), (Uint16)(clip.y2 - clip.y1));
    }
  }
}

/* ============================================================================
   MD3 MODEL RENDERING (GPU)
   ============================================================================
 */

#ifndef MD3_XYZ_SCALE
#define MD3_XYZ_SCALE (1.0f / 64.0f)
#endif

/* MD3 Painter's sort comparator: ascending (far first, near last) */
static int md3_tri_cmp_asc(const void *a, const void *b) {
  float da = ((const struct {
               float depth;
               int base;
             } *)a)
                 ->depth;
  float db = ((const struct {
               float depth;
               int base;
             } *)b)
                 ->depth;
  return (da > db) - (da < db); /* ascending: smaller (farther) first */
}

static void ray_render_md3_gpu(GPU_Target *target, RAY_Sprite *sprite) {
  if (!sprite || !sprite->model)
    return;

  RAY_MD3_Model *model = (RAY_MD3_Model *)sprite->model;
  if (model->header.magic != MD3_MAGIC)
    return;

  /* DEBUG: Track if sprite->rot or position changes during strafe */
  static int md3_dbg_counter = 0;
  if (md3_dbg_counter++ % 60 == 0) {
    printf("MD3 DEBUG: sprite rot=%.4f pos=(%.1f,%.1f,%.1f) cam=(%.1f,%.1f) "
           "cam_rot=%.4f\n",
           sprite->rot, sprite->x, sprite->y, sprite->z, s_cam_x, s_cam_y,
           g_engine.camera.rot);
  }

  float cos_model = cosf(sprite->rot);
  float sin_model = sinf(sprite->rot);
  float scale_factor =
      (sprite->model_scale > 0.0f) ? sprite->model_scale : 1.0f;

  for (int s = 0; s < model->header.numSurfaces; s++) {
    RAY_MD3_Surface *surf = &model->surfaces[s];

    int f1 = sprite->currentFrame;
    int f2 = sprite->nextFrame;
    float interp = sprite->interpolation;

    if (f1 >= surf->header.numFrames)
      f1 = surf->header.numFrames - 1;
    if (f2 >= surf->header.numFrames)
      f2 = surf->header.numFrames - 1;

    md3_vertex_t *v1 = &surf->vertices[f1 * surf->header.numVerts];
    md3_vertex_t *v2 = &surf->vertices[f2 * surf->header.numVerts];

    /* Better Texture Lookup for MD3: Sprite Surface Override > Model Surface
     * Override > Sprite Skin > Model Default */
    /* MD3 Texture Priority:
       1. Sprite-specific surface override
       2. Model-default surface texture
       3. Sprite global skin (if any)
       4. Model global default */
    GPU_Image *img = NULL;
    if (s < 32 && sprite->md3_surface_textures[s] > 0)
      img = get_gpu_texture(sprite->fileID, sprite->md3_surface_textures[s]);
    else if (surf->textureID > 0)
      img = get_gpu_texture(sprite->fileID, surf->textureID);
    else if (sprite->textureID > 0)
      img = get_gpu_texture(sprite->fileID, sprite->textureID);
    else if (model->textureID > 0)
      img = get_gpu_texture(sprite->fileID, model->textureID);

    if (img) {
      GPU_SetWrapMode(img, GPU_WRAP_REPEAT, GPU_WRAP_REPEAT);
      GPU_SetImageFilter(img, GPU_FILTER_LINEAR);
    } else {
      continue;
    }

    /* Render MD3 using Triangle Soup for proper clipping */
    int nt = surf->header.numTriangles;
    float *tri_vb = (float *)malloc(nt * 3 * 5 * sizeof(float));
    if (!tri_vb)
      continue;

    int v_added = 0;
    for (int i = 0; i < nt; i++) {
      int idxvec[3] = {surf->triangles[i].indexes[0],
                       surf->triangles[i].indexes[1],
                       surf->triangles[i].indexes[2]};
      float tz_tri[3];
      float tx_tri[3];
      float dz_tri[3];
      int skip_tri = 0;

      for (int v = 0; v < 3; v++) {
        int idx = idxvec[v];
        float lx = (v1[idx].x + interp * (v2[idx].x - v1[idx].x)) *
                   MD3_XYZ_SCALE * scale_factor;
        float ly = (v1[idx].y + interp * (v2[idx].y - v1[idx].y)) *
                   MD3_XYZ_SCALE * scale_factor;
        float lz = (v1[idx].z + interp * (v2[idx].z - v1[idx].z)) *
                   MD3_XYZ_SCALE * scale_factor;

        /* EXACT temp_repo logic: NO axis remap, direct rotation */
        float wx = lx * cos_model - ly * sin_model + sprite->x;
        float wy = lx * sin_model + ly * cos_model + sprite->y;
        float wz = lz + sprite->z;

        float dx = wx - s_cam_x;
        float dy = wy - s_cam_y;
        float dz = wz - s_cam_z + 16.0f;

        /* Camera transform - IDENTICAL to wall transform */
        float tz = dx * s_cos_ang + dy * s_sin_ang;
        float tx = -dx * s_sin_ang + dy * s_cos_ang;

        if (tz <= NEAR_PLANE) {
          skip_tri = 1;
          break;
        }
        tz_tri[v] = tz;
        tx_tri[v] = tx;
        dz_tri[v] = dz;
      }

      if (skip_tri)
        continue;

      for (int v = 0; v < 3; v++) {
        int idx = idxvec[v];
        float scale = s_focal / tz_tri[v];
        /* MD3 Sync: Enable pitch support + use horizon from walls */
        tri_vb[v_added * 5 + 0] = s_half_w + (tx_tri[v] * scale);
        tri_vb[v_added * 5 + 1] = s_horizon - (dz_tri[v] * scale);
        tri_vb[v_added * 5 + 2] = depth_from_tz(tz_tri[v]);
        tri_vb[v_added * 5 + 3] = surf->texCoords[idx].s;
        tri_vb[v_added * 5 + 4] = surf->texCoords[idx].t;
        v_added++;
      }
    }

    /* Painter's Algorithm with qsort — O(n log n), no polygon limit.
       Sort ascending: smaller depth = farther (depth_from_tz maps near→+100,
       far→-100). Far triangles paint first, near ones overwrite on top. */
    typedef struct {
      float depth;
      int base;
    } Md3TriDepth;
    int num_tris_md3 = v_added / 3;
    if (num_tris_md3 > 0) {
      Md3TriDepth *td =
          (Md3TriDepth *)malloc(num_tris_md3 * sizeof(Md3TriDepth));
      float *sorted_vb = (float *)malloc(v_added * 5 * sizeof(float));
      if (td && sorted_vb) {
        for (int t = 0; t < num_tris_md3; t++) {
          int b = t * 3;
          td[t].depth = (tri_vb[b * 5 + 2] + tri_vb[(b + 1) * 5 + 2] +
                         tri_vb[(b + 2) * 5 + 2]) /
                        3.0f;
          td[t].base = b;
        }
        /* Sort ascending: far (small depth) first, near (large depth) last */
        qsort(td, num_tris_md3, sizeof(Md3TriDepth), md3_tri_cmp_asc);
        for (int t = 0; t < num_tris_md3; t++) {
          int src = td[t].base, dst = t * 3;
          memcpy(&sorted_vb[dst * 5], &tri_vb[src * 5], 3 * 5 * sizeof(float));
        }
        GPU_FlushBlitBuffer();
        GPU_SetBlending(img, 0);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        GPU_TriangleBatch(img, target, v_added, sorted_vb, 0, NULL,
                          GPU_BATCH_XYZ_ST);
        GPU_FlushBlitBuffer();
        GPU_SetBlending(img, 1);
      }
      if (td)
        free(td);
      if (sorted_vb)
        free(sorted_vb);
    }
    free(tri_vb);
  }
}

/* ============================================================================
   SPRITE RENDERING (GPU)
   ============================================================================
 */

/* Triangle depth entry for Painter's Algorithm sort */
typedef struct {
  float depth;
  int base; /* index into vertex buffer: tri starts at base*5*3 */
} GltfTriDepth;

static int gltf_tri_depth_cmp(const void *a, const void *b) {
  /* Sort descending (far first) so near triangles paint on top */
  float da = ((const GltfTriDepth *)a)->depth;
  float db = ((const GltfTriDepth *)b)->depth;
  return (da < db) - (da > db); /* >0 if a<b (a is nearer, goes later) */
}

static void ray_render_gltf_gpu(GPU_Target *target, RAY_Sprite *sprite) {
  if (!sprite || !sprite->model)
    return;
  RAY_GLTF_Model *model = (RAY_GLTF_Model *)sprite->model;
  cgltf_data *data = model->data;

  /* Aplicar animación glTF si el modelo tiene animaciones y se ha definido un
   * índice válido */
  if (data->animations_count > 0 && sprite->glb_anim_index >= 0) {
    ray_gltf_apply_animation(model, sprite->glb_anim_index,
                             sprite->glb_anim_time);
  }

  float cos_model = cosf(sprite->rot);
  float sin_model = sinf(sprite->rot);
  float scale_factor =
      (sprite->model_scale > 0.0f) ? sprite->model_scale : 1.0f;

  for (cgltf_size i = 0; i < data->nodes_count; ++i) {
    cgltf_node *node = &data->nodes[i];
    if (!node->mesh)
      continue;

    for (cgltf_size j = 0; j < node->mesh->primitives_count; ++j) {
      cgltf_primitive *prim = &node->mesh->primitives[j];
      cgltf_accessor *pos_acc = NULL;
      cgltf_accessor *uv_acc = NULL;

      for (cgltf_size k = 0; k < prim->attributes_count; ++k) {
        if (prim->attributes[k].type == cgltf_attribute_type_position)
          pos_acc = prim->attributes[k].data;
        if (prim->attributes[k].type == cgltf_attribute_type_texcoord)
          uv_acc = prim->attributes[k].data;
      }

      if (!pos_acc || !prim->indices)
        continue; // Must have positions and indices

      cgltf_float node_mtx[16];
      cgltf_node_transform_world(node, node_mtx);

      if (!prim->indices)
        continue;
      int ni_total = (int)prim->indices->count;

      /* Split rendering into safe batches to avoid 16-bit vertex index limits
       */
      const int batch_indices = 30000;
      for (int i_start = 0; i_start < ni_total; i_start += batch_indices) {
        int i_count = ni_total - i_start;
        if (i_count > batch_indices)
          i_count = batch_indices;
        i_count = (i_count / 3) * 3; // Must be multiple of 3

        /* Allocate 2x to accommodate extra tris from near-plane clipping */
        float *batch_vb = (float *)malloc(i_count * 2 * 5 * sizeof(float));
        if (!batch_vb)
          break;

        int v_added = 0;
        for (int k = 0; k < i_count; k += 3) {
          /* First pass: compute camera-space coords + UVs for all 3 verts */
          float cam_tx[3], cam_tz[3], cam_dz[3], cam_u[3], cam_v[3];
          for (int tri_v = 0; tri_v < 3; tri_v++) {
            int idx = (int)cgltf_accessor_read_index(prim->indices,
                                                     i_start + k + tri_v);
            float p[3];
            cgltf_accessor_read_float(pos_acc, idx, p, 3);

            float nnx = p[0] * node_mtx[0] + p[1] * node_mtx[4] +
                        p[2] * node_mtx[8] + node_mtx[12];
            float nny = p[0] * node_mtx[1] + p[1] * node_mtx[5] +
                        p[2] * node_mtx[9] + node_mtx[13];
            float nnz = p[0] * node_mtx[2] + p[1] * node_mtx[6] +
                        p[2] * node_mtx[10] + node_mtx[14];

            float fwd = -nnz * scale_factor;
            float right = nnx * scale_factor;
            float height = nny * scale_factor;

            float wx = fwd * cos_model - right * sin_model + sprite->x;
            float wy = fwd * sin_model + right * cos_model + sprite->y;
            float wz = height + sprite->z;

            float dx = wx - s_cam_x, dy = wy - s_cam_y;
            cam_dz[tri_v] = wz - s_cam_z;
            cam_tz[tri_v] = dx * s_cos_ang + dy * s_sin_ang;
            cam_tx[tri_v] = -dx * s_sin_ang + dy * s_cos_ang;

            if (uv_acc) {
              float uv[2];
              cgltf_accessor_read_float(uv_acc, idx, uv, 2);
              cam_u[tri_v] = uv[0];
              cam_v[tri_v] = 1.0f - uv[1];
            } else {
              cam_u[tri_v] = 0.0f;
              cam_v[tri_v] = 0.0f;
            }
          }

          /* Count vertices behind near plane */
          int behind[3], num_behind = 0;
          for (int v = 0; v < 3; v++) {
            behind[v] = (cam_tz[v] <= NEAR_PLANE) ? 1 : 0;
            num_behind += behind[v];
          }

          if (num_behind == 3)
            continue; /* Fully behind camera */

/* Helper macro: project a camera-space vertex to screen */
#define PROJECT_VERT(out, idx5, ttx, ttz, tdz, tu, tv)                         \
  do {                                                                         \
    float sc = s_focal / ttz;                                                  \
    (out)[(idx5) + 0] = s_half_w + (ttx) * sc;                                 \
    (out)[(idx5) + 1] = s_horizon - (tdz) * sc;                                \
    (out)[(idx5) + 2] = depth_from_tz(ttz);                                    \
    (out)[(idx5) + 3] = (tu);                                                  \
    (out)[(idx5) + 4] = (tv);                                                  \
  } while (0)

          if (num_behind == 0) {
            /* All visible — emit as-is */
            for (int v = 0; v < 3; v++)
              PROJECT_VERT(batch_vb, (v_added + v) * 5, cam_tx[v], cam_tz[v],
                           cam_dz[v], cam_u[v], cam_v[v]);
            v_added += 3;
          } else if (num_behind == 1) {
            /* 1 behind: clip to a quad (2 triangles) */
            int b = -1;
            for (int v = 0; v < 3; v++)
              if (behind[v]) {
                b = v;
                break;
              }
            int a = (b + 1) % 3, c = (b + 2) % 3;
            /* Clip edge b->a */
            float t_ba = (NEAR_PLANE - cam_tz[b]) / (cam_tz[a] - cam_tz[b]);
            float clip_tx_ba = cam_tx[b] + t_ba * (cam_tx[a] - cam_tx[b]);
            float clip_dz_ba = cam_dz[b] + t_ba * (cam_dz[a] - cam_dz[b]);
            float clip_u_ba = cam_u[b] + t_ba * (cam_u[a] - cam_u[b]);
            float clip_v_ba = cam_v[b] + t_ba * (cam_v[a] - cam_v[b]);
            /* Clip edge b->c */
            float t_bc = (NEAR_PLANE - cam_tz[b]) / (cam_tz[c] - cam_tz[b]);
            float clip_tx_bc = cam_tx[b] + t_bc * (cam_tx[c] - cam_tx[b]);
            float clip_dz_bc = cam_dz[b] + t_bc * (cam_dz[c] - cam_dz[b]);
            float clip_u_bc = cam_u[b] + t_bc * (cam_u[c] - cam_u[b]);
            float clip_v_bc = cam_v[b] + t_bc * (cam_v[c] - cam_v[b]);
            /* Triangle 1: a, clip_ba, clip_bc */
            PROJECT_VERT(batch_vb, (v_added + 0) * 5, cam_tx[a], cam_tz[a],
                         cam_dz[a], cam_u[a], cam_v[a]);
            PROJECT_VERT(batch_vb, (v_added + 1) * 5, clip_tx_ba, NEAR_PLANE,
                         clip_dz_ba, clip_u_ba, clip_v_ba);
            PROJECT_VERT(batch_vb, (v_added + 2) * 5, clip_tx_bc, NEAR_PLANE,
                         clip_dz_bc, clip_u_bc, clip_v_bc);
            v_added += 3;
            /* Triangle 2: a, clip_bc, c */
            PROJECT_VERT(batch_vb, (v_added + 0) * 5, cam_tx[a], cam_tz[a],
                         cam_dz[a], cam_u[a], cam_v[a]);
            PROJECT_VERT(batch_vb, (v_added + 1) * 5, clip_tx_bc, NEAR_PLANE,
                         clip_dz_bc, clip_u_bc, clip_v_bc);
            PROJECT_VERT(batch_vb, (v_added + 2) * 5, cam_tx[c], cam_tz[c],
                         cam_dz[c], cam_u[c], cam_v[c]);
            v_added += 3;
          } else {
            /* 2 behind: clip to single smaller triangle */
            int f = -1;
            for (int v = 0; v < 3; v++)
              if (!behind[v]) {
                f = v;
                break;
              }
            int b1 = (f + 1) % 3, b2 = (f + 2) % 3;
            float t1 = (NEAR_PLANE - cam_tz[f]) / (cam_tz[b1] - cam_tz[f]);
            float t2 = (NEAR_PLANE - cam_tz[f]) / (cam_tz[b2] - cam_tz[f]);
            float c_tx1 = cam_tx[f] + t1 * (cam_tx[b1] - cam_tx[f]);
            float c_dz1 = cam_dz[f] + t1 * (cam_dz[b1] - cam_dz[f]);
            float c_u1 = cam_u[f] + t1 * (cam_u[b1] - cam_u[f]);
            float c_v1 = cam_v[f] + t1 * (cam_v[b1] - cam_v[f]);
            float c_tx2 = cam_tx[f] + t2 * (cam_tx[b2] - cam_tx[f]);
            float c_dz2 = cam_dz[f] + t2 * (cam_dz[b2] - cam_dz[f]);
            float c_u2 = cam_u[f] + t2 * (cam_u[b2] - cam_u[f]);
            float c_v2 = cam_v[f] + t2 * (cam_v[b2] - cam_v[f]);
            PROJECT_VERT(batch_vb, (v_added + 0) * 5, cam_tx[f], cam_tz[f],
                         cam_dz[f], cam_u[f], cam_v[f]);
            PROJECT_VERT(batch_vb, (v_added + 1) * 5, c_tx1, NEAR_PLANE, c_dz1,
                         c_u1, c_v1);
            PROJECT_VERT(batch_vb, (v_added + 2) * 5, c_tx2, NEAR_PLANE, c_dz2,
                         c_u2, c_v2);
            v_added += 3;
          }
#undef PROJECT_VERT
        }

        if (v_added == 0) {
          free(batch_vb);
          continue;
        }

        GPU_Image *img = NULL;
        if (prim->material) {
          cgltf_texture *tex =
              prim->material->pbr_metallic_roughness.base_color_texture.texture;
          if (tex && tex->image) {
            for (int m = 0; m < model->textures_count; m++) {
              if (&model->data->images[m] == tex->image) {
                img = model->textures[m];
                break;
              }
            }
          }
        }

        if (!img && model->textures_count > 0)
          img = model->textures[0];
        if (!img)
          img = get_gpu_texture(sprite->fileID, sprite->textureID);
        if (img) {
          GPU_SetWrapMode(img, GPU_WRAP_REPEAT, GPU_WRAP_REPEAT);
          GPU_FlushBlitBuffer();

          /* Now that depth_from_tz is correctly inverted for SDL_gpu's ortho,
             the GPU depth buffer handles triangle ordering properly.
             Disable blending for opaque model rendering. */
          GPU_SetBlending(img, 0);
          glEnable(GL_DEPTH_TEST);
          glDepthMask(GL_TRUE);
          glDepthFunc(GL_LESS);

          /* Alpha test: discard fully transparent pixels without blending.
             Fixes holes in textures with alpha cutout areas. */
          glEnable(GL_ALPHA_TEST);
          glAlphaFunc(GL_GREATER, 0.1f);

          GPU_TriangleBatch(img, target, v_added, batch_vb, 0, NULL,
                            GPU_BATCH_XYZ_ST);
          GPU_FlushBlitBuffer();

          GPU_SetBlending(img, 1);
          glDisable(GL_DEPTH_TEST);
          glDisable(GL_ALPHA_TEST);
        }
        free(batch_vb);
      }
    }
  }
}

/* ============================================================================
   SOFTWARE SPRITE OCCLUSION: Check if a sprite is hidden behind island walls.
   Casts a 2D ray from camera to sprite and tests against all island sector
   walls. Returns 1 if occluded, 0 if visible.
   ============================================================================
 */
static int sprite_occluded_by_islands(float sprite_x, float sprite_y,
                                      float sprite_z) {
  float ray_dx = sprite_x - s_cam_x;
  float ray_dy = sprite_y - s_cam_y;
  float sprite_dist_sq = ray_dx * ray_dx + ray_dy * ray_dy;
  if (sprite_dist_sq < 1.0f)
    return 0; /* Too close to camera */

  /* Quick check against pre-cached island sectors */
  for (int i = 0; i < s_num_islands; i++) {
    /* Check if the sprite's Z is within the island's vertical range */
    if (sprite_z > s_island_ceil[i] || sprite_z < s_island_floor[i])
      continue;

    /* Test ray against each wall of this island sector */
    RAY_Sector *sector = s_island_sectors[i];
    for (int w = 0; w < sector->num_walls; w++) {
      RAY_Wall *wall = &sector->walls[w];
      float wx = wall->x2 - wall->x1;
      float wy = wall->y2 - wall->y1;

      float det = ray_dx * wy - ray_dy * wx;
      if (fabsf(det) < 0.001f)
        continue;

      float dx = wall->x1 - s_cam_x;
      float dy = wall->y1 - s_cam_y;

      float t = (dx * wy - dy * wx) / det;
      float u = (dx * ray_dy - dy * ray_dx) / det;

      if (t > 0.01f && t < 0.99f && u >= 0.0f && u <= 1.0f) {
        return 1; /* Occluded */
      }
    }
  }
  return 0;
}

static void render_sprite_gpu(GPU_Target *target, RAY_Sprite *sprite) {
  if (sprite->hidden || sprite->cleanup)
    return;

  /* Software occlusion: skip sprites hidden behind island walls */
  if (sprite_occluded_by_islands(sprite->x, sprite->y, sprite->z))
    return;

  if (sprite->model) {
    uint32_t *magic = (uint32_t *)sprite->model;
    if (*magic == MD3_MAGIC) {
      ray_render_md3_gpu(target, sprite);
      return;
    }
    if (*magic == GLTF_MAGIC) {
      ray_render_gltf_gpu(target, sprite);
      return;
    }
    return;
  }

  /* Regular billboard sprite */
  float dx = sprite->x - s_cam_x, dy = sprite->y - s_cam_y;
  float tz = dx * s_cos_ang + dy * s_sin_ang;
  if (tz < NEAR_PLANE)
    return;

  float tx = -dx * s_sin_ang + dy * s_cos_ang;
  float ty = (sprite->z + sprite->h / 2.0f) - s_cam_z;

  float scale = s_focal / tz;
  float sx = s_half_w + (tx * scale);
  float sy = s_half_h - (ty * scale);
  float sw = sprite->w * scale;
  float sh = sprite->h * scale;
  float sz = depth_from_tz(tz);

  GPU_Image *img = NULL;
  if (sprite->process_ptr) {
    GRAPH *g = instance_graph(sprite->process_ptr);
    if (g)
      img = (GPU_Image *)g->tex;
  }
  img = get_gpu_texture(sprite->fileID, sprite->textureID);
  if (!img)
    return;

  float vb[20] = {
      sx - sw / 2, sy - sh / 2, sz, 0, 0, sx + sw / 2, sy - sh / 2, sz, 1, 0,
      sx + sw / 2, sy + sh / 2, sz, 1, 1, sx - sw / 2, sy + sh / 2, sz, 0, 1};
  unsigned short ib[6] = {0, 1, 2, 0, 2, 3};
  GPU_SetImageFilter(img, GPU_FILTER_LINEAR);
  GPU_TriangleBatch(img, target, 4, vb, 6, ib, GPU_BATCH_XYZ_ST);
}

static int sprite_sorter_gpu(const void *a, const void *b) {
  const RAY_Sprite *sa = (const RAY_Sprite *)a;
  const RAY_Sprite *sb = (const RAY_Sprite *)b;
  if (sa->distance > sb->distance)
    return -1;
  if (sa->distance < sb->distance)
    return 1;
  return 0;
}

void ray_render_frame_gpu(void *dest_graph_ptr) {
  GRAPH *dest = (GRAPH *)dest_graph_ptr;
  if (!dest)
    return;

  GPU_Target *target = NULL;
  if (dest->code == 0) {
    target = GPU_GetContextTarget();
    if (!target)
      target = gRenderer;
  } else {
    GPU_Image *image = (GPU_Image *)dest->tex;
    if (!image) {
      image = GPU_CreateImage(dest->width, dest->height, GPU_FORMAT_RGBA);
      if (image)
        dest->tex = (void *)image;
    }
    if (image) {
      target = image->target;
      if (!target) {
        target = GPU_LoadTarget(image);
        if (target) {
          GPU_AddDepthBuffer(target);
          printf("RAY_GPU: Added depth buffer to FBO target\n");
        }
      }
    }
  }
  if (!target)
    return;

  int current_sector = g_engine.camera.current_sector_id;
  RAY_Sector *cam_sector = find_sector_by_id(current_sector);
  if (!cam_sector)
    return;

  s_screen_w = g_engine.displayWidth;
  s_screen_h = g_engine.displayHeight;
  s_half_w = s_screen_w / 2;
  s_half_h = s_screen_h / 2;
  s_focal = (float)s_half_w;
  s_cam_x = g_engine.camera.x;
  s_cam_y = g_engine.camera.y;
  s_cam_z = g_engine.camera.z;
  float ang = g_engine.camera.rot;
  s_cos_ang = cosf(ang);
  s_sin_ang = sinf(ang);
  s_horizon = s_half_h + (int)g_engine.camera.pitch;

  /* Set global fog from camera's current sector */
  s_fog_r = cam_sector->fog_color_r;
  s_fog_g = cam_sector->fog_color_g;
  s_fog_b = cam_sector->fog_color_b;
  s_fog_density = cam_sector->fog_density;
  s_fog_start = cam_sector->fog_start;
  s_fog_end = cam_sector->fog_end;

  /* DIAGNOSTIC: Check if we have a real depth buffer */
  static int depth_checked = 0;
  if (!depth_checked) {
    int depth_bits = 0;
    SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depth_bits);
    printf("RAY_GPU: Hardware Depth Buffer Bits: %d\n", depth_bits);
    depth_checked = 1;
  }

  /* 1. SETUP DEPTH BUFFER & RENDERING STATE */
  GPU_FlushBlitBuffer();
  /* World rendering: Enable depth test for correct occlusion (islands, RoR,
   * etc) */
  GPU_SetDepthTest(target, 1);
  GPU_SetDepthWrite(target, 1);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);

  /* Disable culling for complex glTF models with non-standard winding */
  glDisable(GL_CULL_FACE);

  /* 1.5 RENDER SKY PANORAMA */
  if (g_engine.skyTextureID > 0) {
    GPU_Image *sky_tex =
        get_gpu_texture(g_engine.fpg_id, g_engine.skyTextureID);
    if (sky_tex) {
      GPU_SetWrapMode(sky_tex, GPU_WRAP_REPEAT, GPU_WRAP_NONE);
      GPU_SetImageFilter(sky_tex, GPU_FILTER_LINEAR);

      /* Calculate texture horizontal offset based on camera rotation.
         Angle 0.0 means center of texture.
         Rotate 360 degrees = full texture wrap. */

      /* Calculate texture horizontal offset based on camera rotation. */
      float angle = g_engine.camera.rot;
      angle = fmodf(angle, 2.0f * M_PI);
      if (angle < 0)
        angle += 2.0f * M_PI;

      /* DOOM STYLE: Larger coverage to avoid fast repetition */
      float u_center = angle / (2.0f * M_PI);
      float fov_ratio = g_engine.fovRadians / (2.0f * M_PI);

      float u0 = u_center - fov_ratio * 0.5f;
      float u1 = u_center + fov_ratio * 0.5f;

      /* Vertical: Doom skies are centered at horizon and move 1:1 with pitch or
       * slower */
      float v_offset = (float)g_engine.camera.pitch / (float)s_screen_h * 0.8f;
      float v0 = 0.0f - v_offset;
      float v1 = 1.0f - v_offset;

      /* Let's use a quad batch for precise panorama wrapping.
         Z = -100 is the FAR plane in our depth_from_tz mapping. */
      float sk_vb[20] = {0,
                         0,
                         -100,
                         u0,
                         v0,
                         (float)s_screen_w,
                         0,
                         -100,
                         u1,
                         v0,
                         (float)s_screen_w,
                         (float)s_screen_h,
                         -100,
                         u1,
                         v1,
                         0,
                         (float)s_screen_h,
                         -100,
                         u0,
                         v1};
      unsigned short sk_ib[6] = {0, 1, 2, 0, 2, 3};

      GPU_FlushBlitBuffer();
      glDisable(GL_DEPTH_TEST);
      glDepthMask(GL_FALSE); /* Don't write sky to depth buffer */

      GPU_TriangleBatch(sky_tex, target, 4, sk_vb, 6, sk_ib, GPU_BATCH_XYZ_ST);

      GPU_FlushBlitBuffer();
      glEnable(GL_DEPTH_TEST);
      glDepthMask(GL_TRUE);
    }
  } else {
    GPU_ClearColor(target, (SDL_Color){30, 30, 30, 255});
    GPU_Clear(target);
  }

  // Actually render the world scene
  ray_render_scene_gpu(target, current_sector);
}

static int sprite_ptr_sorter_gpu(const void *a, const void *b) {
  const RAY_Sprite *sa = *(const RAY_Sprite **)a;
  const RAY_Sprite *sb = *(const RAY_Sprite **)b;
  if (sa->distance > sb->distance)
    return -1;
  if (sa->distance < sb->distance)
    return 1;
  return 0;
}

void ray_render_scene_gpu(GPU_Target *target, int current_sector) {
  if (!g_engine.initialized || current_sector < 0) {
    if (target) {
      GPU_ClearColor(target, (SDL_Color){30, 30, 30, 255});
      GPU_Clear(target);
    }
    return;
  }
  glClear(GL_DEPTH_BUFFER_BIT);

  ClipRect full_clip = {0, 0, (float)s_screen_w, (float)s_screen_h};

  /* Find root parent for correct nested context */
  int render_root = current_sector;
  RAY_Sector *rr_sec = find_sector_by_id(render_root);
  while (rr_sec && rr_sec->parent_sector_id != -1) {
    render_root = rr_sec->parent_sector_id;
    rr_sec = find_sector_by_id(render_root);
  }

  /* Pre-build island cache for sprite occlusion (once per frame).
     A sector is a solid island occluder only if it is a box completely
     INSIDE its parent: floor above parent floor AND ceiling below parent
     ceiling. Elevated-floor sectors (ramps, steps) where ceiling==parent
     ceiling are NOT solid occluders. */
  s_num_islands = 0;
  for (int si = 0;
       si < g_engine.num_sectors && s_num_islands < MAX_ISLAND_SECTORS; si++) {
    RAY_Sector *sec = &g_engine.sectors[si];
    if (sec->parent_sector_id == -1)
      continue;
    RAY_Sector *par = find_sector_by_id(sec->parent_sector_id);
    if (!par)
      continue;
    /* Must be strictly inside the parent vertically on BOTH ends */
    if (sec->floor_z <= par->floor_z + 0.1f)
      continue; /* floor not above parent floor → not a box */
    if (sec->ceiling_z >= par->ceiling_z - 0.1f)
      continue; /* ceiling not below parent ceiling → not a box */
    s_island_sectors[s_num_islands] = sec;
    s_island_floor[s_num_islands] = sec->floor_z;
    s_island_ceil[s_num_islands] = sec->ceiling_z;
    s_num_islands++;
  }

  /* Pass 0: Opaque geometry */
  visited_clear();
  render_sector_gpu(target, render_root, full_clip, 0, 0, -99999.0f, 99999.0f,
                    0);

  /* Flush walls to depth buffer before sprites */
  GPU_FlushBlitBuffer();

  /* 3. RENDER SPRITES AND MODELS (Ordered Back-to-Front) */
  /* Enable depth testing for 3D models via SDL_gpu API */
  GPU_SetDepthTest(target, 1);
  GPU_SetDepthWrite(target, 1);
  GPU_SetDepthFunction(target, GPU_LEQUAL);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LEQUAL);

  /* Pre-calculate distances for sorting */
  if (g_engine.num_sprites > 0) {
    RAY_Sprite **sorted_ptrs =
        (RAY_Sprite **)malloc(g_engine.num_sprites * sizeof(RAY_Sprite *));
    if (sorted_ptrs) {
      for (int i = 0; i < g_engine.num_sprites; i++) {
        float dx = g_engine.sprites[i].x - s_cam_x;
        float dy = g_engine.sprites[i].y - s_cam_y;
        g_engine.sprites[i].distance = sqrtf(dx * dx + dy * dy);
        sorted_ptrs[i] = &g_engine.sprites[i];
      }

      /* Sort the pointer list, not the main array! */
      qsort(sorted_ptrs, g_engine.num_sprites, sizeof(RAY_Sprite *),
            sprite_ptr_sorter_gpu);

      for (int i = 0; i < g_engine.num_sprites; i++) {
        render_sprite_gpu(target, sorted_ptrs[i]);
      }
      free(sorted_ptrs);
    }
  }

  GPU_FlushBlitBuffer();

  /* Pass 1: Transparent liquids (Drawn after sprites so they can be seen
   * through) */
  visited_clear();
  render_sector_gpu(target, render_root, full_clip, 0, 0, -99999.0f, 99999.0f,
                    1);

  GPU_FlushBlitBuffer();
  GPU_SetDepthTest(target, 0);
  GPU_SetDepthWrite(target, 0);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glFrontFace(GL_CCW); /* Restore default winding */
}
