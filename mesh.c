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
    vec3s pos;
};

Vertex icosahedron_vertices[] = {
    {.position = {{0.000000, -1.000000, 0.000000}}},
    {.position = {{0.723600, -0.447215, 0.525720}}},
    {.position = {{-0.276385, -0.447215, 0.850640}}},
    {.position = {{-0.894425, -0.447215, 0.000000}}},
    {.position = {{-0.276385, -0.447215, -0.850640}}},
    {.position = {{0.723600, -0.447215, -0.525720}}},
    {.position = {{0.276385, 0.447215, 0.850640}}},
    {.position = {{-0.723600, 0.447215, 0.525720}}},
    {.position = {{-0.723600, 0.447215, -0.525720}}},
    {.position = {{0.276385, 0.447215, -0.850640}}},
    {.position = {{0.894425, 0.447215, 0.000000}}},
    {.position = {{0.000000, 1.000000, 0.000000}}},
};

u16 icosahedron_indices[] = {
    0, 1, 2, 1, 0,  5, 0,  2,  3, 0, 3,  4,  0, 4,  5, 1, 5,  10, 2, 1,
    6, 3, 2, 7, 4,  3, 8,  5,  4, 9, 1,  10, 6, 2,  6, 7, 3,  7,  8, 4,
    8, 9, 5, 9, 10, 6, 10, 11, 7, 6, 11, 8,  7, 11, 9, 8, 11, 10, 9, 11,
};

#endif // MESH
