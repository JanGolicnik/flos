#include "marrow/marrow.h"
#define UNITY_BUILD

#include "base.c"

#include "mesh.c"
#include "plant.c"
#include "render.c"
#include "game.c"

void render_upload(void) {
    mat4s proj = glms_perspective(to_rad(90.0f), (f32)renderer.width / (f32)renderer.height, 0.01f, 1000.0f);
    glm_mat4_copy(
        glms_mat4_mul(proj, game.player.view).raw,
        renderer.shader_data.data.camera_matrix
    );

    wgpuQueueWriteBuffer(renderer.queue, renderer.shader_data.buffer, 0, &renderer.shader_data.data, sizeof(renderer.shader_data.data));

    // upload planet instances
    {
        Mesh* mesh = &game.planet_meshes[0];
        wgpuDeviceDynamicBufferEnsure(renderer.device, &mesh->instance_buffer, game.n_planets);
        PlanetInstance instances[game.n_planets];
        for (u32 i = 0; i < game.n_planets; i++) {
            instances[i].radius = game.planets[i].radius;
            instances[i].pos = game.planets[i].pos;
        }
        wgpuQueueWriteBuffer(renderer.queue, mesh->instance_buffer.data, 0, &instances, array_size(instances));
    }

    // upload plant instances
    {
        Mesh* mesh = &game.plant_meshes[0];
        wgpuDeviceDynamicBufferEnsure(renderer.device, &mesh->instance_buffer, game.n_plant_positions);
        PlantInstance instances[game.n_plant_positions];
        for (u32 i = 0; i < game.n_plant_positions; i++) {
            instances[i].pos = game.plant_positions[i];
        }
        wgpuQueueWriteBuffer(renderer.queue, mesh->instance_buffer.data, 0, &instances, array_size(instances));
    }
}

void main_loop(void *_) {
    f32 time =
    #ifdef __EMSCRIPTEN__
        emscripten_get_now() * 0.001;
    #else
        glfwGetTime();
    #endif
    game.dt = time - game.prev_time;
    game.dt_accum += game.dt;
    game.dt_n_samples += 1;
    if (game.dt_accum > 0.3f) {
        game.avg_fps = (f32)game.dt_n_samples / game.dt_accum;
        game.dt_accum -= 0.3f;
        game.dt_n_samples = 0;
        mrw_debug_val(game.avg_fps);
    }
    game.prev_time = time;

    RIPPLE(
        FORM(.width = PERCENT(1.0f, SVT_RELATIVE_CHILD), .height = PERCENT(1.0f, SVT_RELATIVE_CHILD)),
        RECTANGLE(.color = RIPPLE_RGBA(0x2e2e2ebf), .radiusBR = .15f))
    {
        if (STATE().hovered) {
            game.keys[KEY_PRESSED][KEY_M1] = false;
        }
        game_update();
    }

    render_upload();

    render(
        (MeshSlice)slice_to(game.plant_meshes, 1),
        (MeshSlice)slice_to(game.planet_meshes, 1)
    );

    bump_allocator_reset(&game.frame_allocator);
}

#ifdef __EMSCRIPTEN__

bool on_mouse(i32 type, const EmscriptenMouseEvent *event, void *user) {
    if (game.mouse.has_lock) {
        if (type == EMSCRIPTEN_EVENT_MOUSEMOVE) {
            game.mouse.dx = (f32)event->movementX;
            game.mouse.dy = (f32)event->movementY;
        } else if (type == EMSCRIPTEN_EVENT_MOUSEDOWN) {
            game.keys[KEY_PRESSED][KEY_M1 + event->button] = true;
        } else if (type == EMSCRIPTEN_EVENT_MOUSEUP) {
            game.keys[KEY_RELEASED][KEY_M1 + event->button] = true;
        }
        return true;
    }
    return ripple_emscripten_mouse_callback(type, event, user);
}

bool on_key(i32 type, const EmscriptenKeyboardEvent *event, void *user) {
    if (!game.mouse.has_lock)
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
    game.keys[state][key] = true;
    return true;
}

bool on_pointerlock_change(i32 type, const EmscriptenPointerlockChangeEvent *event, void *user) {
    game.mouse.has_lock = event->isActive;
    return true;
}

#else // __EMSCRIPTEN__

void on_mouse(GLFWwindow *window, f64 x, f64 y) {
    static f64 prev_x = 0.0;
    static f64 prev_y = 0.0;
    if (game.mouse.has_lock) {
        game.mouse.dx = (f32)(x - prev_x);
        game.mouse.dy = (f32)(y - prev_y);
    }
    prev_x = x;
    prev_y = y;
    ripple_glfw_mouse_pos_callback(window, x, y);
}

void on_mouse_click(GLFWwindow *window, i32 button, i32 action, i32 mods) {
    Key pressed = action == GLFW_PRESS ? KEY_PRESSED : KEY_RELEASED;
    game.keys[pressed][KEY_M1 + button] = true;
    ripple_glfw_mouse_button_callback(window, button, action, mods);
}

void on_key(GLFWwindow *window, i32 key_code, i32 scancode, i32 action, i32 mods) {
    if (!game.mouse.has_lock)
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
    game.keys[state][key] = true;
}

void on_resize(GLFWwindow *window, i32 width, i32 height) {
    renderer.width = width;
    renderer.height = height;
    renderer.reconfigure = true;
}

#endif // __EMSCRIPTEN__

void misc_init(void) {
#ifdef __EMSCRIPTEN__
    ripple_emscripten_register_callbacks(&renderer.ripple_context, "#canvas");
    emscripten_set_mousemove_callback("#canvas", &renderer.ripple_context, EM_TRUE, on_mouse);
    emscripten_set_mousedown_callback("#canvas", &renderer.ripple_context, EM_TRUE, on_mouse);
    emscripten_set_mouseup_callback("#canvas", &renderer.ripple_context, EM_TRUE, on_mouse);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, &renderer.ripple_context, EM_TRUE, on_key);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, &renderer.ripple_context, EM_TRUE, on_key);
    emscripten_set_pointerlockchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, EM_TRUE, on_pointerlock_change);
#else // __EMSCRIPTEN__
    ripple_glfw_register_callbacks(&renderer.ripple_context, renderer.window);
    glfwSetCursorPosCallback(renderer.window, &on_mouse);
    glfwSetMouseButtonCallback(renderer.window, &on_mouse_click);
    glfwSetKeyCallback(renderer.window, &on_key);
    glfwSetWindowSizeCallback(renderer.window, &on_resize);
#endif // __EMSCRIPTEN__
}

i32 main(void) {
    render_init();
    game_init();
    misc_init();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(main_loop, nullptr, 0, true);
#else // __EMSCRIPTEN__
    while (!glfwWindowShouldClose(renderer.window)) {
        glfwPollEvents();
        main_loop(nullptr);
    };
#endif // __EMSCRIPTEN__

    return 1;
}
