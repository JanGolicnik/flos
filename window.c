#ifndef WINDOW
#define WINDOW

#include "ripple/backends/ripple_glfw.h"
#ifndef UNITY_BUILD
#include "base.c"
#endif // UNITY_BUILD

typedef enum {
    KEY_UNKNOWN = 0,
    KEY_W = 1,
    KEY_A = 2,
    KEY_S = 3,
    KEY_D = 4,
    KEY_SPACE = 5,
    KEY_SHIFT = 6,
    KEY_CTRL = 7,
    KEY_ESC = 8,
    KEY_M1 = 9,
    KEY_M2 = 9,
    KEY_M3 = 9,

    KEY_LAST,

    KEY_PRESSED = 0,
    KEY_HELD = 1,
    KEY_RELEASED = 2
} Key;

struct {
    u32 width;
    u32 height;

    bool keys[3][KEY_LAST];
    struct {
        f32 dx, dy;
        bool has_lock;
        bool left;
    } mouse;

#ifdef __EMSCRIPTEN__
    cstr window;
#else // __EMSCRIPTEN__
    GLFWwindow *window;
#endif // __EMSCRIPTEN__

} window = { 0 };

#ifdef __EMSCRIPTEN__

bool on_mouse(i32 type, const EmscriptenMouseEvent *event, void *user) {
    if (window.mouse.has_lock) {
        if (type == EMSCRIPTEN_EVENT_MOUSEMOVE) {
            window.mouse.dx = (f32)event->movementX;
            window.mouse.dy = (f32)event->movementY;
        } else if (type == EMSCRIPTEN_EVENT_MOUSEDOWN) {
            window.keys[KEY_PRESSED][KEY_M1 + event->button] = true;
        } else if (type == EMSCRIPTEN_EVENT_MOUSEUP) {
            window.keys[KEY_RELEASED][KEY_M1 + event->button] = true;
        }
        return true;
    }
    return ripple_emscripten_mouse_callback(type, event, user);
}

bool on_key(i32 type, const EmscriptenKeyboardEvent *event, void *user) {
    if (!window.mouse.has_lock)
        return false;
    if (event->repeat)
        return true;
    if (type != EMSCRIPTEN_EVENT_KEYDOWN && type != EMSCRIPTEN_EVENT_KEYUP)
        return false;
    i32 state = type == EMSCRIPTEN_EVENT_KEYDOWN ? KEY_PRESSED : KEY_RELEASED;
    Key key = KEY_UNKNOWN;
    if (!str_cmp(event->code, "KeyW"))
        key = KEY_W;
    else if (!str_cmp(event->code, "KeyA"))
        key = KEY_A;
    else if (!str_cmp(event->code, "KeyS"))
        key = KEY_S;
    else if (!str_cmp(event->code, "KeyD"))
        key = KEY_D;
    else if (!str_cmp(event->code, "Space"))
        key = KEY_SPACE;
    else if (!str_cmp(event->code, "ShiftLeft"))
        key = KEY_SHIFT;
    window.keys[state][key] = true;
    return true;
}

bool on_pointerlock_change(i32 type, const EmscriptenPointerlockChangeEvent *event, void *user) {
    window.mouse.has_lock = event->isActive;
    return true;
}

#else // __EMSCRIPTEN__

void on_mouse(GLFWwindow *_, f64 x, f64 y) {
    static f64 prev_x = 0.0;
    static f64 prev_y = 0.0;
    if (window.mouse.has_lock) {
        window.mouse.dx = (f32)(x - prev_x);
        window.mouse.dy = (f32)(y - prev_y);
    }
    prev_x = x;
    prev_y = y;
    ripple_glfw_mouse_pos_callback(window.window, x, y);
}

void on_mouse_click(GLFWwindow *_, i32 button, i32 action, i32 mods) {
    Key pressed = action == GLFW_PRESS ? KEY_PRESSED : KEY_RELEASED;
    window.keys[pressed][KEY_M1 + button] = true;
    ripple_glfw_mouse_button_callback(window.window, button, action, mods);
}

void on_key(GLFWwindow *_, i32 key_code, i32 scancode, i32 action, i32 mods) {
    if (!window.mouse.has_lock)
        return;
    if (action == GLFW_REPEAT)
        return;
    i32 state = action == GLFW_PRESS ? KEY_PRESSED : KEY_RELEASED;
    Key key = KEY_UNKNOWN;
    if (key_code == GLFW_KEY_W)
        key = KEY_W;
    else if (key_code == GLFW_KEY_A)
        key = KEY_A;
    else if (key_code == GLFW_KEY_S)
        key = KEY_S;
    else if (key_code == GLFW_KEY_D)
        key = KEY_D;
    else if (key_code == GLFW_KEY_SPACE)
        key = KEY_SPACE;
    else if (key_code == GLFW_KEY_ESCAPE)
        key = KEY_ESC;
    window.keys[state][key] = true;
}

void on_resize(GLFWwindow *_, i32 width, i32 height) {
    window.width = width;
    window.height = height;
}

#endif // __EMSCRIPTEN__

void window_init(void)
{
    window.width = 800;
    window.height = 800;

#ifdef __EMSCRIPTEN__
    window.window = "#canvas";
    emscripten_set_mousemove_callback(window.window, &renderer.ripple_context, EM_TRUE, on_mouse);
    emscripten_set_mousedown_callback(window.window, &renderer.ripple_context, EM_TRUE, on_mouse);
    emscripten_set_mouseup_callback(window.window, &renderer.ripple_context, EM_TRUE, on_mouse);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, &renderer.ripple_context, EM_TRUE, on_key);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, &renderer.ripple_context, EM_TRUE, on_key);
    emscripten_set_pointerlockchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, EM_TRUE, on_pointerlock_change);
#else // __EMSCRIPTEN__
    glfwInit();
    window.window = glfwCreateWindow(window.width, window.height, "hello!", nullptr, nullptr);

    glfwSetCursorPosCallback(window.window, &on_mouse);
    glfwSetMouseButtonCallback(window.window, &on_mouse_click);
    glfwSetKeyCallback(window.window, &on_key);
    glfwSetWindowSizeCallback(window.window, &on_resize);
#endif // __EMSCRIPTEN__
}

#endif // WINDOW
