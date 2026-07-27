#include "base.c"

i32 main(void) {
    memory_init();
    window_init();
    render_init();
    game_init();
    ui_init();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(main_loop, nullptr, 0, true);
#else // __EMSCRIPTEN__
    while (!glfwWindowShouldClose(window.window)) {
        glfwPollEvents();
        game_on_frame(nullptr);
    };
#endif // __EMSCRIPTEN__

    return 1;
}
