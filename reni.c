#include "webgpu/webgpu.h"
#define FLOS_RENI
#include "base.c"

#define RENI_SHADER_MAX_BINDINGS 8
#define RENI_SHADER_BUFFER_MAX_ATTRIBUTES 16
#define RENI_SHADER_MAX_BUFFERS 8
#define RENI_SHADER_MAX_TARGETS 8
#define RENI_BINDING_MAX_ENTRIES 8

#define RENI_HANDLE(name) STRUCT(name) { GenarrHandle h; }
#define RENI_STRUCT(name)\
RENI_HANDLE(name);\
STRUCT(name ## Impl)

#define RENI_ERR(args) mrw_abort("reni error !!: " args)
#define RENI_LOG(args) mrw_debug("reni log: " args)

#ifdef __EMSCRIPTEN__
#define RENI_BACKEND_WINDOW cstr
#else
#define RENI_BACKEND_WINDOW GLFWwindow*
#endif

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
    u8Slice data;
};

RENI_STRUCT(ReniBuffer) {
    ReniBufferConfig config;

    WGPUBuffer buffer;
    usize size;
    usize used;

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
};

RENI_STRUCT(ReniTexture) {
    ReniTextureConfig config;

    bool is_surface;
    WGPUTexture texture;
    WGPUSurfaceTexture surface_texture;

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

typedef enum ReniCompareFunction {
    ReniCompareFunction_Undefined = 0,
    ReniCompareFunction_Less,
    ReniCompareFunction_Equal,
    ReniCompareFunction_LessEqual,
    ReniCompareFunction_Greater,
    ReniCompareFunction_NotEqual,
    ReniCompareFunction_GreaterEqual,
    ReniCompareFunction_Always,
} ReniCompareFunction;

STRUCT(ReniSamplerConfig) {
    str name;
    ReniSamplerAddressMode address; // u, v, w
    ReniSamplerFilterMode filter; // mag, min, mipmap
    ReniCompareFunction compare;
};

RENI_STRUCT(ReniSampler) {
    ReniSamplerConfig config;

    WGPUSampler sampler;

    u32 gen;
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

STRUCT(ReniBindingLayoutEntry)
{
    ReniShaderStage visibility;
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
};

STRUCT(ReniBindingLayoutConfig)
{
    str name;
    ReniBindingLayoutEntry entries[RENI_BINDING_MAX_ENTRIES];
};

RENI_STRUCT(ReniBindingLayout) {
    ReniBindingLayoutConfig config;

    u32 n_entries;

    WGPUBindGroupLayout layout;
};

STRUCT(ReniBindingEntry)
{
    struct {
        ReniBuffer buffer;
        usize offset;
        usize size; // 0 means whole buffer
    } buffer;
    ReniTexture texture;
    ReniSampler sampler;

    u32 _gen;
};

STRUCT(ReniBindingConfig) {
    str name;
    ReniBindingLayout layout;
    ReniBindingEntry entries[RENI_BINDING_MAX_ENTRIES]; // layout hold n_entries
};

RENI_STRUCT(ReniBinding) {
    ReniBindingConfig config;
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
} ReniVertexFormat;

typedef enum ReniBlendFactor {
    ReniBlendFactor_Undefined = 0,
    ReniBlendFactor_Zero,
    ReniBlendFactor_One,
    ReniBlendFactor_SrcAlpha,
    ReniBlendFactor_DstAlpha,
    ReniBlendFactor_OneMinusSrcAlpha,
} ReniBlendFactor;

typedef enum ReniBlendOperation {
    ReniBlendOperation_Undefined = 0,
    ReniBlendOperation_Add,
    ReniBlendOperation_Subtract,
    ReniBlendOperation_ReverseSubtract,
    ReniBlendOperation_Min,
    ReniBlendOperation_Max,
} ReniBlendOperation;

STRUCT(ReniBlendComponent) {
    ReniBlendFactor src;
    ReniBlendFactor dst;
    ReniBlendOperation op;
};

STRUCT(ReniBlendState) {
    ReniBlendComponent color;
    ReniBlendComponent alpha;
};

STRUCT(ReniTarget) {
    ReniTextureFormat format;
    ReniBlendState blend_state;
};

STRUCT(ReniVertexAttribute)
{
    u32 location;
    usize offset;
    ReniVertexFormat format;
};

STRUCT(ReniVertexBufferLayout)
{
    bool instance;
    usize stride;
    ReniVertexAttribute attributes[RENI_SHADER_BUFFER_MAX_ATTRIBUTES];
};

STRUCT(ReniShaderConfig) {
    str name;
    struct {
        struct {
            cstr path;
            cstrSlice includes;
        } file;
        str string;
    } source;

    ReniBindingLayout layouts[RENI_SHADER_MAX_BINDINGS];

    struct {
        str entry;
        ReniVertexBufferLayout buffers[RENI_SHADER_MAX_BUFFERS];
    } vertex;

    struct {
        str entry;
        bool multisample; // true = 4
        ReniTextureFormat depth_format;
        bool dont_write_depth;
        ReniTarget targets[RENI_SHADER_MAX_TARGETS];
    } fragment;

    bool culling; // always CCW
    bool wireframe;
};

RENI_STRUCT(ReniShader) {
    ReniShaderConfig config;

    WGPURenderPipeline pipeline;
    WGPUPipelineLayout layout;
};

STRUCT(ReniSurfaceConfig) {
    RENI_BACKEND_WINDOW window;
    ReniPresentMode mode;
};

STRUCT(ReniSurfaceState) {
    u32 width;
    u32 height;
};

RENI_STRUCT(ReniSurface) {
    ReniSurfaceConfig config;
    ReniSurfaceState state;

    ReniTextureFormat format;
    ReniTexture texture;
    bool acquired;

    WGPUSurface surface;
};

STRUCT(ReniRenderpassTarget)
{
    ReniTexture texture;
    ReniTexture resolve_texture;
    bool clear;
    f32 clear_value[4];
};

STRUCT(ReniRenderpassConfig) {
  ReniRenderpassTarget targets[RENI_SHADER_MAX_TARGETS];
  struct {
      ReniTexture target;
      bool clear;
      f32 clear_value;
  } depth;
};

STRUCT(ReniRenderpass) {
    ReniRenderpassConfig config;

    WGPURenderPassEncoder render_pass; // TODO: hide behind handle
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

    WGPUCommandEncoder encoder; // reni_begin()

    GENARR(ReniSurfaceImpl) surfaces;
    GENARR(ReniBufferImpl) buffers;
    GENARR(ReniShaderImpl) shaders;
    GENARR(ReniTextureImpl) textures;
    GENARR(ReniSamplerImpl) samplers;
    GENARR(ReniBindingLayoutImpl) binding_layouts;
    GENARR(ReniBindingImpl) bindings;
};

static void _reni_request_adapter_callback(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2)
{
    if (status != WGPURequestAdapterStatus_Success) RENI_ERR("Failed to get WebGPU adapter");
    *((WGPUAdapter*)userdata1) = adapter;
}

static void _reni_request_device_callback(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* userdata2)
{
    if (status != WGPURequestDeviceStatus_Success) RENI_ERR("Failed to get WebGPU device");
    *((WGPUDevice*)userdata1) = device;
}

static void _reni_error_callback(WGPUDevice const* device, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2)
{
    ((ReniErrorCallback)userdata1)(message.length == WGPU_STRLEN ? str((char*)message.data) : (str)slice_to((char*)message.data, message.length));
}

Reni reni_create_reni(ReniConfig config)
{
    Reni reni = { .config = config, .instance = wgpuCreateInstance(nullptr) };
    if (reni.instance) RENI_LOG("Successfully created the WebGPU instance!");
    else RENI_ERR("Failed to create WebGPU instance!");

    wgpuInstanceRequestAdapter(reni.instance,
        &(WGPURequestAdapterOptions) { .powerPreference = WGPUPowerPreference_HighPerformance },
        (WGPURequestAdapterCallbackInfo) {
            .mode = WGPUCallbackMode_AllowSpontaneous ,
            .callback = _reni_request_adapter_callback,
            .userdata1 = &reni.adapter
        }
    );
    #ifdef __EMSCRIPTEN__
        while(!reni.adapter) emscripten_sleep(100);
    #endif

    if (reni.adapter) RENI_LOG("Successfully got the adapter!");
    else RENI_ERR("Failed to get the adapter!");

    wgpuAdapterRequestDevice(reni.adapter,
        &(WGPUDeviceDescriptor){
            .label = WEBGPU_STR("Device :D"),
            .defaultQueue.label = WEBGPU_STR("some queue/?"),
            .uncapturedErrorCallbackInfo = (WGPUUncapturedErrorCallbackInfo) {
                .callback = _reni_error_callback,
                .userdata1 = (void*)config.error_callback
            }
        },
        (WGPURequestDeviceCallbackInfo){
            .mode = WGPUCallbackMode_AllowSpontaneous,
            .callback = _reni_request_device_callback,
            .userdata1 = &reni.device
        }
    );
    #ifdef __EMSCRIPTEN__
        while(!reni.device) emscripten_sleep(100);
    #endif

    if (reni.device) RENI_LOG("Succesfully got the device!");
    else RENI_ERR("Failed to get the device!");

    if ((reni.queue = wgpuDeviceGetQueue(reni.device))) RENI_LOG("Succesfully got the queue!");
    else RENI_ERR("Failed to get the queue!");

    genarr_init(reni.surfaces, 1, config.allocator);
    genarr_init(reni.buffers, 1, config.allocator);
    genarr_init(reni.shaders, 1, config.allocator);
    genarr_init(reni.textures, 1, config.allocator);
    genarr_init(reni.samplers, 1, config.allocator);
    genarr_init(reni.binding_layouts, 1, config.allocator);
    genarr_init(reni.bindings, 1, config.allocator);

    return reni;
}

void reni_destroy_reni(Reni* reni)
{
    RENI_ERR("I cant be bothered");
}

WGPUVertexFormat _reni_vertex_format_to_wgpu(ReniVertexFormat format)
{
    switch (format)
    {
        case ReniVertexFormat_Undefined: RENI_ERR("Invalid format provided for conversion");
        case ReniVertexFormat_Uint8: return WGPUVertexFormat_Uint8;
        case ReniVertexFormat_Uint8x2: return WGPUVertexFormat_Uint8x2;
        case ReniVertexFormat_Uint8x4: return WGPUVertexFormat_Uint8x4;
        case ReniVertexFormat_Sint8: return WGPUVertexFormat_Sint8;
        case ReniVertexFormat_Sint8x2: return WGPUVertexFormat_Sint8x2;
        case ReniVertexFormat_Sint8x4: return WGPUVertexFormat_Sint8x4;
        case ReniVertexFormat_Unorm8: return WGPUVertexFormat_Unorm8;
        case ReniVertexFormat_Unorm8x2: return WGPUVertexFormat_Unorm8x2;
        case ReniVertexFormat_Unorm8x4: return WGPUVertexFormat_Unorm8x4;
        case ReniVertexFormat_Snorm8: return WGPUVertexFormat_Snorm8;
        case ReniVertexFormat_Snorm8x2: return WGPUVertexFormat_Snorm8x2;
        case ReniVertexFormat_Snorm8x4: return WGPUVertexFormat_Snorm8x4;
        case ReniVertexFormat_Uint16: return WGPUVertexFormat_Uint16;
        case ReniVertexFormat_Uint16x2: return WGPUVertexFormat_Uint16x2;
        case ReniVertexFormat_Uint16x4: return WGPUVertexFormat_Uint16x4;
        case ReniVertexFormat_Sint16: return WGPUVertexFormat_Sint16;
        case ReniVertexFormat_Sint16x2: return WGPUVertexFormat_Sint16x2;
        case ReniVertexFormat_Sint16x4: return WGPUVertexFormat_Sint16x4;
        case ReniVertexFormat_Unorm16: return WGPUVertexFormat_Unorm16;
        case ReniVertexFormat_Unorm16x2: return WGPUVertexFormat_Unorm16x2;
        case ReniVertexFormat_Unorm16x4: return WGPUVertexFormat_Unorm16x4;
        case ReniVertexFormat_Snorm16: return WGPUVertexFormat_Snorm16;
        case ReniVertexFormat_Snorm16x2: return WGPUVertexFormat_Snorm16x2;
        case ReniVertexFormat_Snorm16x4: return WGPUVertexFormat_Snorm16x4;
        case ReniVertexFormat_Float16: return WGPUVertexFormat_Float16;
        case ReniVertexFormat_Float16x2: return WGPUVertexFormat_Float16x2;
        case ReniVertexFormat_Float16x4: return WGPUVertexFormat_Float16x4;
        case ReniVertexFormat_Float32: return WGPUVertexFormat_Float32;
        case ReniVertexFormat_Float32x2: return WGPUVertexFormat_Float32x2;
        case ReniVertexFormat_Float32x3: return WGPUVertexFormat_Float32x3;
        case ReniVertexFormat_Float32x4: return WGPUVertexFormat_Float32x4;
        case ReniVertexFormat_Uint32: return WGPUVertexFormat_Uint32;
        case ReniVertexFormat_Uint32x2: return WGPUVertexFormat_Uint32x2;
        case ReniVertexFormat_Uint32x3: return WGPUVertexFormat_Uint32x3;
        case ReniVertexFormat_Uint32x4: return WGPUVertexFormat_Uint32x4;
        case ReniVertexFormat_Sint32: return WGPUVertexFormat_Sint32;
        case ReniVertexFormat_Sint32x2: return WGPUVertexFormat_Sint32x2;
        case ReniVertexFormat_Sint32x3: return WGPUVertexFormat_Sint32x3;
        case ReniVertexFormat_Sint32x4: return WGPUVertexFormat_Sint32x4;
    }
}

WGPUBlendFactor _reni_blend_factor_to_wgpu(ReniBlendFactor factor)
{
    switch (factor)
    {
        case ReniBlendFactor_Undefined: return WGPUBlendFactor_Undefined;
        case ReniBlendFactor_Zero: return WGPUBlendFactor_Zero;
        case ReniBlendFactor_One: return WGPUBlendFactor_One;
        case ReniBlendFactor_SrcAlpha: return WGPUBlendFactor_SrcAlpha;
        case ReniBlendFactor_DstAlpha: return WGPUBlendFactor_DstAlpha;
        case ReniBlendFactor_OneMinusSrcAlpha: return WGPUBlendFactor_OneMinusSrcAlpha;
    }
}

WGPUBlendOperation _reni_blend_operation_to_wgpu(ReniBlendOperation operation)
{
    switch (operation)
    {
        case ReniBlendOperation_Undefined: return WGPUBlendOperation_Undefined;
        case ReniBlendOperation_Add: return WGPUBlendOperation_Add;
        case ReniBlendOperation_Subtract: return WGPUBlendOperation_Subtract;
        case ReniBlendOperation_ReverseSubtract: return WGPUBlendOperation_ReverseSubtract;
        case ReniBlendOperation_Min: return WGPUBlendOperation_Min;
        case ReniBlendOperation_Max: return WGPUBlendOperation_Max;
    }
}

WGPUBlendComponent _reni_blend_component_to_wgpu(ReniBlendComponent component)
{
    return (WGPUBlendComponent) {
        .dstFactor = _reni_blend_factor_to_wgpu(component.dst),
        .srcFactor = _reni_blend_factor_to_wgpu(component.src),
        .operation = _reni_blend_operation_to_wgpu(component.op)
    };
}

WGPUCompareFunction _reni_compare_function_to_wgpu(ReniCompareFunction function)
{
    switch (function)
    {
        case ReniCompareFunction_Undefined: return WGPUCompareFunction_Undefined;
        case ReniCompareFunction_Less: return WGPUCompareFunction_Less;
        case ReniCompareFunction_Equal: return WGPUCompareFunction_Equal;
        case ReniCompareFunction_LessEqual: return WGPUCompareFunction_LessEqual;
        case ReniCompareFunction_Greater: return WGPUCompareFunction_Greater;
        case ReniCompareFunction_NotEqual: return WGPUCompareFunction_NotEqual;
        case ReniCompareFunction_GreaterEqual: return WGPUCompareFunction_GreaterEqual;
        case ReniCompareFunction_Always: return WGPUCompareFunction_Always;
    }
}

WGPUAddressMode _reni_sampler_address_mode_to_wgpu(ReniSamplerAddressMode mode)
{
    switch (mode)
    {
        case ReniSamplerAddressMode_Undefined: return WGPUAddressMode_Undefined;
        case ReniSamplerAddressMode_ClampToEdge: return WGPUAddressMode_ClampToEdge;
        case ReniSamplerAddressMode_Repeat: return WGPUAddressMode_Repeat;
        case ReniSamplerAddressMode_MirrorRepeat: return WGPUAddressMode_MirrorRepeat;
    }
}

WGPUFilterMode _reni_sampler_filter_mode_to_wgpu(ReniSamplerFilterMode mode)
{
    switch (mode)
    {
        case ReniSamplerFilterMode_Undefined: return WGPUFilterMode_Undefined;
        case ReniSamplerFilterMode_Nearest: return WGPUFilterMode_Nearest;
        case ReniSamplerFilterMode_Linear: return WGPUFilterMode_Linear;
    }
}

WGPUSamplerBindingType _reni_sampler_type_to_wgpu(ReniSamplerType type)
{
    switch (type)
    {
        case ReniSamplerType_Undefined: return WGPUSamplerBindingType_Undefined;
        case ReniSamplerType_NonFiltering: return WGPUSamplerBindingType_NonFiltering;
        case ReniSamplerType_Filtering: return WGPUSamplerBindingType_Filtering;
        case ReniSamplerType_Comparison: return WGPUSamplerBindingType_Comparison;
    };
}

WGPUTextureSampleType _reni_sample_type_to_wgpu(ReniSampleType type)
{
    switch (type)
    {
        case ReniSampleType_Undefined: return  WGPUTextureSampleType_Undefined;
        case ReniSampleType_Float: return WGPUTextureSampleType_Float;
        case ReniSampleType_UnfilterableFloat: return WGPUTextureSampleType_UnfilterableFloat;
        case ReniSampleType_Depth: return WGPUTextureSampleType_Depth;
        case ReniSampleType_Sint: return WGPUTextureSampleType_Sint;
        case ReniSampleType_Uint: return WGPUTextureSampleType_Uint;
    }
}

WGPUBufferBindingType _reni_buffer_binding_type_to_wgpu(ReniBufferBindingType type)
{
    switch(type)
    {
        case ReniBufferBindingType_Undefined: return WGPUBufferBindingType_Undefined;
        case ReniBufferBindingType_Uniform: return WGPUBufferBindingType_Uniform;
        case ReniBufferBindingType_Storage: return WGPUBufferBindingType_Storage;
        case ReniBufferBindingType_ReadOnlyStorage: return WGPUBufferBindingType_ReadOnlyStorage;
    }
}

ReniTextureFormat _reni_texture_format_from_wgpu(WGPUTextureFormat format)
{
    switch(format)
    {
        case WGPUTextureFormat_R8Unorm: return ReniTextureFormat_R8Unorm;
        case WGPUTextureFormat_R8Snorm: return ReniTextureFormat_R8Snorm;
        case WGPUTextureFormat_R8Uint: return ReniTextureFormat_R8Uint;
        case WGPUTextureFormat_R8Sint: return ReniTextureFormat_R8Sint;
        case WGPUTextureFormat_RG8Unorm: return ReniTextureFormat_RG8Unorm;
        case WGPUTextureFormat_RG8Snorm: return ReniTextureFormat_RG8Snorm;
        case WGPUTextureFormat_RGBA8Unorm: return ReniTextureFormat_RGBA8Unorm;
        case WGPUTextureFormat_RGBA8UnormSrgb: return ReniTextureFormat_RGBA8UnormSrgb;
        case WGPUTextureFormat_RGBA8Snorm: return ReniTextureFormat_RGBA8Snorm;
        case WGPUTextureFormat_RGBA8Uint: return ReniTextureFormat_RGBA8Uint;
        case WGPUTextureFormat_RGBA8Sint: return ReniTextureFormat_RGBA8Sint;
        case WGPUTextureFormat_BGRA8Unorm: return ReniTextureFormat_BGRA8Unorm;
        case WGPUTextureFormat_BGRA8UnormSrgb: return ReniTextureFormat_BGRA8UnormSrgb;
        case WGPUTextureFormat_R16Uint: return ReniTextureFormat_R16Uint;
        case WGPUTextureFormat_R16Sint: return ReniTextureFormat_R16Sint;
        case WGPUTextureFormat_R16Float: return ReniTextureFormat_R16Float;
        case WGPUTextureFormat_RG16Uint: return ReniTextureFormat_RG16Uint;
        case WGPUTextureFormat_RG16Sint: return ReniTextureFormat_RG16Sint;
        case WGPUTextureFormat_RG16Float: return ReniTextureFormat_RG16Float;
        case WGPUTextureFormat_RGBA16Uint: return ReniTextureFormat_RGBA16Uint;
        case WGPUTextureFormat_RGBA16Sint: return ReniTextureFormat_RGBA16Sint;
        case WGPUTextureFormat_RGBA16Float: return ReniTextureFormat_RGBA16Float;
        case WGPUTextureFormat_R32Uint: return ReniTextureFormat_R32Uint;
        case WGPUTextureFormat_R32Sint: return ReniTextureFormat_R32Sint;
        case WGPUTextureFormat_R32Float: return ReniTextureFormat_R32Float;
        case WGPUTextureFormat_RG32Uint: return ReniTextureFormat_RG32Uint;
        case WGPUTextureFormat_RG32Sint: return ReniTextureFormat_RG32Sint;
        case WGPUTextureFormat_RG32Float: return ReniTextureFormat_RG32Float;
        case WGPUTextureFormat_RGBA32Uint: return ReniTextureFormat_RGBA32Uint;
        case WGPUTextureFormat_RGBA32Sint: return ReniTextureFormat_RGBA32Sint;
        case WGPUTextureFormat_RGBA32Float: return ReniTextureFormat_RGBA32Float;
        case WGPUTextureFormat_RGB10A2Unorm: return ReniTextureFormat_RGB10A2Unorm;
        case WGPUTextureFormat_RG11B10Ufloat: return ReniTextureFormat_RG11B10Ufloat;
        case WGPUTextureFormat_Depth16Unorm: return ReniTextureFormat_Depth16Unorm;
        case WGPUTextureFormat_Depth24Plus: return ReniTextureFormat_Depth24Plus;
        default: break;
    }
    return ReniTextureFormat_Undefined;
}

WGPUTextureFormat _reni_texture_format_to_wgpu(ReniTextureFormat format)
{
    switch(format)
    {
        case ReniTextureFormat_Undefined: return WGPUTextureFormat_Undefined;
        case ReniTextureFormat_R8Unorm: return WGPUTextureFormat_R8Unorm;
        case ReniTextureFormat_R8Snorm: return WGPUTextureFormat_R8Snorm;
        case ReniTextureFormat_R8Uint: return WGPUTextureFormat_R8Uint;
        case ReniTextureFormat_R8Sint: return WGPUTextureFormat_R8Sint;
        case ReniTextureFormat_RG8Unorm: return WGPUTextureFormat_RG8Unorm;
        case ReniTextureFormat_RG8Snorm: return WGPUTextureFormat_RG8Snorm;
        case ReniTextureFormat_RGBA8Unorm: return WGPUTextureFormat_RGBA8Unorm;
        case ReniTextureFormat_RGBA8UnormSrgb: return WGPUTextureFormat_RGBA8UnormSrgb;
        case ReniTextureFormat_RGBA8Snorm: return WGPUTextureFormat_RGBA8Snorm;
        case ReniTextureFormat_RGBA8Uint: return WGPUTextureFormat_RGBA8Uint;
        case ReniTextureFormat_RGBA8Sint: return WGPUTextureFormat_RGBA8Sint;
        case ReniTextureFormat_BGRA8Unorm: return WGPUTextureFormat_BGRA8Unorm;
        case ReniTextureFormat_BGRA8UnormSrgb: return WGPUTextureFormat_BGRA8UnormSrgb;
        case ReniTextureFormat_R16Uint: return WGPUTextureFormat_R16Uint;
        case ReniTextureFormat_R16Sint: return WGPUTextureFormat_R16Sint;
        case ReniTextureFormat_R16Float: return WGPUTextureFormat_R16Float;
        case ReniTextureFormat_RG16Uint: return WGPUTextureFormat_RG16Uint;
        case ReniTextureFormat_RG16Sint: return WGPUTextureFormat_RG16Sint;
        case ReniTextureFormat_RG16Float: return WGPUTextureFormat_RG16Float;
        case ReniTextureFormat_RGBA16Uint: return WGPUTextureFormat_RGBA16Uint;
        case ReniTextureFormat_RGBA16Sint: return WGPUTextureFormat_RGBA16Sint;
        case ReniTextureFormat_RGBA16Float: return WGPUTextureFormat_RGBA16Float;
        case ReniTextureFormat_R32Uint: return WGPUTextureFormat_R32Uint;
        case ReniTextureFormat_R32Sint: return WGPUTextureFormat_R32Sint;
        case ReniTextureFormat_R32Float: return WGPUTextureFormat_R32Float;
        case ReniTextureFormat_RG32Uint: return WGPUTextureFormat_RG32Uint;
        case ReniTextureFormat_RG32Sint: return WGPUTextureFormat_RG32Sint;
        case ReniTextureFormat_RG32Float: return WGPUTextureFormat_RG32Float;
        case ReniTextureFormat_RGBA32Uint: return WGPUTextureFormat_RGBA32Uint;
        case ReniTextureFormat_RGBA32Sint: return WGPUTextureFormat_RGBA32Sint;
        case ReniTextureFormat_RGBA32Float: return WGPUTextureFormat_RGBA32Float;
        case ReniTextureFormat_RGB10A2Unorm: return WGPUTextureFormat_RGB10A2Unorm;
        case ReniTextureFormat_RG11B10Ufloat: return WGPUTextureFormat_RG11B10Ufloat;
        case ReniTextureFormat_Depth16Unorm: return WGPUTextureFormat_Depth16Unorm;
        case ReniTextureFormat_Depth24Plus: return WGPUTextureFormat_Depth24Plus;
    }
}

WGPUPresentMode _reni_present_mode_to_wgpu(ReniPresentMode mode)
{
    switch(mode)
    {
        case ReniPresentMode_Undefined: return WGPUPresentMode_Undefined;
        case ReniPresentMode_Fifo: return WGPUPresentMode_Fifo;
        case ReniPresentMode_FifoRelaxed: return WGPUPresentMode_FifoRelaxed;
        case ReniPresentMode_Immediate: return WGPUPresentMode_Immediate;
        case ReniPresentMode_Mailbox: return WGPUPresentMode_Mailbox;
    }
}

WGPUBufferUsage _reni_buffer_usage_to_wgpu(ReniBufferUsage usage)
{
    WGPUBufferUsage ret = 0;
    if (FLAG_HAS(usage, ReniBufferUsage_MapRead)) FLAG_SET(ret, WGPUBufferUsage_MapRead);
    if (FLAG_HAS(usage, ReniBufferUsage_MapWrite)) FLAG_SET(ret, WGPUBufferUsage_MapWrite);
    if (FLAG_HAS(usage, ReniBufferUsage_CopySrc)) FLAG_SET(ret, WGPUBufferUsage_CopySrc);
    if (FLAG_HAS(usage, ReniBufferUsage_CopyDst)) FLAG_SET(ret, WGPUBufferUsage_CopyDst);
    if (FLAG_HAS(usage, ReniBufferUsage_Index)) FLAG_SET(ret, WGPUBufferUsage_Index);
    if (FLAG_HAS(usage, ReniBufferUsage_Vertex)) FLAG_SET(ret, WGPUBufferUsage_Vertex);
    if (FLAG_HAS(usage, ReniBufferUsage_Uniform)) FLAG_SET(ret, WGPUBufferUsage_Uniform);
    if (FLAG_HAS(usage, ReniBufferUsage_Storage)) FLAG_SET(ret, WGPUBufferUsage_Storage);
    if (FLAG_HAS(usage, ReniBufferUsage_Indirect)) FLAG_SET(ret, WGPUBufferUsage_Indirect);
    return ret;
}

WGPUSurface _reni_get_surface(WGPUInstance instance, RENI_BACKEND_WINDOW window) {
    return wgpuInstanceCreateSurface(instance, &(WGPUSurfaceDescriptor) {
        .label = (WGPUStringView){ NULL, WGPU_STRLEN },
        .nextInChain = (WGPUChainedStruct*)
    #if defined(__EMSCRIPTEN__)
        &(WGPUEmscriptenSurfaceSourceCanvasHTMLSelector) {
            .chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector,
            .selector = (WGPUStringView){ window, WGPU_STRLEN },
        },
    #elif defined(__linux__)
        (glfwGetPlatform() == GLFW_PLATFORM_X11 ?
        (void*)&(WGPUSurfaceSourceXlibWindow) {
            .chain.sType =  WGPUSType_SurfaceSourceXlibWindow,
            .display = glfwGetX11Display(),
            .window = glfwGetX11Window(window),
        }  :
        (void*)&(WGPUSurfaceSourceWaylandSurface) {
            .chain.sType = WGPUSType_SurfaceSourceWaylandSurface,
            .display = glfwGetWaylandDisplay(),
            .surface = glfwGetWaylandWindow(window),
        }),
    #else
        &(WGPUSurfaceSourceWindowsHWND) {
            .chain.sType = WGPUSType_SurfaceSourceWindowsHWND,
            .hinstance = GetModuleHandle(NULL),
            .hwnd = glfwGetWin32Window(window)
        },
    #endif
    });
}

WGPUShaderStage _reni_shader_stage_to_wgpu(ReniShaderStage stage)
{
    WGPUShaderStage ret = 0;
    if (FLAG_HAS(stage, ReniShaderStage_Vertex)) FLAG_SET(ret, WGPUShaderStage_Vertex);
    if (FLAG_HAS(stage, ReniShaderStage_Fragment)) FLAG_SET(ret, WGPUShaderStage_Fragment);
    return ret;
}

WGPUTextureUsage _reni_texture_usage_to_wgpu(ReniTextureUsage usage)
{
    WGPUTextureUsage ret = 0;
    if (FLAG_HAS(usage, ReniTextureUsage_CopySrc)) FLAG_SET(ret, WGPUTextureUsage_CopySrc);
    if (FLAG_HAS(usage, ReniTextureUsage_CopyDst)) FLAG_SET(ret, WGPUTextureUsage_CopyDst);
    if (FLAG_HAS(usage, ReniTextureUsage_TextureBinding)) FLAG_SET(ret, WGPUTextureUsage_TextureBinding);
    if (FLAG_HAS(usage, ReniTextureUsage_StorageBinding)) FLAG_SET(ret, WGPUTextureUsage_StorageBinding);
    if (FLAG_HAS(usage, ReniTextureUsage_RenderAttachment)) FLAG_SET(ret, WGPUTextureUsage_RenderAttachment);
    if (FLAG_HAS(usage, ReniTextureUsage_TransientAttachment)) FLAG_SET(ret, WGPUTextureUsage_TransientAttachment);
    if (FLAG_HAS(usage, ReniTextureUsage_StorageAttachment)) FLAG_SET(ret, WGPUTextureUsage_StorageAttachment);
    return ret;
}

ReniTexture reni_recreate_texture(Reni* reni, ReniTexture texture, ReniTextureConfig config);
ReniTexture reni_create_texture(Reni* reni, ReniTextureConfig config)
{
    ReniTexture texture = { .h = genarr_add(reni->textures, (ReniTextureImpl){ .gen = 1 }) };
    reni_recreate_texture(reni, texture, config);
    return texture;
}

ReniTexture reni_recreate_texture(Reni* reni, ReniTexture texture, ReniTextureConfig config)
{
    ReniTextureImpl* impl = genarr_get(reni->textures, texture.h);
    if (!impl) return reni_create_texture(reni, config);

    if (impl->is_surface) RENI_ERR("Cant recreate a surface texture!");

    if (config.width > 0 && config.height > 0) {
        if (impl->view) wgpuTextureViewRelease(impl->view);
        if (impl->texture) wgpuTextureRelease(impl->texture);

        impl->texture = wgpuDeviceCreateTexture(reni->device, &(WGPUTextureDescriptor) {
            .label = WEBGPU_STR_SLICE(config.name),
            .usage = _reni_texture_usage_to_wgpu(config.usage),
            .dimension = WGPUTextureDimension_2D,
            .size = { .width = config.width, .height = config.height, .depthOrArrayLayers = 1 },
            .format = _reni_texture_format_to_wgpu(config.format),
            .sampleCount = config.multisampled ? 4 : 1,
            .mipLevelCount = 1,
        });

        bool is_depth_format = config.format == ReniTextureFormat_Depth16Unorm || config.format == ReniTextureFormat_Depth24Plus;
        impl->view = wgpuTextureCreateView(impl->texture, &(WGPUTextureViewDescriptor) {
            .label = WEBGPU_STR_SLICE(config.name),
            .dimension = WGPUTextureViewDimension_2D,
            .mipLevelCount = 1,
            .arrayLayerCount = 1,
            .aspect = is_depth_format ? WGPUTextureAspect_DepthOnly : WGPUTextureAspect_All
        });
    }


    impl->config = config;
    impl->gen++;
    return texture;
}

const ReniTextureConfig* reni_get_texture_config(Reni* reni, ReniTexture texture)
{
    ReniTextureImpl* impl = genarr_get(reni->textures, texture.h);
    if (!impl) RENI_ERR("Invalid texture handle");
    return &impl->config;
}

void reni_texture_resize(Reni* reni, ReniTexture texture, u32 w, u32 h)
{
    ReniTextureConfig config = *reni_get_texture_config(reni, texture);
    config.width = w; config.height = h;
    reni_recreate_texture(reni, texture, config);
}

ReniTextureFormat reni_texture_get_format(Reni* reni, ReniTexture texture)
{
    return reni_get_texture_config(reni, texture)->format;
}

void reni_release_texture(Reni* reni, ReniTexture texture)
{
    ReniTextureImpl* impl = genarr_get(reni->textures, texture.h);
    if (!impl) RENI_ERR("Invalid texture handle!");
    if (impl->view) wgpuTextureViewRelease(impl->view);
    if (impl->texture) wgpuTextureRelease(impl->texture);
    genarr_remove(reni->textures, texture.h);
}

ReniSurface reni_create_surface(Reni* reni, ReniSurfaceConfig config)
{
    if (!config.mode) config.mode = ReniPresentMode_Fifo;

    ReniSurfaceImpl impl = { .config = config, .surface = _reni_get_surface(reni->instance, config.window) };
    if (!impl.surface) return (ReniSurface){ 0 };

    WGPUSurfaceCapabilities caps; wgpuSurfaceGetCapabilities(impl.surface, reni->adapter, &caps);
    impl.format = _reni_texture_format_from_wgpu(caps.formats[0]);
    wgpuSurfaceCapabilitiesFreeMembers(caps);

    impl.texture = (ReniTexture){ .h = genarr_add(reni->textures, (ReniTextureImpl){ .is_surface = true }) };

    return (ReniSurface){ .h = genarr_add(reni->surfaces, impl) };
}

ReniTextureFormat reni_surface_get_format(Reni* reni, ReniSurface surface)
{
    ReniSurfaceImpl* impl = genarr_get(reni->surfaces, surface.h);
    if (!impl) RENI_ERR("Invalid surface handle");
    return impl->format;
}

void reni_surface_update(Reni* reni, ReniSurface surface, ReniSurfaceState state)
{
    ReniSurfaceImpl* impl = genarr_get(reni->surfaces, surface.h);
    if (!impl) RENI_ERR("Invalid surface handle");

    if (state.width == impl->state.width && state.height == impl->state.height)
        return;

    impl->state = state;

    if (impl->state.width == 0 || impl->state.height == 0) return;

    wgpuSurfaceConfigure(impl->surface,
        &(WGPUSurfaceConfiguration) {
            .width = state.width,
            .height = state.height,
            .device = reni->device,
            .usage = WGPUTextureUsage_RenderAttachment,
            .format = _reni_texture_format_to_wgpu(impl->format),
            .presentMode = _reni_present_mode_to_wgpu(impl->config.mode),
        }
    );
}

void reni_release_surface(Reni* reni, ReniSurface surface)
{
    ReniSurfaceImpl* impl = genarr_get(reni->surfaces, surface.h);
    if (!impl) RENI_ERR("Invalid surface handle");
    if (impl->texture.h.valid) reni_release_texture(reni, impl->texture);
    if (impl->surface) wgpuSurfaceRelease(impl->surface);
    genarr_remove(reni->surfaces, surface.h);
}

ReniSampler reni_recreate_sampler(Reni* reni, ReniSampler sampler, ReniSamplerConfig config);

ReniSampler reni_create_sampler(Reni* reni, ReniSamplerConfig config)
{
    ReniSampler sampler = { .h = genarr_add(reni->samplers, (ReniSamplerImpl){ 0 }) };
    reni_recreate_sampler(reni, sampler, config);
    return sampler;
}

ReniSampler reni_recreate_sampler(Reni* reni, ReniSampler sampler, ReniSamplerConfig config)
{
    ReniSamplerImpl* impl = genarr_get(reni->samplers, sampler.h);
    if (!impl) return reni_create_sampler(reni, config);

    if (impl->sampler) wgpuSamplerRelease(impl->sampler);

    WGPUAddressMode address_mode = _reni_sampler_address_mode_to_wgpu(config.address);
    WGPUFilterMode filter_mode = _reni_sampler_filter_mode_to_wgpu(config.filter);
    impl->sampler = wgpuDeviceCreateSampler(reni->device, &(WGPUSamplerDescriptor) {
        .label = WEBGPU_STR_SLICE(config.name),
        .addressModeU = address_mode,
        .addressModeV = address_mode,
        .addressModeW = address_mode,
        .magFilter = filter_mode,
        .minFilter = filter_mode,
        .mipmapFilter = WGPUMipmapFilterMode_Linear,
        .compare = _reni_compare_function_to_wgpu(config.compare)
    });

    impl->config = config;
    impl->gen++;
    return sampler;
}

void reni_release_sampler(Reni* reni, ReniSampler sampler)
{
    ReniSamplerImpl* impl = genarr_get(reni->samplers, sampler.h);
    if (!impl) RENI_ERR("Invalid sampler handle!");
    wgpuSamplerRelease(impl->sampler);
    genarr_remove(reni->samplers, sampler.h);
}

void reni_buffer_write(Reni* reni, ReniBuffer buffer, u8Slice data, usize offset)
{
    ReniBufferImpl* impl = genarr_get(reni->buffers, buffer.h);
    if (!impl) RENI_ERR("Invalid buffer handle");

    if (slice_size(data) == 0) return;
    if (slice_size(data) % 4 != 0) RENI_ERR("Buffer write size must be a multiple of 4");
    if (offset % 4 != 0) RENI_ERR("Buffer write offset must be a multiple of 4");

    usize required = offset + slice_size(data);
    if (required > impl->size) {
        WGPUBuffer prev_buffer = impl->buffer;
        usize prev_size = impl->size;

        impl->size = max(u64_nextpow2(required), 256);

        impl->buffer = wgpuDeviceCreateBuffer(reni->device, &(WGPUBufferDescriptor) {
            .label = WEBGPU_STR_SLICE(impl->config.name),
            .usage = _reni_buffer_usage_to_wgpu(impl->config.usage) | WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst,
            .size = impl->size,
        });

        if (prev_buffer) {
            WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(reni->device, &(WGPUCommandEncoderDescriptor){ .label = WEBGPU_STR("reni buffer copy encoder") });
            wgpuCommandEncoderCopyBufferToBuffer(encoder, prev_buffer, 0, impl->buffer, 0, prev_size);
            WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &(WGPUCommandBufferDescriptor){ .label = WEBGPU_STR("reni buffer command buffer") });
            wgpuQueueSubmit(reni->queue, 1, &command);
            wgpuCommandBufferRelease(command);
            wgpuCommandEncoderRelease(encoder);
            wgpuBufferRelease(prev_buffer);
        }

        impl->gen++;
    }

    wgpuQueueWriteBuffer(reni->queue, impl->buffer, offset, data.start, slice_size(data));

    impl->used = max(impl->used, required);
    return;
}

ReniBuffer reni_create_buffer(Reni* reni, ReniBufferConfig config)
{
    ReniBuffer buffer = {
        .h = genarr_add(reni->buffers, (ReniBufferImpl){
            .config.name = config.name,
            .config.usage = config.usage,
            .gen = 1
        })
    };
    if (slice_size(config.data)) reni_buffer_write(reni, buffer, config.data, 0);
    return buffer;
}

void reni_release_buffer(Reni* reni, ReniBuffer buffer)
{
    ReniBufferImpl* impl = genarr_get(reni->buffers, buffer.h);
    if (!impl) RENI_ERR("Invalid buffer handle");
    if (impl->buffer) wgpuBufferRelease(impl->buffer);
    genarr_remove(reni->buffers, buffer.h);
}

ReniBindingLayout reni_create_binding_layout(Reni* reni, ReniBindingLayoutConfig config)
{
    ReniBindingLayoutImpl impl = { .config = config, .n_entries = array_len(config.entries) };
    WGPUBindGroupLayoutEntry entries[impl.n_entries];
    for (u32 i = 0; i < impl.n_entries; i++) {
        ReniBindingLayoutEntry* entry = &config.entries[i];
        WGPUBindGroupLayoutEntry* out = &entries[i];
        *out = (WGPUBindGroupLayoutEntry) {
            .binding = i,
            .visibility = _reni_shader_stage_to_wgpu(entry->visibility)
        };

        if (entry->buffer.type != ReniBufferBindingType_Undefined) {
            out->buffer = (WGPUBufferBindingLayout) {
                .type = _reni_buffer_binding_type_to_wgpu(entry->buffer.type),
                .hasDynamicOffset = entry->buffer.dynamic_offset
            };
        }
        else if (entry->texture.type != ReniSampleType_Undefined) {
            out->texture = (WGPUTextureBindingLayout) {
                .sampleType = _reni_sample_type_to_wgpu(entry->texture.type),
                .multisampled = entry->texture.multisampled,
                .viewDimension = WGPUTextureViewDimension_2D
            };
        }
        else if (entry->sampler.type != ReniSamplerType_Undefined) {
            out->sampler = (WGPUSamplerBindingLayout) {
                .type = _reni_sampler_type_to_wgpu(entry->sampler.type),
            };
        }
        else {
            impl.n_entries = i;
            break;
        }
    }

    if (impl.n_entries == 0)
        RENI_ERR("Cannot create bind group layout without entries");

    impl.layout = wgpuDeviceCreateBindGroupLayout(reni->device, &(WGPUBindGroupLayoutDescriptor) {
        .label = WEBGPU_STR_SLICE(config.name),
        .entryCount = impl.n_entries,
        .entries = entries
    });

    return (ReniBindingLayout) { .h = genarr_add(reni->binding_layouts, impl) };
}

void reni_release_binding_layout(Reni* reni, ReniBindingLayout layout)
{
    ReniBindingLayoutImpl* impl = genarr_get(reni->binding_layouts, layout.h);
    if (!impl) RENI_ERR("Invalid binding layout handle");
    if (impl->layout) wgpuBindGroupLayoutRelease(impl->layout);
    genarr_remove(reni->binding_layouts, layout.h);
}

ReniBinding reni_create_binding(Reni* reni, ReniBindingConfig config)
{
    return (ReniBinding){ .h = genarr_add(reni->bindings, (ReniBindingImpl){ .config = config }) };
}

const ReniBindingConfig* reni_get_binding_config(Reni* reni, ReniBinding binding)
{
    ReniBindingImpl* impl = genarr_get(reni->bindings, binding.h);
    if (!impl) RENI_ERR("Invalid binding handle");
    return &impl->config;
}

WGPUBindGroup _reni_get_binding_bind_group(Reni* reni, ReniBinding binding)
{
    ReniBindingImpl* impl = genarr_get(reni->bindings, binding.h);
    if (!impl) RENI_ERR("Invalid binding handle");

    ReniBindingLayoutImpl* layout = genarr_get(reni->binding_layouts, impl->config.layout.h);
    if (!layout) RENI_ERR("Invalid binding layout handle");

    bool recreate = impl->bind_group == nullptr;
    for (u32 i = 0; i < layout->n_entries; i++) {
        if (recreate)
            break;

        ReniBindingEntry* entry = &impl->config.entries[i];
        if (layout->config.entries[i].buffer.type != ReniBufferBindingType_Undefined) {
            ReniBufferImpl* buffer = genarr_get(reni->buffers, entry->buffer.buffer.h);
            if (!buffer) RENI_ERR("Invalid buffer handle");
            recreate |= buffer->gen != entry->_gen;
            entry->_gen = buffer->gen;
        }
        else if (layout->config.entries[i].texture.type != ReniSampleType_Undefined) {
            ReniTextureImpl* texture = genarr_get(reni->textures, entry->texture.h);
            if (!texture) RENI_ERR("Invalid texture handle");
            recreate |= texture->gen != entry->_gen;
            entry->_gen = texture->gen;
        }
        else if (layout->config.entries[i].sampler.type != ReniSamplerType_Undefined) {
            ReniSamplerImpl* sampler = genarr_get(reni->samplers, entry->sampler.h);
            if (!sampler) RENI_ERR("Invalid sampler handle");
            recreate |= sampler->gen != entry->_gen;
            entry->_gen = sampler->gen;
        }
        else {
            RENI_ERR("Unreachable");
        }
    }

    if (recreate) {
        if (impl->bind_group) wgpuBindGroupRelease(impl->bind_group);
        WGPUBindGroupEntry entries[layout->n_entries];
        for (u32 i = 0; i < layout->n_entries; i++) {
            ReniBindingEntry* entry = &impl->config.entries[i];
            WGPUBindGroupEntry* out = &entries[i];
            *out = (WGPUBindGroupEntry) { .binding = i };
            if (layout->config.entries[i].buffer.type != ReniBufferBindingType_Undefined) {
                ReniBufferImpl* buffer = genarr_get(reni->buffers, entry->buffer.buffer.h);
                if (!buffer) RENI_ERR("Invalid buffer handle");
                out->buffer = buffer->buffer;
                out->size = entry->buffer.size ? entry->buffer.size : buffer->size;
                out->offset = entry->buffer.offset;
                entry->_gen = buffer->gen;
            }
            else if (layout->config.entries[i].texture.type != ReniSampleType_Undefined) {
                ReniTextureImpl* texture = genarr_get(reni->textures, entry->texture.h);
                if (!texture) RENI_ERR("Invalid texture handle");
                out->textureView = texture->view;
                entry->_gen = texture->gen;
            }
            else if (layout->config.entries[i].sampler.type != ReniSamplerType_Undefined) {
                ReniSamplerImpl* sampler = genarr_get(reni->samplers, entry->sampler.h);
                if (!sampler) RENI_ERR("Invalid sampler handle");
                out->sampler = sampler->sampler;
                entry->_gen = sampler->gen;
            }
            else {
                RENI_ERR("Unreachable");
            }
        }

        impl->bind_group = wgpuDeviceCreateBindGroup(reni->device, &(WGPUBindGroupDescriptor){
            .label = WEBGPU_STR_SLICE(impl->config.name),
            .layout = layout->layout,
            .entryCount = layout->n_entries,
            .entries = entries
        });
    }

    return impl->bind_group;
}

ReniBinding reni_recreate_binding(Reni* reni, ReniBinding binding, ReniBindingConfig config)
{
    ReniBindingImpl* impl = genarr_get(reni->bindings, binding.h);
    if (!impl) return reni_create_binding(reni, config);
    impl->config = config;
    return binding;
}

void reni_release_binding(Reni* reni, ReniBinding binding)
{
    ReniBindingImpl* impl = genarr_get(reni->bindings, binding.h);
    if (!impl) RENI_ERR("Invalid binding handle");
    if (impl->bind_group) wgpuBindGroupRelease(impl->bind_group);
    genarr_remove(reni->bindings, binding.h);
}

ReniShader reni_recreate_shader(Reni*, ReniShader, ReniShaderConfig);

ReniShader reni_create_shader(Reni* reni, ReniShaderConfig config)
{
    ReniShader shader = { .h = genarr_add(reni->shaders, (ReniShaderImpl) { 0 }) };
    reni_recreate_shader(reni, shader, config);
    return shader;
}

const ReniShaderConfig* reni_get_shader_config(Reni* reni, ReniShader shader)
{
    ReniShaderImpl* impl = genarr_get(reni->shaders, shader.h);
    if (!impl) RENI_ERR("Invalid shader handle");
    return &impl->config;
}

WGPUShaderModule _reni_load_shader_module(Reni* reni, cstr path, cstrSlice includes)
{
    FILE *fp = fopen(path, "rb");
    fseek(fp, 0, SEEK_END); u64 len = ftell(fp);
    fseek(fp, 0, SEEK_SET); char* buf = mrw_alloc_n(reni->config.frame_allocator, char, len);
    fread(buf, 1, len, fp); fclose(fp);

    str full_shader = slice_to(buf, len);
    for (u32 i = slice_count(includes) - 1; i < slice_count(includes); i--)
    {
        FILE *fp = fopen(includes.start[i], "rb");
        fseek(fp, 0, SEEK_END); u64 len = ftell(fp);
        fseek(fp, 0, SEEK_SET); char* buf = mrw_alloc_n(reni->config.frame_allocator, char, len);
        fread(buf, 1, len, fp); fclose(fp);
        str include_shader = slice_to(buf, len);
        full_shader = mrw_format("{}\n{}", reni->config.frame_allocator, include_shader, full_shader);
    }

    return wgpuDeviceCreateShaderModule(reni->device, &(WGPUShaderModuleDescriptor) {
        .label = WEBGPU_STR("planet shader descriptor"),
        .nextInChain = (WGPUChainedStruct*)&(WGPUShaderSourceWGSL) {
            .chain.sType = WGPUSType_ShaderSourceWGSL,
            .code = WEBGPU_STR_SLICE(full_shader)
        }
    });
}

ReniShader reni_recreate_shader(Reni* reni, ReniShader shader, ReniShaderConfig config)
{
    ReniShaderImpl* impl = genarr_get(reni->shaders, shader.h);
    if (!impl) return reni_create_shader(reni, config);

    WGPUShaderModule module = str_len(config.source.string) == 0 ?
        _reni_load_shader_module(reni, config.source.file.path, config.source.file.includes) :
        wgpuDeviceCreateShaderModule(reni->device, &(WGPUShaderModuleDescriptor) {
            .label = WEBGPU_STR("planet shader descriptor"),
            .nextInChain = (WGPUChainedStruct*)&(WGPUShaderSourceWGSL) {
                .chain.sType = WGPUSType_ShaderSourceWGSL,
                .code = WEBGPU_STR_SLICE(config.source.string)
            }
        });

    u32 n_layouts = array_len(config.layouts);
    WGPUBindGroupLayout layouts[n_layouts];
    for (u32 i = 0; i < n_layouts; i++)
    {
        if (!config.layouts[i].h.valid) {
            n_layouts = i;
            break;
        }

        ReniBindingLayoutImpl* layout = genarr_get(reni->binding_layouts, config.layouts[i].h);
        if (!layout) RENI_ERR("Invalid layout handle!");
        layouts[i] = layout->layout;
    }

    if (impl->layout) wgpuPipelineLayoutRelease(impl->layout);
    impl->layout = wgpuDeviceCreatePipelineLayout(reni->device, &(WGPUPipelineLayoutDescriptor) {
        .label = WEBGPU_STR_SLICE(config.name),
        .bindGroupLayoutCount = n_layouts,
        .bindGroupLayouts = layouts
    });
    WGPUStencilFaceState keep = {
        .compare = WGPUCompareFunction_Always,
        .failOp = WGPUStencilOperation_Keep,
        .depthFailOp = WGPUStencilOperation_Keep,
        .passOp = WGPUStencilOperation_Keep
    };

    u32 n_targets = array_len(config.fragment.targets);
    WGPUColorTargetState targets[n_targets];
    WGPUBlendState blend_states[n_targets];
    for (u32 i = 0; i < n_targets; i++)
    {
        ReniTarget* target = &config.fragment.targets[i];
        if (target->format == ReniTextureFormat_Undefined) {
            n_targets = i;
            break;
        }
        blend_states[i] = (WGPUBlendState){
            .color = _reni_blend_component_to_wgpu(target->blend_state.color),
            .alpha = _reni_blend_component_to_wgpu(target->blend_state.alpha)
        };
    }

    for (u32 i = 0; i < n_targets; i++)
    {
        ReniTarget* target = &config.fragment.targets[i];
        targets[i] = (WGPUColorTargetState){
            .format = _reni_texture_format_to_wgpu(target->format),
            .writeMask = WGPUColorWriteMask_All,
            .blend = &blend_states[i],
        };
    }

    u32 n_buffers = array_len(config.vertex.buffers);
    WGPUVertexBufferLayout buffers[n_buffers];
    WGPUVertexAttribute attributes[n_buffers][RENI_SHADER_BUFFER_MAX_ATTRIBUTES];
    for (u32 i = 0; i < n_buffers; i++)
    {
        ReniVertexBufferLayout* layout = &config.vertex.buffers[i];
        if (layout->attributes[0].format == ReniVertexFormat_Undefined) {
            n_buffers = i;
            break;
        }

        u32 n_attributes = array_len(layout->attributes);
        for (u32 j = 0; j < n_attributes; j++)
        {
            ReniVertexAttribute* attribute = &layout->attributes[j];
            if (attribute->format == ReniVertexFormat_Undefined) {
                n_attributes = j;
                break;
            }
            attributes[i][j] = (WGPUVertexAttribute){
                .shaderLocation = attribute->location,
                .format = _reni_vertex_format_to_wgpu(attribute->format),
                .offset = attribute->offset
            };
        }

        buffers[i] = (WGPUVertexBufferLayout){
            .stepMode = layout->instance ? WGPUVertexStepMode_Instance : WGPUVertexStepMode_Vertex,
            .arrayStride = layout->stride,
            .attributeCount = n_attributes,
            .attributes = attributes[i]
        };
    }

    if (impl->pipeline) wgpuRenderPipelineRelease(impl->pipeline);
    impl->pipeline = wgpuDeviceCreateRenderPipeline(reni->device, &(WGPURenderPipelineDescriptor) {
        .label = WEBGPU_STR_SLICE(config.name),
        .layout = impl->layout,
        .vertex = {
            .module = module,
            .entryPoint = WEBGPU_STR_SLICE(config.vertex.entry),
            .bufferCount = n_buffers,
            .buffers = buffers
        },
        .fragment = n_targets > 0 ? &(WGPUFragmentState){
            .module = module,
            .entryPoint = WEBGPU_STR_SLICE(config.fragment.entry),
            .targetCount = n_targets,
            .targets = targets,
        } : nullptr,
        .multisample = { .count = config.fragment.multisample ? 4 : 1, .mask = ~0 },
        .primitive = {
            .topology = config.wireframe ? WGPUPrimitiveTopology_LineStrip : WGPUPrimitiveTopology_TriangleList,
            .frontFace = WGPUFrontFace_CCW,
            .cullMode = config.culling ? WGPUCullMode_Back : WGPUCullMode_None
        },
        .depthStencil = config.fragment.depth_format != ReniTextureFormat_Undefined ? &(WGPUDepthStencilState){
            .format = _reni_texture_format_to_wgpu(config.fragment.depth_format),
            .depthWriteEnabled = !config.fragment.dont_write_depth,
            .depthCompare = WGPUCompareFunction_Less,
            .stencilFront = keep,
            .stencilBack = keep,
            .stencilReadMask = ~0,
            .stencilWriteMask = ~0
        } : nullptr
    });

    wgpuShaderModuleRelease(module);

    impl->config = config;
    return shader;
}

void reni_release_shader(Reni* reni, ReniShader shader)
{
    ReniShaderImpl* impl = genarr_get(reni->shaders, shader.h);
    if (!impl) RENI_ERR("Invalid shader handle!");
    if (impl->pipeline) wgpuRenderPipelineRelease(impl->pipeline);
    if (impl->layout) wgpuPipelineLayoutRelease(impl->layout);
    genarr_remove(reni->shaders, shader.h);
}

// id like to figure out a better api than the one webgpu does but that also comes at a later date
ReniRenderpass reni_create_renderpass(Reni* reni, ReniRenderpassConfig config)
{
    ReniRenderpass renderpass = { .config = config };
    u32 n_targets = array_len(config.targets);
    WGPURenderPassColorAttachment color_attachments[n_targets];
    for (u32 i = 0; i < n_targets; i++)
    {
        ReniRenderpassTarget* target = &config.targets[i];
        WGPURenderPassColorAttachment* out = &color_attachments[i];

        if (!target->texture.h.valid) {
            n_targets = i;
            break;
        }

        ReniTextureImpl* texture = genarr_get(reni->textures, target->texture.h);
        if (!texture) RENI_ERR("Invalid texture handle");

        *out = (WGPURenderPassColorAttachment) {
            .view = texture->view,
            .clearValue = (WGPUColor) { target->clear_value[0], target->clear_value[1], target->clear_value[2], target->clear_value[3] },
            .loadOp = target->clear ? WGPULoadOp_Clear : WGPULoadOp_Load,
            .storeOp = WGPUStoreOp_Store,
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED
        };

        if (target->resolve_texture.h.valid) {
            ReniTextureImpl* resolve_texture = genarr_get(reni->textures, target->resolve_texture.h);
            if (!resolve_texture) RENI_ERR("Invalid resolve texture handle");
            out->resolveTarget = resolve_texture->view;
        }
    }

    WGPURenderPassDepthStencilAttachment depth = { 0 };
    if (config.depth.target.h.valid) {
        depth = (WGPURenderPassDepthStencilAttachment){
            .depthLoadOp = config.depth.clear ? WGPULoadOp_Clear : WGPULoadOp_Load,
            .depthClearValue = config.depth.clear_value,
            .depthStoreOp = WGPUStoreOp_Store
        };
        ReniTextureImpl* depth_texture = genarr_get(reni->textures, config.depth.target.h);
        if (!depth_texture) RENI_ERR("Invalid depth texture handle!");
        depth.view = depth_texture->view;
    }

    renderpass.render_pass = wgpuCommandEncoderBeginRenderPass(reni->encoder, &(WGPURenderPassDescriptor) {
        .colorAttachmentCount = n_targets,
        .colorAttachments = color_attachments,
        .depthStencilAttachment = config.depth.target.h.valid ? &depth : 0
    });

    return renderpass;
}

void reni_renderpass_set_shader(Reni* reni, ReniRenderpass* pass, ReniShader shader)
{
    ReniShaderImpl* impl = genarr_get(reni->shaders, shader.h);
    if (!impl) RENI_ERR("Invalid shader handle");
    wgpuRenderPassEncoderSetPipeline(pass->render_pass, impl->pipeline);
}

void reni_renderpass_set_binding(Reni* reni, ReniRenderpass* pass, u32 index, ReniBinding binding)
{
    wgpuRenderPassEncoderSetBindGroup(pass->render_pass, index, _reni_get_binding_bind_group(reni, binding), 0, nullptr);
}

STRUCT(ReniDrawConfig)
{
    ReniBuffer vertices;
    usize n_vertices; // only required if drawing without index buffer
    ReniBuffer indices;
    bool u32_indices; // default is u16
    ReniBuffer instances;
    usize n_instances;
};

void reni_renderpass_draw(Reni* reni, ReniRenderpass* pass, ReniDrawConfig config)
{
    if (config.vertices.h.valid) {
        ReniBufferImpl* vertices = genarr_get(reni->buffers, config.vertices.h);
        if (!vertices) RENI_ERR("Invalid vertices buffer handle!");
        wgpuRenderPassEncoderSetVertexBuffer(pass->render_pass, 0, vertices->buffer, 0, vertices->used);
    }

    if (config.instances.h.valid) {
        ReniBufferImpl* instances = genarr_get(reni->buffers, config.instances.h);
        if (!instances) RENI_ERR("Invalid instances buffer handle!");
        wgpuRenderPassEncoderSetVertexBuffer(pass->render_pass, 1, instances->buffer, 0, instances->used);
    }

    usize n_instances = config.n_instances ? config.n_instances : 1;
    if (config.indices.h.valid) {
        ReniBufferImpl* indices = genarr_get(reni->buffers, config.indices.h);
        if (!indices) RENI_ERR("Invalid indices buffer handle!");
        wgpuRenderPassEncoderSetIndexBuffer(pass->render_pass, indices->buffer, config.u32_indices ? WGPUIndexFormat_Uint32 : WGPUIndexFormat_Uint16, 0, indices->used);

        wgpuRenderPassEncoderDrawIndexed(pass->render_pass, indices->used / (config.u32_indices ? sizeof(u32) : sizeof(u16)), n_instances, 0, 0, 0);
    }
    else
    {
        wgpuRenderPassEncoderDraw(pass->render_pass, config.n_vertices, n_instances, 0, 0);
    }
}

void reni_submit_renderpass(Reni* reni, ReniRenderpass renderpass)
{
    mrw_unused reni;
    wgpuRenderPassEncoderEnd(renderpass.render_pass);
    wgpuRenderPassEncoderRelease(renderpass.render_pass);
}

typedef enum ReniSurfaceStatus {
    ReniSurfaceStatus_SuccessOptimal,
    ReniSurfaceStatus_SuccessSuboptimal,
    ReniSurfaceStatus_Timeout,
    ReniSurfaceStatus_Outdated,
    ReniSurfaceStatus_Lost,
    ReniSurfaceStatus_Error,
} ReniSurfaceStatus;

STRUCT(ReniSurfaceAcquired) {
    ReniSurfaceStatus status;
    ReniTexture texture;
};

ReniSurfaceAcquired reni_surface_acquire(Reni* reni, ReniSurface surface)
{
    ReniSurfaceImpl* impl = genarr_get(reni->surfaces, surface.h);
    if (!impl) RENI_ERR("Invalid surface handle");

    ReniTextureImpl* texture = genarr_get(reni->textures, impl->texture.h);
    if (!texture) RENI_ERR("Invalid texture handle");

    if (texture->view) wgpuTextureViewRelease(texture->view);
    if (texture->texture) wgpuTextureRelease(texture->texture);

    wgpuSurfaceGetCurrentTexture(impl->surface, &texture->surface_texture);
    texture->texture = texture->surface_texture.texture;
    texture->view = wgpuTextureCreateView(
        texture->surface_texture.texture,
        &(WGPUTextureViewDescriptor){
            .label = WEBGPU_STR("Surface texture view"),
            .format = wgpuTextureGetFormat(texture->surface_texture.texture),
            .dimension = WGPUTextureViewDimension_2D,
            .mipLevelCount = 1,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_All,
        }
    );
    texture->config = (ReniTextureConfig){
        .width = impl->state.width,
        .height = impl->state.height,
        .format = impl->format
    };

    impl->acquired = true;
    texture->gen++; // !!!!
    return (ReniSurfaceAcquired) {
        .status = ReniSurfaceStatus_SuccessOptimal, // TODO
        .texture = impl->texture
    };
}

void reni_begin(Reni* reni)
{
    if (reni->encoder) RENI_ERR("Begin called without a matching reni_end()");
    reni->encoder = wgpuDeviceCreateCommandEncoder(reni->device, &(WGPUCommandEncoderDescriptor){
        .label = WEBGPU_STR("Encoder dude")
    });
}

void reni_end(Reni* reni)
{
    if (!reni->encoder) RENI_ERR("End called without a matching reni_begin()");
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(reni->encoder, &(WGPUCommandBufferDescriptor){ .label = WEBGPU_STR("Command dude") });
    wgpuCommandEncoderRelease(reni->encoder); reni->encoder = nullptr;
    wgpuQueueSubmit(reni->queue, 1, &command);
    wgpuCommandBufferRelease(command);

    GENARR_ITER(ReniSurfaceImpl) iter = { 0 };
    while (genarr_next_valid(reni->surfaces, &iter)){
        ReniSurfaceImpl* impl = iter._val;
        if (!impl->acquired) continue;
        wgpuSurfacePresent(impl->surface);
        impl->acquired = false;
    }
}

#define RENI_BLEND_STATE_BLEND (ReniBlendComponent){\
    .src = ReniBlendFactor_SrcAlpha,\
    .dst = ReniBlendFactor_OneMinusSrcAlpha,\
    .op = ReniBlendOperation_Add\
}

#define RENI_BLEND_STATE_ADD (ReniBlendComponent){\
    .src = ReniBlendFactor_One,\
    .dst = ReniBlendFactor_One,\
    .op = ReniBlendOperation_Add\
}

#define RENI_BLEND_STATE_OVERWRITE (ReniBlendComponent){\
    .src = ReniBlendFactor_Zero,\
    .dst = ReniBlendFactor_One,\
    .op = ReniBlendOperation_Add\
}
