#include "marrow/allocator.h"
#include "marrow/marrow.h"
#include "webgpu/webgpu.h"
#define FLOS_RENDER_BG
#include "base.c"

// each structure will also probably store its config so resizing and stuff is easy afterwards
// gotta make sure lifetimes are fine for shader sources and stuff tho

// i find the weay wgpu does bind groups pretty annoying  cause they have to be recreted when the underlying resource changes
// itd be cool if i kept a mechanism where i didnt have to worry about that, maybe if they checked if the resource theyre holding has changed and recretating themselves on use or smth

// itd also be conventient to make an effort in making the buffers type safe like vektor, mapa and genarr are
// but thats for another date

#define RENI_SHADER_MAX_INCLUDES 8
#define RENI_SHADER_MAX_BINDINGS 8
#define RENI_SHADER_BUFFER_MAX_ATTRIBUTES 16
#define RENI_SHADER_MAX_BUFFERS 8
#define RENI_SHADER_MAX_TARGETS 8
#define RENI_BINDING_MAX_ENTRIES 8

#define RENI_HANDLE(name) STRUCT(name) { GenarrHandle h; }
#define RENI_STRUCT(name)\
RENI_HANDLE(name);\
STRUCT(name ## Impl)

typedef enum ReniBufferUsage {
    ReniBufferUsage_Undefined = 0,
    ReniBufferUsage_MapRead = BIT(0),
    ReniBufferUsage_MapWrite = BIT(1),
    ReniBufferUsage_CopySrc = BIT(2),
    ReniBufferUsage_CopyDst = BIT(3),
    ReniBufferUsage_Index = BIT(4),
    ReniBufferUsage_Vertex = BIT(5),
    ReniBufferUsage_Uniform = BIT(6),
    ReniBufferUsage_Storage = BIT(7),
    ReniBufferUsage_Indirect = BIT(8),
} ReniBufferUsage;

STRUCT(ReniBufferConfig) {
    str name;
    ReniBufferUsage usage;
    usize size;
    u8Slice initial_data;
};

RENI_STRUCT(ReniBuffer) {
    ReniBufferConfig config;

    WGPUDynamicBuffer buffer;

    u32 gen; // bindings check against this to know if they should recreate themselves or not
};

typedef enum ReniTextureFormat {
    ReniTextureFormat_Undefined = 0,
    ReniTextureFormat_R8Unorm,
    ReniTextureFormat_R8Snorm,
    ReniTextureFormat_R8Uint,
    ReniTextureFormat_R8Sint,
    ReniTextureFormat_RG8Unorm,
    ReniTextureFormat_RG8Snorm,
    ReniTextureFormat_RGBA8Unorm,
    ReniTextureFormat_RGBA8UnormSrgb,
    ReniTextureFormat_RGBA8Snorm,
    ReniTextureFormat_RGBA8Uint,
    ReniTextureFormat_RGBA8Sint,
    ReniTextureFormat_BGRA8Unorm,
    ReniTextureFormat_BGRA8UnormSrgb,
    ReniTextureFormat_R16Uint,
    ReniTextureFormat_R16Sint,
    ReniTextureFormat_R16Float,
    ReniTextureFormat_RG16Uint,
    ReniTextureFormat_RG16Sint,
    ReniTextureFormat_RG16Float,
    ReniTextureFormat_RGBA16Uint,
    ReniTextureFormat_RGBA16Sint,
    ReniTextureFormat_RGBA16Float,
    ReniTextureFormat_R32Uint,
    ReniTextureFormat_R32Sint,
    ReniTextureFormat_R32Float,
    ReniTextureFormat_RG32Uint,
    ReniTextureFormat_RG32Sint,
    ReniTextureFormat_RG32Float,
    ReniTextureFormat_RGBA32Uint,
    ReniTextureFormat_RGBA32Sint,
    ReniTextureFormat_RGBA32Float,
    ReniTextureFormat_RGB10A2Unorm,
    ReniTextureFormat_RG11B10Ufloat,
    ReniTextureFormat_Depth16Unorm,
    ReniTextureFormat_Depth24Plus,
} ReniTextureFormat;

typedef enum ReniTextureUsage {
    ReniTextureUsage_Undefined = 0,
    ReniTextureUsage_CopySrc = BIT(0),
    ReniTextureUsage_CopyDst = BIT(1),
    ReniTextureUsage_TextureBinding = BIT(2),
    ReniTextureUsage_StorageBinding = BIT(3),
    ReniTextureUsage_RenderAttachment = BIT(4),
    ReniTextureUsage_TransientAttachment = BIT(5),
    ReniTextureUsage_StorageAttachment = BIT(6),
} ReniTextureUsage;

typedef enum ReniPresentMode {
    ReniPresentMode_Undefined = 0,
    ReniPresentMode_Fifo,
    ReniPresentMode_FifoRelaxed,
    ReniPresentMode_Immediate,
    ReniPresentMode_Mailbox,
} ReniPresentMode;

STRUCT(ReniTextureConfig) {
    str name;
    u32 width;
    u32 height;
    bool multisampled;
    ReniTextureFormat format;
    ReniTextureUsage usage;
    struct {
        ReniPresentMode mode;
    } surface;
};

RENI_STRUCT(ReniTexture) {
    ReniTextureConfig config;

    WGPUTexture texture;
    WGPUTextureView view;

    u32 gen; // bindings check against this to know if they should recreate themselves or not
};

typedef enum ReniSamplerAddressMode {
    ReniSamplerAddressMode_Undefined = 0,
    ReniSamplerAddressMode_ClampToEdge,
    ReniSamplerAddressMode_Repeat,
    ReniSamplerAddressMode_MirrorRepeat,
} ReniSamplerAddressMode;

typedef enum ReniSamplerFilterMode {
    ReniSamplerFilterMode_Undefined = 0,
    ReniSamplerFilterMode_Nearest,
    ReniSamplerFilterMode_Linear,
} ReniSamplerFilterMode;

STRUCT(ReniSamplerConfig) {
    str name;
    ReniSamplerAddressMode address; // u, v, w
    ReniSamplerAddressMode filter; // mag, min, mipmap
};

RENI_STRUCT(ReniSampler) {
    ReniSamplerConfig config;

    WGPUSampler sampler;
};

typedef enum ReniShaderStage {
    ReniShaderStage_Undefined = 0,
    ReniShaderStage_Vertex = BIT(0),
    ReniShaderStage_Fragment = BIT(1),
} ReniShaderStage;

typedef enum ReniBufferBindingType {
    ReniBufferBindingType_Undefined = 0,
    ReniBufferBindingType_Uniform,
    ReniBufferBindingType_Storage,
    ReniBufferBindingType_ReadOnlyStorage,
} ReniBufferBindingType;

typedef enum ReniSamplerType {
    ReniSamplerType_Undefined = 0,
    ReniSamplerType_NonFiltering,
    ReniSamplerType_Filtering,
    ReniSamplerType_Comparison,
} ReniSamplerType;

typedef enum ReniSampleType {
    ReniSampleType_Undefined = 0,
    ReniSampleType_Float,
    ReniSampleType_UnfilterableFloat,
    ReniSampleType_Depth,
    ReniSampleType_Sint,
    ReniSampleType_Uint,
} ReniSampleType;

STRUCT(ReniBindingLayout) {
    ReniShaderStage visibility;
    struct {
        struct {
            ReniBufferBindingType type;
            bool dynamic_offset;
        } buffer;
        struct {
            ReniSampleType type;
            bool multisampled;
        } texture;
        struct {
            ReniSamplerType type;
        } sampler;
    } entries[RENI_BINDING_MAX_ENTRIES];
    u32 n_entries;
};

STRUCT(ReniBindingConfig) {
    ReniBindingLayout layout;
    struct {
        struct {
            ReniBuffer buffer;
            usize offset;
            usize size;
        } buffer;
        ReniTexture texture;
        ReniSampler sampler;
    } entries[RENI_BINDING_MAX_ENTRIES]; // layout hold n_entries
};

RENI_STRUCT(ReniBinding) {
    ReniBindingConfig config;

    u32 gens[RENI_BINDING_MAX_ENTRIES]; // compares with the bindings inside config to know if it should recreate itself

    WGPUBindGroup bind_group;
};

typedef enum ReniVertexFormat {
    ReniVertexFormat_Undefined = 0,
    ReniVertexFormat_Uint8, ReniVertexFormat_Uint8x2, ReniVertexFormat_Uint8x4,
    ReniVertexFormat_Sint8, ReniVertexFormat_Sint8x2, ReniVertexFormat_Sint8x4,
    ReniVertexFormat_Unorm8, ReniVertexFormat_Unorm8x2, ReniVertexFormat_Unorm8x4,
    ReniVertexFormat_Snorm8, ReniVertexFormat_Snorm8x2, ReniVertexFormat_Snorm8x4,
    ReniVertexFormat_Uint16, ReniVertexFormat_Uint16x2, ReniVertexFormat_Uint16x4,
    ReniVertexFormat_Sint16, ReniVertexFormat_Sint16x2, ReniVertexFormat_Sint16x4,
    ReniVertexFormat_Unorm16, ReniVertexFormat_Unorm16x2, ReniVertexFormat_Unorm16x4,
    ReniVertexFormat_Snorm16, ReniVertexFormat_Snorm16x2, ReniVertexFormat_Snorm16x4,
    ReniVertexFormat_Float16, ReniVertexFormat_Float16x2, ReniVertexFormat_Float16x4,
    ReniVertexFormat_Float32, ReniVertexFormat_Float32x2, ReniVertexFormat_Float32x3, ReniVertexFormat_Float32x4,
    ReniVertexFormat_Uint32, ReniVertexFormat_Uint32x2, ReniVertexFormat_Uint32x3, ReniVertexFormat_Uint32x4,
    ReniVertexFormat_Sint32, ReniVertexFormat_Sint32x2, ReniVertexFormat_Sint32x3, ReniVertexFormat_Sint32x4,
} ReniVertexFormat WGPU_ENUM_ATTRIBUTE;

typedef enum ReniBlendFactor {
    ReniBlendFactor_Undefined = 0,
    ReniBlendFactor_Zero,
    ReniBlendFactor_One,
    ReniBlendFactor_SrcAlpha,
    ReniBlendFactor_OneMinusSrcAlpha,
} ReniBlendFactor;

typedef enum ReniBlendOperation {
    ReniBlendOperation_Undefined = 0,
    ReniBlendOperation_Add,
} ReniBlendOperation WGPU_ENUM_ATTRIBUTE;

STRUCT(ReniBlendComponent) {
    ReniBlendOperation operation;
    ReniBlendFactor src;
    ReniBlendFactor dst;
};

STRUCT(ReniBlendState) {
    ReniBlendComponent color;
    ReniBlendComponent alpha;
};

STRUCT(ReniShaderConfig) {
    str name;
    struct {
        struct {
            str file;
            str includes[RENI_SHADER_MAX_INCLUDES];
            u32 n_includes;
        } file;
        str string;
    } source;

    ReniBindingLayout layouts[RENI_SHADER_MAX_BINDINGS];
    u32 n_bindings;

    struct {
        str entry;
        struct {
            bool instance;
            usize stride;
            struct {
                ReniVertexFormat format;
                usize offset;
            } attributes[RENI_SHADER_BUFFER_MAX_ATTRIBUTES];
            u32 n_attributes;
        } buffers[RENI_SHADER_MAX_BUFFERS];
        u32 n_buffers;
    } vertex;

    struct {
        str entry;
        u32 multisample;
        ReniTextureFormat depth_format;
        struct {
            ReniTextureFormat format;
            ReniBlendState blend_state;
        } targets[RENI_SHADER_MAX_TARGETS];
        u32 n_targets;
    } fragment;

    bool culling; // always CCW
    bool wireframe;
};

RENI_STRUCT(ReniShader) {
    ReniShaderConfig config;

    WGPURenderPipeline pipeline;
};

STRUCT(ReniSurfaceConfig) {
#ifdef __EMSCRIPTEN__
    cstr canvas_name;
#else
    GLFWwindow* window;
#endif
};

RENI_STRUCT(ReniSurface) {
    WGPUSurface surface;
    ReniTexture texture;
    bool acquired;
};

STRUCT(ReniRenderpassConfig) {
  struct {
      ReniTexture texture;
      ReniTexture resolve_texture;
      bool clear;
      f32 clear_value[4];
  } targets[RENI_SHADER_MAX_TARGETS];
  u32 n_targets;

  struct {
      ReniTexture target;
      bool clear;
      f32 clear_value;
  } depth;
};

STRUCT(ReniRenderpass) {
    ReniRenderpassConfig config;

    WGPURenderPassEncoder render_pass;
};

typedef void (*ReniErrorCallback)(str message);
STRUCT(ReniConfig) {
    str name;

    ReniErrorCallback error_callback;

    Allocator* allocator;
    Allocator* frame_allocator;
};

STRUCT(Reni)
{
    ReniConfig config;

    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;

    WGPUCommandEncoder encoder; // remi_begin()

    GENARR(ReniSurface) surfaces;
    GENARR(ReniBuffer) buffers;
    GENARR(ReniShader) shaders;
    GENARR(ReniTexture) textures;
    GENARR(ReniSampler) samplers;
    GENARR(ReniBinding) bindings;
};

Reni reni_create_reni(ReniConfig);

Surface reni_create_surface(Reni*, SurfaceConfig);
void reni_release_surface(Reni*, Surface);

typedef enum {

} TextureFormat;

Texture reni_create_texture(Reni*, TextureConfig);
TextureConfig* reni_get_texture_config(Reni*, Texture);
void reni_recreate_texture(Reni*, Texture, TextureConfig);
void reni_resize_texture(Reni*, Texture, u32 w, u32 h); // keeps prev config
void reni_release_texture(Reni*, Texture);

TextureFormat reni_get_texture_format(Reni*, Texture);

Buffer reni_create_buffer(Reni*, BufferConfig);
BufferConfig* reni_get_buffer_config(Reni*, Buffer);
void reni_recreate_buffer(Reni*, Buffer, BufferConfig);
void reni_buffer_write(Reni*, Buffer, void* data);
void reni_release_buffer(Reni*, Buffer);

BindGroup reni_create_bind_group(Reni*, BindGroupConfig);
BindGroupConfig* reni_get_bind_group_config(Reni*, BindGroup);
void reni_recreate_bind_group(Reni*, BindGroup, BindGroupConfig);
void reni_relese_bind_group(Reni*, BindGroup);

Shader reni_create_shader(Reni*, ShaderConfig, Path path, Path[] includes);
ShaderConfig* reni_get_shader_config(Reni*, Shader);
void reni_recreate_shader(Reni*, Shader, ShaderConfig, Path path, Path[] includes);
void reni_release_shader(Reni*, Shader);

// id like to figure out a better api than the one webgpu does but that also comes at a later date

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

void reni_begin(Reni*);
// submits all surfaces that were acquired
void reni_end(Reni*, Texture);
