#include "game.h"
#include "game_structs.h"
#include "../input/input.h"

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
    camera_vp = mat4x4_mul_mat4x4(mat4x4_projection(120.0f * (f32)DEG_2_RAD, 16.0f / 9.0f, 0.03f, 3000.0f), camera_vp);
}

/* === static scene === */
/* FIX: remake (not static anymore) */

static u32     static_props_count = 0;
static Entity* static_props       = NULL;

b32 start_static_scene(void) {
    /* allocate arrays */ {
        static_props_count = 100;
        static_props       = calloc(static_props_count, sizeof(Entity));
        if(static_props == NULL) {
            LOG_ERROR("failed to allocate static props");
            goto fail;
        }
    }

    /* initialize props */ {
        for(u32 i = 0; i != static_props_count; i++) {
            static_props[i] = (Entity) {
                .name      = "static_prop_",
                .mesh_name = "./out/data/models/sphere.glb",
                .entity_id = i,
                .position  = {1.0f + (f32)i, 0.0, 0.0},
                .scale     = {1.0, 1.0, 1.0},
                .rotation  = {0.0, 0.0, 0.0, 1.0}
            };

            char num_buffer[4] = {0};
            snprintf(num_buffer, sizeof(num_buffer), "%u", i);
            strcat_s(static_props[i].name, PATH_LENGTH, num_buffer);

            graphics_default_materials_add(&static_props[i]);
        }
    }

    return TRUE;

    fail: {
        return FALSE;
        free(static_props);
        static_props_count = 0;
        static_props       = NULL;
    }
}

void finish_static_scene(void) {
    free(static_props);
    static_props_count = 0;
    static_props       = NULL;
}

void update_props(void) {
    for(u32 i = 0; i != static_props_count; i++) {
        static_props[i].position.y = sinf((f32)input.time + (f32)i);
        static_props[i].is_updated = TRUE;
    }
}

/* ==== ==== ==== ==== ==== ==== ==== ==== ==== 
    game core
   ==== ==== ==== ==== ==== ==== ==== ==== ==== */

b32 game_run(b32 is_debug) {
    if(!graphics_init(is_debug)) {
        LOG_ERROR("init failed");
        goto fail;
    }

    /* start */
    start_static_scene();

    while(!input_process_window_should_close()) {
        input_gather_input(&input);

        /* update */
        update_camera();
        update_props();

        if(!graphics_render_frame(camera_vp, camera_iv)) {
            LOG_ERROR("failed to render frame");
            goto fail;
        }
    }

    /* finish */
    finish_static_scene();

    return TRUE;

    fail: {
        return FALSE;
    }
}
