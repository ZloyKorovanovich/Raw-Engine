#include "game.h"
#include "game_structs.h"

typedef struct {
    Vec3 origin;
    Vec3 direction;
} Ray;

/* === global === */

static Input input = (Input){0};

/* === camera === */

static Vec2   camera_turn = {0.0, 0.0};
static Vec3   camera_pos  = {0.0, 0.0, 0.0};
static Vec4   camera_rot  = {0.0, 0.0, 0.0, 1.0};
static Mat4x4 camera_vp   = {
    .raw = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    }
};
static Mat4x4 camera_iv   = {
    .raw = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    }
};

void update_camera(void) {
    if(input.action_0) {
        camera_pos = (Vec3){0.0, 0.0, 0.0};
    }

    /* hide cursor on rotate*/
    static b32 cursor_shown = TRUE;
    if(input.action_1 && cursor_shown) {
        input_use_cursor(FALSE);
        cursor_shown = FALSE;
    }
    if(!input.action_1 && !cursor_shown) {
        input_use_cursor(TRUE);
        cursor_shown = TRUE;
    }
    
    if(!cursor_shown) {
        camera_turn.x += input.mouse_delta_x;
        camera_turn.y += input.mouse_delta_y;
    }

    /* apply rotation */
    Vec4 rotator_y = {0.0, sinf(camera_turn.x / 2.0f), 0.0, cosf(camera_turn.x / 2.0f)};
    Vec4 rotator_x = {sinf(camera_turn.y / 2.0f), 0.0, 0.0, cosf(camera_turn.y / 2.0f)};
    camera_rot = quat_mul_quat(rotator_y, rotator_x);

    /* apply movement */
    Vec3 fwd = quat_mul_vec3(camera_rot, (Vec3){0.0, 0.0, 1.0});
    Vec3 rhs = quat_mul_vec3(camera_rot, (Vec3){1.0, 0.0, 0.0});
    //Vec3 up  = quat_mul_vec3(camera_rot, Vec3(0.0, 1.0, 0.0));

    camera_pos = vec3_add(camera_pos, vec3_mul_f32(fwd, input.movement_vertical   * (f32)input.delta * (input.boost ? 10.0f : 1.0f)));
    camera_pos = vec3_add(camera_pos, vec3_mul_f32(rhs, input.movement_horizontal * (f32)input.delta * (input.boost ? 10.0f : 1.0f)));

    /* build transform matrix */
    camera_iv = (Mat4x4) {
        .raw = {
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0
        }
    };
    camera_iv = mat4x4_mul_mat4x4(camera_iv, mat4x4_translation(camera_pos));
    camera_iv = mat4x4_mul_mat4x4(camera_iv, mat4x4_rotation(camera_rot));
    camera_vp = mat4x4_inverse(camera_iv);
    camera_vp = mat4x4_mul_mat4x4(mat4x4_projection(80.0f * (f32)DEG_2_RAD, (f32)input.screen_x / (f32)input.screen_y, 0.03f, 3000.0f), camera_vp);
}

Ray screen_point_to_ray(Vec2 screen_pos) {
    f32 pos_x = CLAMP(0.0f, 1.0f, screen_pos.x / (f32)input.screen_x) * 2.0f - 1.0f;
    f32 pos_y = CLAMP(0.0f, 1.0f, screen_pos.y / (f32)input.screen_y) * 2.0f - 1.0f;
    Vec4 position_cs_near = {pos_x, pos_y, 0.0, 1.0};
    Vec4 position_cs_far  = {pos_x, pos_y, 1.0, 1.0};
    
    Mat4x4 camera_ivp = mat4x4_inverse(camera_vp);

    Vec4 near_nd = mat4x4_mul_vec4(camera_ivp, position_cs_near);
    Vec4 far_nd  = mat4x4_mul_vec4(camera_ivp, position_cs_far);
    Vec3 pos_ws_near = vec3_div_f32((Vec3){near_nd.x, near_nd.y, near_nd.z}, near_nd.w);
    Vec3 pos_ws_far  = vec3_div_f32((Vec3){far_nd.x, far_nd.y, far_nd.z}, far_nd.w);

    return (Ray) {
        pos_ws_near,
        vec3_normalize(vec3_sub(pos_ws_far, pos_ws_near))
    };
}

b32 ray_intersect_sphere(Ray ray, Vec3 sphere_origin, f32 sphere_radius, Vec3* hit_point) {
    Vec3 o = ray.origin;
    Vec3 d = ray.direction;
    Vec3 p = sphere_origin;
    f32  r = sphere_radius;

    f32  t = vec3_dot(vec3_sub(p, o), d);
    Vec3 m = vec3_add(o, vec3_mul_f32(d, t));
    f32  y = vec3_len(vec3_sub(m, p));

    if(y < r) {
        if(hit_point != NULL) {
            f32 x  = sqrtf(r*r - y*y);
            f32 t0 = t-x;
            f32 t1 = t+x;

            if(t1 > 0.0) {
                *hit_point = vec3_add(o, vec3_mul_f32(d, t0));
            } else {
                *hit_point = vec3_add(o, vec3_mul_f32(d, t1));
            }
        }
        return TRUE;
    } else {
        if(hit_point != NULL) {
            *hit_point = (Vec3){0};
        }
        return FALSE;
    }
}

b32 start(void) {
    for(u32 i = 0; i != 10; i++) {
        char number[32] = {0};
        char name[PATH_LENGTH] = {0};
        sprintf_s(number, 32, "%u", i);
        strcpy_s(name, PATH_LENGTH, "bull_shark_");
        strcat_s(name, PATH_LENGTH, number);

        Entity* new_entity = entity_create(name);
        if(new_entity == NULL) {
            LOG_ERROR("failed to create entity");
            goto fail;
        }

        f32 angle = (f32)rand() / (f32)RAND_MAX * 2.0f * (f32)PI; // [0, 2π)
        f32 half  = angle * 0.5f;

        new_entity->position = (Vec3){(f32)(rand() % 10), (f32)(rand() % 10), (f32)(rand() % 10)};
        new_entity->rotation = (Vec4){ 0.0f, sinf(half), 0.0f, cosf(half) };
        new_entity->scale    = 1.0;
        graphics_opaque_lit_assign(new_entity, mesh_register("./out/data/models/bull_shark.glb"), texture_register("./out/data/textures/demo.png"));
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

b32 game_run(b32 is_debug) {
    if(!engine_init(is_debug)) {
        LOG_ERROR("init failed");
        goto fail;
    }

    /* start */
    if(!start()) {
        LOG_ERROR("start failed");
        goto fail;
    }

    while(!input_process_window_should_close()) {
        input_gather_input(&input);

        /* update */
        update_camera();

        if(!graphics_render_frame(camera_vp, camera_iv)) {
            LOG_ERROR("failed to render frame");
            goto fail;
        }
    }
    
    engine_terminate();

    return TRUE;

    fail: {
        return FALSE;
    }
}
