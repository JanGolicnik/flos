#define FLOS_ENTITY
#include "base.c"

typedef enum {
    CT_Transform = BIT(0),
    CT_Mesh = BIT(1),
    CT_Physics = BIT(2),
    CT_Behaviour = BIT(3),
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
    bool hidden;
    ComponentType components;
    struct {
        struct {
            struct {
                vec3s pos;
                quats rot;
                float scale;
            } local, world;
        } transform;

        struct {
            u32 mesh;
            u32 shader;
        } mesh;

        struct {
            vec3s vel;
            bool on_ground;
            u32 planet;
        } physics;

        struct {
            BehaviourType type;
            union {
                struct {
                    f32 _;
                } plant;

                struct {
                    f32 gravity;
                    f32 radius;
                } planet;

                struct {
                    f32 _;
                } player;
            };
        } behaviour;
    };
};
