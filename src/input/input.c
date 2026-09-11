#include "input.h"
#include <GLFW/glfw3.h>

static GLFWwindow* window = NULL;
static f64 mouse_x = 0.0;
static f64 mouse_y = 0.0;
static f64 time    = 0.0;

b32 input_hook_window(void* glfw_window) {
    window = glfw_window;
    glfwSetTime(0.0);
    return TRUE;
}

b32 input_process_window_should_close(void) {
    glfwPollEvents();
    return glfwWindowShouldClose(window);
}

void input_gather_input(Input* input) {
    /* mouse delta & position */
    f64 new_mouse_x = 0.0;
    f64 new_mouse_y = 0.0;
    i32 screen_width  = 0;
    i32 screen_height = 0;

    glfwGetWindowSize(window, &screen_width, &screen_height);
    glfwGetCursorPos(window, &new_mouse_x, &new_mouse_y);
    f64 mouse_delta_x = (new_mouse_x - mouse_x) / (f64)MIN(screen_width, screen_height);
    f64 mouse_delta_y = (new_mouse_y - mouse_y) / (f64)MIN(screen_width, screen_height);
    mouse_x = new_mouse_x;
    mouse_y = new_mouse_y;

    b32 boost = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

    /* mouse buttons */
    b32 action_0 = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    b32 action_1 = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    /* movement axis */
    i32 vertical_axis   = 0;
    i32 horizontal_axis = 0;

    if(glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        vertical_axis += 1;
    }
    if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        vertical_axis -= 1;
    }
    if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        horizontal_axis -= 1;
    }
    if(glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        horizontal_axis += 1;
    }

    /* time */
    f64 new_time = glfwGetTime();
    f64 delta    = new_time - time;
    time = new_time;

    *input = (Input) {
        .mouse_x             = (f32)mouse_x,
        .mouse_y             = (f32)mouse_y,
        .mouse_delta_x       = (f32)mouse_delta_x,
        .mouse_delta_y       = (f32)mouse_delta_y,
        .movement_vertical   = (f32)vertical_axis,
        .movement_horizontal = (f32)horizontal_axis,
        .boost               = boost,
        .action_0            = action_0,
        .action_1            = action_1,
        .time                = time,
        .delta               = delta
    };
}

void input_use_cursor(b32 state) {
    glfwSetInputMode(window, GLFW_CURSOR, state ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
}
