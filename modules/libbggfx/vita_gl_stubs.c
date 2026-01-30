#include <SDL2/SDL.h>
#include <GLES2/gl2.h>

// Stubs for missing OpenGL functions in vitaGL

void glGetTexParameteriv(GLenum target, GLenum pname, GLint *params) {
    if (params) *params = 0;
}

void glDetachShader(GLuint program, GLuint shader) {
    // No-op
}

void glGetUniformiv(GLuint program, GLint location, GLint *params) {
    if (params) *params = 0;
}

void glGetUniformfv(GLuint program, GLint location, GLfloat *params) {
    if (params) *params = 0.0f;
}

// Technically not GLES2 standard but missing in linker
void glGetUniformuiv(GLuint program, GLint location, GLuint *params) {
    if (params) *params = 0;
}
