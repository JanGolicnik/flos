#include "webgpu/webgpu.h"
#define FLOS_RENDER
#include "base.c"

STRUCT(Mesh) {
    WGPUBuffer vertex_buffer;
    WGPUBuffer index_buffer;
    WGPUDynamicBuffer instance_buffer;
    VEKTOR(u8) instance_data;
    u32 shader;
};

STRUCT(AtmospherePlanet) {
    vec3s pos;
    f32 radius;
};

str common_includes[] = {
    sstr("./res/shaders/common.wgsl")
};

typedef GENARR_ITER_ALIAS(Mesh, mesh) MeshIter;

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
            mat4 inv_camera_matrix;
            vec3s camera_position;
            f32 time;
            vec2s res;
            f32 atmosphere_height;
            f32 atmosphere_density;
            f32 atmosphere_falloff;
        } data;
    } shader_data;

    struct {
        WGPUTexture texture;
        WGPUTextureView view;
        WGPUExtent3D extent;
    } depth;

    struct {
        WGPURenderPipeline pipeline;
    } planets;

    struct {
        WGPURenderPipeline pipeline;
    } plants;

    struct {
        WGPUBindGroupLayout layout;
        WGPUBindGroup bind_group;
        WGPUDynamicBuffer buffer;
        WGPURenderPipeline pipeline;
    } atmosphere;

    GENARR(Mesh) meshes;

    RippleContext ripple_context;
} renderer = { 0 };

MeshHandle render_mesh_create(u8Slice vertices, u8Slice indices, usize instance_size, u32 shader) {
    Mesh mesh = (Mesh) {
        .vertex_buffer = wgpuDeviceCreateBufferWithData(renderer.device, renderer.queue, vertices, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex),
        .index_buffer = wgpuDeviceCreateBufferWithData(renderer.device, renderer.queue, indices, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index),
        .instance_buffer = wgpuDeviceCreateDynamicBuffer(renderer.device, 8, instance_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex),
        .shader = shader,
    };
    vektor_init(mesh.instance_data, 1, memory.stable);
    return genarr_add(renderer.meshes, mesh);
}

void render_mesh_re_create(MeshHandle old, u8Slice vertices, u8Slice indices, usize instance_size, u32 shader) {
    Mesh* mesh = genarr_get(renderer.meshes, old);

    wgpuBufferRelease(mesh->vertex_buffer);
    wgpuBufferRelease(mesh->index_buffer);
    wgpuDynamicBufferRelease(&mesh->instance_buffer);

    mesh->vertex_buffer = wgpuDeviceCreateBufferWithData(renderer.device, renderer.queue, vertices, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex);
    mesh->index_buffer = wgpuDeviceCreateBufferWithData(renderer.device, renderer.queue, indices, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index);
    mesh->instance_buffer = wgpuDeviceCreateDynamicBuffer(renderer.device, 8, instance_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex);
    mesh->shader = shader;
}

void render_mesh_free(MeshHandle handle) {
    Mesh* mesh = genarr_get(renderer.meshes, handle);

    wgpuBufferRelease(mesh->vertex_buffer);
    wgpuBufferRelease(mesh->index_buffer);
    wgpuDynamicBufferRelease(&mesh->instance_buffer);
    vektor_free(mesh->instance_data);

    genarr_remove(renderer.meshes, handle);
}

void render_init_planets(void) {
    WGPUShaderModule shader_module = load_shader_module_from_file(renderer.device, "./res/shaders/planet.wgsl", (strSlice)array_slice(common_includes), memory.frame);

    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(renderer.device,
        &(WGPUPipelineLayoutDescriptor) {
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = (WGPUBindGroupLayout[]){
                renderer.shader_data.layout
            }
        }
    );

    renderer.planets.pipeline = wgpuDeviceCreateRenderPipeline(renderer.device,
        &(WGPURenderPipelineDescriptor) {
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
                        .attributeCount = 6,
                        .attributes = (WGPUVertexAttribute[]) {
                            {
                                .shaderLocation = 3,
                                .format = WGPUVertexFormat_Float32x4,
                                .offset = offsetof(PlanetInstance, mat.col[0])
                            },
                            {
                                .shaderLocation = 4,
                                .format = WGPUVertexFormat_Float32x4,
                                .offset = offsetof(PlanetInstance, mat.col[1])
                            },
                            {
                                .shaderLocation = 5,
                                .format = WGPUVertexFormat_Float32x4,
                                .offset = offsetof(PlanetInstance, mat.col[2])
                            },
                            {
                                .shaderLocation = 6,
                                .format = WGPUVertexFormat_Float32x4,
                                .offset = offsetof(PlanetInstance, mat.col[3])
                            },
                            {
                                .shaderLocation = 7,
                                .format = WGPUVertexFormat_Float32,
                                .offset = offsetof(PlanetInstance, shell_t)
                            },
                            {
                                .shaderLocation = 8,
                                .format = WGPUVertexFormat_Float32,
                                .offset = offsetof(PlanetInstance, scale)
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
            .multisample = { .count = 1, .mask = ~0u },
            .primitive = {
                // .topology = WGPUPrimitiveTopology_LineStrip,
                .topology = WGPUPrimitiveTopology_TriangleList,
                // .stripIndexFormat = WGPUIndexFormat_Uint16,
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
        }
    );

    wgpuPipelineLayoutRelease(layout);
    wgpuShaderModuleRelease(shader_module);
}

void render_init_plants(void) {
    WGPUShaderModule shader_module = load_shader_module_from_file(renderer.device, "./res/shaders/plant.wgsl", (strSlice)array_slice(common_includes), memory.frame);

    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(renderer.device, &(WGPUPipelineLayoutDescriptor) {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = (WGPUBindGroupLayout[]){
            renderer.shader_data.layout
        }
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
                    .arrayStride = sizeof(Instance),
                    .stepMode = WGPUVertexStepMode_Instance,
                    .attributeCount = 4,
                    .attributes = (WGPUVertexAttribute[]) {
                        {
                            .shaderLocation = 3,
                            .format = WGPUVertexFormat_Float32x4,
                            .offset = offsetof(Instance, mat.col[0])
                        },
                        {
                            .shaderLocation = 4,
                            .format = WGPUVertexFormat_Float32x4,
                            .offset = offsetof(Instance, mat.col[1])
                        },
                        {
                            .shaderLocation = 5,
                            .format = WGPUVertexFormat_Float32x4,
                            .offset = offsetof(Instance, mat.col[2])
                        },
                        {
                            .shaderLocation = 6,
                            .format = WGPUVertexFormat_Float32x4,
                            .offset = offsetof(Instance, mat.col[3])
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
        .multisample = { .count = 1, .mask = ~0u },
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
}

void render_init_atmosphere(void) {
    WGPUShaderModule shader_module = load_shader_module_from_file(renderer.device, "./res/shaders/atmosphere.wgsl", (strSlice)array_slice(common_includes), memory.frame);

    renderer.atmosphere.layout = wgpuDeviceCreateBindGroupLayout(renderer.device, &(WGPUBindGroupLayoutDescriptor) {
        .entryCount = 2,
        .entries = (WGPUBindGroupLayoutEntry[]) {
            {
                .binding = 0,
                .visibility = WGPUShaderStage_Fragment,
                .buffer.type = WGPUBufferBindingType_ReadOnlyStorage,
            },
            {
                .binding = 1,
                .visibility = WGPUShaderStage_Fragment,
                .texture = {
                    .sampleType = WGPUTextureSampleType_Depth,
                    .viewDimension = WGPUTextureViewDimension_2D,
                },
            }
        }
    });

    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(renderer.device, &(WGPUPipelineLayoutDescriptor) {
        .bindGroupLayoutCount = 2,
        .bindGroupLayouts = (WGPUBindGroupLayout[]){
            renderer.shader_data.layout,
            renderer.atmosphere.layout
        }
    });

    renderer.atmosphere.buffer = wgpuDeviceCreateDynamicBuffer(renderer.device, 1, sizeof(AtmospherePlanet), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage);

    renderer.atmosphere.pipeline = wgpuDeviceCreateRenderPipeline(renderer.device, &(WGPURenderPipelineDescriptor) {
        .label = WEBGPU_STR("atmosphere"),
        .layout = layout,
        .vertex = {
            .module = shader_module,
            .entryPoint = WEBGPU_STR("vs_main"),
        },
        .fragment = &(WGPUFragmentState) {
            .module = shader_module,
            .entryPoint = WEBGPU_STR("fs_main"),
            .targetCount = 1,
            .targets = &(WGPUColorTargetState) {
                .format = renderer.surface_format,
                .writeMask = WGPUColorWriteMask_All,
                .blend = &wgpu_normal_blend_state_add
            }
        },
        .multisample = { .count = 1, .mask = ~0u },
        .primitive = {
            .topology = WGPUPrimitiveTopology_TriangleList,
        },
    });

    wgpuPipelineLayoutRelease(layout);
    wgpuShaderModuleRelease(shader_module);
}

void render_init(void) {
    renderer.instance = wgpuCreateInstance(nullptr);
    if (!renderer.instance)
        mrw_error("Failed to create WebGPU instance.");
    else
        mrw_debug("Successfully created the WebGPU instance!");

    renderer.adapter = get_adapter(renderer.instance, (WGPURequestAdapterOptions) {
        .powerPreference = WGPUPowerPreference_HighPerformance
    });
    if (!renderer.adapter)
        mrw_error("Failed to get the adapter!");
    else
        mrw_debug("Successfully got the adapter!");

    renderer.device = get_device(renderer.adapter);
    if (!renderer.device)
        mrw_error("Failed to get the device!");
    else
        mrw_debug("Succesfully got the device!");

    renderer.queue = wgpuDeviceGetQueue(renderer.device);
    if (!renderer.queue)
        mrw_error("Failed to get the queue!");
    else
        mrw_debug("Succesfully got the queue!");

    renderer.surface = get_surface(renderer.instance
    #ifndef __EMSCRIPTEN__
        , window.window
    #endif
    );

    if (!renderer.surface)
        mrw_error("Failed to get the surface");
    else
        mrw_debug("Succefully got the surface!");

    WGPUSurfaceCapabilities caps;
    wgpuSurfaceGetCapabilities(renderer.surface, renderer.adapter, &caps);
    renderer.surface_format = caps.formats[0];
    mrw_debug("Preferred surface format is {}", (u32)renderer.surface_format);

    renderer.shader_data.buffer = wgpuDeviceCreateBuffer(renderer.device, &(WGPUBufferDescriptor){
        .size = sizeof(renderer.shader_data.data),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform
    });

    renderer.shader_data.layout = wgpuDeviceCreateBindGroupLayout(renderer.device, &(WGPUBindGroupLayoutDescriptor) {
        .entryCount = 1,
        .entries = &(WGPUBindGroupLayoutEntry) {
            .binding = 0,
            .visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment,
            .buffer = {
                .type = WGPUBufferBindingType_Uniform,
                .minBindingSize = sizeof(renderer.shader_data.data)
            }
        }
    });

    renderer.shader_data.bind_group = wgpuDeviceCreateBindGroup(renderer.device, &(WGPUBindGroupDescriptor) {
        .layout = renderer.shader_data.layout,
        .entryCount = 1,
        .entries = &(WGPUBindGroupEntry) {
            .binding = 0,
            .buffer = renderer.shader_data.buffer,
            .offset = 0,
            .size = sizeof(renderer.shader_data.data)
        }
    });

    renderer.shader_data.data.atmosphere_height = 1.2f;
    renderer.shader_data.data.atmosphere_density = 1.2;
    renderer.shader_data.data.atmosphere_falloff = 2.0f;

    render_init_planets();
    render_init_plants();
    render_init_atmosphere();

    renderer.width = 0;
    renderer.height = 0;

    renderer.ripple_context = ripple_initialize((RippleBackendRendererConfig){
        .device = renderer.device,
        .queue = renderer.queue,
        .surface_format = renderer.surface_format
    });
    ripple_make_active_context(&renderer.ripple_context);
}

void renderer_reconfigure_atmosphere_bind_group(void) {
    if (renderer.atmosphere.bind_group) {
        wgpuBindGroupRelease(renderer.atmosphere.bind_group);
    }

    renderer.atmosphere.bind_group = wgpuDeviceCreateBindGroup(renderer.device, &(WGPUBindGroupDescriptor) {
        .layout = renderer.atmosphere.layout,
        .entryCount = 2,
        .entries = (WGPUBindGroupEntry[]) {
            {
                .binding = 0,
                .buffer = renderer.atmosphere.buffer.data,
                .size = wgpuDynamicBufferGetSize(&renderer.atmosphere.buffer)
            },
            {
                .binding = 1,
                .textureView = renderer.depth.view,
            }
        }
    });
}

void renderer_reconfigure(void) {
    wgpuSurfaceConfigure(renderer.surface,
        &(WGPUSurfaceConfiguration){
            .width = renderer.width,
            .height = renderer.height,
            .format = renderer.surface_format,
            .usage = WGPUTextureUsage_RenderAttachment,
            .device = renderer.device,
            .presentMode = WGPUPresentMode_Mailbox
        }
    );

    if (renderer.depth.texture) {
        wgpuTextureRelease(renderer.depth.texture);
        wgpuTextureViewRelease(renderer.depth.view);
    }

    renderer.depth.extent = (WGPUExtent3D){
        .width = renderer.width,
        .height = renderer.height,
        .depthOrArrayLayers = 1
    };
    renderer.depth.texture = wgpuDeviceCreateTexture(renderer.device,
        &(WGPUTextureDescriptor){
            .label = WEBGPU_STR("depth texture"),
            .size = renderer.depth.extent,
            .format = WGPUTextureFormat_Depth24Plus,
            .usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
            .dimension = WGPUTextureDimension_2D,
            .mipLevelCount = 1,
            .sampleCount = 1
        }
    );
    renderer.depth.view = wgpuTextureCreateView(renderer.depth.texture,
        &(WGPUTextureViewDescriptor){
            .format = WGPUTextureFormat_Depth24Plus,
            .dimension = WGPUTextureViewDimension_2D,
            .mipLevelCount = 1,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_DepthOnly,
        }
    );
    renderer_reconfigure_atmosphere_bind_group();
}

void render_render_meshes(Scene* scene, WGPUCommandEncoder encoder, WGPUTextureView surface_texture_view) {
    {
        MeshIter mesh_iter = { 0 };
        while (genarr_next_valid(renderer.meshes, &mesh_iter)) {
            vektor_clear(mesh_iter.mesh->instance_data);
        }
    }

    {
        EntityIter iter = { .include = CT_Mesh | CT_Transform };
        while (scene_next_entity(scene, &iter)) {
            Entity* entity = iter.entity;
            Mesh* mesh = genarr_get(renderer.meshes, entity->mesh.mesh);

            if (entity_has(entity, CT_Planet)) {
                PlanetInstance shells[16] = { 0 };
                for (u32 i = 0; i < 16; i++) {
                    shells[i] = (PlanetInstance){
                        .mat = entity->transform._matrix,
                        .shell_t = i / 15.0f,
                        .scale = entity->transform.world.scale,
                    };
                }
                u8Slice slice = slice_to((u8*)shells, array_size(shells));
                vektor_add_arr(mesh->instance_data, slice);
                continue;
            }

            u8Slice slice = slice_u8_one(&entity->transform._matrix);
            vektor_add_arr(mesh->instance_data, slice);
        }
    }

    {
        MeshIter mesh_iter = { 0 };
        while (genarr_next_valid(renderer.meshes, &mesh_iter)) {
            u8Slice slice = slice_vektor(mesh_iter.mesh->instance_data);
            wgpuDeviceQueueWriteDynamicBufferRaw(renderer.device, renderer.queue, &mesh_iter.mesh->instance_buffer, slice);
        }
    }

    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(
        encoder,
        &(WGPURenderPassDescriptor) {
            .colorAttachmentCount = 1,
            .colorAttachments = &(WGPURenderPassColorAttachment) {
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
        }
    );

    MeshIter iter = { 0 };
    while (genarr_next_valid(renderer.meshes, &iter)) {
        Mesh* mesh = iter.mesh;
        wgpuRenderPassEncoderSetPipeline(render_pass, iter.mesh->shader == 0 ? renderer.plants.pipeline : renderer.planets.pipeline);
        wgpuRenderPassEncoderSetBindGroup(render_pass, 0, renderer.shader_data.bind_group, 0, nullptr);

        wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, mesh->vertex_buffer, 0, wgpuBufferGetSize(mesh->vertex_buffer));
        wgpuRenderPassEncoderSetVertexBuffer(render_pass, 1, mesh->instance_buffer.data, 0, wgpuBufferGetSize(mesh->instance_buffer.data));
        wgpuRenderPassEncoderSetIndexBuffer(render_pass, mesh->index_buffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(mesh->index_buffer));
        wgpuRenderPassEncoderDrawIndexed(render_pass, wgpuBufferGetSize(mesh->index_buffer) / sizeof(u16), wgpuDynamicBufferGetCount(&mesh->instance_buffer), 0, 0, 0);
    }

    wgpuRenderPassEncoderEnd(render_pass);
    wgpuRenderPassEncoderRelease(render_pass);
}

void render_render_atmosphere(Scene* scene, WGPUCommandEncoder encoder, WGPUTextureView surface_texture_view) {
    u32 n_planets = 0;

    {
        EntityIter planet_iter = { .include = CT_Planet | CT_Mesh };
        while (scene_next_entity(scene, &planet_iter)) {
            n_planets++;
        }
    }

    AtmospherePlanet buffer[n_planets];
    u32 i = 0;
    EntityIter iter = { .include = CT_Planet | CT_Mesh };
    while (scene_next_entity(scene, &iter)) {
        buffer[i].pos = iter.entity->transform.world.pos;
        buffer[i].radius = iter.entity->transform.world.scale;
        i++;
    }

    if (wgpuDeviceQueueWriteDynamicBufferRaw(renderer.device, renderer.queue, &renderer.atmosphere.buffer, slice_u8_arr(buffer)))
    {
        renderer_reconfigure_atmosphere_bind_group();
    }

    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(
        encoder,
        &(WGPURenderPassDescriptor) {
            .colorAttachmentCount = 1,
            .colorAttachments = &(WGPURenderPassColorAttachment) {
                .view = surface_texture_view,
                .loadOp = WGPULoadOp_Load,
                .storeOp = WGPUStoreOp_Store,
                .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED
            },
        }
    );

    wgpuRenderPassEncoderSetPipeline(render_pass, renderer.atmosphere.pipeline);
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, renderer.shader_data.bind_group, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(render_pass, 1, renderer.atmosphere.bind_group, 0, nullptr);
    wgpuRenderPassEncoderDraw(render_pass, 6, 1, 0, 0);

    wgpuRenderPassEncoderEnd(render_pass);
    wgpuRenderPassEncoderRelease(render_pass);
}

f32 planet_grass_scale = 0.01;

void render_prepare(Scene* scene) {
    if (window.width != renderer.width || window.height != renderer.height) {
        renderer.width = window.width;
        renderer.height = window.height;
        renderer_reconfigure();
    }

    // upload render data
    {
        Entity* camera = scene_get_entity(scene, scene->camera);
        mat4s proj = glms_perspective(to_rad(80.0f), (f32)renderer.width / (f32)renderer.height, 0.01f, 1000.0f);
        mat4s world_mat = glms_mat4_from_transform(&camera->transform.world);
        mat4s view = glms_mat4_inv(world_mat);
        mat4s vp = glms_mat4_mul(proj, view);
        glm_mat4_copy(vp.raw, renderer.shader_data.data.camera_matrix);
        glm_mat4_copy(glms_mat4_inv(vp).raw, renderer.shader_data.data.inv_camera_matrix);
        renderer.shader_data.data.camera_position = camera->transform.world.pos;

        renderer.shader_data.data.res.x = (f32)window.width;
        renderer.shader_data.data.res.y = (f32)window.height;

        wgpuQueueWriteBuffer(renderer.queue, renderer.shader_data.buffer, 0, &renderer.shader_data.data, sizeof(renderer.shader_data.data));
    }
}

void render_render(Scene* scene) {
    render_prepare(scene);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(renderer.device, &(WGPUCommandEncoderDescriptor){ .label = WEBGPU_STR("Command encoder") });

    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(renderer.surface, &surface_texture);
    WGPUTextureView surface_texture_view = wgpuTextureCreateView(
        surface_texture.texture,
        &(WGPUTextureViewDescriptor){
            .label = WEBGPU_STR("Surface texture view"),
            .format = wgpuTextureGetFormat(surface_texture.texture),
            .dimension = WGPUTextureViewDimension_2D,
            .mipLevelCount = 1,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_All,
        }
    );

    render_render_meshes(scene, encoder, surface_texture_view);
    render_render_atmosphere(scene, encoder, surface_texture_view);

    ripple_submit(&renderer.ripple_context,
        renderer.width, renderer.height,
        (RippleRenderData){
            .queue = renderer.queue,
            .device = renderer.device,
            .encoder = encoder,
            .texture_view = surface_texture_view
        }
    );

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
