#include "marrow/marrow.h"
#define FLOS_ENTITY
#include "base.c"

typedef enum {
    CT_Transform = BIT(0),
    CT_Mesh = BIT(1),
    CT_Physics = BIT(2),
    CT_IsHidden = BIT(3),
    CT_Player = BIT(4),
    CT_Plant  = BIT(5),
    CT_Planet = BIT(6),
    CT_Camera = BIT(7),
} ComponentType;

typedef struct Scene Scene;

typedef GenarrHandle EntityHandle;

STRUCT(Entity) {
    str name;
    Scene* scene;
    EntityHandle parent, first_child, next_sibling;

    ComponentType components;

    struct TransformC {
        struct Transform {
            vec3s pos;
            quats rot;
            f32 scale;
        } local, world;
        mat4s _matrix;
    } transform;

    struct MeshC {
        MeshHandle mesh;
    } mesh;

    struct PhysicsC {
        vec3s vel;
        bool on_ground;
        EntityHandle planet;
    } physics;

    struct PlayerC {
        u8 _;
    } player;

    struct PlanetC {
        f32 gravity;
    } planet;

    struct PlantC {
        u8 _;
    } plant;

    struct CameraC {
        f32 pitch;
    } camera;
};

void entity_enable_components(Entity* entity, ComponentType components) {
    FLAG_SET(entity->components, components);
}

void entity_disable_components(Entity* entity, ComponentType components) {
    FLAG_CLEAR(entity->components, components);
}

bool entity_has(Entity* entity, ComponentType components) {
    return FLAG_HAS_ALL(entity->components, components);
}

mat4s glms_mat4_from_transform(struct Transform* transform)
{
    mat4s mat = glms_quat_mat4(transform->rot);
    mat.col[0] = glms_vec4_scale(mat.col[0], transform->scale);
    mat.col[1] = glms_vec4_scale(mat.col[1], transform->scale);
    mat.col[2] = glms_vec4_scale(mat.col[2], transform->scale);
    mat.col[3] = (vec4s){ .x = transform->pos.x, .y = transform->pos.y, .z = transform->pos.z, .w = 1.0f };
    return mat;
}
