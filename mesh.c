#define FLOS_MESH
#include "base.c"

STRUCT(Vertex) {
    vec3s position;
    vec3s normal;
};

STRUCT(PlanetInstance) {
    mat4s mat;
    f32 shell_t;
    f32 scale;
    f32 _pad[2];
};

STRUCT(Instance) {
    mat4s mat;
};

typedef GenarrHandle MeshHandle;
