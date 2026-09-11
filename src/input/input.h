#ifndef _INPUT_INCLUDED
#define _INPUT_INCLUDED

#include "../base.h"

typedef struct {
    f32 mouse_x;
    f32 mouse_y;
    f32 mouse_delta_x; /* left right */
    f32 mouse_delta_y; /* up down */

    f32 movement_vertical;   /* WS */
    f32 movement_horizontal; /* AD */

    b32 boost; /* shift */
    b32 action_0; /* lmb */
    b32 action_1; /* rmb */

    f64 time;
    f64 delta;
} Input;

b32 input_hook_window(void* glfw_window);
b32 input_process_window_should_close(void);
void input_gather_input(Input* input);
void input_use_cursor(b32 state);

#endif
