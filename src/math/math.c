#include "math.h"

/* === vector === */

Vec2 vec2_add(Vec2 a, Vec2 b) {
    return (Vec2) {
        a.x + b.x,
        a.y + b.y
    };
}
Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3) {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z
    };
}
Vec4 vec4_add(Vec4 a, Vec4 b) {
    return (Vec4) {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z,
        a.w + b.w
    };
}

Vec2 vec2_sub(Vec2 a, Vec2 b) {
    return (Vec2) {
        a.x - b.x,
        a.y - b.y
    };
}
Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return (Vec3) {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z
    };
}
Vec4 vec4_sub(Vec4 a, Vec4 b) {
    return (Vec4) {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z,
        a.w - b.w
    };
}

Vec2 vec2_mul_f32(Vec2 a, f32 b) {
    return (Vec2) {
        a.x * b,
        a.y * b
    };
}
Vec3 vec3_mul_f32(Vec3 a, f32 b) {
    return (Vec3) {
        a.x * b,
        a.y * b,
        a.z * b
    };
}
Vec4 vec4_mul_f32(Vec4 a, f32 b) {
    return (Vec4) {
        a.x * b,
        a.y * b,
        a.z * b,
        a.w * b
    };
}

Vec2 vec2_div_f32(Vec2 a, f32 b) {
    return (Vec2) {
        a.x / b,
        a.y / b
    };
}
Vec3 vec3_div_f32(Vec3 a, f32 b) {
    return (Vec3) {
        a.x / b,
        a.y / b,
        a.z / b
    };
}
Vec4 vec4_div_f32(Vec4 a, f32 b) {
    return (Vec4) {
        a.x / b,
        a.y / b,
        a.z / b,
        a.w / b
    };
}

f32 vec2_dot(Vec2 a, Vec2 b) {
    return a.x * a.x + a.y * a.y;
}
f32 vec3_dot(Vec3 a, Vec3 b) {
    return a.x * a.x + a.y * a.y + a.z * a.z;
}
f32 vec4_dot(Vec4 a, Vec4 b) {
    return a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w;
}

f32 vec2_len(Vec2 a) {
    return sqrtf(vec2_dot(a, a));
}
f32 vec3_len(Vec3 a) {
    return sqrtf(vec3_dot(a, a));
}
f32 vec4_len(Vec4 a) {
    return sqrtf(vec4_dot(a, a));
}

Vec2 vec2_normalize(Vec2 a) {
    f32 len = vec2_len(a);
    if(len > EPSILON) {
        return vec2_div_f32(a, len);
    } else {
        return (Vec2){0};   
    }
}
Vec3 vec3_normalize(Vec3 a) {
    f32 len = vec3_len(a);
    if(len > EPSILON) {
        return vec3_div_f32(a, len);
    } else {
        return (Vec3){0};   
    }
}
Vec4 vec4_normalize(Vec4 a) {
    f32 len = vec4_len(a);
    if(len > EPSILON) {
        return vec4_div_f32(a, len);
    } else {
        return (Vec4){0};
    }
}

Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3) {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}
Vec4 quat_mul_quat(Vec4 a, Vec4 b) {
    return (Vec4) {
        .x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        .y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        .z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        .w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
    };
}
Vec3 quat_mul_vec3(Vec4 a, Vec3 b) {
    Vec3 q = { a.x, a.y, a.z };

    Vec3 t = {
        2.0f * (q.y * b.z - q.z * b.y),
        2.0f * (q.z * b.x - q.x * b.z),
        2.0f * (q.x * b.y - q.y * b.x)
    };

    return (Vec3) {
        b.x + a.w * t.x + (q.y * t.z - q.z * t.y),
        b.y + a.w * t.y + (q.z * t.x - q.x * t.z),
        b.z + a.w * t.z + (q.x * t.y - q.y * t.x)
    };
}

/* === matrix === */

Mat4x4 mat4x4_translation(Vec3 trans) {
    return (Mat4x4) {
        .raw = {
            1.0, 0.0, 0.0, trans.x,
            0.0, 1.0, 0.0, trans.y,
            0.0, 0.0, 1.0, trans.z,
            0.0, 0.0, 0.0, 1.0
        }
    };
}

Mat4x4 mat4x4_rotation(Vec4 quat) {
    float mag = sqrtf(quat.x*quat.x + quat.y*quat.y + quat.z*quat.z + quat.w*quat.w);
    if (mag == 0.0f) {
        return (Mat4x4) {
            .raw = {
                1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0
            }
        };
    }
    
    float qx = quat.x / mag;
    float qy = quat.y / mag;
    float qz = quat.z / mag;
    float qw = quat.w / mag;

    float xx = qx * qx;
    float yy = qy * qy;
    float zz = qz * qz;
    float xy = qx * qy;
    float xz = qx * qz;
    float xw = qx * qw;
    float yz = qy * qz;
    float yw = qy * qw;
    float zw = qz * qw;
    
    return (Mat4x4) {
        .raw = {
            1.0f - 2.0f * (yy + zz),  2.0f * (xy - zw),         2.0f * (xz + yw),       0.0f,
            2.0f * (xy + zw),         1.0f - 2.0f * (xx + zz), 2.0f * (yz - xw),        0.0f,
            2.0f * (xz - yw),         2.0f * (yz + xw),        1.0f - 2.0f * (xx + yy), 0.0f,
            0.0f,                     0.0f,                    0.0f,                    1.0f
        } 
    };
}

Mat4x4 mat4x4_scaling(Vec3 scale) {
    return (Mat4x4) {
        .raw = {
            scale.x, 0.0    , 0.0    , 0.0,
            0.0    , scale.y, 0.0    , 0.0,
            0.0    , 0.0    , scale.z, 0.0,
            0.0    , 0.0    , 0.0    , 1.0
        }
    };
}

Mat4x4 mat4x4_projection(f32 fov, f32 aspect, f32 near, f32 far) {
    f32 f = 1.0f / tanf(fov * 0.5f);

    return (Mat4x4) {
        .raw = {
             f / aspect, 0.0f,  0.0f,                         0.0f,
             0.0f,      -f,    0.0f,                         0.0f,
             0.0f,       0.0f, near / (near - far),          (near * far) / (far - near),
             0.0f,       0.0f, 1.0f,                         0.0f
        }
    };
}

f32 mat4x4_det(Mat4x4 a) {
    f32 A00 = a.m11 * a.m22 * a.m33 
            + a.m12 * a.m23 * a.m31 
            + a.m13 * a.m21 * a.m32 
            - a.m13 * a.m22 * a.m31 
            - a.m12 * a.m21 * a.m33 
            - a.m11 * a.m23 * a.m32;
    
    f32 A01 = a.m10 * a.m22 * a.m33 
            + a.m12 * a.m23 * a.m30 
            + a.m13 * a.m20 * a.m32 
            - a.m13 * a.m22 * a.m30 
            - a.m12 * a.m20 * a.m33 
            - a.m10 * a.m23 * a.m32;
    
    f32 A02 = a.m10 * a.m21 * a.m33 
            + a.m11 * a.m23 * a.m30 
            + a.m13 * a.m20 * a.m31 
            - a.m13 * a.m21 * a.m30 
            - a.m11 * a.m20 * a.m33 
            - a.m10 * a.m23 * a.m31;
    
    f32 A03 = a.m10 * a.m21 * a.m32 
            + a.m11 * a.m22 * a.m30 
            + a.m12 * a.m20 * a.m31 
            - a.m12 * a.m21 * a.m30 
            - a.m11 * a.m20 * a.m32 
            - a.m10 * a.m22 * a.m31;

    return a.m00 * A00 - a.m01 * A01 + a.m02 * A02 - a.m03 * A03;
}

Mat4x4 mat4x4_transpose(Mat4x4 a) {
    return (Mat4x4) {
        .raw = {
            a.m00, a.m10, a.m20, a.m30,
            a.m01, a.m11, a.m21, a.m31, 
            a.m02, a.m12, a.m22, a.m32,
            a.m03, a.m13, a.m23, a.m33
        }
    };
}

Mat4x4 mat4x4_inverse(Mat4x4 a) {
    Mat4x4 inv = (Mat4x4){0};

    inv.raw[0] =
         a.raw[5]  * a.raw[10] * a.raw[15]
        -a.raw[5]  * a.raw[11] * a.raw[14]
        -a.raw[9]  * a.raw[6]  * a.raw[15]
        +a.raw[9]  * a.raw[7]  * a.raw[14]
        +a.raw[13] * a.raw[6]  * a.raw[11]
        -a.raw[13] * a.raw[7]  * a.raw[10];

    inv.raw[4] =
        -a.raw[4]  * a.raw[10] * a.raw[15]
        +a.raw[4]  * a.raw[11] * a.raw[14]
        +a.raw[8]  * a.raw[6]  * a.raw[15]
        -a.raw[8]  * a.raw[7]  * a.raw[14]
        -a.raw[12] * a.raw[6]  * a.raw[11]
        +a.raw[12] * a.raw[7]  * a.raw[10];

    inv.raw[8] =
         a.raw[4]  * a.raw[9] * a.raw[15]
        -a.raw[4]  * a.raw[11] * a.raw[13]
        -a.raw[8]  * a.raw[5] * a.raw[15]
        +a.raw[8]  * a.raw[7] * a.raw[13]
        +a.raw[12] * a.raw[5] * a.raw[11]
        -a.raw[12] * a.raw[7] * a.raw[9];

    inv.raw[12] =
        -a.raw[4]  * a.raw[9] * a.raw[14]
        +a.raw[4]  * a.raw[10] * a.raw[13]
        +a.raw[8]  * a.raw[5] * a.raw[14]
        -a.raw[8]  * a.raw[6] * a.raw[13]
        -a.raw[12] * a.raw[5] * a.raw[10]
        +a.raw[12] * a.raw[6] * a.raw[9];

    inv.raw[1] =
        -a.raw[1]  * a.raw[10] * a.raw[15]
        +a.raw[1]  * a.raw[11] * a.raw[14]
        +a.raw[9]  * a.raw[2] * a.raw[15]
        -a.raw[9]  * a.raw[3] * a.raw[14]
        -a.raw[13] * a.raw[2] * a.raw[11]
        +a.raw[13] * a.raw[3] * a.raw[10];

    inv.raw[5] =
         a.raw[0]  * a.raw[10] * a.raw[15]
        -a.raw[0]  * a.raw[11] * a.raw[14]
        -a.raw[8]  * a.raw[2] * a.raw[15]
        +a.raw[8]  * a.raw[3] * a.raw[14]
        +a.raw[12] * a.raw[2] * a.raw[11]
        -a.raw[12] * a.raw[3] * a.raw[10];

    inv.raw[9] =
        -a.raw[0]  * a.raw[9] * a.raw[15]
        +a.raw[0]  * a.raw[11] * a.raw[13]
        +a.raw[8]  * a.raw[1] * a.raw[15]
        -a.raw[8]  * a.raw[3] * a.raw[13]
        -a.raw[12] * a.raw[1] * a.raw[11]
        +a.raw[12] * a.raw[3] * a.raw[9];

    inv.raw[13] =
         a.raw[0]  * a.raw[9] * a.raw[14]
        -a.raw[0]  * a.raw[10] * a.raw[13]
        -a.raw[8]  * a.raw[1] * a.raw[14]
        +a.raw[8]  * a.raw[2] * a.raw[13]
        +a.raw[12] * a.raw[1] * a.raw[10]
        -a.raw[12] * a.raw[2] * a.raw[9];

    inv.raw[2] =
         a.raw[1]  * a.raw[6] * a.raw[15]
        -a.raw[1]  * a.raw[7] * a.raw[14]
        -a.raw[5]  * a.raw[2] * a.raw[15]
        +a.raw[5]  * a.raw[3] * a.raw[14]
        +a.raw[13] * a.raw[2] * a.raw[7]
        -a.raw[13] * a.raw[3] * a.raw[6];

    inv.raw[6] =
        -a.raw[0]  * a.raw[6] * a.raw[15]
        +a.raw[0]  * a.raw[7] * a.raw[14]
        +a.raw[4]  * a.raw[2] * a.raw[15]
        -a.raw[4]  * a.raw[3] * a.raw[14]
        -a.raw[12] * a.raw[2] * a.raw[7]
        +a.raw[12] * a.raw[3] * a.raw[6];

    inv.raw[10] =
         a.raw[0]  * a.raw[5] * a.raw[15]
        -a.raw[0]  * a.raw[7] * a.raw[13]
        -a.raw[4]  * a.raw[1] * a.raw[15]
        +a.raw[4]  * a.raw[3] * a.raw[13]
        +a.raw[12] * a.raw[1] * a.raw[7]
        -a.raw[12] * a.raw[3] * a.raw[5];

    inv.raw[14] =
        -a.raw[0]  * a.raw[5] * a.raw[14]
        +a.raw[0]  * a.raw[6] * a.raw[13]
        +a.raw[4]  * a.raw[1] * a.raw[14]
        -a.raw[4]  * a.raw[2] * a.raw[13]
        -a.raw[12] * a.raw[1] * a.raw[6]
        +a.raw[12] * a.raw[2] * a.raw[5];

    inv.raw[3] =
        -a.raw[1] * a.raw[6] * a.raw[11]
        +a.raw[1] * a.raw[7] * a.raw[10]
        +a.raw[5] * a.raw[2] * a.raw[11]
        -a.raw[5] * a.raw[3] * a.raw[10]
        -a.raw[9] * a.raw[2] * a.raw[7]
        +a.raw[9] * a.raw[3] * a.raw[6];

    inv.raw[7] =
         a.raw[0] * a.raw[6] * a.raw[11]
        -a.raw[0] * a.raw[7] * a.raw[10]
        -a.raw[4] * a.raw[2] * a.raw[11]
        +a.raw[4] * a.raw[3] * a.raw[10]
        +a.raw[8] * a.raw[2] * a.raw[7]
        -a.raw[8] * a.raw[3] * a.raw[6];

    inv.raw[11] =
        -a.raw[0] * a.raw[5] * a.raw[11]
        +a.raw[0] * a.raw[7] * a.raw[9]
        +a.raw[4] * a.raw[1] * a.raw[11]
        -a.raw[4] * a.raw[3] * a.raw[9]
        -a.raw[8] * a.raw[1] * a.raw[7]
        +a.raw[8] * a.raw[3] * a.raw[5];

    inv.raw[15] =
         a.raw[0] * a.raw[5] * a.raw[10]
        -a.raw[0] * a.raw[6] * a.raw[9]
        -a.raw[4] * a.raw[1] * a.raw[10]
        +a.raw[4] * a.raw[2] * a.raw[9]
        +a.raw[8] * a.raw[1] * a.raw[6]
        -a.raw[8] * a.raw[2] * a.raw[5];

    f32 det =
        a.raw[0] * inv.raw[0] +
        a.raw[1] * inv.raw[4] +
        a.raw[2] * inv.raw[8] +
        a.raw[3] * inv.raw[12];

    if(det == 0.0f) {
        return (Mat4x4){0};
    }

    f32 inv_det = 1.0f / det;

    for(u32 i = 0; i < 16; i++) {
        inv.raw[i] *= inv_det;
    }

    return inv;
}

Mat4x4 mat4x4_mul_mat4x4(Mat4x4 a, Mat4x4 b) {
    Mat4x4 r = {0};

    for(u32 row = 0; row < 4; row++) {
        for(u32 col = 0; col < 4; col++) {
            for(u32 k = 0; k < 4; k++) {
                r.raw[row * 4 + col] +=
                    a.raw[row * 4 + k] *
                    b.raw[k * 4 + col];
            }
        }
    }

    return r;
}

Vec4 mat4x4_mul_vec4(Mat4x4 a, Vec4 b) {
    return (Vec4) {
        a.m00 * b.x + a.m01 * b.y + a.m02 * b.z + a.m03 * b.w,
        a.m10 * b.x + a.m11 * b.y + a.m12 * b.z + a.m13 * b.w,
        a.m20 * b.x + a.m21 * b.y + a.m22 * b.z + a.m23 * b.w,
        a.m30 * b.x + a.m31 * b.y + a.m32 * b.z + a.m33 * b.w
    };
}
