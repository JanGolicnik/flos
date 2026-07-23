#ifndef RENDER
#define RENDER
#include "webgpu/webgpu.h"
#ifndef UNITY_BUILD
#include "base.c"
#include "window.c"
#include "mesh.c"
#endif

STRUCT(Mesh) {
    WGPUBuffer vertex_buffer;
    WGPUBuffer index_buffer;
    WGPUDynamicBuffer instance_buffer;
    u32 n_instances;
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
    } planets;

    struct {
        WGPURenderPipeline pipeline;
    } plants;

    RippleContext ripple_context;
} renderer = { 0 };

void render_init_planets(void) {
    WGPUShaderModule shader_module = load_shader_module_from_file(renderer.device, "./res/planet_shader.wgsl");

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
    WGPUShaderModule shader_module = load_shader_module_from_file(renderer.device, "./res/plant_shader.wgsl");

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
                    .arrayStride = sizeof(PlantInstance),
                    .stepMode = WGPUVertexStepMode_Instance,
                    .attributeCount = 4,
                    .attributes = (WGPUVertexAttribute[]) {
                        {
                            .shaderLocation = 3,
                            .format = WGPUVertexFormat_Float32x4,
                            .offset = offsetof(PlantInstance, mat.col[0])
                        },
                        {
                            .shaderLocation = 4,
                            .format = WGPUVertexFormat_Float32x4,
                            .offset = offsetof(PlantInstance, mat.col[1])
                        },
                        {
                            .shaderLocation = 5,
                            .format = WGPUVertexFormat_Float32x4,
                            .offset = offsetof(PlantInstance, mat.col[2])
                        },
                        {
                            .shaderLocation = 6,
                            .format = WGPUVertexFormat_Float32x4,
                            .offset = offsetof(PlantInstance, mat.col[3])
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

    render_init_planets();
    render_init_plants();

    renderer.width = 0;
    renderer.height = 0;

    renderer.ripple_context = ripple_initialize((RippleBackendRendererConfig){
        .device = renderer.device,
        .queue = renderer.queue,
        .surface_format = renderer.surface_format
    });
    ripple_make_active_context(&renderer.ripple_context);
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
            .usage = WGPUTextureUsage_RenderAttachment,
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
}

void render_prepare(void) {
    if (window.width != renderer.width || window.height != renderer.height) {
        renderer.width = window.width;
        renderer.height = window.height;
        renderer_reconfigure();
    }
}

void render_planets(WGPUCommandEncoder encoder, MeshSlice meshes, WGPUTextureView surface_texture_view) {
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

    wgpuRenderPassEncoderSetPipeline(render_pass, renderer.planets.pipeline);
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, renderer.shader_data.bind_group, 0, nullptr);

    slice_for_each(meshes, mesh, Mesh) {
        wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, mesh->vertex_buffer, 0, wgpuBufferGetSize(mesh->vertex_buffer));
        wgpuRenderPassEncoderSetVertexBuffer(render_pass, 1, mesh->instance_buffer.data, 0, wgpuBufferGetSize(mesh->instance_buffer.data));
        wgpuRenderPassEncoderSetIndexBuffer(render_pass, mesh->index_buffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(mesh->index_buffer));
        wgpuRenderPassEncoderDrawIndexed(render_pass, wgpuBufferGetSize(mesh->index_buffer) / sizeof(u16), wgpuDynamicBufferGetCount(&mesh->instance_buffer), 0, 0, 0);
    }

    wgpuRenderPassEncoderEnd(render_pass);
    wgpuRenderPassEncoderRelease(render_pass);
}

void render_plants(WGPUCommandEncoder encoder, MeshSlice meshes, WGPUTextureView surface_texture_view) {
    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(
        encoder,
        &(WGPURenderPassDescriptor) {
            .colorAttachmentCount = 1,
            .colorAttachments = &(WGPURenderPassColorAttachment) {
                    .view = surface_texture_view,
                    .loadOp = WGPULoadOp_Load,
                    .storeOp = WGPUStoreOp_Store,
                    .clearValue = (WGPUColor){84.0f / 255.0f, 119.0f / 255.0f, 146.0f / 255.0f, 1.0f},
                    .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED
            },
            .depthStencilAttachment = &(WGPURenderPassDepthStencilAttachment){
                .view = renderer.depth.view,
                .depthLoadOp = WGPULoadOp_Load,
                .depthClearValue = 1.0f,
                .depthStoreOp = WGPUStoreOp_Store
            }
        }
    );

    wgpuRenderPassEncoderSetPipeline(render_pass, renderer.plants.pipeline);
    wgpuRenderPassEncoderSetBindGroup( render_pass, 0, renderer.shader_data.bind_group, 0, nullptr);

    slice_for_each(meshes, mesh, Mesh) {
        wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, mesh->vertex_buffer, 0, wgpuBufferGetSize(mesh->vertex_buffer));
        wgpuRenderPassEncoderSetVertexBuffer(render_pass, 1, mesh->instance_buffer.data, 0, wgpuBufferGetSize(mesh->instance_buffer.data));
        wgpuRenderPassEncoderSetIndexBuffer(render_pass, mesh->index_buffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(mesh->index_buffer));
        wgpuRenderPassEncoderDrawIndexed(render_pass, wgpuBufferGetSize(mesh->index_buffer) / sizeof(u16), mesh->n_instances, 0, 0, 0);
    }

    wgpuRenderPassEncoderEnd(render_pass);
    wgpuRenderPassEncoderRelease(render_pass);
}

Mesh render_mesh_create(u8Slice vertices, u8Slice indices, usize instance_size) {
    return (Mesh) {
        .vertex_buffer = wgpuDeviceCreateBufferWithData(renderer.device, renderer.queue, vertices, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex),
        .index_buffer = wgpuDeviceCreateBufferWithData(renderer.device, renderer.queue, indices, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index),
        .instance_buffer = wgpuDeviceCreateDynamicBuffer(renderer.device, 8, instance_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex)
    };
}

void render_mesh_free(Mesh mesh) {
    wgpuBufferRelease(mesh.vertex_buffer);
    wgpuBufferRelease(mesh.index_buffer);
    wgpuDynamicBufferRelease(&mesh.instance_buffer);
}

void render_render(MeshSlice plant_meshes, MeshSlice planet_meshes) {
    render_prepare();

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

    render_planets(encoder, planet_meshes, surface_texture_view);
    render_plants(encoder, plant_meshes, surface_texture_view);

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

#endif // RENDER
