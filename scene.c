#include "marrow/genarr.h"
#include "marrow/marrow.h"
#define FLOS_SCENE
#include "base.c"

STRUCT(Scene) {
    GENARR(Entity) entities;
    EntityHandle player;
    EntityHandle planets[2];
};

Entity* scene_get_entity(Scene* scene, EntityHandle handle) {
    return genarr_get((scene)->entities, handle);
}

EntityHandle _scene_create_entity(Scene* scene, EntityHandle handle) {
    Entity* entity = scene_get_entity(scene, handle);
    if (entity->components & CT_Transform)
    {
        entity->transform.local.rot = glms_quat_normalize(entity->transform.local.rot);
        entity->transform.world.rot = glms_quat_normalize(entity->transform.world.rot);
    }

    if (!entity->parent.valid) return handle;
    Entity* parent = scene_get_entity(scene, entity->parent);
    entity->next_sibling = parent->first_child;
    parent->first_child = handle;
    return handle;
}

#define scene_create_entity(_scene, _comps, ...)\
    _scene_create_entity(genarr_add((_scene)->entities, (Entity){ .components = (_comps), __VA_ARGS__ }))

STRUCT(EntityIter) {
    Entity* entity;
    u64 _i;
    ComponentType include;
    ComponentType any;
    ComponentType exclude;
};

bool scene_next_entity(Scene* scene, EntityIter* iter) {
    FLAG_SET(iter->exclude, CT_IsHidden);
    FLAG_CLEAR(iter->exclude, iter->include);
    while ((iter->entity = genarr_next_valid(scene->entities, &iter->_i))) {
        ComponentType comps = iter->entity->components;
        if (FLAG_HAS_ANY(comps, iter->exclude)) continue;
        if (!FLAG_HAS_ALL(comps, iter->include)) continue;
        if (iter->any && FLAG_HAS_ANY(comps, iter->any)) return true;
    }
    return false;
}
