#define FLOS_UI
#include "base.c"

void ui_init(void) {
#ifdef __EMSCRIPTEN__
    ripple_emscripten_init(&renderer.ripple_context, window.window, false);
#else // __EMSCRIPTEN__
    ripple_glfw_init(&renderer.ripple_context, window.window, false);
#endif // __EMSCRIPTEN__
}
