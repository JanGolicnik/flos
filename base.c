#ifndef FLOS_BASE
#define FLOS_BASE

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
#include <marrow/genarr.h>

#define RIPPLE_IMPLEMENTATION
#include <ripple/ripple.h>
#include <ripple/ripple_widgets.h>

#define CGLM_OMIT_NS_FROM_STRUCT_API
#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cglm/struct.h>

typedef versors quats;

#ifndef FLOS_MEMORY
#include "memory.c"

#ifndef FLOS_UTILS
#include "utils.c"

#ifndef FLOS_WINDOW
#include "window.c"

#ifndef FLOS_MESH
#include "mesh.c"

#ifndef FLOS_PLANT
#include "plant.c"

#ifndef FLOS_PLANET
#include "planet.c"

#ifndef FLOS_ENTITY
#include "entity.c"

#ifndef FLOS_SCENE
#include "scene.c"

#ifndef FLOS_RENI
#include "reni.c"

#ifndef FLOS_RENDER
#include "render.c"

#ifndef FLOS_UI
#include "ui.c"

#ifndef FLOS_GAME
#include "game.c"

#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif

#endif // FLOS_BASE
