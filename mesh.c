#define FLOS_MESH
#include "base.c"

STRUCT(Vertex) {
    vec3s position;
    vec3s normal;
};

STRUCT(PlanetInstance) {
    f32 radius;
    vec3s pos;
};

STRUCT(PlantInstance) {
    mat4s mat;
};

typedef GenarrHandle MeshHandle;
