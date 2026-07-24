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

void entity_enable(Entity* entity, ComponentType components) {
    entity->components |= components;
}

bool entity_has(Entity* entity, ComponentType components) {
    return entity->components & components;
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
