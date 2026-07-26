#include "cglm/struct/quat.h"
#include "cglm/struct/vec3.h"
#include "marrow/marrow.h"
#define FLOS_GAME
#include "base.c"

struct {
    f32 prev_time;
    f32 dt;
    f32 dt_accum;
    f32 avg_fps;
    u32 dt_n_samples;

    PlantTemplate plant_templates[1];

    VEKTOR(Scene*) scenes;
    Scene* current_scene;

    MeshHandle plant_mesh;
    MeshHandle planet_mesh;

    BumpAllocator _frame_allocator;
    Allocator* frame_allocator;
    Allocator* stable_allocator;
} game = { 0 };

Scene* game_new_scene(void) {
    Scene* scene = mrw_alloc(game.stable_allocator, Scene);
    vektor_add(game.scenes, scene);
    *scene = (Scene) { 0 };
    genarr_init(scene->entities, 12, game.stable_allocator);
    return scene;
}

void game_update_player(Scene* scene) {
    Entity* entity = scene_get_entity(scene, scene->player);
    struct PhysicsC* phys = &entity->physics;

    struct Transform* world = &entity->transform.world;

    text(mrw_format("pos: {.2f} {.2f} {.2f}", game.frame_allocator,
        world->pos.x,
        world->pos.y,
        world->pos.z
    ));

    Entity* planet = scene_get_entity(scene, phys->planet);

    vec3s target_up = glms_normalize(glms_vec3_sub(world->pos, planet->transform.world.pos));
    vec3s curr_up = glms_quat_rotatev(world->rot, GLMS_YUP);
    vec3s up = glms_normalize(glms_vec3_lerp(curr_up, target_up, 1.0f - expf(-10.0f * game.dt)));

    quats align = glms_quat_from_vecs(curr_up, up);
    world->rot = glms_quat_normalize(glms_quat_mul(align, world->rot));

    f32 yaw_change = -window.mouse.dx * 0.01;
    quats yaw = glms_quatv(yaw_change, up);
    world->rot = glms_quat_normalize(glms_quat_mul(yaw, world->rot));

    f32 speed  = window.keys[KEY_HELD][KEY_SHIFT] ? 2.0f : 1.0f;
    f32 move_x = window.keys[KEY_HELD][KEY_A] - window.keys[KEY_HELD][KEY_D];
    f32 move_z = window.keys[KEY_HELD][KEY_W] - window.keys[KEY_HELD][KEY_S];

    vec3s move_xz = glms_vec3_scale((move_x != 0.0f || move_z != 0.0f) ?(vec3s){{ move_x, 0.0f, move_z }} : GLMS_VEC3_ZERO, speed);

    if (window.keys[KEY_HELD][KEY_SPACE] && phys->on_ground) {
        phys->vel.y = 2.0f;
    }
    phys->vel.x = move_xz.x;
    phys->vel.z = move_xz.z;

    // TODO: use entity_first_child_with
    for_each_entity_children(entity, child)
    {
        if (!entity_has(child, CT_Camera)) continue;

        struct Transform* local = &child->transform.local;

        child->camera.pitch = clamp(child->camera.pitch + window.mouse.dy * 0.01, -M_PI * 0.5, M_PI * 0.5);

        local->rot = glms_quat_mul(glms_quatv(child->camera.pitch, GLMS_XUP), glms_quatv(M_PI, GLMS_YUP));
        entity_transform_apply_local(child);

        if (window.keys[KEY_PRESSED][KEY_M1]) {
            struct Transform* world = &child->transform.world;
            vec3s forward = glms_vec3_scale(glms_quat_rotatev(world->rot, GLMS_ZUP), -1.0f);
            f32 t1, t2;
            f32 closest_dist = 99999.0f;
            EntityIter planet_iter = { .include = CT_Planet | CT_Transform };
            while (scene_next_entity(scene, &planet_iter)) {
                struct Transform planet_world = planet_iter.entity->transform.world;
                bool clicked = glms_ray_sphere(world->pos, forward,
                    (vec4s){
                        .x = planet_world.pos.x,
                        .y = planet_world.pos.y,
                        .z = planet_world.pos.z,
                        .w = planet_world.scale
                    }, &t1, &t2);
                if (clicked && t1 > 0.0f && t1 < closest_dist) {
                    phys->planet = planet_iter.handle;
                    closest_dist = t1;
                }
                mrw_debug_val(clicked);
            }
        }

        break;
    }

    text(mrw_format("vely: {.3f}", game.frame_allocator, phys->vel.y));
}

void game_update_physics(Scene* scene) {
    EntityIter iter = { .include = CT_Physics | CT_Transform };
    while(scene_next_entity(scene, &iter))
    {
        struct Transform* world = &iter.entity->transform.world;
        struct PhysicsC* phys = &iter.entity->physics;

        Entity* planet = scene_get_entity(scene, phys->planet);

        phys->vel.y = phys->on_ground ?
            max(phys->vel.y, 0.0f) :
            (phys->vel.y + planet->planet.gravity * game.dt);

        vec3s right   = glms_vec3_scale(glms_quat_rotatev(world->rot, GLMS_XUP), phys->vel.x);
        vec3s up      = glms_vec3_scale(glms_quat_rotatev(world->rot, GLMS_YUP), phys->vel.y);
        vec3s forward = glms_vec3_scale(glms_quat_rotatev(world->rot, GLMS_ZUP), phys->vel.z);

        vec3s vel  = glms_vec3_add(right, glms_vec3_add(up, forward));
        world->pos = glms_vec3_add(world->pos, glms_vec3_scale(vel, game.dt));

        vec3s to = glms_vec3_sub(world->pos, planet->transform.world.pos);
        f32 dist = glms_vec3_norm(to);
        phys->on_ground = dist < planet->transform.world.scale + 0.01f;
        if (phys->on_ground && phys->vel.y < 0.0f) {
            world->pos = glms_vec3_add(
                planet->transform.world.pos,
                glms_vec3_scale(glms_vec3_divs(to, dist), planet->transform.world.scale)
            );
        }

        entity_transform_apply_world(iter.entity);
    }
}

void game_update(Scene* scene) {
    text(mrw_format("hello! you are running at {} fps.", game.frame_allocator, game.avg_fps));

    if (slider("hello !", &branch, 0.0f, 2.0f, game.frame_allocator)) {
        PlantTemplate template = game.plant_templates[0] = plant_generate();
        PlantMesh mesh = plant_meshify(&template, game.frame_allocator);
        render_mesh_re_create(game.plant_mesh, slice_u8(mesh.vertices), slice_u8(mesh.indices), sizeof(PlantInstance), 0);
    }

    game_update_player(scene);
    game_update_physics(scene);
}

void game_init(void) {
    game.stable_allocator = nullptr;
    game._frame_allocator = bump_allocator_create();
    game.frame_allocator = (Allocator*)&game._frame_allocator;
    Scene* scene = game.current_scene = game_new_scene();

    // meshes
    {
        FILE *fp = fopen("./res/plant.json", "rb");
        fseek(fp, 0, SEEK_END);
        usize size = ftell(fp);
        rewind(fp);
        char buf[size];
        fread(buf, 1, size, fp);
        fclose(fp);
        str file_slice = array_slice(buf);
        PlantConfig config = plant_parse_config(json_parse(file_slice));
        mrw_unused config;

        {
            PlantTemplate plant = game.plant_templates[0] = plant_generate();
            PlantMesh mesh = plant_meshify(&plant, game.frame_allocator);
            game.plant_mesh = render_mesh_create(slice_u8(mesh.vertices), slice_u8(mesh.indices), sizeof(PlantInstance), 0);
        }
        {
            PlanetMesh mesh = planet_meshify(game.frame_allocator);
            game.planet_mesh = render_mesh_create(slice_u8(mesh.vertices), slice_u8(mesh.indices), sizeof(PlanetInstance), 1);
        }
    }

    EntityHandle planet = scene->planets[0] = scene_create_entity(scene, CT_Transform | CT_Mesh | CT_Planet,
        .name = sstr("planet"),
        .mesh = { game.planet_mesh },
        .planet.gravity = -2.0f,
    );

    scene->planets[1] = scene_create_entity(scene, CT_Transform | CT_Mesh | CT_Planet,
        .name = sstr("planet2"),
        .mesh = { game.planet_mesh },
        .transform.world = {
            .pos = { .x = 5.0f, .y = 5.0f, .z = 5.0f },
            .scale = 2.0f,
        },
        .planet.gravity = -4.0f,
    );

    for (u32 i = 0; i < 1000; i++) {
        vec3s pos = random_on_sphere();
        vec3s up = glms_vec3_normalize(pos);
        scene_create_entity(scene, CT_Transform | CT_Mesh | CT_Plant,
            .name = sstr("plant"),
            .parent = planet,
            .transform.world = {
                .pos = glms_vec3_scale(pos, 1.0f),
                .scale = mrw_random_f32(1.0, 3.0) * 0.03,
                .rot = glms_quat_mul(glms_quatv(mrw_random_f32(-M_PI, M_PI), up), glms_quat_from_vecs(GLMS_YUP, up)),
            },
            .mesh = { game.plant_mesh },
        );
    }

    scene->player = scene_create_entity(scene, CT_Transform | CT_Physics | CT_Player,
        .name = sstr("player"),
        .transform.world.pos = { .x = 0.0f, .y = 1.0f, .z = -0.5f },
        .physics.planet = planet,
    );

    scene->camera = scene_create_entity(scene, CT_Transform | CT_Camera,
        .name = sstr("camera"),
        .parent = scene->player,
        .transform.local = {
            .scale = 1.0f,
            .pos = { .x = 0.0f, .y = 0.3f, .z = 0.0f }
        },
    );
}

static mat4s basis_from_up(vec3s up, vec3s hint) {
    if (fabsf(glms_vec3_dot(up, glms_vec3_normalize(hint))) > 0.9999f)
        hint = glms_vec3_ortho(up);

    vec3s x = glms_vec3_normalize(glms_vec3_cross(hint, up));
    vec3s z = glms_vec3_cross(x, up);

    mat4s m = glms_mat4_identity();
    m.col[0] = glms_vec4(x, 0.0f);
    m.col[1] = glms_vec4(up, 0.0f);
    m.col[2] = glms_vec4(z, 0.0f);
    return m;
}

void game_on_frame(void *_) {
    f32 time =
    #ifdef __EMSCRIPTEN__
        emscripten_get_now() * 0.001;
    #else
        glfwGetTime();
    #endif

    game.dt = time - game.prev_time;
    game.dt_accum += game.dt;
    game.dt_n_samples += 1;
    if (game.dt_accum > 1.0f) {
        game.avg_fps = (f32)game.dt_n_samples / game.dt_accum;
        game.dt_accum -= 0.3f;
        game.dt_n_samples = 0;
        mrw_debug_val(game.avg_fps);
    }
    game.prev_time = time;

    RIPPLE(
        FORM(.width = PERCENT(1.0f, SVT_RELATIVE_CHILD), .height = PERCENT(1.0f, SVT_RELATIVE_CHILD)),
        RECTANGLE(.color = RIPPLE_RGBA(0x2e2e2ebf), .radiusBR = .15f))
    {
        if (STATE().hovered) {
            window.keys[KEY_PRESSED][KEY_M1] = false;
        }

        game_update(game.current_scene);
        window_update_input(game.frame_allocator);
    }

    render_render(game.current_scene);

    bump_allocator_reset(&game._frame_allocator);
}
