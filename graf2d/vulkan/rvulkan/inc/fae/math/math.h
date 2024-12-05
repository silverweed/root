#pragma once

#include "../core/types.h"
#include "../core/log.h"

#include <math.h>
#include <float.h>
#include <assert.h>

inline const f64 K_PI64           = 3.14159265358979323846;
inline const f32 K_PI             = 3.1415927f;
inline const f32 K_TAU            = 6.2831853f;
inline const f32 K_HALF_PI        = 1.5707963f;
inline const f32 K_QUARTER_PI     = 0.7853982f;
inline const f32 K_ONE_OVER_SQRT2 = 0.70710678f;

typedef struct V2 {
  f32 x, y;
} V2;

typedef struct V3 {
  f32 x, y, z;
} V3;

typedef struct V4 {
  f32 x, y, z, w;
} V4;

typedef union M33 {
  // column major:
  // m[1][2] = m21 = row2, col1
  f32 m[3][3];
  V3  col[3];
} M33;

typedef union M44 {
  // column major:
  // m[1][2] = m21 = row2, col1
  f32 m[4][4];
  V4  col[4];
} M44;

inline
f32 deg2rad(f32 deg)
{
  return deg * 0.017453292f;
}

inline
f32 rad2deg(f32 rad)
{
  return rad * 57.29578f;
}

inline
f32 lerp(f32 a, f32 b, f32 t)
{
  return a * (1.f - t) + b * t;
}

inline
V2 v2(f32 x, f32 y)
{
  V2 v;
  v.x = x;
  v.y = y;
  return v;  
}

inline
V2 v2_xz(V3 v)
{
  V2 res;
  res.x = v.x;
  res.y = v.z;
  return res;
}

inline
V2 v2_neg(V2 v)
{
  V2 res;
  res.x = -v.x;
  res.y = -v.y;
  return res;
}

inline
V3 v3(f32 x, f32 y, f32 z)
{
  V3 v;
  v.x = x;
  v.y = y;
  v.z = z;
  return v;
}

inline
V3 v3s(f32 s)
{
  V3 res = v3(s, s, s);
  return res;
}

inline
V3 v3_from_v4(V4 v)
{
  V3 res;
  res.x = v.x;
  res.y = v.y;
  res.z = v.z;
  return res;
}

// Converts a number like 0xff00ff to a normalized (color) vector like v3(1, 0, 1)
inline
V3 v3_from_hex(u32 hex)
{
  V3 v;
  v.x = ((hex >> 16) & 0xFF) / 255.f;
  v.y = ((hex >> 8) & 0xFF) / 255.f;
  v.z = (hex & 0xFF) / 255.f;
  return v;  
}

inline
V3 v3_neg(V3 v)
{
  V3 n;
  n.x = -v.x;
  n.y = -v.y;
  n.z = -v.z;
  return n;
}

inline
V4 v4(f32 x, f32 y, f32 z, f32 w)
{
  V4 v;
  v.x = x;
  v.y = y;
  v.z = z;
  v.w = w;
  return v;
}

inline
V4 v4_from_v3(V3 v, f32 w)
{
  V4 res;
  res.x = v.x;
  res.y = v.y;
  res.z = v.z;
  res.w = w;
  return res;
}

inline
f32 v4_dot(V4 a, V4 b)
{
  f32 res = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  return res;
}

inline
f32 v2_len2(V2 v)
{
  f32 res = v.x * v.x + v.y * v.y;
  return res;
}

inline
f32 v2_len(V2 v)
{
  f32 res = v2_len2(v);
  res = sqrtf(res);
  return res;
}

inline
V2 v2_normalized(V2 v)
{
  f32 len2 = v2_len2(v);
  if (len2 <= FLT_EPSILON)
    return v;

  f32 len = sqrtf(len2);
  V2 res = v;
  res.x /= len;
  res.y /= len;
  return res;
}

inline
void v2_normalize(V2 *v)
{
  *v = v2_normalized(*v);
}

inline
V2 v2_muls(V2 v, f32 s)
{
  V2 res = v;
  res.x *= s;
  res.y *= s;
  return res;
}

inline
V2 v2_add(V2 a, V2 b)
{
  V2 res = a;
  res.x += b.x;
  res.y += b.y;
  return res;
}

inline
V2 v2_sub(V2 a, V2 b)
{
  V2 res = a;
  res.x -= b.x;
  res.y -= b.y;
  return res;
}

inline
V3 v3_x0z(V2 xz)
{
  V3 v;
  v.x = xz.x;
  v.y = 0.f;
  v.z = xz.y;
  return v;
}

inline
V3 v3_xy0(V2 xz)
{
  V3 v;
  v.x = xz.x;
  v.y = xz.y;
  v.z = 0.f;
  return v;
}

inline
V3 v3_add(V3 a, V3 b)
{
  V3 res = a;
  res.x += b.x;
  res.y += b.y;
  res.z += b.z;
  return res;
}

inline
V3 v3_sub(V3 a, V3 b)
{
  V3 res = a;
  res.x -= b.x;
  res.y -= b.y;
  res.z -= b.z;
  return res;
}

inline
V3 v3_muls(V3 v, f32 s)
{
  V3 res = v;
  res.x *= s;
  res.y *= s;
  res.z *= s;
  return res;
}

inline
V3 v3_avg(V3 a, V3 b)
{
  V3 res = v3_add(a, b);
  res = v3_muls(res, 0.5f);
  return res;
}

inline
V3 v3_mul(V3 a, V3 b)
{
  V3 res = a;
  res.x *= b.x;
  res.y *= b.y;
  res.z *= b.z;
  return res;
}

inline
f32 v3_len2(V3 v)
{
  f32 res = v.x * v.x + v.y * v.y + v.z * v.z;
  return res;
}

inline
f32 v3_len(V3 v)
{
  f32 l = v3_len2(v);
  l = sqrtf(l);
  return l;
}

inline
f32 v3_distance(V3 a, V3 b)
{
  V3 d = v3_sub(b, a);
  f32 l = v3_len(d);
  return l;
}

inline
V3 v3_normalized(V3 v)
{
  f32 len2 = v3_len2(v);
  if (len2 <= FLT_EPSILON)
    return v;

  f32 len = sqrtf(len2);
  V3 res = v;
  res.x /= len;
  res.y /= len;
  res.z /= len;
  return res;
}

inline
f32 v3_dot(V3 a, V3 b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline
V3 cross(V3 a, V3 b)
{
  return (V3) {
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x
  };
}

inline
M33 m33(f32 m00, f32 m01, f32 m02,
        f32 m10, f32 m11, f32 m12,
        f32 m20, f32 m21, f32 m22)
{
  M33 r;
  r.m[0][0] = m00; r.m[0][1] = m10; r.m[0][2] = m20;
  r.m[1][0] = m01; r.m[1][1] = m11; r.m[1][2] = m21;
  r.m[2][0] = m02; r.m[2][1] = m12; r.m[2][2] = m22;
  return r;
}

inline
M44 m44(f32 m00, f32 m01, f32 m02, f32 m03,
        f32 m10, f32 m11, f32 m12, f32 m13,
        f32 m20, f32 m21, f32 m22, f32 m23,
        f32 m30, f32 m31, f32 m32, f32 m33)
{
  M44 r;
  r.m[0][0] = m00; r.m[0][1] = m10; r.m[0][2] = m20; r.m[0][3] = m30;
  r.m[1][0] = m01; r.m[1][1] = m11; r.m[1][2] = m21; r.m[1][3] = m31;
  r.m[2][0] = m02; r.m[2][1] = m12; r.m[2][2] = m22; r.m[2][3] = m32;
  r.m[3][0] = m03; r.m[3][1] = m13; r.m[3][2] = m23; r.m[3][3] = m33;
  return r;
}

inline
M44 ortho_proj(f32 w, f32 h, f32 d)
{
  // Note that, differently from OpenGL, Vulkan's default depth range is [0, 1]
  // and its clip Y coordinate grows downward. 
  // NOTE: assuming near = 0
  M44 m = m44(2.f / w, 0.f,     0.f,      0.f,
              0.f,     2.f / h, 0.f,      0.f,
              0.f,     0.f,     1.f / d,  1.f,
              0.f,     0.f,     0.f,      1.f);
  return m;
}

inline
M44 rev_ortho_proj(f32 w, f32 h, f32 d)
{
  // Note that, differently from OpenGL, Vulkan's default depth range is [0, 1]
  // and its clip Y coordinate grows downward. 
  // NOTE: assuming near = 0
  M44 m = m44(2.f / w, 0.f,     0.f,      0.f,
              0.f,     2.f / h, 0.f,      0.f,
              0.f,     0.f,     -1.f / d, 1.f,
              0.f,     0.f,     0.f,      1.f);
  return m;
}

// NOTE: aspect_ratio = width / height
inline
M44 rev_persp_proj(f32 fovy, f32 aspect_ratio, f32 n)
{
  const f32 PROJ_EPSILON = 1e-8f;

  f32 epsilon = PROJ_EPSILON;
  f32 s = aspect_ratio;
  f32 g = 1.f / tanf(fovy * 0.5f);

  return m44(
    g / s, 0.f, 0.f,     0.f,
      0.f,   g, 0.f,     0.f,
      0.f, 0.f, epsilon, n * (1 - epsilon),
      0.f, 0.f, 1.f,     0.f
  );
}

inline
M33 m33_identity()
{
  M33 m = {};
  m.m[0][0] = m.m[1][1] = m.m[2][2] = 1.f;
  return m;
}

inline
M33 m33_from_m44(const M44 *m)
{
  M33 res = m33(
    m->m[0][0], m->m[1][0], m->m[2][0],
    m->m[0][1], m->m[1][1], m->m[2][1],
    m->m[0][2], m->m[1][2], m->m[2][2]
  );
  return res;
}

inline
M44 m44_identity()
{
  M44 m = {};
  m.m[0][0] = m.m[1][1] = m.m[2][2] = m.m[3][3] = 1.f;
  return m;
}

inline
M44 m44_from_m33(const M33 *m)
{
  M44 res = m44(
    m->m[0][0], m->m[1][0], m->m[2][0], 0.f,
    m->m[0][1], m->m[1][1], m->m[2][1], 0.f,
    m->m[0][2], m->m[1][2], m->m[2][2], 0.f,
    0.f,       0.f,       0.f,       1.f
  );
  return res;
}

inline
M44 m44_transposed(const M44 *m)
{
  M44 res = m44(
    m->m[0][0], m->m[1][0], m->m[2][0], m->m[3][0],
    m->m[0][1], m->m[1][1], m->m[2][1], m->m[3][1],
    m->m[0][2], m->m[1][2], m->m[2][2], m->m[3][2],
    m->m[0][3], m->m[1][3], m->m[2][3], m->m[3][3]
  );
  return res;
}

inline
M44 m44_inverse(const M44 *m)
{
  V3 a = v3_from_v4(m->col[0]);
  V3 b = v3_from_v4(m->col[1]);
  V3 c = v3_from_v4(m->col[2]);
  V3 d = v3_from_v4(m->col[3]);

  f32 x = m->m[0][3];
  f32 y = m->m[1][3];
  f32 z = m->m[2][3];
  f32 w = m->m[3][3];

  V3 s = cross(a, b);
  V3 t = cross(c, d);
  V3 u = v3_sub(v3_muls(a, y), v3_muls(b, x));
  V3 v = v3_sub(v3_muls(c, w), v3_muls(d, z));

  f32 inv_det = 1.f / (v3_dot(s, v) + v3_dot(t, u));
  s = v3_muls(s, inv_det);
  t = v3_muls(t, inv_det);
  u = v3_muls(u, inv_det);
  v = v3_muls(v, inv_det);

  V3 r0 = v3_add(cross(b, v), v3_muls(t, y));
  V3 r1 = v3_sub(cross(v, a), v3_muls(t, x));
  V3 r2 = v3_add(cross(d, u), v3_muls(s, w));
  V3 r3 = v3_sub(cross(u, c), v3_muls(s, z));

  return m44(
    r0.x, r0.y, r0.z, -v3_dot(b, t),
    r1.x, r1.y, r1.z,  v3_dot(a, t),
    r2.x, r2.y, r2.z, -v3_dot(d, s),
    r3.x, r3.y, r3.z,  v3_dot(c, s)
  );
}

typedef struct {
  union {
    struct {
      f32 x, y, z;
    };
    V3 v; // = axis * cos(angle/2)
  };
  f32 w;  // = sin(angle/2)
} Quat;

inline
M33 quat_to_rot_matrix(Quat q)
{
  f32 x2 = Fae_Square(q.x);
  f32 y2 = Fae_Square(q.y);
  f32 z2 = Fae_Square(q.z);
  f32 xy = q.x * q.y;
  f32 xz = q.x * q.z;
  f32 yz = q.y * q.z;
  f32 wx = q.w * q.x;
  f32 wy = q.w * q.y;
  f32 wz = q.w * q.z;
  M33 res = m33(
    1.f - 2.f * (y2 + z2), 2.f * (xy - wz), 2.f * (xz + wy),
    2.f * (xy + wz), 1.f - 2.f * (x2 + z2), 2.f * (yz - wx),
    2.f * (xz - wy), 2.f * (yz + wx), 1.f - 2.f * (x2 + y2)
  );
  return res;
}

inline
M44 transform_inverse(const M44 *m) {
  V3 a = v3_from_v4(m->col[0]);
  V3 b = v3_from_v4(m->col[1]);
  V3 c = v3_from_v4(m->col[2]);
  V3 d = v3_from_v4(m->col[3]);

  V3 s = cross(a, b);
  V3 t = cross(c, d);

  f32 inv_det = 1.f / v3_dot(s, c);
  s = v3_muls(s, inv_det);
  t = v3_muls(t, inv_det);

  V3 v = v3_muls(c, inv_det);

  V3 r0 = cross(b, v);
  V3 r1 = cross(v, a);

  // @Xform
  return m44(
    r0.x, r0.y, r0.z, -v3_dot(b, t),
    r1.x, r1.y, r1.z,  v3_dot(a, t),
     s.x,  s.y,  s.z, -v3_dot(d, s),
     0.f,  0.f,  0.f,  1.f
  );
}

inline
M33 m33_mul(const M33 *a, const M33 *b)
{
  M33 res = m33(
    a->m[0][0] * b->m[0][0] + a->m[1][0] * b->m[0][1] + a->m[2][0] * b->m[0][2],
    a->m[0][0] * b->m[1][0] + a->m[1][0] * b->m[1][1] + a->m[2][0] * b->m[1][2],
    a->m[0][0] * b->m[2][0] + a->m[1][0] * b->m[2][1] + a->m[2][0] * b->m[2][2],

    a->m[0][1] * b->m[0][0] + a->m[1][1] * b->m[0][1] + a->m[2][1] * b->m[0][2],
    a->m[0][1] * b->m[1][0] + a->m[1][1] * b->m[1][1] + a->m[2][1] * b->m[1][2],
    a->m[0][1] * b->m[2][0] + a->m[1][1] * b->m[2][1] + a->m[2][1] * b->m[2][2],

    a->m[0][2] * b->m[0][0] + a->m[1][2] * b->m[0][1] + a->m[2][2] * b->m[0][2],
    a->m[0][2] * b->m[1][0] + a->m[1][2] * b->m[1][1] + a->m[2][2] * b->m[1][2],
    a->m[0][2] * b->m[2][0] + a->m[1][2] * b->m[2][1] + a->m[2][2] * b->m[2][2]
  );

  return res;
}

inline
M44 m44_mul(const M44 *a, const M44 *b)
{
  M44 res = m44(
    a->m[0][0] * b->m[0][0] + a->m[1][0] * b->m[0][1] + a->m[2][0] * b->m[0][2] + a->m[3][0] * b->m[0][3],
    a->m[0][0] * b->m[1][0] + a->m[1][0] * b->m[1][1] + a->m[2][0] * b->m[1][2] + a->m[3][0] * b->m[1][3],
    a->m[0][0] * b->m[2][0] + a->m[1][0] * b->m[2][1] + a->m[2][0] * b->m[2][2] + a->m[3][0] * b->m[2][3],
    a->m[0][0] * b->m[3][0] + a->m[1][0] * b->m[3][1] + a->m[2][0] * b->m[3][2] + a->m[3][0] * b->m[3][3],

    a->m[0][1] * b->m[0][0] + a->m[1][1] * b->m[0][1] + a->m[2][1] * b->m[0][2] + a->m[3][1] * b->m[0][3],
    a->m[0][1] * b->m[1][0] + a->m[1][1] * b->m[1][1] + a->m[2][1] * b->m[1][2] + a->m[3][1] * b->m[1][3],
    a->m[0][1] * b->m[2][0] + a->m[1][1] * b->m[2][1] + a->m[2][1] * b->m[2][2] + a->m[3][1] * b->m[2][3],
    a->m[0][1] * b->m[3][0] + a->m[1][1] * b->m[3][1] + a->m[2][1] * b->m[3][2] + a->m[3][1] * b->m[3][3],

    a->m[0][2] * b->m[0][0] + a->m[1][2] * b->m[0][1] + a->m[2][2] * b->m[0][2] + a->m[3][2] * b->m[0][3],
    a->m[0][2] * b->m[1][0] + a->m[1][2] * b->m[1][1] + a->m[2][2] * b->m[1][2] + a->m[3][2] * b->m[1][3],
    a->m[0][2] * b->m[2][0] + a->m[1][2] * b->m[2][1] + a->m[2][2] * b->m[2][2] + a->m[3][2] * b->m[2][3],
    a->m[0][2] * b->m[3][0] + a->m[1][2] * b->m[3][1] + a->m[2][2] * b->m[3][2] + a->m[3][2] * b->m[3][3],

    a->m[0][3] * b->m[0][0] + a->m[1][3] * b->m[0][1] + a->m[2][3] * b->m[0][2] + a->m[3][3] * b->m[0][3],
    a->m[0][3] * b->m[1][0] + a->m[1][3] * b->m[1][1] + a->m[2][3] * b->m[1][2] + a->m[3][3] * b->m[1][3],
    a->m[0][3] * b->m[2][0] + a->m[1][3] * b->m[2][1] + a->m[2][3] * b->m[2][2] + a->m[3][3] * b->m[2][3],
    a->m[0][3] * b->m[3][0] + a->m[1][3] * b->m[3][1] + a->m[2][3] * b->m[3][2] + a->m[3][3] * b->m[3][3]
  );

  return res;
}

inline
V4 m44_mul_v4(const M44 *m, V4 v)
{
  return v4(
    m->m[0][0] * v.x + m->m[1][0] * v.y + m->m[2][0] * v.z + m->m[3][0] * v.w,
    m->m[0][1] * v.x + m->m[1][1] * v.y + m->m[2][1] * v.z + m->m[3][1] * v.w,
    m->m[0][2] * v.x + m->m[1][2] * v.y + m->m[2][2] * v.z + m->m[3][2] * v.w,
    m->m[0][3] * v.x + m->m[1][3] * v.y + m->m[2][3] * v.z + m->m[3][3] * v.w
  );
}

inline
V3 trans_mul_v3(const M44 *m, V3 v)
{
  return v3(
    m->m[0][0] * v.x + m->m[1][0] * v.y + m->m[2][0] * v.z,
    m->m[0][1] * v.x + m->m[1][1] * v.y + m->m[2][1] * v.z,
    m->m[0][2] * v.x + m->m[1][2] * v.y + m->m[2][2] * v.z
  );
}

inline
void m44_print(const M44 *m) {
  DEBUG("\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f",
    m->m[0][0], m->m[1][0], m->m[2][0], m->m[3][0],
    m->m[0][1], m->m[1][1], m->m[2][1], m->m[3][1],
    m->m[0][2], m->m[1][2], m->m[2][2], m->m[3][2],
    m->m[0][3], m->m[1][3], m->m[2][3], m->m[3][3]
  );
}

inline
M44 m44_rot_x(f32 angle)
{
  f32 c = cosf(angle);
  f32 s = sinf(angle);

  return m44(
    1.f, 0.f, 0.f, 0.f,
    0.f,   c,  -s, 0.f,
    0.f,   s,   c, 0.f,
    0.f, 0.f, 0.f, 1.f
  );
}

inline
M44 m44_rot_y(f32 angle)
{
  f32 c = cosf(angle);
  f32 s = sinf(angle);

  return m44(
      c, 0.f,   s, 0.f,
    0.f, 1.f, 0.f, 0.f,
     -s, 0.f,   c, 0.f, 
    0.f, 0.f, 0.f, 1.f
  );
}

inline
M44 m44_rot_z(f32 angle)
{
  f32 c = cosf(angle);
  f32 s = sinf(angle);

  return m44(
      c,  -s, 0.f, 0.f,
      s,   c, 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f,
    0.f, 0.f, 0.f, 1.f
  );
}

inline
M33 m33_look_at(V3 look_dir)
{
  V3 up;
  if (fabs(look_dir.z) < 0.99)
    up = v3(0, 0, 1);
  else
    up = v3(1, 0, 0);

  V3 y = look_dir;
  V3 x = v3_normalized(cross(y, up));
  V3 z = cross(x, y);

  M33 res = m33(
    x.x, x.y, x.z,
    y.x, y.y, y.z,
    z.x, z.y, z.z
  );
  return res;
}

inline
M33 m33_look_at_viewspace(V3 look_dir)
{
  V3 up;
  if (fabs(look_dir.y) < 0.99)
    up = v3(0, -1, 0);
  else
    up = v3(1, 0, 0);

  V3 z = look_dir;
  V3 x = v3_normalized(cross(up, z));
  V3 y = cross(z, x);

  M33 res = m33(
    x.x, x.y, x.z,
    y.x, y.y, y.z,
    z.x, z.y, z.z
  );
  return res;
}

inline
M33 m33_look_from_to(V3 from, V3 to)
{
  V3 v = v3_sub(to, from);
  f32 len2 = v3_len2(v);
  if (len2 < FLT_EPSILON)
    return m33_identity();

  f32 len = sqrtf(len2);
  v = v3_muls(v, 1.f / len);
  M33 res = m33_look_at(v);
  return res;
}

inline
M33 m33_look_from_to_viewspace(V3 from, V3 to)
{
  V3 v = v3_sub(to, from);
  f32 len2 = v3_len2(v);
  if (len2 < FLT_EPSILON)
    return m33_identity();

  f32 len = sqrtf(len2);
  v = v3_muls(v, 1.f / len);
  M33 res = m33_look_at_viewspace(v);
  return res;
}

inline
M44 m44_scale_uniform(f32 s) {
  return m44(
    s,   0.f, 0.f, 0.f,
    0.f, s,   0.f, 0.f,
    0.f, 0.f, s,   0.f,
    0.f, 0.f, 0.f, 1.f
  );
}

inline
M44 m44_scale_xz(f32 sx, f32 sz) {
  return m44(
    sx,  0.f, 0.f, 0.f,
    0.f, 1.f, 0.f, 0.f,
    0.f, 0.f,  sz, 0.f,
    0.f, 0.f, 0.f, 1.f
  );
}

inline
M44 m44_scale(V3 s) {
  return m44(
    s.x, 0.f, 0.f, 0.f,
    0.f, s.y, 0.f, 0.f,
    0.f, 0.f, s.z, 0.f,
    0.f, 0.f, 0.f, 1.f
  );
}

inline
M44 m44_translation(V3 t) {
  return m44(
    1.f, 0.f, 0.f, t.x,
    0.f, 1.f, 0.f, t.y,
    0.f, 0.f, 1.f, t.z,
    0.f, 0.f, 0.f, 1.f
  );
}

inline
M44 transform_from_pos_rot_scale(V3 pos, Quat rot, V3 scale)
{
  // @Speed: derive a simpler formula!
  M44 s = m44_scale(scale);
  M33 r3 = quat_to_rot_matrix(rot);
  M44 r = m44_from_m33(&r3);
  M44 t = m44_translation(pos);
  M44 res = m44_mul(&r, &s);
  res = m44_mul(&t, &res);
  return res;
}

inline
V3 transform_dir3(const M44 *m, V3 v) {
  return v3(
    m->m[0][0] * v.x + m->m[1][0] * v.y + m->m[2][0] * v.z,
    m->m[0][1] * v.x + m->m[1][1] * v.y + m->m[2][1] * v.z,
    m->m[0][2] * v.x + m->m[1][2] * v.y + m->m[2][2] * v.z
  );
}

inline
V3 transform_pos3(const M44 *m, V3 v) {
  return v3(
    m->m[0][0] * v.x + m->m[1][0] * v.y + m->m[2][0] * v.z + m->m[3][0],
    m->m[0][1] * v.x + m->m[1][1] * v.y + m->m[2][1] * v.z + m->m[3][1],
    m->m[0][2] * v.x + m->m[1][2] * v.y + m->m[2][2] * v.z + m->m[3][2]
  );
}

typedef struct {
  f32 x;
  f32 y;
  f32 width;
  f32 height;
} Rect;

inline
Rect rect(f32 x, f32 y, f32 w, f32 h)
{
  Rect r;
  r.x = x;
  r.y = y;
  r.width = w;
  r.height = h;
  return r;
}

inline
Rect rect_pos_size(V2 pos, V2 size)
{
  Rect r;
  r.x = pos.x;
  r.y = pos.y;
  r.width = size.x;
  r.height = size.y;
  return r;
}

inline
Rect rect_center_halfsize(V2 center, V2 half_size)
{
  Rect r;
  r.x = center.x - half_size.x;
  r.y = center.y - half_size.y;
  r.width = 2.f * half_size.x;
  r.height = 2.f * half_size.y;
  return r;
}

inline
b8 rect_contains(Rect rect, V2 point)
{
  b8 res = rect.x <= point.x && rect.y <= point.y && rect.x + rect.width >= point.x && rect.y + rect.height >= point.y;
  return res;
}

// Returns a world space position that is at `unproj_depth` meters away from the camera along the camera Z axis
// (in other words, the view-space Z will be exactly `unproj_depth`).
// `unproj_depth` can be negative or zero.
inline
V3 unproject_screen_pos(V2 screen_pos, const M44 *inv_view_proj, const M44 *proj, Rect viewport_px, f32 unproj_depth)
{
  f32 z_clip = unproj_depth * proj->m[2][2] + proj->m[3][2];
  f32 persp_div = (Fae_Abs(unproj_depth) >= FLT_EPSILON) ? (1.f / unproj_depth) : 1.f / z_clip;
  // NOTE: proj->m[2][3] is 1 if proj is perspective, 0 otherwise. proj->m[3][3] is 1 if proj is ortho, 0 otherwise.
  // If proj is ortho we don't need to do the perspective divide, so in that case z_ndc == z_clip.
  f32 z_ndc = z_clip * (proj->m[2][3] * persp_div + proj->m[3][3]);
  V4 ndc = v4(
    2.f * (screen_pos.x - viewport_px.x) / viewport_px.width - 1.f,
    2.f * (screen_pos.y - viewport_px.y) / viewport_px.height - 1.f,
    z_ndc,
    1.f
  );
  V4 unproj = m44_mul_v4(inv_view_proj, ndc);
  f32 invw = 1.f / unproj.w;
  V3 res = v3(
    unproj.x * invw,
    unproj.y * invw,
    unproj.z * invw
  );
  return res;
}

typedef union {
  struct { f32 x, y, z, w; };
  struct { V3 n; f32 d; };
} Plane;

inline
Plane plane4(f32 x, f32 y, f32 z, f32 w)
{
  Plane p = {{ x, y, z, w }};
  return p;
}

inline
Plane plane_nd(V3 n, f32 d)
{
  Plane p;
  p.n = n;
  p.d = d;
  return p;
}

inline
Plane plane_from_normal_pos(V3 normal, V3 pos)
{
  f32 d = -v3_dot(normal, pos);
  return plane_nd(normal, d);
}

inline
Plane plane_mul_transform(Plane f, const M44 *h)
{
  return plane4(
    f.x * h->m[0][0] + f.y * h->m[0][1] + f.z * h->m[0][2],
    f.x * h->m[1][0] + f.y * h->m[1][1] + f.z * h->m[1][2],
    f.x * h->m[2][0] + f.y * h->m[2][1] + f.z * h->m[2][2],
    f.x * h->m[3][0] + f.y * h->m[3][1] + f.z * h->m[3][2] + f.w
  );
}

// Returns `t0` (where ray(t) = ray_origin + ray_dir * t) in case of intersection
// or -1 if it doesn't exist.
// NOTE: the plane is considered as two-sided, so the raycast will succeed from either side!
inline
f32 plane_raycast_from(V3 ray_origin, V3 ray_dir, Plane plane)
{
  assert(fabs(v3_len2(ray_dir)) - 1.f < 0.0001f);
  
  f32 nd = v3_dot(ray_dir, plane.n);

  if (fabs(nd) < FLT_EPSILON)
    return -1.f;

  f32 on = v3_dot(ray_origin, plane.n);
  f32 t = -(plane.d + on) / nd;
  if (t >= 0.f)
    return t;
  else
    return -1.f;
}

typedef struct {
  u8 vertex_index[2];
  u8 face_index[2];
} Edge;

#define MAX_POLYHEDRON_VERTEX_COUNT    28
#define MAX_POLYHEDRON_FACE_COUNT      16
#define MAX_POLYHEDRON_EDGE_COUNT      ((MAX_POLYHEDRON_VERTEX_COUNT - 2) * 3)
#define MAX_POLYHEDRON_FACE_EDGE_COUNT (MAX_POLYHEDRON_FACE_COUNT - 1)

typedef struct {
  u8 edge_count;
  u8 edge_index[MAX_POLYHEDRON_FACE_EDGE_COUNT];
} Face;

typedef struct {
  u8 vertex_count;
  u8 edge_count;
  u8 face_count;

  V3    vertices[MAX_POLYHEDRON_VERTEX_COUNT];
  Edge  edges[MAX_POLYHEDRON_EDGE_COUNT];
  Face  faces[MAX_POLYHEDRON_FACE_COUNT];
  Plane planes[MAX_POLYHEDRON_FACE_COUNT];
} Polyhedron;

typedef struct {
  // Order of planes:
  // 0, 1, 2, 3: lateral
  // 4: near
  // 5: far
  Plane p[6];
} Frustum;

inline
Plane plane_normalize(Plane p)
{
  V3 n_norm = v3_normalized(p.n);
  Plane res = plane_nd(n_norm, p.d);
  return res;
}

inline
Frustum frustum_from_polyhedron(const Polyhedron *p)
{
  Frustum f;
  for (u32 i = 0; i < 6; ++i)
    f.p[i] = plane_normalize(p->planes[i]);
  return f;
}

inline
Frustum build_persp_frustum(const M44 *cam_xform, const M44 *cam_xform_inv,
                            f32 proj_distance, f32 aspect_ratio, f32 n)
{
  Frustum frustum;
  
  f32 g = proj_distance;
  f32 s = aspect_ratio;
  f32 f = 1000000; // @Arbitrary far plane

  // Lateral planes
  f32 mx = 1.f / sqrtf(g * g + s * s);
  f32 my = 1.f / sqrtf(g * g + 1.f);
  frustum.p[0] = plane_mul_transform(plane4(-g * mx, 0.f, s * mx, 0.f), cam_xform_inv);
  frustum.p[1] = plane_mul_transform(plane4(0.f, g * my, my, 0.f), cam_xform_inv);
  frustum.p[2] = plane_mul_transform(plane4(g * mx, 0.f, s * mx, 0.f), cam_xform_inv);
  frustum.p[3] = plane_mul_transform(plane4(0.f, -g * my, my, 0.f), cam_xform_inv);

  // Near and far planes
  f32 d = v4_dot(cam_xform->col[2], cam_xform->col[3]);
  frustum.p[4] = plane_nd(v3_from_v4(cam_xform->col[2]), -(d + n));
  frustum.p[5] = plane_nd(v3_neg(v3_from_v4(cam_xform->col[2])), d + f);

  return frustum;
}

inline
Frustum build_ortho_frustum(const M44 *cam_xform, V2 viewport_size, f32 n)
{
  f32 w = viewport_size.x;
  f32 h = viewport_size.y;
  f32 x = cam_xform->col[3].x;
  f32 z = cam_xform->col[3].z;
  Frustum f;
  f.p[0] = plane4(1.f, 0.f, 0.f, w - x);
  f.p[1] = plane4(-1.f, 0.f, 0.f, w + x);
  f.p[2] = plane4(0.f, 0.f, 1.f, h - z);
  f.p[3] = plane4(0.f, 0.f, -1.f, h + z);
  f.p[4] = plane4(0.f, 1.f, 0.f, n);
  return f;
}

inline
Quat quat_identity()
{
  Quat q = { 0, 0, 0, 1 };
  return q;
}

inline
Quat quat(f32 x, f32 y, f32 z, f32 w)
{
  Quat q;
  q.x = x;
  q.y = y;
  q.z = z;
  q.w = w;
  return q;
}

inline
Quat quat_conj(Quat q)
{
  Quat res = q;
  res.v = v3_neg(res.v);
  return res;
}

inline
f32 quat_len2(Quat q)
{
  f32 res = v3_len2(q.v) + q.w * q.w;
  return res;
}

inline
Quat quat_muls(Quat a, f32 s)
{
  Quat res = a;
  res.x *= s;
  res.y *= s;
  res.z *= s;
  res.w *= s;
  return res;
}

inline
Quat quat_inverse(Quat q)
{
  Quat res = quat_muls(quat_conj(q), 1.f / quat_len2(q));
  return res;
}

inline
Quat quat_normalized(Quat q)
{
  Quat res = q;
  f32 len2 = quat_len2(q);
  if (len2 < FLT_EPSILON)
    return q;
  f32 rl = 1.f / sqrtf(len2);
  res.x *= rl;
  res.y *= rl;
  res.z *= rl;
  res.w *= rl;
  return res;
}

inline
Quat quat_from_axis_angle(V3 axis, f32 angle)
{
  f32 s = sinf(angle * 0.5f);
  f32 c = cosf(angle * 0.5f);
  Quat q;
  q.v = v3_muls(axis, s);
  q.w = c;
  q = quat_normalized(q);
  return q;
}

inline
Quat quat_from_euler(f32 pitch, f32 roll, f32 yaw)
{
    f32 cp = cosf(pitch * 0.5f);
    f32 sp = sinf(pitch * 0.5f);
    f32 cr = cosf(roll * 0.5f);
    f32 sr = sinf(roll * 0.5f);
    f32 cy = cosf(yaw * 0.5f);
    f32 sy = sinf(yaw * 0.5f);

    Quat q;
    q.w = cp * cr * cy + sp * sr * sy;
    q.x = sp * cr * cy - cp * sr * sy;
    q.y = cp * sr * cy + sp * cr * sy;
    q.z = cp * cr * sy - sp * sr * cy;

    return q;
}

inline
Quat quat_from_rot_matrix(const M33 *m)
{
  f32 m00 = m->m[0][0];
  f32 m11 = m->m[1][1];
  f32 m22 = m->m[2][2];
  f32 sum = m00 + m11 + m22;

  Quat res;
  if (sum > 0.f) {
    res.w = sqrtf(sum + 1.f) * 0.5f;
    f32 f = 0.25f / res.w;
    res.x = (m->m[1][2] - m->m[2][1]) * f;
    res.y = (m->m[2][0] - m->m[0][2]) * f;
    res.z = (m->m[0][1] - m->m[1][0]) * f;
  } else if ((m00 > m11) && (m00 > m22)) {
    res.x = sqrtf(m00 - m11 - m22 + 1.f) * 0.5f;
    f32 f = 0.25f / res.x;
    res.y = (m->m[0][1] + m->m[1][0]) * f;
    res.z = (m->m[2][0] + m->m[0][2]) * f;
    res.w = (m->m[1][2] - m->m[2][1]) * f;
  } else if (m11 > m22) {
    res.y = sqrtf(m11 - m00 - m22 + 1.f) * 0.5f;
    f32 f = 0.25f / res.y;
    res.x = (m->m[0][1] + m->m[1][0]) * f;
    res.z = (m->m[1][2] + m->m[2][1]) * f;
    res.w = (m->m[2][0] - m->m[0][2]) * f;
  } else {
    res.z = sqrtf(m22 - m00 - m11 + 1.f) * 0.5f;
    f32 f = 0.25f / res.z;
    res.x = (m->m[2][0] + m->m[0][2]) * f;
    res.y = (m->m[1][2] + m->m[2][1]) * f;
    res.w = (m->m[0][1] - m->m[1][0]) * f;
  }

  return res;
}

inline
Quat quat_mul(Quat a, Quat b)
{
  Quat res = quat(
    a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
    a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
    a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
    a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
  );
  return res;
}

inline
V3 quat_transform(Quat q, V3 v)
{
  f32 b2 = v3_len2(q.v);
  f32 a = v3_dot(v, q.v) * 2.f;
  V3 c = v3_muls(cross(q.v, v), q.w * 2.f);
  V3 res = v3_muls(v, q.w * q.w - b2);
  res = v3_add(res, v3_muls(q.v, a));
  res = v3_add(res, c);
  return res;
}

typedef struct {
  V3 center;
  V3 half_extent;
} Box3;

inline
Box3 box3_from_minmax(V3 min, V3 max)
{
  Box3 b;
  b.center = v3_muls(v3_add(min, max), 0.5);
  b.half_extent = v3_muls(v3_sub(max, min), 0.5);
  return b;
}

inline
void box3_get_minmax(Box3 box, V3 *min, V3 *max)
{
  *min = v3_sub(box.center, box.half_extent);
  *max = v3_add(box.center, box.half_extent);
}

inline
Box3 box3_expand(Box3 box, f32 extra_thickness)
{
  Box3 res;
  res.center = box.center;
  res.half_extent = v3_add(box.half_extent, v3s(extra_thickness));
  return res;
}

inline
f32 box3_point_distance2(Box3 box, V3 point)
{
  V3 min, max;
  box3_get_minmax(box, &min, &max);

  f32 dx = Fae_Max(Fae_Max(0, min.x - point.x), point.x - max.x);
  f32 dy = Fae_Max(Fae_Max(0, min.y - point.y), point.y - max.y);
  f32 dz = Fae_Max(Fae_Max(0, min.z - point.z), point.z - max.z);
  f32 res = dx * dx + dy * dy + dz * dz;
  return res;
}

inline
V3 v3_min(V3 a, V3 b)
{
  V3 res;
  res.x = Fae_Min(a.x, b.x);
  res.y = Fae_Min(a.y, b.y);
  res.z = Fae_Min(a.z, b.z);
  return res;
}

inline
V3 v3_max(V3 a, V3 b)
{
  V3 res;
  res.x = Fae_Max(a.x, b.x);
  res.y = Fae_Max(a.y, b.y);
  res.z = Fae_Max(a.z, b.z);
  return res;
}

inline
b8 aabb_visible(Plane *planes, u32 n_planes, Box3 aabb) 
{
  V3 size = v3_muls(aabb.half_extent, 2.0);
  for (u32 i = 0; i < n_planes; ++i) {
    Plane *g = &planes[i];
    f32 rg = fabs(g->x * size.x) + fabs(g->y * size.y) + fabs(g->z * size.z);
    if (v3_dot(g->n, aabb.center) <= -rg)
      return false;
  }

  return true;
}

typedef struct {
  i32 x, y;
} V2i;

inline
V2i v2i(i32 x, i32 y)
{
  V2i v;
  v.x = x;
  v.y = y;
  return v;
}

inline
V2 v2_from_v2i(V2i v)
{
  V2 res = v2(v.x, v.y);
  return res;
}

