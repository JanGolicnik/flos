#include "marrow/marrow.h"
#define FLOS_ENTITY
#include "base.c"

typedef enum {
    CT_Transform = BIT(0),
    CT_Mesh = BIT(1),
    CT_Physics = BIT(2),
    CT_Behaviour = BIT(3),
    CT_IsHidden = BIT(4),
} ComponentType;

typedef enum {
    BT_Player = BIT(3),
    BT_Plant  = BIT(4),
    BT_Planet = BIT(5),
} BehaviourType;

typedef struct Scene Scene;

typedef GenarrHandle EntityHandle;

STRUCT(Entity) {
    str name;
    Scene* scene;
    EntityHandle parent, first_child, next_sibling;
    ComponentType components;
    struct {
        struct {
            struct {
                vec3s pos;
                quats rot;
                float scale;
            } local, world;
            mat4s _world;
        } transform;

        MeshHandle mesh;

        struct {
            vec3s vel;
            bool on_ground;
            u32 planet;
        } physics;

        struct {
            BehaviourType type;
            union {
                struct {
                    u8 _;
                } player;
                struct {
                    f32 gravity;
                    f32 radius;
                } planet;
                struct {
                    u8 _;
                } plant;

            };
        } behaviour;
    };
};

void entity_enable(Entity* entity, ComponentType components) {
    FLAG_SET(entity->components, components);
}

bool entity_has(Entity* entity, ComponentType components) {
    return FLAG_HAS_ALL(entity->components, components);
}
