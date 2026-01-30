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

struct {
    u32 width, height;

    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurface surface;
    WGPUTextureFormat surface_format;

    WGPURenderPipeline pipeline;

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

    WGPUBuffer vertex_buffer;
    WGPUBuffer index_buffer;

#ifndef __EMSCRIPTEN__
    GLFWwindow* window;
#endif

    RippleContext ripple_context;
} renderer;

struct {
    f32 prev_time;
    f32 dt_accum;
    f32 avg_fps;
    u32 dt_n_samples;

    BumpAllocator frame_allocator;
} game;

void render_debug_info(void)
{
    RIPPLE( FORM( .width = PERCENT(1.0f, SVT_RELATIVE_CHILD), .height = PERCENT(1.0f, SVT_RELATIVE_CHILD)), RECTANGLE( .color = { 0x2e2e2e }))
    {
        text(mrw_format("hello! you are running at {} fps.", (Allocator*)&game.frame_allocator, game.avg_fps));
    }
}

void main_loop(void* _)
{
    f32 time =
    #ifdef __EMSCRIPTEN__
        emscripten_get_now() * 0.001;
    #else
        glfwGetTime();
    #endif
    f32 dt = time - game.prev_time;
    game.dt_accum += dt;
    game.dt_n_samples += 1;
    if (game.dt_accum > 0.3f)
    {
        game.avg_fps = (f32)game.dt_n_samples / game.dt_accum;
        game.dt_accum -= 0.3f;
        game.dt_n_samples = 0;
    }
    game.prev_time = time;

    render_debug_info();

    {
        f32 zoom = 5.0f;
        renderer.shader_data.data.camera_position = (vec3s){ .x = sin(time) * 40.0f, .y = 40.0f, .z = cos(time) * 40.0f};
        mat4s proj = glms_ortho(-zoom, +zoom, -zoom, +zoom, 0.1f, 1000.0f);
        mat4s view = glms_lookat(renderer.shader_data.data.camera_position, (vec3s){ .x = 0.0f, .y = 2.0f, .z = 0.0f}, GLMS_YUP );
        glm_mat4_copy(glms_mat4_mul(proj, view).raw, renderer.shader_data.data.camera_matrix);
    }

    wgpuQueueWriteBuffer(renderer.queue, renderer.shader_data.buffer, 0, &renderer.shader_data.data, sizeof(renderer.shader_data.data));

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

    {
        WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(encoder, &(WGPURenderPassDescriptor)
        {
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

        wgpuRenderPassEncoderSetPipeline(render_pass, renderer.pipeline);
        wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, renderer.vertex_buffer, 0, wgpuBufferGetSize(renderer.vertex_buffer));
        wgpuRenderPassEncoderSetIndexBuffer(render_pass, renderer.index_buffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(renderer.index_buffer));
        wgpuRenderPassEncoderSetBindGroup(render_pass, 0, renderer.shader_data.bind_group, 0, nullptr);
        wgpuRenderPassEncoderDrawIndexed(render_pass, array_len(icosahedron_indices), 1, 0, 0, 0);

        wgpuRenderPassEncoderEnd(render_pass);
        wgpuRenderPassEncoderRelease(render_pass);
    }

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

    bump_allocator_reset(&game.frame_allocator);

#ifndef __EMSCRIPTEN__
    wgpuSurfacePresent(renderer.surface);
#endif
}

void init_renderer(void)
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

    FILE *fp = fopen("./res/shader.wgsl", "rb");
    fseek(fp, 0, SEEK_END);
    u64 len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char shader_code[len];
    fread(shader_code, 1, len, fp);
    shader_code[len] = 0;
    fclose(fp);
    WGPUShaderModule shader_module = wgpuDeviceCreateShaderModule(renderer.device, &(WGPUShaderModuleDescriptor) {
        .label = WEBGPU_STR("main pipeline descriptor"),
        .nextInChain = (WGPUChainedStruct*)&(WGPUShaderSourceWGSL)
        {
            .chain.sType = WGPUSType_ShaderSourceWGSL,
            .code = WEBGPU_STR(shader_code)
        }
    });

    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(renderer.device, &(WGPUPipelineLayoutDescriptor) {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = (WGPUBindGroupLayout[]) { renderer.shader_data.layout }
    });

    renderer.pipeline = wgpuDeviceCreateRenderPipeline(renderer.device, &(WGPURenderPipelineDescriptor) {
        .label = WEBGPU_STR("main pipeline"),
        .layout = layout,
        .vertex = {
            .module = shader_module,
            .entryPoint = WEBGPU_STR("vs_main"),
            .bufferCount = 1,
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

    array_for_each(icosahedron_vertices, v)
    {
        v->position = v->normal = glms_vec3_normalize(v->position);
    }

    renderer.vertex_buffer = wgpuDeviceCreateBuffer(renderer.device, &(WGPUBufferDescriptor) {
        .size = array_size(icosahedron_vertices),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex
    });
    wgpuQueueWriteBuffer(renderer.queue, renderer.vertex_buffer, 0, &icosahedron_vertices, array_size(icosahedron_vertices));

    renderer.index_buffer = wgpuDeviceCreateBuffer(renderer.device, &(WGPUBufferDescriptor) {
        .size = array_size(icosahedron_indices),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index
    });
    wgpuQueueWriteBuffer(renderer.queue, renderer.index_buffer, 0, &icosahedron_indices, array_size(icosahedron_indices));

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

void init_game(void)
{
    game.frame_allocator = bump_allocator_create();
}

int main(void)
{
    init_renderer();
    init_game();

    renderer.ripple_context = ripple_initialize((RippleBackendRendererConfig){
        .device = renderer.device,
        .queue = renderer.queue,
        .surface_format = renderer.surface_format
    });
    ripple_make_active_context(&renderer.ripple_context);

#ifdef __EMSCRIPTEN__
    ripple_emscripten_register_callbacks(&renderer.ripple_context, "#canvas");
    emscripten_set_main_loop_arg(main_loop, nullptr, 0, true);
#else
    ripple_glfw_register_callbacks(&renderer.ripple_context, renderer.window);
    while (!glfwWindowShouldClose(renderer.window))
    {
        glfwPollEvents();
        main_loop(nullptr);
    };
#endif

    return 1;
}
