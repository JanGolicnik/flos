#ifndef PLANET
#define PLANET
#ifndef UNITY_BUILD
#include "base.c"
#include "mesh.c"
#include "plant.c"
#endif // UNITY_BUILD

STRUCT(Planet) {
    f32 gravity;
    f32 radius;
    vec3s pos;
    usize mesh;

    Plant plants[100];
    u32 n_plants;
};

STRUCT(PlanetMesh) {
    VertexSlice vertices;
    u16Slice indices;
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

static void subdivide(VertexSlice *vertices, u16 *indices, usize *n_indices) {
    u32 n = *n_indices;
    for (u32 i = 0; i < n; i += 3) {
        u16 i1 = indices[i + 0];
        u16 i3 = indices[i + 1];
        u16 i5 = indices[i + 2];
        Vertex v1 = vertices->start[i1];
        Vertex v3 = vertices->start[i3];
        Vertex v5 = vertices->start[i5];

        Vertex v2 = { .position = glms_vec3_scale(glms_vec3_add(v1.position, v3.position), 0.5f) };
        Vertex v4 = { .position = glms_vec3_scale(glms_vec3_add(v3.position, v5.position), 0.5f) };
        Vertex v6 = { .position = glms_vec3_scale(glms_vec3_add(v5.position, v1.position), 0.5f) };

        u16 i2 = slice_count(*vertices);
        *(vertices->end++) = v2;
        u16 i4 = slice_count(*vertices);
        *(vertices->end++) = v4;
        u16 i6 = slice_count(*vertices);
        *(vertices->end++) = v6;

        indices[i] = i1;
        indices[i + 1] = i2;
        indices[i + 2] = i6;
        indices[(*n_indices)++] = i2;
        indices[(*n_indices)++] = i3;
        indices[(*n_indices)++] = i4;
        indices[(*n_indices)++] = i4;
        indices[(*n_indices)++] = i5;
        indices[(*n_indices)++] = i6;
        indices[(*n_indices)++] = i6;
        indices[(*n_indices)++] = i2;
        indices[(*n_indices)++] = i4;
    }
}

PlanetMesh planet_meshify(Allocator* allocator) {
    usize n_vertices_start = array_len(icosahedron_indices) / 3;
    usize n_indices_start = array_len(icosahedron_indices);
    usize n_vertices = n_vertices_start * 6 * 6;
    usize n_indices = n_indices_start * 4 * 4;
    Vertex* vertices = allocator_alloc(allocator, n_vertices * sizeof(Vertex), alignof(Vertex));
    u16* indices = allocator_alloc(allocator, n_indices * sizeof(u16), alignof(u16));

    buf_copy(vertices, icosahedron_vertices, sizeof(icosahedron_vertices));
    buf_copy(indices, icosahedron_indices, sizeof(icosahedron_indices));

    VertexSlice vertex_slice = slice_to(vertices, n_vertices_start);
    subdivide(&vertex_slice, indices, &n_indices_start);
    subdivide(&vertex_slice, indices, &n_indices_start);

    for (u32 i = 0; i < n_vertices; i++) {
        Vertex* v = &vertices[i];
        v->position = v->normal = glms_vec3_normalize(v->position);
    }

    return (PlanetMesh) {
      .vertices = slice_to(vertices, n_vertices),
      .indices = slice_to(indices, n_indices),
    };
}

#endif // PLANET
