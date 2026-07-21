#ifndef BASE
#define BASE

#include <float.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

#include <marrow/allocator.h>
#include <marrow/marrow.h>
#include <marrow/json.h>
#include <marrow/webgpu_utils.h>

#define RIPPLE_IMPLEMENTATION
#include <ripple/ripple.h>
#include <ripple/ripple_widgets.h>

#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cglm/struct.h>

#endif // BASE
