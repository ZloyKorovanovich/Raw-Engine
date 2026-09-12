#ifndef _GAME_STRUCTS_INCLUDED
#define _GAME_STRUCTS_INCLUDED

#include "game.h"

typedef struct {
    char name     [PATH_LENGTH];
    char mesh_name[PATH_LENGTH];
    u64  entity_id;
    b32  is_updated;
    f32  scale;
    Vec3 position;
    Vec4 rotation;
} Entity;

b32 graphics_init(b32 is_debug);
void graphics_terminate(void);
b32 graphics_render_frame(Mat4x4 camera_vp, Mat4x4 camera_iv);
/* default materials */
b32 graphics_default_materials_add(Entity* entity);
void graphics_default_materials_remove(Entity* entity);
void graphics_default_materials_clear(void);
void render_default_materials(void);

#endif
