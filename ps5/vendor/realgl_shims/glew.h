/* Minimal stand-in for GLEW: sdl-gpu's OpenGL_3/4 renderers #include "glew.h"
 * purely to get a complete set of GL types/constants/function prototypes
 * (traditionally via GLEW's own runtime extension-loading machinery). On
 * this platform ps5-opengl's GL/gl.h + GL/glext.h already declare every
 * OpenGL 4.6 Core entry point directly (statically linked, no runtime
 * extension loading needed) - this file just forwards to those instead of
 * implementing any of GLEW's own loader. */
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GL/gl.h>
#include <GL/glext.h>

/* GLEW's own runtime extension-presence check. This driver always reports a
 * full OpenGL 4.6 Core implementation (no optional-extension gaps the
 * callers of this check in sdl-gpu care about), so "always present" is
 * correct here, not a stub standing in for missing functionality. */
static inline int glewIsExtensionSupported(const char *extension_str) {
    (void)extension_str;
    return 1;
}

/* glewInit()/GLEW_OK/glewExperimental: GLEW's own "load extension function
 * pointers" entry point and status. Every function this driver exposes is
 * already a real, statically-linked symbol (no runtime extension loading at
 * all, see glewIsExtensionSupported above) - glewInit() has nothing to do
 * and always succeeds. */
#define GLEW_OK 0
static int glewExperimental = 1;
static inline int glewInit(void) { return GLEW_OK; }
