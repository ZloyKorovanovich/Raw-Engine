#ifndef _MATH_INCLUDED
#define _MATH_INCLUDED

/* math lib made as simple as possible for vector & matrix linear algebra. 
   tries to compensate for lack of operator overloading and works on structs copies
   to improve ease of use compared to cglm */

#include "../base.h"
#include <math.h>

/* coordinates are left-handed unity-like x-right y-up z-forward */

#define PI        (3.14159274)
#define DEG_2_RAD (0.01745329)
#define EPSILON   (0.00000001)

typedef struct {
    f32 x;
    f32 y;
} Vec2;

typedef struct {
    f32 x;
    f32 y;
    f32 z;
} Vec3;

typedef struct {
    f32 x;
    f32 y;
    f32 z;
    f32 w;
} Vec4;

Vec2 vec2_add(Vec2 a, Vec2 b);
Vec3 vec3_add(Vec3 a, Vec3 b);
Vec4 vec4_add(Vec4 a, Vec4 b);

Vec2 vec2_sub(Vec2 a, Vec2 b);
Vec3 vec3_sub(Vec3 a, Vec3 b);
Vec4 vec4_sub(Vec4 a, Vec4 b);

Vec2 vec2_mul_f32(Vec2 a, f32 b);
Vec3 vec3_mul_f32(Vec3 a, f32 b);
Vec4 vec4_mul_f32(Vec4 a, f32 b);

Vec2 vec2_div_f32(Vec2 a, f32 b);
Vec3 vec3_div_f32(Vec3 a, f32 b);
Vec4 vec4_div_f32(Vec4 a, f32 b);

f32 vec2_dot(Vec2 a, Vec2 b);
f32 vec3_dot(Vec3 a, Vec3 b);
f32 vec4_dot(Vec4 a, Vec4 b);

f32 vec2_len(Vec2 a);
f32 vec3_len(Vec3 a);
f32 vec4_len(Vec4 a);

/* normalization of zero length will give zero vector */
Vec2 vec2_normalize(Vec2 a);
Vec3 vec3_normalize(Vec3 a);
Vec4 vec4_normalize(Vec4 a);

Vec3 vec3_cross(Vec3 a, Vec3 b);

Vec4 quat_mul_quat(Vec4 a, Vec4 b);
Vec3 quat_mul_vec3(Vec4 a, Vec3 b);

/* row major */
typedef union {
    struct {
        f32 m00; f32 m01; f32 m02; f32 m03;
        f32 m10; f32 m11; f32 m12; f32 m13;
        f32 m20; f32 m21; f32 m22; f32 m23;
        f32 m30; f32 m31; f32 m32; f32 m33;
    };
    f32 raw[16];
} Mat4x4;

Mat4x4 mat4x4_translation(Vec3 trans);
Mat4x4 mat4x4_rotation(Vec4 quat);
Mat4x4 mat4x4_scaling(Vec3 scale);
Mat4x4 mat4x4_projection(f32 fov, f32 width_over_height, f32 near, f32 far);

f32    mat4x4_det(Mat4x4 a);
Mat4x4 mat4x4_transpose(Mat4x4 a);
Mat4x4 mat4x4_inverse(Mat4x4 a);

Mat4x4 mat4x4_mul_mat4x4(Mat4x4 a, Mat4x4 b);
Vec4   mat4x4_mul_vec4(Mat4x4 a, Vec4 b);

#endif
