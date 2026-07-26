#include "marrow/genarr.h"
#include "marrow/marrow.h"
#define FLOS_SCENE
#include "base.c"

STRUCT(Scene) {
    GENARR(Entity) entities;
    EntityHandle player;
    EntityHandle camera;
    EntityHandle planets[2];
};

Entity* scene_get_entity(Scene* scene, EntityHandle handle) {
    return genarr_get((scene)->entities, handle);
}

#define for_each_entity_children(e, val)\
    for (Entity* val = scene_get_entity((e)->scene, (e)->first_child); val; val = scene_get_entity((e)->scene, val->next_sibling))

typedef enum {
    ETU_LOCAL,
    ETU_WORLD,
} EntityTransformUpdate;

struct Transform transform_calculate_world(struct Transform parent, struct Transform local) {
    return (struct Transform){
      .pos = glms_vec3_add(parent.pos, glms_quat_rotatev(parent.rot, glms_vec3_scale(local.pos, parent.scale))),
      .rot = glms_quat_mul(parent.rot, local.rot),
      .scale = parent.scale * local.scale,
    };
}

struct Transform transform_calculate_local(struct Transform parent, struct Transform world) {
    quats parent_inv_rot = glms_quat_inv(parent.rot);
    return (struct Transform){
      .pos = glms_vec3_scale(glms_quat_rotatev(parent_inv_rot, glms_vec3_sub(world.pos, parent.pos)), 1.0f / parent.scale),
      .rot = glms_quat_mul(parent_inv_rot, world.rot),
      .scale = world.scale / parent.scale,
    };
}

void entity_transform_apply(Entity* entity, Entity* parent, EntityTransformUpdate update) {
    if (update == ETU_LOCAL)
    {
        entity->transform.world = parent ? transform_calculate_world(parent->transform.world, entity->transform.local) : entity->transform.local;
        entity->transform._matrix = glms_mat4_from_transform(&entity->transform.world);
    }
    else
    {
        entity->transform.local = parent ? transform_calculate_local(parent->transform.world, entity->transform.world) : entity->transform.world;
    }

    for_each_entity_children(entity, child) {
        if (entity_has(child, CT_IsHidden)) continue;
        entity_transform_apply(child, entity, ETU_LOCAL);
    }
}

void entity_transform_apply_local(Entity* entity) {
    entity_transform_apply(entity, scene_get_entity(entity->scene, entity->parent), ETU_LOCAL);
}

void entity_transform_apply_world(Entity* entity) {
    entity_transform_apply(entity, scene_get_entity(entity->scene, entity->parent), ETU_WORLD);
}

void entity_set_hidden(Entity* entity, bool val) {
    if (val) {
        entity_enable_components(entity, CT_IsHidden);
    }
    else {
        entity_disable_components(entity, CT_IsHidden);
        entity_transform_apply_world(entity);
    }
}

EntityHandle _scene_create_entity(Scene* scene, EntityHandle handle) {
    Entity* entity = scene_get_entity(scene, handle);
    entity->scene = scene;

    if (entity->parent.valid) {
        Entity* parent = scene_get_entity(scene, entity->parent);
        entity->next_sibling = parent->first_child;
        parent->first_child = handle;
    }

    if (entity_has(entity, CT_Transform)) {
        if (entity->transform.local.scale == 0.0f) {
            entity->transform.world.rot = glms_quat_normalize(entity->transform.world.rot);
            if (entity->transform.world.scale == 0.0f) entity->transform.world.scale = 1.0f;
            entity_transform_apply_world(entity);
            entity->transform._matrix = glms_mat4_from_transform(&entity->transform.world);
        }
        else  {
            entity->transform.local.rot = glms_quat_normalize(entity->transform.local.rot);
            if (entity->transform.local.scale == 0.0f) entity->transform.local.scale = 1.0f;
            entity_transform_apply_local(entity);
        }
    }

    return handle;
}

#define scene_create_entity(_scene, _comps, ...)\
    _scene_create_entity((_scene), genarr_add((_scene)->entities, (Entity){ .components = (_comps), __VA_ARGS__ }))

STRUCT(EntityIter) {
    GENARR_ITER_ALIAS(Entity, entity);
    ComponentType include;
    ComponentType any;
    ComponentType exclude;
};

bool scene_next_entity(Scene* scene, EntityIter* iter) {
    FLAG_SET(iter->exclude, CT_IsHidden);
    FLAG_CLEAR(iter->exclude, iter->include);
    while (genarr_next_valid(scene->entities, iter)) {
        ComponentType comps = iter->entity->components;
        if (FLAG_HAS_ANY(comps, iter->exclude)) continue;
        if (iter->any && FLAG_HAS_ANY(comps, iter->any)) return true;
        if (!FLAG_HAS_ALL(comps, iter->include)) continue;
        return true;
    }
    return false;
}
