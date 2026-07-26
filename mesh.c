#define FLOS_MESH
#include "base.c"

STRUCT(Vertex) {
    vec3s position;
    vec3s normal;
};

STRUCT(PlanetInstance) {
    mat4s mat;
};

STRUCT(PlantInstance) {
    mat4s mat;
};

typedef GenarrHandle MeshHandle;
