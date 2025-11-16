#include <printccy/printccy.h>
#include <marrow/marrow.h>

#define RIPPLE_IMPLEMENTATION
#define RIPPLE_BACKEND RIPPLE_WGPU | RIPPLE_GLFW
#include <ripple/ripple.h>
#include <ripple/ripple_widgets.h>

#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cglm/struct.h>

struct(Texture) {
    WGPUTexture texture;
    WGPUTextureView view;
    WGPUExtent3D extent;
};

struct(Context) {
    Texture target;
    Texture depth;

    WGPUBuffer vertex_buffer;
    WGPUBuffer index_buffer;
    WGPUBuffer instance_buffer;
    u32 n_vertices;
    u32 n_indices;

    WGPURenderPipeline pipeline;
    WGPUPipelineLayout pipeline_layout;

    struct {
        WGPUBindGroup group;
        WGPUBindGroupLayout layout;
        WGPUBuffer buffer;
    } shader_data;
};

struct(ShaderData) {
    mat4 camera_matrix;
    vec3s camera_position;
    f32 time;
};

struct(Vertex) {
    vec3s position;
    vec3s normal;
};

struct(Instance) {
    mat4 model_matrix;
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

static Instance instances[1] = {0};

void subdivide(Vertex* vertices, u32* n_vertices, u16* indices, u32* n_indices)
{
    u32 n = *n_indices;
    for (u32 i = 0; i < n; i+= 3)
    {
        u16 i1 = indices[i + 0];
        u16 i3 = indices[i + 1];
        u16 i5 = indices[i + 2];
        Vertex v1 = vertices[i1];
        Vertex v3 = vertices[i3];
        Vertex v5 = vertices[i5];

        Vertex v2 = { .position = glms_vec3_scale(glms_vec3_add(v1.position, v3.position), 0.5f) };
        Vertex v4 = { .position = glms_vec3_scale(glms_vec3_add(v3.position, v5.position), 0.5f) };
        Vertex v6 = { .position = glms_vec3_scale(glms_vec3_add(v5.position, v1.position), 0.5f) };

        u16 i2 = *n_vertices; vertices[(*n_vertices)++] = v2;
        u16 i4 = *n_vertices; vertices[(*n_vertices)++] = v4;
        u16 i6 = *n_vertices; vertices[(*n_vertices)++] = v6;

        indices[i] = i1; indices[i + 1] = i2; indices[i + 2] = i6;
        indices[(*n_indices)++] = i2; indices[(*n_indices)++] = i3; indices[(*n_indices)++] = i4;
        indices[(*n_indices)++] = i4; indices[(*n_indices)++] = i5; indices[(*n_indices)++] = i6;
        indices[(*n_indices)++] = i6; indices[(*n_indices)++] = i2; indices[(*n_indices)++] = i4;
    }

    for_each_n(vertex, vertices, *n_vertices)
    {
        vertex->position = vertex->normal = glms_vec3_normalize(vertex->position);
    }
}

Texture create_texture(WGPUDevice device, WGPUStringView label, u32 w, u32 h, WGPUTextureFormat format, WGPUTextureUsage usage, WGPUTextureAspect aspect)
{
    Texture tex;

    tex.extent = (WGPUExtent3D){ .width = w, .height = h, .depthOrArrayLayers = 1 };

    tex.texture = wgpuDeviceCreateTexture(device, &(WGPUTextureDescriptor){
            .label = label,
            .size = tex.extent,
            .format = format,
            .usage = usage,
            .dimension = WGPUTextureDimension_2D,
            .mipLevelCount = 1,
            .sampleCount = 1
        });

    tex.view = wgpuTextureCreateView(tex.texture, &(WGPUTextureViewDescriptor){
            .format = format,
            .dimension = WGPUTextureViewDimension_2D,
            .mipLevelCount = 1,
            .arrayLayerCount = 1,
            .aspect = aspect,
        });

    return tex;
}

Context create_context(WGPUDevice device, WGPUQueue queue)
{
    Context ctx;
    ctx.target = create_texture(device, WEBGPU_STR("surface_texture"), 1000, 1000, WGPUTextureFormat_BGRA8UnormSrgb, WGPUTextureUsage_TextureBinding | WGPUTextureUsage_RenderAttachment, WGPUTextureAspect_All);
    ctx.depth = create_texture(device, WEBGPU_STR("depth_texture"), 1000, 1000, WGPUTextureFormat_Depth24Plus, WGPUTextureUsage_RenderAttachment, WGPUTextureAspect_DepthOnly);

    {
        ctx.shader_data.layout = wgpuDeviceCreateBindGroupLayout(device, &(WGPUBindGroupLayoutDescriptor){
            .entryCount = 1,
            .entries = (WGPUBindGroupLayoutEntry[]) {
                [0] = {
                    .binding = 0,
                    .visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment,
                    .buffer = {
                        .type = WGPUBufferBindingType_Uniform,
                        .minBindingSize = sizeof(ShaderData)
                    }
                }
            }
        });

        ctx.shader_data.buffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor){
            .size = sizeof(ShaderData),
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
        });

        ctx.shader_data.group = wgpuDeviceCreateBindGroup(device, &(WGPUBindGroupDescriptor){
            .layout = ctx.shader_data.layout,
            .entryCount = 1,
            .entries = (WGPUBindGroupEntry[]){
                [0] = {
                    .binding = 0,
                    .buffer = ctx.shader_data.buffer,
                    .offset = 0,
                    .size = sizeof(ShaderData)
                }
            }
        });

        FILE *fp = fopen("shader.wgsl", "rb");
        fseek(fp, 0, SEEK_END);
        u64 len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char shader_code[len];
        fread(shader_code, 1, len, fp);
        shader_code[len] = 0;
        fclose(fp);

        WGPUShaderModule shader_module = wgpuDeviceCreateShaderModule(device, &(WGPUShaderModuleDescriptor){
                .label = WEBGPU_STR("Shader descriptor"),
                .nextInChain = (WGPUChainedStruct*)&(WGPUShaderSourceWGSL) {
                    .chain.sType = WGPUSType_ShaderSourceWGSL,
                    .code = WEBGPU_STR(shader_code)
                }
            });

        ctx.pipeline_layout = wgpuDeviceCreatePipelineLayout(device, &(WGPUPipelineLayoutDescriptor){
                .bindGroupLayoutCount = 1,
                .bindGroupLayouts = &ctx.shader_data.layout
        });

        WGPUStencilFaceState keep_always = {
            .compare = WGPUCompareFunction_Always,
            .failOp = WGPUStencilOperation_Keep,
            .depthFailOp = WGPUStencilOperation_Keep,
            .passOp = WGPUStencilOperation_Keep
        };

        ctx.pipeline = wgpuDeviceCreateRenderPipeline(device, &(WGPURenderPipelineDescriptor){
                .vertex = {
                    .module = shader_module,
                    .entryPoint = WEBGPU_STR("vs_main"),
                    .bufferCount = 2,
                    .buffers = (WGPUVertexBufferLayout[]) {
                        [0] = {
                            .attributeCount = 2,
                            .attributes = (WGPUVertexAttribute[]) {
                                [0] = {
                                    .shaderLocation = 0,
                                    .format = WGPUVertexFormat_Float32x3,
                                    .offset = 0
                                },
                                [1] = {
                                    .shaderLocation = 1,
                                    .format = WGPUVertexFormat_Float32x3,
                                    .offset = offsetof(Vertex, normal)
                                }
                            },
                            .arrayStride = sizeof(Vertex),
                            .stepMode = WGPUVertexStepMode_Vertex,
                        },
                        [1] = {
                            .attributeCount = 4,
                            .attributes = (WGPUVertexAttribute[]) {
                                [0] = {
                                    .shaderLocation = 2,
                                    .format = WGPUVertexFormat_Float32x4,
                                    .offset = 0
                                },
                                [1] = {
                                    .shaderLocation = 3,
                                    .format = WGPUVertexFormat_Float32x4,
                                    .offset = sizeof(vec4)
                                },
                                [2] = {
                                    .shaderLocation = 4,
                                    .format = WGPUVertexFormat_Float32x4,
                                    .offset = sizeof(vec4) * 2
                                },
                                [3] = {
                                    .shaderLocation = 5,
                                    .format = WGPUVertexFormat_Float32x4,
                                    .offset = sizeof(vec4) * 3
                                },
                            },
                            .arrayStride = sizeof(Instance),
                            .stepMode = WGPUVertexStepMode_Instance,
                        }
                    }
                },
                .primitive = {
                    .topology = WGPUPrimitiveTopology_TriangleList,
                    /* .topology = WGPUPrimitiveTopology_TriangleStrip, */
                    .stripIndexFormat = WGPUIndexFormat_Undefined,
                    .frontFace = WGPUFrontFace_CCW,
                    .cullMode = WGPUCullMode_None
                },
                .fragment = &(WGPUFragmentState) {
                    .module = shader_module,
                    .entryPoint = WEBGPU_STR("fs_main"),
                    .targetCount = 1,
                    .targets = &(WGPUColorTargetState) {
                        .format = WGPUTextureFormat_BGRA8UnormSrgb,
                        .writeMask = WGPUColorWriteMask_All,
                        .blend = &(WGPUBlendState) {
                            .color =  {
                                .srcFactor = WGPUBlendFactor_SrcAlpha,
                                .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
                                .operation = WGPUBlendOperation_Add,
                            },
                            .alpha =  {
                                .srcFactor = WGPUBlendFactor_Zero,
                                .dstFactor = WGPUBlendFactor_One,
                                .operation = WGPUBlendOperation_Add,
                            }
                        },
                    }
                },
                .multisample = {
                    .count = 1,
                    .mask = ~0u,
                },
                .layout = ctx.pipeline_layout,
                .depthStencil = &(WGPUDepthStencilState) {
                    .format = WGPUTextureFormat_Depth24Plus,
                    .depthWriteEnabled = true,
                    .depthCompare = WGPUCompareFunction_Less,
                    .stencilFront = keep_always,
                    .stencilBack = keep_always,
                    .stencilReadMask = ~0,
                    .stencilWriteMask = ~0,
                }
            });

        wgpuShaderModuleRelease(shader_module);

        ctx.n_vertices = array_len(icosahedron_indices) / 3;
        ctx.n_indices = array_len(icosahedron_indices);
        Vertex vertices[ctx.n_vertices * 6 * 6];
        u16 indices[ctx.n_indices * 4 * 4];
        buf_copy(vertices, icosahedron_vertices, sizeof(icosahedron_vertices));
        buf_copy(indices, icosahedron_indices, sizeof(icosahedron_indices));
        subdivide(vertices, &ctx.n_vertices, indices, &ctx.n_indices);
        subdivide(vertices, &ctx.n_vertices, indices, &ctx.n_indices);

        ctx.vertex_buffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor) {
            .size = sizeof(vertices),
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
            });
        wgpuQueueWriteBuffer(queue, ctx.vertex_buffer, 0, vertices, sizeof(vertices));

        ctx.index_buffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor) {
            .size = sizeof(indices),
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index,
            });
        wgpuQueueWriteBuffer(queue, ctx.index_buffer, 0, indices, sizeof(indices));

        ctx.instance_buffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor) {
                .size = sizeof(instances),
                .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
            });
        wgpuQueueWriteBuffer(queue, ctx.instance_buffer, 0, instances, sizeof(instances));
    }

    return ctx;
}

int main(int argc, char* argv[])
{
    BumpAllocator str_allocator = bump_allocator_create();

    RippleBackendRendererConfig config = ripple_backend_renderer_default_config();
    ripple_initialize(ripple_backend_window_default_config(), config);

    WGPUQueue queue = wgpuDeviceGetQueue(config.device);

    Context ctx = create_context(config.device, queue);

    f32 prev_time = 0.0f;
    f32 dt_accum = 0.0f;
    f32 dt_samples = 0.0f;

    ShaderData shader_data = { 0 };

    f32 camera_sensitivity = 10.0f;
    f32 camera_yaw = 0.0f;
    f32 camera_pitch = 0.0f;

    bool main_is_open = true;
    while (main_is_open) {
        f32 time = shader_data.time = glfwGetTime();
        f32 dt = shader_data.time - prev_time;
        if ((dt_accum += dt) > 1.0f)
        {
            mrw_debug("fps: {}", 1.0f / (dt_accum / dt_samples));
            dt_accum = 0.0f;
            dt_samples = 0.0f;
        }
        dt_samples += 1.0f;
        prev_time = time;

        SURFACE( .title = S8("surface"), .width = 800, .height = 800, .clear_color = dark, .is_open = &main_is_open, .cursor_disabled = true )
        {
            camera_yaw = (CURSOR().x / 1000.0f) * camera_sensitivity;
            camera_pitch = -(CURSOR().y / 1000.0f) * camera_sensitivity;

            if (camera_pitch >  1.55f) camera_pitch =  1.55f;
            if (camera_pitch < -1.55f) camera_pitch = -1.55f;

            shader_data.camera_position = (vec3s){ .z = 20.0f };

            mat4s proj = glms_ortho(-5.0f, +5.0f, -5.0f, +5.0f, 0.1f, 1000.0f);
            mat4s view = glms_translate(glms_rotate(glms_rotate(GLMS_MAT4_IDENTITY, camera_yaw, GLMS_YUP), camera_pitch, GLMS_XUP), shader_data.camera_position);
            glm_mat4_copy(glms_mat4_mul(proj, view).raw, shader_data.camera_matrix);

            glm_mat4_copy(glms_rotate(GLMS_MAT4_IDENTITY, time, GLMS_YUP).raw, instances[0].model_matrix);

            RIPPLE( IMAGE( ctx.target.view ) );
            RIPPLE( FORM( .fixed = true ) )
            {
                text(mrw_format("fps rn is: {.2f}", &str_allocator, dt_samples / dt_accum));
                text(mrw_format("yaw: {}", &str_allocator, camera_yaw));
                text(mrw_format("pitch: {}", &str_allocator, camera_pitch));
            }
        }

        wgpuQueueWriteBuffer(queue, ctx.instance_buffer, 0, &instances, sizeof(instances));
        wgpuQueueWriteBuffer(queue, ctx.shader_data.buffer, 0, &shader_data, sizeof(shader_data));

        RippleRenderData render_data = RIPPLE_RENDER_BEGIN();

        {
            WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(render_data.encoder, &(WGPURenderPassDescriptor){
                    .colorAttachmentCount = 1,
                    .colorAttachments = &(WGPURenderPassColorAttachment){
                        .view = ctx.target.view,
                        .loadOp = WGPULoadOp_Clear,
                        .storeOp = WGPUStoreOp_Store,
                        .clearValue = (WGPUColor){ 0.09f, 0.192f, 0.243f, 1.0f },
                        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED
                    },
                    .depthStencilAttachment = &(WGPURenderPassDepthStencilAttachment){
                        .view = ctx.depth.view,
                        .depthLoadOp = WGPULoadOp_Clear,
                        .depthClearValue = 1.0f,
                        .depthStoreOp = WGPUStoreOp_Store,
                    },
                    .occlusionQuerySet = 0
                });

            wgpuRenderPassEncoderSetPipeline(render_pass, ctx.pipeline);
            wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, ctx.vertex_buffer, 0, wgpuBufferGetSize(ctx.vertex_buffer));
            wgpuRenderPassEncoderSetIndexBuffer(render_pass, ctx.index_buffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(ctx.index_buffer));
            wgpuRenderPassEncoderSetVertexBuffer(render_pass, 1, ctx.instance_buffer, 0, wgpuBufferGetSize(ctx.instance_buffer));
            wgpuRenderPassEncoderSetBindGroup(render_pass, 0, ctx.shader_data.group, 0, NULL);
            wgpuRenderPassEncoderDrawIndexed(render_pass, ctx.n_indices, array_len(instances), 0, 0, 0);

            wgpuRenderPassEncoderEnd(render_pass);
            wgpuRenderPassEncoderRelease(render_pass);
        }

        RIPPLE_RENDER_END(render_data);

        bump_allocator_reset(&str_allocator);
    }
}
