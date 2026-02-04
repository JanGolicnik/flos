#ifndef COLERE_H
#define COLERE_H

#include <marrow/marrow.h>
#include <marrow/json.h>

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
