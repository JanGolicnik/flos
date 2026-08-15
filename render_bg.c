#include "webgpu/webgpu.h"
#define FLOS_RENDER_BG
#include "base.c"

// each structure will also probably store its config so resizing and stuff is easy afterwards
// gotta make sure lifetimes are fine for shader sources and stuff tho

// i find the weay wgpu does bind groups pretty annoying  cause they have to be recreted when the underlying resource changes
// itd be cool if i kept a mechanism where i didnt have to worry about that, maybe if they checked if the resource theyre holding has changed and recretating themselves on use or smth

// itd also be conventient to make an effort in making the buffers type safe like vektor, mapa and genarr are
// but thats for another date

#define RENI_HANDLE(name) STRUCT(name) { GenarrHandle h; }
#define RENI_STRUCT(name)\
RENI_HANDLE(name);\
STRUCT(name#Impl)

RENI_STRUCT(Buffer)
{
    // api should be like Vektor, dynamically sized
};

RENI_STRUCT(ShaderImpl)
{
    // handle to the guy, should make reloadable so it might be nice if it saved its config
};

RENI_STRUCT(TextureImpl)
{
    // just data
    // should support resizing and shit
    // can also be a surface
};

RENI_STRUCT(SurfaceImpl)
{
    // holds the raw surface
    // a Texture
    // width height and should_reconfigure
    // also if it has already been acquired
}

RENI_STRUCT(SamplerImpl)
{
    // just data
};

RENI_STRUCT(RenderpassImpl)
{
    // just config and then a submit
};

RENI_STRUCT(BindGroupImpl)
{
    // arr of texture / buffer / sampler
};

STRUCT(Reni)
{
    // adapter, instance, device, queue
    // and all the others
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;

    GENARR(Surface) surfaces;
    GENARR(Buffer) buffers;
    GENARR(Shader) shaders;
    GENARR(Texture) textures;
    GENARR(Sampler) samplers;
    GENARR(BindGroup) bind_groups;
};

STRUCT(ReniConfig)
{
    // adapter and perfomance options
    // acquired device once first surface is created otherwise it crashes  :P
};

Reni reni_create_reni(ReniConfig);

STRUCT(SurfaceConfig)
{

};

Surface reni_create_surface(Reni*, SurfaceConfig);
void reni_release_surface(Reni*, Surface);

typedef enum {

} TextureFormat;

STRUCT(TextureConfig)
{
    union {
        struct {

        } texture;
        struct {

        } surface ;
    }
};

Texture reni_create_texture(Reni*, TextureConfig);
TextureConfig* reni_get_texture_config(Reni*, Texture);
void reni_recreate_texture(Reni*, Texture, TextureConfig);
void reni_resize_texture(Reni*, Texture, u32 w, u32 h); // keeps prev config
void reni_release_texture(Reni*, Texture);

TextureFormat reni_get_texture_format(Reni*, Texture);

STRUCT(BufferConfig)
{
    // usage
    // size / initial data
};

Buffer reni_create_buffer(Reni*, BufferConfig);
BufferConfig* reni_get_buffer_config(Reni*, Buffer);
void reni_recreate_buffer(Reni*, Buffer, BufferConfig);
void reni_buffer_write(Reni*, Buffer, void* data);
void reni_release_buffer(Reni*, Buffer);

STRUCT(BindGroupConfig)
{

};

BindGroup reni_create_bind_group(Reni*, BindGroupConfig);
BindGroupConfig* reni_get_bind_group_config(Reni*, BindGroup);
void reni_recreate_bind_group(Reni*, BindGroup, BindGroupConfig);
void reni_relese_bind_group(Reni*, BindGroup);

STRUCT(ShaderConfig)
{
    union {
        struct {

        } file;
        struct {

        } string;
    } source;

    // layout + descriptor
    // + bind group configs
};

Shader reni_create_shader(Reni*, ShaderConfig, Path path, Path[] includes);
ShaderConfig* reni_get_shader_config(Reni*, Shader);
void reni_recreate_shader(Reni*, Shader, ShaderConfig, Path path, Path[] includes);
void reni_release_shader(Reni*, Shader);

// id like to figure out a better api than the one webgpu does but that also comes at a later date

STRUCT(RenderpassConfig)
{
  // color and depth attachments
  // and frame allocator for the stuff that gets recorded
};

// reni should automatically create a wgpu encoder when the first renderpass is called
// then sumbit everything in reni_render_end
Renderpass reni_create_renderpass(Reni*, RenderpassConfig);
void reni_renderpass_set_shader(Renderpass, Shader, BindGroup[]);

STRUCT(DrawConfig)
{
    Buffer vertex;
    u32 n_vertices;
    Buffer index;
    bool u8_indices;
    u32 n_indices;
    Buffer instance;
    u32 n_instances;
};
void reni_renderpass_draw(Renderpass, DrawConfig);
void reni_submit_renderpass(Reni*, Renderpass);

STRUCT(Acquired)
{
    bool success; // should be an enum
    Texture texture;
    bool reconfigured; // happens if acquire had to reconfigure the surface
};
// also bumps the textures inner bind group gen handle so any that were referencing it get re-created
Acquired reni_surface_acquire(Reni*, Surface);

// idk what begin does
void reni_begin(Reni*);
// submits all surfaces that were acquired
void reni_end(Reni*, Texture);
