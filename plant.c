#ifndef PLANT
#define PLANT
#ifndef UNITY_BUILD
#include "base.c"
#include "mesh.c"
#endif

STRUCT(PlantConfig) {
    struct {
        u32 iterations;
        char initial[128];
        struct {
            char name;
            char result[128];
        } rules[8];
        u32 n_rules;
    } rules;
    struct {
        char name;
        f32 width;
        f32 length;
        f32 angle;
    } shapes[8];
    u32 n_shapes;
};

STRUCT(PlantShape) {
    vec3s start, end;
    f32 width;
};

STRUCT(PlantMesh) {
    VertexSlice vertices;
    u16Slice indices;
};

STRUCT(PlantTemplate) {
    PlantShape shapes[1024];
    u32 n_shapes;
};

STRUCT(Plant)
{
    vec3s pos;
    vec3s up;
    f32 scale;
    u32 mesh;
};

f32 branch = 1.0f;
PlantShape *plant_step(PlantShape prev, PlantShape *shapes, f32 angle, u32 gen) {
    if (gen > 6)
        return shapes;
    vec3s dir = glms_vec3_scale(
        glms_vec3_rotate(GLMS_YUP, angle, GLMS_ZUP), 1.0f / ((float)gen + 1.0f)
    );
    vec3s start = prev.end;
    PlantShape shape = (PlantShape){
        .start = start,
        .end = glms_vec3_add(start, dir),
        .width = .1f
    };
    (*shapes++) = shape;
    shapes = plant_step(shape, shapes, angle - branch, gen + 1);
    shapes = plant_step(shape, shapes, angle + branch, gen + 1);
    return shapes;
}

PlantTemplate plant_generate(void) {
    PlantTemplate p = { 0 };
    p.n_shapes = plant_step(p.shapes[0], p.shapes, 0.0f, 0) - p.shapes;
    return p;
}

PlantConfig plant_parse_config(JsonObject json) {
    PlantConfig config = {0};
    for (json = json_first(json); json.val.type; json = json_next(json)) {
        if (str_cmp(json.label, str("rules")) == 0) {
            config.rules.iterations = json_find(json, str("iterations")).val.integer;
            str initial = json_find(json, str("initial")).val.string;
            buf_copy(config.rules.initial, initial.start, slice_size(initial));
            JsonObject rules = json_find(json, str("rules"));
            for (rules = json_first(rules); rules.val.type;
                rules = json_next(rules)) {
                u32 i = config.rules.n_rules++;
                config.rules.rules[i].name = rules.label.start[0];
                str result = json_find(rules, str("result")).val.string;
                buf_copy(config.rules.rules[i].result, result.start, slice_size(result));
            }
        } else if (str_cmp(json.label, str("shapes")) == 0) {
            JsonObject shapes = json;
            for (shapes = json_first(shapes); shapes.val.type; shapes = json_next(shapes)) {
                u32 i = config.n_shapes++;
                config.shapes[i].name = shapes.label.start[0];
                config.shapes[i].width = json_find(shapes, str("width")).val.decimal;
                config.shapes[i].length = json_find(shapes, str("length")).val.decimal;
                config.shapes[i].angle = json_find(shapes, str("angle")).val.decimal;
            }
        }
    }
    return config;
}

PlantMesh plant_meshify(PlantTemplate *plant, Allocator* allocator) {
    usize n_vertices = plant->n_shapes * 4;
    usize n_indices = plant->n_shapes * 6;
    Vertex* vertices = allocator_alloc(allocator, n_vertices * sizeof(Vertex), alignof(Vertex));
    u16* indices = allocator_alloc(allocator, n_indices * sizeof(u16), alignof(u16));

    u32 vi = 0, ii = 0;
    for (u32 i = 0; i < plant->n_shapes; i++) {
        PlantShape s = plant->shapes[i];
        vec3s dir = glms_vec3_normalize(glms_vec3_sub(s.start, s.end));
        vec3s right = fabsf(dir.y) > 0.995
                        ? (vec3s){ .x = 1.0f }
                        : glms_vec3_normalize((vec3s){ .x = -dir.z, .z = dir.x });
        right = glms_vec3_scale(right, 0.5f * s.width);

        i32 r = vi;
        vertices[vi++] = (Vertex){ .position = glms_vec3_add(s.start, right) };
        vertices[vi++] = (Vertex){ .position = glms_vec3_add(s.end, right) };
        vertices[vi++] = (Vertex){ .position = glms_vec3_sub(s.start, right) };
        vertices[vi++] = (Vertex){ .position = glms_vec3_sub(s.end, right) };
        indices[ii++] = r + 0;
        indices[ii++] = r + 1;
        indices[ii++] = r + 2;
        indices[ii++] = r + 1;
        indices[ii++] = r + 2;
        indices[ii++] = r + 3;
    }

    return (PlantMesh) {
        .vertices = slice_to(vertices, n_vertices),
        .indices = slice_to(indices, n_indices)
    };
}

#endif // PLANT
