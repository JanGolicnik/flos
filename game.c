#ifndef GAME
#define GAME

#include "marrow/marrow.h"
#ifndef UNITY_BUILD
#include "base.c"
#include "mesh.c"
#include "plant.c"
#include "render.c"
#endif

STRUCT(Planet) {
    f32 gravity;
    f32 radius;
    vec3s pos;
};

STRUCT(PlanetMesh) {
    VertexSlice vertices;
    u16Slice indices;
};

typedef enum {
    KEY_UNKNOWN = 0,
    KEY_W = 1,
    KEY_A = 2,
    KEY_S = 3,
    KEY_D = 4,
    KEY_SPACE = 5,
    KEY_SHIFT = 6,
    KEY_CTRL = 7,
    KEY_ESC = 8,
    KEY_M1 = 9,
    KEY_M2 = 9,
    KEY_M3 = 9,

    KEY_LAST,

    KEY_PRESSED = 0,
    KEY_HELD = 1,
    KEY_RELEASED = 2
} Key;

struct {
    f32 prev_time;
    f32 dt;
    f32 dt_accum;
    f32 avg_fps;
    u32 dt_n_samples;

    bool keys[3][KEY_LAST];
    struct {
        f32 dx, dy;
        bool has_lock;
        bool left;
    } mouse;

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

    struct {
        Plant plant;
        usize mesh;
    } plants[1];
    u32 n_plants;
    vec3s plant_positions[1];
    u32 n_plant_positions;

    Planet planets[8];
    u32 n_planets;

    Mesh planet_meshes[1];
    Mesh plant_meshes[1];

    BumpAllocator frame_allocator;
} game;

void game_update_player(void) {
    text(mrw_format("pos: {.2f} {.2f} {.2f}",
        (Allocator *)&game.frame_allocator,
        game.player.pos.x,
        game.player.pos.y,
        game.player.pos.z)
    );

    Planet planet = game.planets[game.player.current_planet];

    game.player.speed = game.keys[KEY_HELD][KEY_SHIFT] ? 2.0f : 1.0f;

    f32 yaw_change = -game.mouse.dx * 0.01;
    game.player.pitch = clamp(game.player.pitch + game.mouse.dy * 0.01, to_rad(-89.0), to_rad(89.0));
    game.player.yaw += yaw_change;
    game.player.local_vel.x = game.keys[KEY_HELD][KEY_A] - game.keys[KEY_HELD][KEY_D];
    game.player.local_vel.z = game.keys[KEY_HELD][KEY_W] - game.keys[KEY_HELD][KEY_S];
    if (game.player.onground) {
        game.player.local_vel.y = max(game.player.local_vel.y, 0.0f);
        if (game.keys[KEY_HELD][KEY_SPACE]) {
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

    if (game.keys[KEY_PRESSED][KEY_M1]) {
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
    if (game.keys[KEY_PRESSED][KEY_ESC]) {
        game.mouse.has_lock = false;
        glfwSetInputMode(renderer.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
#endif

    for (i32 i = 1; i < (i32)KEY_LAST; i++) {
        if (game.keys[KEY_PRESSED][i])
            game.keys[KEY_HELD][i] = true;
        if (game.keys[KEY_RELEASED][i])
            game.keys[KEY_HELD][i] = false;
        game.keys[KEY_PRESSED][i] = false;
        game.keys[KEY_RELEASED][i] = false;
    }

    if (!CURSOR().consumed && CURSOR().left.pressed) {
    #ifdef __EMSCRIPTEN__
        emscripten_request_pointerlock("#canvas", EM_TRUE);
    #else
        glfwSetInputMode(renderer.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        game.mouse.has_lock = true;
    #endif
    }

    text(mrw_format("mousedxdy: {.2f} {.2f}",
        (Allocator *)&game.frame_allocator,
        game.mouse.dx,
        game.mouse.dy
    ));
    game.mouse.dx = 0.0f;
    game.mouse.dy = 0.0f;
}

void subdivide(VertexSlice *vertices, u16 *indices, usize *n_indices) {
    u32 n = *n_indices;
    for (u32 i = 0; i < n; i += 3) {
        u16 i1 = indices[i + 0];
        u16 i3 = indices[i + 1];
        u16 i5 = indices[i + 2];
        Vertex v1 = vertices->start[i1];
        Vertex v3 = vertices->start[i3];
        Vertex v5 = vertices->start[i5];

        Vertex v2 = { .position = glms_vec3_scale(glms_vec3_add(v1.position, v3.position), 0.5f) };
        Vertex v4 = { .position = glms_vec3_scale(glms_vec3_add(v3.position, v5.position), 0.5f) };
        Vertex v6 = { .position = glms_vec3_scale(glms_vec3_add(v5.position, v1.position), 0.5f) };

        u16 i2 = slice_count(*vertices);
        *(vertices->end++) = v2;
        u16 i4 = slice_count(*vertices);
        *(vertices->end++) = v4;
        u16 i6 = slice_count(*vertices);
        *(vertices->end++) = v6;

        indices[i] = i1;
        indices[i + 1] = i2;
        indices[i + 2] = i6;
        indices[(*n_indices)++] = i2;
        indices[(*n_indices)++] = i3;
        indices[(*n_indices)++] = i4;
        indices[(*n_indices)++] = i4;
        indices[(*n_indices)++] = i5;
        indices[(*n_indices)++] = i6;
        indices[(*n_indices)++] = i6;
        indices[(*n_indices)++] = i2;
        indices[(*n_indices)++] = i4;
    }
}

PlanetMesh planet_generate(Allocator* allocator) {
    usize n_vertices_start = array_len(icosahedron_indices) / 3;
    usize n_indices_start = array_len(icosahedron_indices);
    usize n_vertices = n_vertices_start * 6 * 6;
    usize n_indices = n_indices_start * 4 * 4;
    Vertex* vertices = allocator_alloc(allocator, n_vertices * sizeof(Vertex), alignof(Vertex));
    u16* indices = allocator_alloc(allocator, n_indices * sizeof(u16), alignof(u16));
    buf_copy(vertices, icosahedron_vertices, sizeof(icosahedron_vertices));
    buf_copy(indices, icosahedron_indices, sizeof(icosahedron_indices));
    VertexSlice vertex_slice = slice_to(vertices, n_vertices_start);
    subdivide(&vertex_slice, indices, &n_indices_start);
    subdivide(&vertex_slice, indices, &n_indices_start);
    for (u32 i = 0; i < n_vertices; i++) {
        Vertex* v = &vertices[i];
        v->position = v->normal = glms_vec3_normalize(v->position);
    }

    return (PlanetMesh) {
      .vertices = slice_to(vertices, n_vertices),
      .indices = slice_to(indices, n_indices),
    };

    // usize n_vertices_start = array_len(icosahedron_indices) / 3;
    // usize n_indices_start = array_len(icosahedron_indices);
    // // usize n_vertices = n_vertices_start * 6 * 6;
    // // usize n_indices = n_indices_start * 4 * 4;
    // usize n_vertices = n_vertices_start;
    // usize n_indices = n_indices_start;
    // PlanetMesh mesh = {
    //     .vertices = slice_to((Vertex*)allocator_alloc(allocator, n_vertices * sizeof(Vertex), alignof(Vertex)), n_vertices),
    //     .indices = slice_to((u16*)allocator_alloc(allocator, n_indices * sizeof(u16), alignof(u16)), n_indices)
    // };

    // Vertex* vertices = mesh.vertices.start;
    // u16* indices = mesh.indices.start;
    // buf_copy(vertices, icosahedron_vertices, sizeof(icosahedron_vertices));
    // buf_copy(indices, icosahedron_indices, sizeof(icosahedron_indices));

    // // VertexSlice vertex_slice = slice_to(vertices, n_vertices_start);
    // // subdivide(&vertex_slice, indices, &n_indices);
    // // subdivide(&vertex_slice, indices, &n_indices);

    // slice_for_each(mesh.vertices, v) {
    //     v->position = v->normal = glms_vec3_normalize(v->position);
    // }

    // return mesh;
}

Mesh planet_upload_mesh(PlanetMesh planet) {
    Mesh mesh = {
        .vertex_buffer = wgpuDeviceCreateBuffer(
            renderer.device,
            &(WGPUBufferDescriptor){
                .size = slice_size(planet.vertices),
                .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex
            }
        ),
        .index_buffer = wgpuDeviceCreateBuffer(
            renderer.device,
            &(WGPUBufferDescriptor){
                .size = slice_size(planet.indices),
                .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index
            }
        ),
        .instance_buffer = wgpuDeviceCreateDynamicBuffer(
            renderer.device, 1, sizeof(PlanetInstance),
            WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex
        )
    };

    wgpuQueueWriteBuffer(renderer.queue, mesh.vertex_buffer, 0, planet.vertices.start, slice_size(planet.vertices));
    wgpuQueueWriteBuffer(renderer.queue, mesh.index_buffer, 0, planet.indices.start, slice_size(planet.indices));

    return mesh;
}


Mesh plant_upload_mesh(PlantMesh plant) {
    Mesh mesh = {
        .vertex_buffer = wgpuDeviceCreateBuffer(
            renderer.device,
            &(WGPUBufferDescriptor){
                .size = slice_size(plant.vertices),
                .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex
            }
        ),
        .index_buffer = wgpuDeviceCreateBuffer(
            renderer.device,
            &(WGPUBufferDescriptor){
                .size = slice_size(plant.indices),
                .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index
            }
        ),
        .instance_buffer = wgpuDeviceCreateDynamicBuffer(
            renderer.device, 8, sizeof(PlantInstance),
            WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex
        )
    };

    wgpuQueueWriteBuffer(renderer.queue, mesh.vertex_buffer, 0, plant.vertices.start, slice_size(plant.vertices));
    wgpuQueueWriteBuffer(renderer.queue, mesh.index_buffer, 0, plant.indices.start, slice_size(plant.indices));

    return mesh;
}

void mesh_free(Mesh mesh) {
    wgpuBufferRelease(mesh.vertex_buffer);
    wgpuBufferRelease(mesh.index_buffer);
    wgpuDynamicBufferRelease(&mesh.instance_buffer);
}

void game_update(void) {
    text(mrw_format("hello! you are running at {} fps.",
        (Allocator *)&game.frame_allocator,
        game.avg_fps
    ));

    if (slider("hello !", &branch, 0.0f, 2.0f, (Allocator *)&game.frame_allocator)) {
        mesh_free(game.plant_meshes[game.plants[0].mesh]);
        Plant plant = game.plants[0].plant = plant_generate();
        game.plant_meshes[0] = plant_upload_mesh(plant_meshify(&plant, (Allocator*)&game.frame_allocator));
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

    game.n_planets = 0;
    game.planets[game.n_planets++] = (Planet){
        .gravity = -2.0f,
        .radius = 1.0f,
        .pos = { .x = 0.0f, .y = 0.0f, .z = 0.0f }
    };
    game.planets[game.n_planets++] = (Planet){
        .gravity = -2.0f,
        .radius = 2.0f,
        .pos = { .x = 5.0f, .y = 5.0f, .z = 5.0f }
    };

    game.planet_meshes[0] = planet_upload_mesh(planet_generate((Allocator*)&game.frame_allocator));

    {
        FILE *fp = fopen("./res/plant.json", "rb");
        fseek(fp, 0, SEEK_END);
        usize size = ftell(fp);
        rewind(fp);
        char buf[size];
        fread(buf, 1, size, fp);
        fclose(fp);
        s8 file_slice = array_slice(buf);
        PlantConfig config = plant_parse_config(json_parse(file_slice));
        mrw_unused config;

        game.n_plants = 0;
        Plant plant = game.plants[0].plant = plant_generate();
        game.plant_meshes[0] = plant_upload_mesh(plant_meshify(&plant, (Allocator*)&game.frame_allocator));
        game.plants[0].mesh = 0;
        game.n_plants++;

        game.n_plant_positions = 0;
        game.plant_positions[game.n_plant_positions++] = (vec3s){ .y = 1.0f };
    }
}

#endif // GAME
