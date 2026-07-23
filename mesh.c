#ifndef MESH
#define MESH
#ifndef UNITY_BUILD
#include "base.c"
#endif // UNITY_BUILD

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

#endif // MESH
