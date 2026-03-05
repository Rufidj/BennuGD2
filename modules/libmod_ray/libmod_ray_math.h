#ifndef __LIBMOD_RAY_MATH_H
#define __LIBMOD_RAY_MATH_H

#include <math.h>
#include <string.h>

/* Simple 4x4 Column-Major Matrix (Matches GL/cgltf) */
typedef float mat4[16];

static inline void mat4_identity(mat4 m) {
  memset(m, 0, sizeof(mat4));
  m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static inline void mat4_mul(mat4 out, const mat4 a, const mat4 b) {
  mat4 res;
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      res[col * 4 + row] =
          a[0 * 4 + row] * b[col * 4 + 0] + a[1 * 4 + row] * b[col * 4 + 1] +
          a[2 * 4 + row] * b[col * 4 + 2] + a[3 * 4 + row] * b[col * 4 + 3];
    }
  }
  memcpy(out, res, sizeof(mat4));
}

static inline void mat4_from_trs(mat4 m, const float t[3], const float q[4],
                                 const float s[3]) {
  mat4_identity(m);

  /* Rotation from Quaternion */
  float x = q[0], y = q[1], z = q[2], w = q[3];
  float x2 = x + x, y2 = y + y, z2 = z + z;
  float xx = x * x2, xy = x * y2, xz = x * z2;
  float yy = y * y2, yz = y * z2, zz = z * z2;
  float wx = w * x2, wy = w * y2, wz = w * z2;

  m[0] = (1.0f - (yy + zz));
  m[4] = (xy - wz);
  m[8] = (xz + wy);
  m[12] = t[0];
  m[1] = (xy + wz);
  m[5] = (1.0f - (xx + zz));
  m[9] = (yz - wx);
  m[13] = t[1];
  m[2] = (xz - wy);
  m[6] = (yz + wx);
  m[10] = (1.0f - (xx + yy));
  m[14] = t[2];
  m[3] = 0.0f;
  m[7] = 0.0f;
  m[11] = 0.0f;
  m[15] = 1.0f;

  /* Scale */
  m[0] *= s[0];
  m[1] *= s[0];
  m[2] *= s[0];
  m[4] *= s[1];
  m[5] *= s[1];
  m[6] *= s[1];
  m[8] *= s[2];
  m[9] *= s[2];
  m[10] *= s[2];
}

static inline void vec3_mul_mat4(float out[3], const float v[3], const mat4 m) {
  float res[3];
  res[0] = v[0] * m[0] + v[1] * m[4] + v[2] * m[8] + m[12];
  res[1] = v[0] * m[1] + v[1] * m[5] + v[2] * m[9] + m[13];
  res[2] = v[0] * m[2] + v[1] * m[6] + v[2] * m[10] + m[14];
  memcpy(out, res, sizeof(float) * 3);
}

#endif
