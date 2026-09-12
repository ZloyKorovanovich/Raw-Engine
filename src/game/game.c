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
    camera_vp = mat4x4_mul_mat4x4(mat4x4_projection(80.0f * (f32)DEG_2_RAD, 16.0f / 9.0f, 0.03f, 3000.0f), camera_vp);
}

/* === entity pool === */

#define INVALID_ENTITY_ID (0llu)

static u64 global_entity_id = 1;

static u32     entity_pool_free_slots_count = 0;
static u32     entity_pool_capacity         = 0;
static u32*    entity_pool_free_slots       = NULL;
static Entity* entity_pool                  = NULL;

/* just zeroed entity with id set */
Entity* entity_create(void) {
    /* reallocate pool */
    if(entity_pool_free_slots_count == 0) {
        u32     new_capacity   = entity_pool_capacity + 1024;
        u32     new_free_count = new_capacity - entity_pool_capacity;
        u32*    new_free_slots = realloc(entity_pool_free_slots, new_capacity * sizeof(u32));
        Entity* new_pool       = realloc(entity_pool, new_capacity * sizeof(Entity));

        if(new_pool == NULL || new_free_slots == NULL) {
            LOG_ERROR("failed to reallocate entity pool");
            entity_pool_free_slots = new_free_slots == NULL ? entity_pool_free_slots : new_free_slots;
            entity_pool            = new_pool       == NULL ? entity_pool            : new_pool;
            goto fail;
        }

        for(u32 i = 0; i != new_free_count; i++) {
            new_free_slots[i] = i + entity_pool_capacity;
        }

        entity_pool_free_slots_count = new_free_count;
        entity_pool_free_slots       = new_free_slots;
        entity_pool_capacity         = new_capacity;
        entity_pool                  = new_pool;
    }

    u32     slot_id = entity_pool_free_slots[entity_pool_free_slots_count - 1];
    Entity* entity  = &entity_pool[slot_id];
    entity_pool_free_slots_count--;
    
    *entity = (Entity) {
        .entity_id = __atomic_fetch_add(&global_entity_id, 1, __ATOMIC_SEQ_CST)
    };

    return entity;

    fail: {
        return NULL;
    }
}

void entity_destroy(Entity* entity) {
    u32 entity_index = (u32)(((u64)entity - (u64)entity_pool) / sizeof(Entity));
    if(entity_index >= entity_pool_capacity) {
        LOG_ERROR("trying to destroy entity that does not belong to pool");
        goto fail;
    }
    if(entity_pool[entity_index].entity_id == INVALID_ENTITY_ID) {
        LOG_ERROR("trying to destroy entity that is already free");
        goto fail;
    }

    entity_pool[entity_index] = (Entity){0};
    entity_pool_free_slots[entity_pool_free_slots_count] = entity_index;
    entity_pool_free_slots_count++;

    fail: {}
}

void entity_pool_free(void) {
    free(entity_pool_free_slots);
    free(entity_pool);
    entity_pool_free_slots_count = 0;
    entity_pool_capacity         = 0;
    entity_pool_free_slots       = NULL;
    entity_pool                  = NULL;
}


b32 start(void) {
    for(u32 i = 0; i != 10; i++) {
        char number[32] = {0};
        sprintf_s(number, 32, "%u", i);

        Entity* new_entity = entity_create();
        if(new_entity == NULL) {
            LOG_ERROR("failed to create entity");
            goto fail;
        }

        f32 angle = (f32)rand() / (f32)RAND_MAX * 2.0f * (f32)PI; // [0, 2π)
        f32 half  = angle * 0.5f;

        strcpy_s(new_entity->name, PATH_LENGTH, "bull_shark_");
        strcat_s(new_entity->name, PATH_LENGTH, number);
        strcat_s(new_entity->mesh_name, PATH_LENGTH, "./out/data/models/bull_shark.glb");
        new_entity->position = (Vec3){(f32)(rand() % 10), (f32)(rand() % 10), (f32)(rand() % 10)};
        new_entity->rotation = (Vec4){ 0.0f, sinf(half), 0.0f, cosf(half) };
        new_entity->scale    = 1.0;

        graphics_default_materials_add(new_entity);

        LOG_MESSAGE("added entity id: %llu name: \"%s\"", new_entity->entity_id, new_entity->name);
    }

    return TRUE;

    fail: {
        return FALSE;
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

    /* finish */
    entity_pool_free();

    return TRUE;

    fail: {
        return FALSE;
    }
}
