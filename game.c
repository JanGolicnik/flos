#ifndef GAME
#define GAME
#include "cglm/struct/affine.h"
#include "cglm/struct/mat4.h"
#include "cglm/struct/vec3.h"
#include "marrow/marrow.h"
#ifndef UNITY_BUILD
#include "base.c"
#include "utils.c"
#include "window.c"
#include "mesh.c"
#include "planet.c"
#include "plant.c"
#include "render.c"
#endif

struct {
    f32 prev_time;
    f32 dt;
    f32 dt_accum;
    f32 avg_fps;
    u32 dt_n_samples;

    struct {
        f32 yaw;
        f32 pitch;
        vec3s pos;
        vec3s up;
        vec3s local_vel;
        vec3s vel;
        mat4s view;
        vec3s forward;
        bool onground;
        u32 current_planet;
        f32 speed;
    } player;

    PlantTemplate plant_templates[1];
    Mesh plant_meshes[1];

    Planet planets[8];
    u32 n_planets;
    Mesh planet_meshes[1];

    BumpAllocator frame_allocator;
} game = { 0 };

void game_update_player(void) {
    text(mrw_format("pos: {.2f} {.2f} {.2f}",
        (Allocator *)&game.frame_allocator,
        game.player.pos.x,
        game.player.pos.y,
        game.player.pos.z)
    );

    Planet planet = game.planets[game.player.current_planet];

    game.player.speed = window.keys[KEY_HELD][KEY_SHIFT] ? 2.0f : 1.0f;

    f32 yaw_change = -window.mouse.dx * 0.01;
    game.player.pitch = clamp(game.player.pitch + window.mouse.dy * 0.01, to_rad(-89.0), to_rad(89.0));
    game.player.yaw += yaw_change;
    game.player.local_vel.x = window.keys[KEY_HELD][KEY_A] - window.keys[KEY_HELD][KEY_D];
    game.player.local_vel.z = window.keys[KEY_HELD][KEY_W] - window.keys[KEY_HELD][KEY_S];
    if (game.player.onground) {
        game.player.local_vel.y = max(game.player.local_vel.y, 0.0f);
        if (window.keys[KEY_HELD][KEY_SPACE]) {
            game.player.local_vel.y = 1.0f;
        }
    } else {
        game.player.local_vel.y += planet.gravity * game.dt;
    }

    vec3s target_up = glms_normalize(glms_vec3_sub(game.player.pos, planet.pos));
    vec3s up = game.player.up = glms_normalize(
        glms_vec3_lerp(game.player.up, target_up, 1.0f - expf(-10.0f * game.dt))
    );
    mat4s yaw_mat = glms_rotate_make(yaw_change, up);
    vec3s forward = game.player.forward;
    forward = glms_vec3_sub(forward, glms_vec3_scale(up, glms_dot(forward, up)));
    forward = glms_vec3_rotate_m4(yaw_mat, forward);
    vec3s right = glms_cross(up, forward);
    mat4s pitch_mat = glms_rotate_make(game.player.pitch, right);

    game.player.vel = glms_vec3_add(
        glms_vec3_scale(target_up, game.player.local_vel.y),
        glms_normalize(
            glms_vec3_add(
                glms_vec3_scale(right, game.player.local_vel.x),
                glms_vec3_scale(forward, game.player.local_vel.z)
            )
        )
    );
    game.player.pos = glms_vec3_add(
        game.player.pos,
        glms_vec3_scale(game.player.vel, game.player.speed * game.dt)
    );

    game.player.forward = forward;
    forward = glms_normalize(glms_vec3_rotate_m4(pitch_mat, forward));
    game.player.view = glms_look(game.player.pos, forward, up);

    game.player.onground =
        glms_vec3_norm(glms_vec3_sub(game.player.pos, planet.pos)) <= planet.radius + 0.1f;

    text(mrw_format("vely: {.3f}",
        (Allocator *)&game.frame_allocator,
        game.player.local_vel.y)
    );

    if (window.keys[KEY_PRESSED][KEY_M1]) {
        f32 t1, t2;
        f32 closest_dist = 99999.0f;
        for (u32 i = 0; i < game.n_planets; i++) {
            Planet p = game.planets[i];
            bool clicked = glms_ray_sphere(game.player.pos, forward, (vec4s){.x = p.pos.x, .y = p.pos.y, .z = p.pos.z, .w = p.radius}, &t1, &t2);
            if (clicked && t1 > 0.0f && t1 < closest_dist) {
                game.player.current_planet = i;
                closest_dist = t1;
            }
        }
    }
}

void game_update_collision(void) {
    for (u32 i = 0; i < game.n_planets; i++) {
        Planet p = game.planets[i];
        vec3s to = glms_vec3_sub(game.player.pos, p.pos);
        f32 dist = glms_vec3_norm(to);
        if (dist > p.radius) continue;
        game.player.pos = glms_vec3_add(
            p.pos, glms_vec3_scale(glms_vec3_divs(to, dist), p.radius)
        );
    }
}

void game_update_input(void) {
#ifndef __EMSCRIPTEN__
    if (window.keys[KEY_PRESSED][KEY_ESC]) {
        window.mouse.has_lock = false;
        glfwSetInputMode(window.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
#endif

    for (i32 i = 1; i < (i32)KEY_LAST; i++) {
        if (window.keys[KEY_PRESSED][i])
            window.keys[KEY_HELD][i] = true;
        if (window.keys[KEY_RELEASED][i])
            window.keys[KEY_HELD][i] = false;
        window.keys[KEY_PRESSED][i] = false;
        window.keys[KEY_RELEASED][i] = false;
    }

    if (!CURSOR().consumed && CURSOR().left.pressed) {
    #ifdef __EMSCRIPTEN__
        emscripten_request_pointerlock("#canvas", EM_TRUE);
    #else
        glfwSetInputMode(window.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        window.mouse.has_lock = true;
    #endif
    }

    text(mrw_format("mousedxdy: {.2f} {.2f}",
        (Allocator *)&game.frame_allocator,
        window.mouse.dx,
        window.mouse.dy
    ));
    window.mouse.dx = 0.0f;
    window.mouse.dy = 0.0f;
}

void game_update(void) {
    text(mrw_format("hello! you are running at {} fps.",
        (Allocator *)&game.frame_allocator,
        game.avg_fps
    ));

    if (slider("hello !", &branch, 0.0f, 2.0f, (Allocator *)&game.frame_allocator)) {
        render_mesh_free(game.plant_meshes[0]);
        PlantTemplate template = game.plant_templates[0] = plant_generate();
        PlantMesh mesh = plant_meshify(&template, (Allocator*)&game.frame_allocator);
        game.plant_meshes[0] = render_mesh_create(slice_u8(mesh.vertices), slice_u8(mesh.indices), sizeof(PlantInstance));
    }

    game_update_player();
    game_update_collision();
    game_update_input();
}

void game_init(void) {
    game.frame_allocator = bump_allocator_create();
    game.player.pos.y = 1.0f;
    game.player.pos.z = -0.5f;
    game.player.forward.z = 1.0f;
    game.player.current_planet = 0;

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

        PlantTemplate plant = game.plant_templates[0] = plant_generate();
        PlantMesh mesh = plant_meshify(&plant, (Allocator*)&game.frame_allocator);
        game.plant_meshes[0] = render_mesh_create(slice_u8(mesh.vertices), slice_u8(mesh.indices), sizeof(PlantInstance));
    }

    game.n_planets = 0;
    {
        Planet planet = {
            .gravity = -2.0f,
            .radius = 1.0f,
            .pos = { .x = 0.0f, .y = 0.0f, .z = 0.0f },
            .mesh = 0
        };

        for (u32 i = 0; i < 100; i++)
        {
            vec3s pos = random_on_sphere();
            planet.plants[planet.n_plants++] = (Plant) {
                .pos = glms_vec3_scale(pos, planet.radius),
                .up = pos,
                .scale = mrw_random_f32(1.0, 3.0) * 0.03,
                .mesh = 0
            };
        }

        game.planets[game.n_planets++] = planet;

        game.planets[game.n_planets++] = (Planet){
            .gravity = -2.0f,
            .radius = 2.0f,
            .pos = { .x = 5.0f, .y = 5.0f, .z = 5.0f },
            .mesh = 0
        };

        PlanetMesh mesh = planet_meshify((Allocator*)&game.frame_allocator);
        game.planet_meshes[0] = render_mesh_create(slice_u8(mesh.vertices), slice_u8(mesh.indices), sizeof(PlanetInstance));
    }
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

void render_upload(void) {
    mat4s proj = glms_perspective(to_rad(80.0f), (f32)renderer.width / (f32)renderer.height, 0.01f, 1000.0f);
    glm_mat4_copy(
        glms_mat4_mul(proj, game.player.view).raw,
        renderer.shader_data.data.camera_matrix
    );

    wgpuQueueWriteBuffer(renderer.queue, renderer.shader_data.buffer, 0, &renderer.shader_data.data, sizeof(renderer.shader_data.data));

    array_for_each(game.planet_meshes, m, Mesh){
        m->n_instances = 0;
    }
    array_for_each(game.plant_meshes, m, Mesh){
        m->n_instances = 0;
    }

    static f32 s = 1.0f;

    slider("label, ", &s, 0.0, 1.0, (Allocator*)&game.frame_allocator);

    for (u32 i = 0; i < game.n_planets; i++) {
        Planet* planet = &game.planets[i];

        Mesh* planet_mesh = &game.planet_meshes[planet->mesh];
        PlanetInstance instance = {
            .radius = game.planets[i].radius,
            .pos = game.planets[i].pos,
        };
        wgpuDeviceQueueWriteDynamicBuffer(renderer.device, renderer.queue, &planet_mesh->instance_buffer, slice_u8_one(&instance), ++planet_mesh->n_instances);
        for (u32 j = 0; j < planet->n_plants; j++) {
            Plant* plant = &planet->plants[j];

            Mesh* mesh = &game.plant_meshes[plant->mesh];
            mat4s mat = basis_from_up(plant->up, GLMS_XUP);
            mat = glms_translated(mat, plant->pos);
            PlantInstance instance = {
                .mat = glms_scale(mat, glms_vec3_scale(GLMS_VEC3_ONE, plant->scale * s)),
            };
            wgpuDeviceQueueWriteDynamicBuffer(renderer.device, renderer.queue, &mesh->instance_buffer, slice_u8_one(&instance), mesh->n_instances++);
        }
    }
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
    if (game.dt_accum > 0.3f) {
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

        game_update();
    }

    render_upload();

    render_render(
        (MeshSlice)slice_to(game.plant_meshes, 1),
        (MeshSlice)slice_to(game.planet_meshes, 1)
    );

    bump_allocator_reset(&game.frame_allocator);
}

#endif // GAME
