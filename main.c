#include <float.h>
#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#else
    #include <glfw/glfw3.h>
#endif

#include <marrow/marrow.h>
#include <marrow/allocator.h>
#include <marrow/webgpu_utils.h>
#define RIPPLE_IMPLEMENTATION
#include <ripple/ripple.h>
#include <ripple/ripple_widgets.h>

#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cglm/struct.h>

STRUCT(Vertex)
{
    vec3s position;
    vec3s normal;
};

STRUCT(PlanetInstance)
{
    f32 radius;
    vec3s pos;
};

STRUCT(PlantInstance)
{
    vec3s pos;
    f32 radius;
    f32 height;
};

Vertex icosahedron_vertices[] = {
    { .position = {{ 0.000000, -1.000000,  0.000000 }} },
    { .position = {{ 0.723600, -0.447215,  0.525720 }} },
    { .position = {{ -0.276385, -0.447215,  0.850640 }} },
    { .position = {{ -0.894425, -0.447215,  0.000000 }} },
    { .position = {{ -0.276385, -0.447215, -0.850640 }} },
    { .position = {{ 0.723600, -0.447215, -0.525720 }} },
    { .position = {{ 0.276385,  0.447215,  0.850640 }} },
    { .position = {{ -0.723600,  0.447215,  0.525720 }} },
    { .position = {{ -0.723600,  0.447215, -0.525720 }} },
    { .position = {{ 0.276385,  0.447215, -0.850640 }} },
    { .position = {{ 0.894425,  0.447215,  0.000000 }} },
    { .position = {{ 0.000000,  1.000000,  0.000000 }} },
};

u16 icosahedron_indices[] = {
    0, 1, 2,
    1, 0, 5,
    0, 2, 3,
    0, 3, 4,
    0, 4, 5,
    1, 5, 10,
    2, 1, 6,
    3, 2, 7,
    4, 3, 8,
    5, 4, 9,
    1, 10, 6,
    2, 6, 7,
    3, 7, 8,
    4, 8, 9,
    5, 9, 10,
    6, 10, 11,
    7, 6, 11,
    8, 7, 11,
    9, 8, 11,
    10, 9, 11,
};

Vertex prism_vertices[] = {
    { .position = {{ -0.5f, -0.5f, -0.288675f }}, .normal   = {{ -0.866025f, 0.0f, -0.5f }} },
    { .position = {{  0.5f, -0.5f, -0.288675f }}, .normal   = {{  0.866025f, 0.0f, -0.5f }} },
    { .position = {{  0.0f, -0.5f,  0.577350f }}, .normal   = {{  0.0f, 0.0f,  1.0f }} },
    { .position = {{ -0.5f,  0.5f, -0.288675f }}, .normal   = {{ -0.866025f, 0.0f, -0.5f }} },
    { .position = {{  0.5f,  0.5f, -0.288675f }}, .normal   = {{  0.866025f, 0.0f, -0.5f }} },
    { .position = {{  0.0f,  0.5f,  0.577350f }}, .normal   = {{  0.0f, 0.0f,  1.0f }} },
};

u16 prism_indices[] = {
    0, 1, 4,
    0, 4, 3,
    1, 2, 5,
    1, 5, 4,
    2, 0, 3,
    2, 3, 5,
};

struct {
    u32 width, height;

    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurface surface;
    WGPUTextureFormat surface_format;

    struct {
        WGPUBindGroupLayout layout;
        WGPUBindGroup bind_group;
        WGPUBuffer buffer;
        struct {
            mat4 camera_matrix;
            vec3s camera_position;
            f32 time;
        } data;
    } shader_data;

    struct {
        WGPUTexture texture;
        WGPUTextureView view;
        WGPUExtent3D extent;
    } depth;

    struct {
        WGPURenderPipeline pipeline;
        WGPUBuffer vertex_buffer;
        WGPUBuffer index_buffer;
        WGPUDynamicBuffer instance_buffer;
    } planets;

    struct {
        WGPURenderPipeline pipeline;
        WGPUBuffer vertex_buffer;
        WGPUBuffer index_buffer;
        WGPUDynamicBuffer instance_buffer;
    } plants;

#ifndef __EMSCRIPTEN__
    GLFWwindow* window;
#endif
    RippleContext ripple_context;
} renderer;

STRUCT(Planet)
{
    f32 gravity;
    f32 radius;
    vec3s pos;
};

STRUCT(Plant)
{
    vec3s pos;
    f32 radius;
    f32 height;
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

    Plant plants[1];
    u32 n_plants;

    Planet planets[8];
    u32 n_planets;

    BumpAllocator frame_allocator;
} game;

void subdivide(VertexSlice* vertices, u16* indices, u32* n_indices);

void game_update_player(void)
{
    Planet planet = game.planets[game.player.current_planet];

    game.player.speed = game.keys[KEY_HELD][KEY_SHIFT] ? 2.0f : 1.0f;

    f32 yaw_change = -game.mouse.dx * 0.01;
    game.player.pitch = clamp(game.player.pitch + game.mouse.dy * 0.01, to_rad(-89.0), to_rad(89.0));
    game.player.yaw += yaw_change;
    game.player.local_vel.x = game.keys[KEY_HELD][KEY_A] - game.keys[KEY_HELD][KEY_D];
    game.player.local_vel.z = game.keys[KEY_HELD][KEY_W] - game.keys[KEY_HELD][KEY_S];
    if (game.player.onground)
    {
        game.player.local_vel.y = max(game.player.local_vel.y, 0.0f);
        if (game.keys[KEY_HELD][KEY_SPACE]) game.player.local_vel.y = 1.0f;
    }
    else
    {
        game.player.local_vel.y += planet.gravity * game.dt;
    }

    vec3s target_up = glms_normalize(glms_vec3_sub(game.player.pos, planet.pos));
    vec3s up = game.player.up = glms_normalize(glms_vec3_lerp(game.player.up, target_up, 1.0f - expf(-10.0f * game.dt)));
    mat4s yaw_mat = glms_rotate_make(yaw_change, up);
    vec3s forward = game.player.forward;
    forward = glms_vec3_sub(forward, glms_vec3_scale(up, glms_dot(forward, up)));
    forward = glms_vec3_rotate_m4(yaw_mat, forward);
    vec3s right = glms_cross(up, forward);
    mat4s pitch_mat = glms_rotate_make(game.player.pitch, right);

    game.player.vel = glms_vec3_add(
        glms_normalize(glms_vec3_add(
            glms_vec3_scale(right, game.player.local_vel.x),
            glms_vec3_scale(forward, game.player.local_vel.z)
        )),
        glms_vec3_scale(target_up, game.player.local_vel.y)
    );
    game.player.pos = glms_vec3_add(game.player.pos, glms_vec3_scale(game.player.vel, game.player.speed * game.dt));

    game.player.forward = forward;
    forward = glms_normalize(glms_vec3_rotate_m4(pitch_mat, forward));
    game.player.view = glms_look(game.player.pos, forward, up);

    game.player.onground = glms_vec3_norm(glms_vec3_sub(game.player.pos, planet.pos)) <= planet.radius + 0.1f;

    text(mrw_format("vely: {.3f}", (Allocator*)&game.frame_allocator, game.player.local_vel.y));

    if (game.keys[KEY_PRESSED][KEY_M1]) {
        f32 t1, t2;
        f32 closest_dist = 99999.0f;
        for (u32 i = 0; i < game.n_planets; i++) {
            Planet p = game.planets[i];
            if (glms_ray_sphere(game.player.pos, forward, (vec4s){.x = p.pos.x, .y = p.pos.y, .z = p.pos.z, .w = p.radius}, &t1, &t2)) {
                if (t1 > 0.0f && t1 < closest_dist) {
                    game.player.current_planet = i;
                    closest_dist = t1;
                }
            }
        }
    }

}

void game_update_collision(void)
{
    for (u32 i = 0; i < game.n_planets; i++)
    {
        Planet p = game.planets[i];
        vec3s to = glms_vec3_sub(game.player.pos, p.pos);
        f32 dist = glms_vec3_norm(to);
        if (dist < p.radius) game.player.pos = glms_vec3_add(p.pos, glms_vec3_scale(glms_vec3_divs(to, dist), p.radius));
    }
}

void game_update_input(void)
{
    for (i32 i = 1; i < (i32)KEY_LAST; i++)
    {
        if (game.keys[KEY_PRESSED][i])
            game.keys[KEY_HELD][i] = true;
        if (game.keys[KEY_RELEASED][i])
            game.keys[KEY_HELD][i] = false;
        game.keys[KEY_PRESSED][i] = false;
        game.keys[KEY_RELEASED][i] = false;
    }

    if (!CURSOR().consumed && CURSOR().left.pressed)
    {
        emscripten_request_pointerlock("#canvas", EM_TRUE);
    }

    game.mouse.dx = 0.0f;
    game.mouse.dy = 0.0f;
}

void game_update(void)
{
    text(mrw_format("hello! you are running at {} fps.", (Allocator*)&game.frame_allocator, game.avg_fps));
    game_update_player();
    game_update_collision();
    game_update_input();
}

void render_prepare(void)
{
    // shader data
    mat4s proj = glms_perspective(to_rad(90.0f), 1.0f, 0.01f, 1000.0f);
    glm_mat4_copy(glms_mat4_mul(proj, game.player.view).raw, renderer.shader_data.data.camera_matrix);

    wgpuQueueWriteBuffer(renderer.queue, renderer.shader_data.buffer, 0, &renderer.shader_data.data, sizeof(renderer.shader_data.data));

    // planets
    {
        wgpuDeviceDynamicBufferEnsure(renderer.device, &renderer.planets.instance_buffer, 8);
        PlanetInstance instances[game.n_planets];
        for (u32 i = 0; i < game.n_planets; i++)
        {
            instances[i].radius = game.planets[i].radius;
            instances[i].pos = game.planets[i].pos;
        }
        wgpuQueueWriteBuffer(renderer.queue,  renderer.planets.instance_buffer.data, 0, &instances, array_size(instances));
    }

    // plants
    {
        wgpuDeviceDynamicBufferEnsure(renderer.device, &renderer.plants.instance_buffer, 8);
        PlantInstance instances[game.n_plants];
        for (u32 i = 0; i < game.n_plants; i++)
        {
            instances[i].pos = game.plants[i].pos;
            instances[i].radius = game.plants[i].radius;
            instances[i].height = game.plants[i].height;
        }
        wgpuQueueWriteBuffer(renderer.queue,  renderer.plants.instance_buffer.data, 0, &instances, array_size(instances));
    }
}

void render_planets(WGPUCommandEncoder encoder, WGPUTextureView surface_texture_view)
{
    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(encoder, &(WGPURenderPassDescriptor) {
        .colorAttachmentCount = 1,
        .colorAttachments = &(WGPURenderPassColorAttachment)
        {
            .view = surface_texture_view,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = (WGPUColor){ 84.0f / 255.0f, 119.0f / 255.0f, 146.0f / 255.0f, 1.0f },
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED
        },
        .depthStencilAttachment = &(WGPURenderPassDepthStencilAttachment) {
            .view = renderer.depth.view,
            .depthLoadOp = WGPULoadOp_Clear,
            .depthClearValue = 1.0f,
            .depthStoreOp = WGPUStoreOp_Store
        }
    });
    wgpuRenderPassEncoderSetPipeline(render_pass, renderer.planets.pipeline);
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, renderer.planets.vertex_buffer, 0, wgpuBufferGetSize(renderer.planets.vertex_buffer));
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 1, renderer.planets.instance_buffer.data, 0, wgpuBufferGetSize(renderer.planets.instance_buffer.data));
    wgpuRenderPassEncoderSetIndexBuffer(render_pass, renderer.planets.index_buffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(renderer.planets.index_buffer));
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, renderer.shader_data.bind_group, 0, nullptr);
    wgpuRenderPassEncoderDrawIndexed(render_pass, wgpuBufferGetSize(renderer.planets.index_buffer) / sizeof(icosahedron_indices[0]), game.n_planets, 0, 0, 0);

    wgpuRenderPassEncoderEnd(render_pass);
    wgpuRenderPassEncoderRelease(render_pass);
}

void render_plants(WGPUCommandEncoder encoder, WGPUTextureView surface_texture_view)
{
    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(encoder, &(WGPURenderPassDescriptor) {
        .colorAttachmentCount = 1,
        .colorAttachments = &(WGPURenderPassColorAttachment)
        {
            .view = surface_texture_view,
            .loadOp = WGPULoadOp_Load,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = (WGPUColor){ 84.0f / 255.0f, 119.0f / 255.0f, 146.0f / 255.0f, 1.0f },
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED
        },
        .depthStencilAttachment = &(WGPURenderPassDepthStencilAttachment) {
            .view = renderer.depth.view,
            .depthLoadOp = WGPULoadOp_Load,
            .depthClearValue = 1.0f,
            .depthStoreOp = WGPUStoreOp_Store
        }
    });
    wgpuRenderPassEncoderSetPipeline(render_pass, renderer.plants.pipeline);
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, renderer.plants.vertex_buffer, 0, wgpuBufferGetSize(renderer.plants.vertex_buffer));
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 1, renderer.plants.instance_buffer.data, 0, wgpuBufferGetSize(renderer.plants.instance_buffer.data));
    wgpuRenderPassEncoderSetIndexBuffer(render_pass, renderer.plants.index_buffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(renderer.plants.index_buffer));
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, renderer.shader_data.bind_group, 0, nullptr);
    wgpuRenderPassEncoderDrawIndexed(render_pass, wgpuBufferGetSize(renderer.plants.index_buffer) / sizeof(prism_indices[0]), game.n_plants, 0, 0, 0);

    wgpuRenderPassEncoderEnd(render_pass);
    wgpuRenderPassEncoderRelease(render_pass);
}

void render(void)
{
    render_prepare();

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(renderer.device, &(WGPUCommandEncoderDescriptor) { .label = WEBGPU_STR("Command encoder")  });

    WGPUSurfaceTexture surface_texture; wgpuSurfaceGetCurrentTexture(renderer.surface, &surface_texture);
    WGPUTextureView surface_texture_view = wgpuTextureCreateView(surface_texture.texture, &(WGPUTextureViewDescriptor){
        .label = WEBGPU_STR("Surface texture view"),
        .format = wgpuTextureGetFormat(surface_texture.texture),
        .dimension = WGPUTextureViewDimension_2D,
        .mipLevelCount = 1,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
    });

    render_planets(encoder, surface_texture_view);
    render_plants(encoder, surface_texture_view);

    ripple_submit(&renderer.ripple_context, renderer.width, renderer.height, (RippleRenderData)
    {
        .queue = renderer.queue,
        .device = renderer.device,
        .encoder = encoder,
        .texture_view = surface_texture_view
    });

    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &(WGPUCommandBufferDescriptor){ .label = WEBGPU_STR("Command buffer") });
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(renderer.queue, 1, &command);
    wgpuCommandBufferRelease(command);

    wgpuTextureViewRelease(surface_texture_view);
    wgpuTextureRelease(surface_texture.texture);

    #ifndef __EMSCRIPTEN__
        wgpuSurfacePresent(renderer.surface);
    #endif
}

void main_loop(void* _)
{
    f32 time =
    #ifdef __EMSCRIPTEN__
        emscripten_get_now() * 0.001;
    #else
        glfwGetTime();
    #endif
    game.dt = time - game.prev_time;
    game.dt_accum += game.dt; game.dt_n_samples += 1;
    if (game.dt_accum > 0.3f)
    {
        game.avg_fps = (f32)game.dt_n_samples / game.dt_accum;
        game.dt_accum -= 0.3f;
        game.dt_n_samples = 0;
    }
    game.prev_time = time;

    RIPPLE( FORM( .width = PERCENT(1.0f, SVT_RELATIVE_CHILD), .height = PERCENT(1.0f, SVT_RELATIVE_CHILD)), RECTANGLE( .color = { 0x2e2e2e }))
    {
        game_update();
    }
    render();
    bump_allocator_reset(&game.frame_allocator);
}

void render_init_planets(void)
{
    WGPUShaderModule shader_module = load_shader_module_from_file(renderer.device, "./res/planet_shader.wgsl");

    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(renderer.device, &(WGPUPipelineLayoutDescriptor) {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = (WGPUBindGroupLayout[]) { renderer.shader_data.layout }
    });

    renderer.planets.pipeline = wgpuDeviceCreateRenderPipeline(renderer.device, &(WGPURenderPipelineDescriptor) {
        .label = WEBGPU_STR("planet shader"),
        .layout = layout,
        .vertex = {
            .module = shader_module,
            .entryPoint = WEBGPU_STR("vs_main"),
            .bufferCount = 2,
            .buffers = (WGPUVertexBufferLayout[]) {
                {
                    .arrayStride = sizeof(Vertex),
                    .stepMode = WGPUVertexStepMode_Vertex,
                    .attributeCount = 2,
                    .attributes = (WGPUVertexAttribute[]) {
                        {
                            .shaderLocation = 0,
                            .format = WGPUVertexFormat_Float32x3,
                            .offset = 0
                        },
                        {
                            .shaderLocation = 1,
                            .format = WGPUVertexFormat_Float32x3,
                            .offset = offsetof(Vertex, normal)
                        }
                    }
                },
                {
                    .arrayStride = sizeof(PlanetInstance),
                    .stepMode = WGPUVertexStepMode_Instance,
                    .attributeCount = 2,
                    .attributes = (WGPUVertexAttribute[]) {
                        {
                            .shaderLocation = 2,
                            .format = WGPUVertexFormat_Float32,
                            .offset = offsetof(PlanetInstance, radius)
                        },
                        {
                            .shaderLocation = 3,
                            .format = WGPUVertexFormat_Float32x3,
                            .offset = offsetof(PlanetInstance, pos)
                        }
                    }
                }
            }
        },
        .fragment = &(WGPUFragmentState) {
            .module = shader_module,
            .entryPoint = WEBGPU_STR("fs_main"),
            .targetCount = 1,
            .targets = &(WGPUColorTargetState) {
                .format = renderer.surface_format,
                .writeMask = WGPUColorWriteMask_All,
                .blend = &wgpu_normal_blend_state
            }
        },
        .multisample = {
            .count = 1,
            .mask = ~0u
        },
        .primitive = {
            .topology = WGPUPrimitiveTopology_TriangleList,
            .stripIndexFormat = WGPUIndexFormat_Undefined,
            .frontFace = WGPUFrontFace_CCW,
            .cullMode = WGPUCullMode_None
        },
        .depthStencil = &(WGPUDepthStencilState) {
            .format = WGPUTextureFormat_Depth24Plus,
            .depthWriteEnabled = true,
            .depthCompare = WGPUCompareFunction_Less,
            .stencilFront = wgpu_stencil_keep_always,
            .stencilBack = wgpu_stencil_keep_always,
            .stencilReadMask = ~0,
            .stencilWriteMask = ~0,
        }
    });

    wgpuPipelineLayoutRelease(layout);
    wgpuShaderModuleRelease(shader_module);

    u32 n_vertices = array_len(icosahedron_indices) / 3;
    u32 n_indices = array_len(icosahedron_indices);
    Vertex vertices[n_vertices * 6 * 6];
    VertexSlice vertex_slice = slice_to(vertices, n_vertices);
    u16 indices[n_indices * 4 * 4];
    buf_copy(vertices, icosahedron_vertices, sizeof(icosahedron_vertices));
    buf_copy(indices, icosahedron_indices, sizeof(icosahedron_indices));
    subdivide(&vertex_slice, indices, &n_indices);
    subdivide(&vertex_slice, indices, &n_indices);
    array_for_each(vertices, v) v->position = v->normal = glms_vec3_normalize(v->position);

    renderer.planets.vertex_buffer = wgpuDeviceCreateBuffer(renderer.device, &(WGPUBufferDescriptor) {
        .size = array_size(vertices),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex
    });
    wgpuQueueWriteBuffer(renderer.queue, renderer.planets.vertex_buffer, 0, &vertices, array_size(vertices));

    renderer.planets.index_buffer = wgpuDeviceCreateBuffer(renderer.device, &(WGPUBufferDescriptor) {
        .size = array_size(indices),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index
    });
    wgpuQueueWriteBuffer(renderer.queue, renderer.planets.index_buffer, 0, &indices, array_size(indices));

    renderer.planets.instance_buffer = wgpuDeviceCreateDynamicBuffer(renderer.device, 8, sizeof(PlanetInstance), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex);
}

void render_init_plants(void)
{
    WGPUShaderModule shader_module = load_shader_module_from_file(renderer.device, "./res/plant_shader.wgsl");

    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(renderer.device, &(WGPUPipelineLayoutDescriptor) {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = (WGPUBindGroupLayout[]) { renderer.shader_data.layout }
    });

    renderer.plants.pipeline = wgpuDeviceCreateRenderPipeline(renderer.device, &(WGPURenderPipelineDescriptor) {
        .label = WEBGPU_STR("plant shader"),
        .layout = layout,
        .vertex = {
            .module = shader_module,
            .entryPoint = WEBGPU_STR("vs_main"),
            .bufferCount = 2,
            .buffers = (WGPUVertexBufferLayout[]) {
                {
                    .arrayStride = sizeof(Vertex),
                    .stepMode = WGPUVertexStepMode_Vertex,
                    .attributeCount = 2,
                    .attributes = (WGPUVertexAttribute[]) {
                        {
                            .shaderLocation = 0,
                            .format = WGPUVertexFormat_Float32x3,
                            .offset = 0
                        },
                        {
                            .shaderLocation = 1,
                            .format = WGPUVertexFormat_Float32x3,
                            .offset = offsetof(Vertex, normal)
                        }
                    }
                },
                {
                    .arrayStride = sizeof(PlantInstance),
                    .stepMode = WGPUVertexStepMode_Instance,
                    .attributeCount = 3,
                    .attributes = (WGPUVertexAttribute[]) {
                        {
                            .shaderLocation = 2,
                            .format = WGPUVertexFormat_Float32,
                            .offset = offsetof(PlantInstance, pos)
                        },
                        {
                            .shaderLocation = 3,
                            .format = WGPUVertexFormat_Float32,
                            .offset = offsetof(PlantInstance, radius)
                        },
                        {
                            .shaderLocation = 4,
                            .format = WGPUVertexFormat_Float32,
                            .offset = offsetof(PlantInstance, height)
                        }
                    }
                }
            }
        },
        .fragment = &(WGPUFragmentState) {
            .module = shader_module,
            .entryPoint = WEBGPU_STR("fs_main"),
            .targetCount = 1,
            .targets = &(WGPUColorTargetState) {
                .format = renderer.surface_format,
                .writeMask = WGPUColorWriteMask_All,
                .blend = &wgpu_normal_blend_state
            }
        },
        .multisample = {
            .count = 1,
            .mask = ~0u
        },
        .primitive = {
            .topology = WGPUPrimitiveTopology_TriangleList,
            .stripIndexFormat = WGPUIndexFormat_Undefined,
            .frontFace = WGPUFrontFace_CCW,
            .cullMode = WGPUCullMode_None
        },
        .depthStencil = &(WGPUDepthStencilState) {
            .format = WGPUTextureFormat_Depth24Plus,
            .depthWriteEnabled = true,
            .depthCompare = WGPUCompareFunction_Less,
            .stencilFront = wgpu_stencil_keep_always,
            .stencilBack = wgpu_stencil_keep_always,
            .stencilReadMask = ~0,
            .stencilWriteMask = ~0,
        }
    });

    wgpuPipelineLayoutRelease(layout);
    wgpuShaderModuleRelease(shader_module);

    renderer.plants.vertex_buffer = wgpuDeviceCreateBuffer(renderer.device, &(WGPUBufferDescriptor) {
        .size = array_size(prism_vertices),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex
    });
    wgpuQueueWriteBuffer(renderer.queue, renderer.plants.vertex_buffer, 0, &prism_vertices, array_size(prism_vertices));

    renderer.plants.index_buffer = wgpuDeviceCreateBuffer(renderer.device, &(WGPUBufferDescriptor) {
        .size = array_size(prism_indices),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index
    });
    wgpuQueueWriteBuffer(renderer.queue, renderer.plants.index_buffer, 0, &prism_indices, array_size(prism_indices));

    renderer.plants.instance_buffer = wgpuDeviceCreateDynamicBuffer(renderer.device, 8, sizeof(PlantInstance), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex);
}

void render_init(void)
{
#ifndef __EMSCRIPTEN__
    glfwInit();
    renderer.window = glfwCreateWindow(800, 800, "hello!", nullptr, nullptr);
#endif

    renderer.instance = wgpuCreateInstance(nullptr);
    if (!renderer.instance) mrw_error("Failed to create WebGPU instance.");
    else mrw_debug("Successfully created the WebGPU instance!");

    renderer.adapter = get_adapter(renderer.instance, (WGPURequestAdapterOptions){ .powerPreference = WGPUPowerPreference_HighPerformance });
    if (!renderer.adapter) mrw_error("Failed to get the adapter!");
    else mrw_debug("Successfully got the adapter!");

    renderer.device = get_device(renderer.adapter);
    if (!renderer.device) mrw_error("Failed to get the device!");
    else mrw_debug("Succesfully got the device!");

    renderer.queue = wgpuDeviceGetQueue(renderer.device);
    if (!renderer.queue) mrw_error("Failed to get the queue!");
    else mrw_debug("Succesfully got the queue!");

    renderer.surface = get_surface(renderer.instance
        #ifndef __EMSCRIPTEN__
        , renderer.window
        #endif
    );
    if (!renderer.surface) mrw_error("Failed to get the surface");
    else mrw_debug("Succefully got the surface!");

    WGPUSurfaceCapabilities caps;
    wgpuSurfaceGetCapabilities(renderer.surface, renderer.adapter, &caps);
    renderer.surface_format = caps.formats[0];
    mrw_debug("Preferred surface format is {}", (u32)renderer.surface_format);

    renderer.width = 800;
    renderer.height = 800;

    wgpuSurfaceConfigure(renderer.surface, &(WGPUSurfaceConfiguration) {
        .width = renderer.width,
        .height = renderer.height,
        .format = renderer.surface_format,
        .usage = WGPUTextureUsage_RenderAttachment,
        .device = renderer.device,
        .presentMode = WGPUPresentMode_Fifo
    });

    renderer.shader_data.buffer = wgpuDeviceCreateBuffer(renderer.device, &(WGPUBufferDescriptor) {
        .size = sizeof(renderer.shader_data.data),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform
    });

    {
        renderer.shader_data.layout = wgpuDeviceCreateBindGroupLayout(renderer.device, &(WGPUBindGroupLayoutDescriptor) {
            .entryCount = 1,
            .entries = (WGPUBindGroupLayoutEntry[]) {
                {
                    .binding = 0,
                    .visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment,
                    .buffer = {
                        .type = WGPUBufferBindingType_Uniform,
                        .minBindingSize = sizeof(renderer.shader_data.data)
                    }
                }
            }
        });

        renderer.shader_data.bind_group = wgpuDeviceCreateBindGroup(renderer.device, &(WGPUBindGroupDescriptor) {
            .layout = renderer.shader_data.layout,
            .entryCount = 1,
            .entries = (WGPUBindGroupEntry[]) {
                {
                    .binding = 0,
                    .buffer = renderer.shader_data.buffer,
                    .offset = 0,
                    .size = sizeof(renderer.shader_data.data)
                }
            }
        });
    }

    render_init_planets();
    render_init_plants();

    renderer.depth.extent = (WGPUExtent3D){ .width = renderer.width, .height = renderer.height, .depthOrArrayLayers = 1 };
    renderer.depth.texture = wgpuDeviceCreateTexture(renderer.device, &(WGPUTextureDescriptor){
            .label = WEBGPU_STR("depth texture"),
            .size = renderer.depth.extent,
            .format = WGPUTextureFormat_Depth24Plus,
            .usage = WGPUTextureUsage_RenderAttachment,
            .dimension = WGPUTextureDimension_2D,
            .mipLevelCount = 1,
            .sampleCount = 1
        });
    renderer.depth.view = wgpuTextureCreateView(renderer.depth.texture, &(WGPUTextureViewDescriptor){
            .format = WGPUTextureFormat_Depth24Plus,
            .dimension = WGPUTextureViewDimension_2D,
            .mipLevelCount = 1,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_DepthOnly,
        });
}

void game_init(void)
{
    game.frame_allocator = bump_allocator_create();
    game.player.pos.y = 1.0f;
    game.player.forward.z = 1.0f;
    game.player.current_planet = 0;

    game.n_planets = 0;
    game.planets[game.n_planets++] = (Planet){
        .gravity = -2.0f ,
        .radius = 1.0f,
        .pos = { .x = 0.0f, .y = 0.0f, .z = 0.0f }
    };
    game.planets[game.n_planets++] = (Planet){
        .gravity = -2.0f ,
        .radius = 2.0f,
        .pos = { .x = 5.0f, .y = 5.0f, .z = 5.0f }
    };

    game.n_plants = 0;
    game.plants[game.n_plants++] = (Plant)
    {
        .pos = { .x = 0.0f, .y = 1.0f, .z = 0.0f },
        .radius = .1f,
        .height = 3.0f
    };
}

bool on_mouse(int type, const EmscriptenMouseEvent* event, void* user)
{
    if (game.mouse.has_lock) {
        if (type == EMSCRIPTEN_EVENT_MOUSEMOVE) {
            game.mouse.dx = (f32)event->movementX;
            game.mouse.dy = (f32)event->movementY;
        }
        else if (type == EMSCRIPTEN_EVENT_MOUSEDOWN) {
            game.keys[KEY_PRESSED][KEY_M1 + event->button] = true;
        }
        else if (type == EMSCRIPTEN_EVENT_MOUSEUP) {
            game.keys[KEY_RELEASED][KEY_M1 + event->button] = true;
        }
        return true;
    }
    return ripple_emscripten_mouse_callback(type, event, user);
}

bool on_key(int type, const EmscriptenKeyboardEvent* event, void* user)
{
    if (!game.mouse.has_lock) return false;
    if (event->repeat) return true;
    if (type != EMSCRIPTEN_EVENT_KEYDOWN && type != EMSCRIPTEN_EVENT_KEYUP) return false;
    i32 state = type == EMSCRIPTEN_EVENT_KEYDOWN ? KEY_PRESSED : KEY_RELEASED;
    Key key = KEY_UNKNOWN;
    if (!str_cmp(event->code, "KeyW")) key = KEY_W;
    else if (!str_cmp(event->code, "KeyA")) key = KEY_A;
    else if (!str_cmp(event->code, "KeyS")) key = KEY_S;
    else if (!str_cmp(event->code, "KeyD")) key = KEY_D;
    else if (!str_cmp(event->code, "Space")) key = KEY_SPACE;
    else if (!str_cmp(event->code, "ShiftLeft")) key = KEY_SHIFT;
    game.keys[state][key] = true;
    return true;
}

bool on_pointerlock_change(int type, const EmscriptenPointerlockChangeEvent* event, void* user)
{
    game.mouse.has_lock = event->isActive;
    return true;
}

void misc_init(void)
{
    renderer.ripple_context = ripple_initialize((RippleBackendRendererConfig){
        .device = renderer.device,
        .queue = renderer.queue,
        .surface_format = renderer.surface_format
    });
    ripple_make_active_context(&renderer.ripple_context);

#ifdef __EMSCRIPTEN__
    ripple_emscripten_register_callbacks(&renderer.ripple_context, "#canvas");
    emscripten_set_mousemove_callback("#canvas", &renderer.ripple_context, EM_TRUE, on_mouse);
    emscripten_set_mousedown_callback("#canvas", &renderer.ripple_context, EM_TRUE, on_mouse);
    emscripten_set_mouseup_callback("#canvas", &renderer.ripple_context, EM_TRUE, on_mouse);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, &renderer.ripple_context, EM_TRUE, on_key);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, &renderer.ripple_context, EM_TRUE, on_key);
    emscripten_set_pointerlockchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, EM_TRUE, on_pointerlock_change);
#else
    ripple_glfw_register_callbacks(&renderer.ripple_context, renderer.window);
#endif
}

int main(void)
{
    render_init();
    game_init();
    misc_init();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(main_loop, nullptr, 0, true);
#else
    while (!glfwWindowShouldClose(renderer.window))
    {
        glfwPollEvents();
        main_loop(nullptr);
    };
#endif

    return 1;
}

void subdivide(VertexSlice* vertices, u16* indices, u32* n_indices)
{
    u32 n = *n_indices;
    for (u32 i = 0; i < n; i+= 3)
    {
        u16 i1 = indices[i + 0];
        u16 i3 = indices[i + 1];
        u16 i5 = indices[i + 2];
        Vertex v1 = vertices->start[i1];
        Vertex v3 = vertices->start[i3];
        Vertex v5 = vertices->start[i5];

        Vertex v2 = { .position = glms_vec3_scale(glms_vec3_add(v1.position, v3.position), 0.5f) };
        Vertex v4 = { .position = glms_vec3_scale(glms_vec3_add(v3.position, v5.position), 0.5f) };
        Vertex v6 = { .position = glms_vec3_scale(glms_vec3_add(v5.position, v1.position), 0.5f) };

        u16 i2 = slice_count(*vertices); *(vertices->end++) = v2;
        u16 i4 = slice_count(*vertices); *(vertices->end++) = v4;
        u16 i6 = slice_count(*vertices); *(vertices->end++) = v6;

        indices[i] = i1; indices[i + 1] = i2; indices[i + 2] = i6;
        indices[(*n_indices)++] = i2; indices[(*n_indices)++] = i3; indices[(*n_indices)++] = i4;
        indices[(*n_indices)++] = i4; indices[(*n_indices)++] = i5; indices[(*n_indices)++] = i6;
        indices[(*n_indices)++] = i6; indices[(*n_indices)++] = i2; indices[(*n_indices)++] = i4;
    }
}
