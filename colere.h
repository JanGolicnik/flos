#ifndef COLERE_H
#define COLERE_H

#include <marrow/marrow.h>
#include <marrow/json.h>

struct(PlantVertex)
{
    vec3s position;
    vec3s normal;
};

typedef u16 PlantIndex;
typedef SLICE(PlantIndex) PlantIndexSlice;

struct(Plant)
{
    PlantVertexSlice vertices;
    PlantIndexSlice indices;
};

struct(PlantRule)
{
    char name;
    char content[127];
};

struct(PlantConfig)
{
    char initial[128];
    PlantRule rules[7];
};

static inline PlantConfig plant_config_from_json(s8 s)
{
    PlantConfig config = { 0 };
    JsonObject json = json_parse(s);

    JsonObject initial = json_find(json, str("initial"));
    slice_copy_to_ptr(initial.val.string, config.initial);

    JsonObject rules = json_find(json, str("rules"));
    u32 i = 0;
    for (JsonObject child = json_first(rules); child.val.type; child = json_next(child))
    {
        config.rules[i].name = *child.label.start;
        slice_copy_to_ptr(child.val.string, config.rules[i].content);
        i++;
    }

    return config;
}

PlantVertex icosahedron_vertices[] = {
    { .position = {{ 0.000000, -1.000000,  0.000000 }} },
    { .position = {{ 0.723600, -0.447215,  0.525720 }} },
    { .position = {{ -0.276385, -0.447215,  0.850640 }} },
    { .position = {{ -0.894425, -0.447215,  0.000000 }} },
    { .position = {{ -0.276385, -0.447215, -0.850640 }} },
    { .position = {{ 0.723600, -0.447215, -0.525720 }} },
    { .position = {{ 0.276385,  0.447215,  0.850640 }} },
    { .position = {{ -0.723600,  0.447215,  0.525720 }} },
    { .position = {{ -0.723600,  0.447215, -0.525720 }} },
    { .position = {{ 0.276385,  0.447215, -0.850640 }} },
    { .position = {{ 0.894425,  0.447215,  0.000000 }} },
    { .position = {{ 0.000000,  1.000000,  0.000000 }} },
};

PlantIndex icosahedron_indices[] = {
    0, 1, 2,
    1, 0, 5,
    0, 2, 3,
    0, 3, 4,
    0, 4, 5,
    1, 5, 10,
    2, 1, 6,
    3, 2, 7,
    4, 3, 8,
    5, 4, 9,
    1, 10, 6,
    2, 6, 7,
    3, 7, 8,
    4, 8, 9,
    5, 9, 10,
    6, 10, 11,
    7, 6, 11,
    8, 7, 11,
    9, 8, 11,
    10, 9, 11,
};

static inline Plant generate_plant(u64 seed, PlantConfig conifig)
{
    Plant p = {
        .vertices = (PlantVertexSlice)array_slice(icosahedron_vertices),
        .indices = (PlantIndexSlice)array_slice(icosahedron_indices)
    };
    slice_for_each(p.vertices, v)
    {
        v->position = v->normal = glms_vec3_normalize(v->position);
    }

    return p;
}

#endif // COLERE_H
