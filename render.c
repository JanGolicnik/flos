#define FLOS_RENDER
#include "base.c"

STRUCT(Mesh) {
    ReniBuffer vertex_buffer;
    ReniBuffer index_buffer;
    ReniBuffer instance_buffer;
    VEKTOR(u8) instance_data;
    u32 n_instances;
    u32 shader;
};

STRUCT(AtmospherePlanet) {
    vec3s pos;
    f32 radius;
};

cstr common_includes[] = {
    "./res/shaders/common.wgsl"
};

typedef GENARR_ITER_ALIAS(Mesh, mesh) MeshIter;

struct {
    u32 width, height;
    Reni* reni;
    ReniSurface surface;

    struct {
        ReniBindingLayout layout;
        ReniBinding binding;
        ReniBuffer buffer;
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
        ReniTexture texture;
    } depth;

    struct {
        ReniShader shader;
    } planets;

    struct {
        ReniShader shader;
    } plants;

    struct {
        ReniBindingLayout layout;
        ReniBinding binding;
        ReniBuffer buffer;
        ReniShader shader;
    } atmosphere;

    GENARR(Mesh) meshes;

    RippleContext ripple_context;
} renderer = { 0 };

MeshHandle render_mesh_create(u8Slice vertices, u8Slice indices, usize instance_size, u32 shader) {
    Mesh mesh = (Mesh) {
        .vertex_buffer = reni_create_buffer(renderer.reni, (ReniBufferConfig) {  .data = vertices, .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex  }),
        .index_buffer = reni_create_buffer(renderer.reni, (ReniBufferConfig) {  .data = indices, .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index  }),
        .instance_buffer = reni_create_buffer(renderer.reni, (ReniBufferConfig) {  .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex  }),
        .shader = shader,
    };
    vektor_init(mesh.instance_data, 1, memory.stable);
    return genarr_add(renderer.meshes, mesh);
}

void render_mesh_re_create(MeshHandle old, u8Slice vertices, u8Slice indices, usize instance_size, u32 shader) {
    Mesh* mesh = genarr_get(renderer.meshes, old);
    reni_buffer_write(renderer.reni, mesh->vertex_buffer, vertices, 0);
    reni_buffer_write(renderer.reni, mesh->index_buffer, indices, 0);
    mesh->shader = shader;
}

void render_mesh_free(MeshHandle handle) {
    Mesh* mesh = genarr_get(renderer.meshes, handle);

    reni_release_buffer(renderer.reni, mesh->vertex_buffer);
    reni_release_buffer(renderer.reni, mesh->index_buffer);
    reni_release_buffer(renderer.reni, mesh->instance_buffer);
    vektor_free(mesh->instance_data);

    genarr_remove(renderer.meshes, handle);
}

void render_init_planets(void) {
    renderer.planets.shader = reni_create_shader(renderer.reni, (ReniShaderConfig){
        .name = sstr("planet shader"),
        .source.file = {
            .path = "./res/shaders/planet.wgsl",
            .includes = array_slice(common_includes)
        },
        .layouts[0] = renderer.shader_data.layout,
        .vertex = {
            .entry = sstr("vs_main"),
            .buffers[0] = {
                .stride = sizeof(Vertex),
                .attributes[0] = {
                    .location = 0,
                    .offset = offsetof(Vertex, position),
                    .format = ReniVertexFormat_Float32x3,
                },
                .attributes[1] = {
                    .location = 1,
                    .offset = offsetof(Vertex, normal),
                    .format = ReniVertexFormat_Float32x3,
                }
            },
            .buffers[1] = {
                .instance = true,
                .stride = sizeof(PlanetInstance),
                .attributes[0] = {
                    .location = 3,
                    .format = ReniVertexFormat_Float32x4,
                    .offset = offsetof(PlanetInstance, mat.col[0])
                },
                .attributes[1] = {
                    .location = 4,
                    .format = ReniVertexFormat_Float32x4,
                    .offset = offsetof(PlanetInstance, mat.col[1])
                },
                .attributes[2] = {
                    .location = 5,
                    .format = ReniVertexFormat_Float32x4,
                    .offset = offsetof(PlanetInstance, mat.col[2])
                },
                .attributes[3] = {
                    .location = 6,
                    .format = ReniVertexFormat_Float32x4,
                    .offset = offsetof(PlanetInstance, mat.col[3])
                },
                .attributes[4] = {
                    .location = 7,
                    .format = ReniVertexFormat_Float32,
                    .offset = offsetof(PlanetInstance, shell_t)
                },
                .attributes[5] = {
                    .location = 8,
                    .format = ReniVertexFormat_Float32,
                    .offset = offsetof(PlanetInstance, scale)
                }
            }
        },
        .fragment = {
            .entry = sstr("fs_main"),
            .depth_format = ReniTextureFormat_Depth24Plus,
            .targets[0] = {
                .format = reni_surface_get_format(renderer.reni, renderer.surface),
                .blend_state = {
                    .color = RENI_BLEND_STATE_BLEND,
                    .alpha = RENI_BLEND_STATE_OVERWRITE
                }
            }
        }
    });
}

void render_init_plants(void) {
    renderer.plants.shader = reni_create_shader(renderer.reni, (ReniShaderConfig){
        .name = sstr("plant shader"),
        .source.file = {
            .path = "./res/shaders/plant.wgsl",
            .includes = array_slice(common_includes)
        },
        .layouts[0] = renderer.shader_data.layout,
        .vertex = {
            .entry = sstr("vs_main"),
            .buffers[0] = {
                .stride = sizeof(Vertex),
                .attributes[0] = {
                    .location = 0,
                    .offset = offsetof(Vertex, position),
                    .format = ReniVertexFormat_Float32x3,
                },
                .attributes[1] = {
                    .location = 1,
                    .offset = offsetof(Vertex, normal),
                    .format = ReniVertexFormat_Float32x3,
                }
            },
            .buffers[1] = {
                .instance = true,
                .stride = sizeof(Instance),
                .attributes[0] = {
                    .location = 3,
                    .format = ReniVertexFormat_Float32x4,
                    .offset = offsetof(PlanetInstance, mat.col[0])
                },
                .attributes[1] = {
                    .location = 4,
                    .format = ReniVertexFormat_Float32x4,
                    .offset = offsetof(PlanetInstance, mat.col[1])
                },
                .attributes[2] = {
                    .location = 5,
                    .format = ReniVertexFormat_Float32x4,
                    .offset = offsetof(PlanetInstance, mat.col[2])
                },
                .attributes[3] = {
                    .location = 6,
                    .format = ReniVertexFormat_Float32x4,
                    .offset = offsetof(PlanetInstance, mat.col[3])
                }
            }
        },
        .fragment = {
            .entry = sstr("fs_main"),
            .depth_format = ReniTextureFormat_Depth24Plus,
            .targets[0] = {
                .format = reni_surface_get_format(renderer.reni, renderer.surface),
                .blend_state = {
                    .color = RENI_BLEND_STATE_BLEND,
                    .alpha = RENI_BLEND_STATE_OVERWRITE
                }
            }
        }
    });
}

void render_init_atmosphere(void) {
    renderer.atmosphere.layout = reni_create_binding_layout(renderer.reni, (ReniBindingLayoutConfig){
       .name = sstr("atmosphere bindinding layout"),
       .entries[0] = {
           .visibility = ReniShaderStage_Fragment,
           .buffer.type = ReniBufferBindingType_ReadOnlyStorage
       },
       .entries[1] = {
           .visibility = ReniShaderStage_Fragment,
           .texture.type = ReniSampleType_Depth
       },
    });

    renderer.atmosphere.shader = reni_create_shader(renderer.reni, (ReniShaderConfig){
        .name = sstr("atmosphere shader"),
        .source.file = {
            .path = "./res/shaders/atmosphere.wgsl",
            .includes = array_slice(common_includes)
        },
        .layouts[0] = renderer.shader_data.layout,
        .layouts[1] = renderer.atmosphere.layout,
        .vertex.entry = sstr("vs_main"),
        .fragment = {
            .entry = sstr("fs_main"),
            .targets[0] = {
                .format = reni_surface_get_format(renderer.reni, renderer.surface),
                .blend_state = {
                    .color = RENI_BLEND_STATE_ADD,
                    .alpha = RENI_BLEND_STATE_OVERWRITE
                }
            }
        }
    });

    renderer.atmosphere.buffer = reni_create_buffer(renderer.reni, (ReniBufferConfig) {
        .name = sstr("atmosphere buffer"),
        .usage = ReniBufferUsage_CopyDst | ReniBufferUsage_Storage
    });

    renderer.atmosphere.binding = reni_create_binding(renderer.reni, (ReniBindingConfig) {
        .name = sstr("atmosphere biunding"),
        .layout = renderer.atmosphere.layout,
        .entries[0].buffer.buffer = renderer.atmosphere.buffer,
        .entries[1].texture = renderer.depth.texture
    });
}

static void render_error_callback(str msg)
{
    mrw_debug("Render error: {}", msg);
}

void render_init(void) {
    renderer.reni = reni_create_reni((ReniConfig){
        .name = sstr("Reni !"),
        .error_callback = render_error_callback,
        .allocator = memory.stable,
        .frame_allocator = memory.frame
    });
    renderer.surface = reni_create_surface(renderer.reni, (ReniSurfaceConfig) {
        window.window,
        .mode = ReniPresentMode_Mailbox
    });

    renderer.depth.texture = reni_create_texture(renderer.reni, (ReniTextureConfig){
        .name = sstr("Depth texture"),
        .format = ReniTextureFormat_Depth24Plus,
        .usage = ReniTextureUsage_RenderAttachment | ReniTextureUsage_TextureBinding
    });

    renderer.shader_data.layout = reni_create_binding_layout(renderer.reni, (ReniBindingLayoutConfig) {
        .entries[0] = {
            .visibility = ReniShaderStage_Vertex | ReniShaderStage_Fragment,
            .buffer.type = ReniBufferBindingType_Uniform,
        }
    });

    renderer.shader_data.buffer = reni_create_buffer(renderer.reni, (ReniBufferConfig){
        .name = sstr("shader data buffer"),
        .usage = ReniBufferUsage_CopyDst | ReniBufferUsage_Uniform
    });

    renderer.shader_data.binding = reni_create_binding(renderer.reni, (ReniBindingConfig) {
        .name = sstr("shader data"),
        .layout = renderer.shader_data.layout,
        .entries[0].buffer.buffer = renderer.shader_data.buffer
    });

    renderer.shader_data.data.atmosphere_height = 1.2f;
    renderer.shader_data.data.atmosphere_density = 1.1f;
    renderer.shader_data.data.atmosphere_falloff = 2.7f;

    render_init_planets();
    render_init_plants();
    render_init_atmosphere();

    renderer.width = 0;
    renderer.height = 0;

    renderer.ripple_context = ripple_initialize((RippleBackendRendererConfig){0});
    // renderer.ripple_context = ripple_initialize((RippleBackendRendererConfig){
    //     .reni = renderer.reni
    // });
    ripple_make_active_context(&renderer.ripple_context);
}

void render_render_meshes(Scene* scene, ReniTexture surface_texture) {
    {
        MeshIter mesh_iter = { 0 };
        while (genarr_next_valid(renderer.meshes, &mesh_iter)) {
            mesh_iter.mesh->n_instances = 0;
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
                    mesh->n_instances++;
                }

                u8Slice slice = slice_to((u8*)shells, array_size(shells));
                vektor_add_arr(mesh->instance_data, slice);
                continue;
            }

            u8Slice slice = slice_u8_one(&entity->transform._matrix);
            vektor_add_arr(mesh->instance_data, slice);
            mesh->n_instances++;
        }
    }

    {
        MeshIter mesh_iter = { 0 };
        while (genarr_next_valid(renderer.meshes, &mesh_iter)) {
            u8Slice slice = slice_vektor(mesh_iter.mesh->instance_data);
            reni_buffer_write(renderer.reni, mesh_iter.mesh->instance_buffer, slice, 0);
        }
    }

    ReniRenderpass pass = reni_create_renderpass(renderer.reni, (ReniRenderpassConfig) {
        .targets[0] = {
            .texture = surface_texture,
            .clear = true,
            .clear_value = { 84.0f / 255.0f, 119.0f / 255.0f, 146.0f / 255.0f, 1.0f },
        },
        .depth = {
            .target = renderer.depth.texture,
            .clear = true,
            .clear_value = 1.0f,
        }
    });

    MeshIter iter = { 0 };
    while (genarr_next_valid(renderer.meshes, &iter)) {
        Mesh* mesh = iter.mesh;
        reni_renderpass_set_shader(renderer.reni, pass, iter.mesh->shader == 0 ? renderer.plants.shader : renderer.planets.shader);
        reni_renderpass_set_binding(renderer.reni, pass, 0, renderer.shader_data.binding);
        reni_renderpass_draw(renderer.reni, pass, (ReniDrawConfig) {
           .vertices = mesh->vertex_buffer,
           .indices = mesh->index_buffer,
           .instances = mesh->instance_buffer,
           .n_instances = mesh->n_instances
        });
    }

    reni_submit_renderpass(renderer.reni, pass);
}

void render_render_atmosphere(Scene* scene, ReniTexture surface_texture) {
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
    reni_buffer_write(renderer.reni, renderer.atmosphere.buffer, slice_u8_arr(buffer), 0);

    ReniRenderpass pass = reni_create_renderpass(renderer.reni, (ReniRenderpassConfig){ .targets[0].texture = surface_texture });

    reni_renderpass_set_shader(renderer.reni, pass, renderer.atmosphere.shader);
    reni_renderpass_set_binding(renderer.reni, pass, 0, renderer.shader_data.binding);
    reni_renderpass_set_binding(renderer.reni, pass, 1, renderer.atmosphere.binding);
    reni_renderpass_draw(renderer.reni, pass, (ReniDrawConfig){ .n_vertices = 6, .n_instances = 1 });

    reni_submit_renderpass(renderer.reni, pass);
}

f32 planet_grass_scale = 0.01;

void render_prepare(Scene* scene) {
    if (window.width != renderer.width || window.height != renderer.height) {
        renderer.width = window.width;
        renderer.height = window.height;
        reni_surface_update(renderer.reni, renderer.surface, (ReniSurfaceState){
            .width = renderer.width,
            .height = renderer.height,
        });

        reni_texture_resize(renderer.reni, renderer.depth.texture, renderer.width, renderer.height);
    }

    // upload render data
    {
        Entity* camera = scene_get_entity(scene, scene->camera);
        mat4s proj = glms_perspective(to_rad(80.0f), (f32)renderer.width / (f32)renderer.height, 0.01f, 1000.0f);
        mat4s world_mat = mat4_from_transform(&camera->transform.world);
        mat4s view = mat4_inv(world_mat);
        mat4s vp = mat4_mul(proj, view);
        glm_mat4_copy(vp.raw, renderer.shader_data.data.camera_matrix);
        glm_mat4_copy(mat4_inv(vp).raw, renderer.shader_data.data.inv_camera_matrix);
        renderer.shader_data.data.camera_position = camera->transform.world.pos;

        renderer.shader_data.data.res.x = (f32)window.width;
        renderer.shader_data.data.res.y = (f32)window.height;

        reni_buffer_write(renderer.reni, renderer.shader_data.buffer, slice_u8_one(&renderer.shader_data.data), 0);
    }
}

void render_render(Scene* scene) {
    render_prepare(scene);

    reni_begin(renderer.reni);

    ReniSurfaceAcquired surface = reni_surface_acquire(renderer.reni, renderer.surface);
    if (surface.status != ReniSurfaceStatus_SuccessOptimal)
        mrw_error("Surface acquire error {}", (u32)surface.status);

    render_render_meshes(scene, surface.texture);
    render_render_atmosphere(scene, surface.texture);

    // ripple_submit(&renderer.ripple_context,
    //     renderer.width, renderer.height,
    //     (RippleRenderData) {
    //         .reni = renderer.reni,
    //         .texture_view = surface.texture
    //     }
    // );

    reni_end(renderer.reni);
}
