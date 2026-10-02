#ifndef _GAME_STRUCTS_INCLUDED
#define _GAME_STRUCTS_INCLUDED

#include "game.h"
#include "../math/math.h"
#include "../resources/resources.h"
#include "../input/input.h"

#define INVALID_ENTITY_ID (0)

typedef enum {
    ENTITY_TYPE_STATIC = 0,
    ENTITY_TYPE_SHARK  = 1
} EntityType;

typedef struct {
    /* id */
    u64        entity_id;
    char       name[PATH_LENGTH];
    b32        is_updated;
    EntityType type;
    /* transform */
    f32        scale;
    Vec3       position;
    Vec4       rotation;
} Entity;

typedef struct {
    Entity* entity;
    u64     entity_id;
} EntityReference;

/* graphics */
b32  graphics_render_frame(Mat4x4 camera_vp, Mat4x4 camera_iv);
b32  graphics_opaque_lit_assign(Entity* entity, MeshHandle mesh, TextureHandle texture);
void graphics_opaque_lit_withdraw(Entity* entity);

/* game */
Entity* entity_create(const char* name);
void    entity_destroy(Entity* entity);
b32     engine_init(b32 is_debug);
void    engine_terminate(void);

#endif
