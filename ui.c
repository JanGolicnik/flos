#ifndef UI
#define UI

#ifndef UNITY_BUILD
#include "base.c"
#include "render.c"
#endif // UNITY_BUILD

void ui_init(void) {
#ifdef __EMSCRIPTEN__
    ripple_emscripten_init(&renderer.ripple_context, window.window, false);
#else // __EMSCRIPTEN__
    ripple_glfw_init(&renderer.ripple_context, window.window, false);
#endif // __EMSCRIPTEN__
}


#endif // UI
