#include "marrow/allocator.h"
#include "marrow/genarr.h"
#include "marrow/marrow.h"
#include "webgpu/webgpu.h"
#define FLOS_RENI
#include "base.c"

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

#define RENI_ERR(args) mrw_error("reni error !!: " args)
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
    RENI_BACKEND_WINDOW window;
    ReniPresentMode mode;
};

STRUCT(ReniSurfaceState) {
    u32 width;
    u32 height;
    ReniTextureFormat format;
};

RENI_STRUCT(ReniSurface) {
    ReniSurfaceConfig config;
    ReniSurfaceState state;

    ReniTexture texture;
    bool acquired;

    WGPUSurface surface;
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

    WGPUCommandEncoder encoder; // reni_begin()

    GENARR(ReniSurfaceImpl) surfaces;
    GENARR(ReniBufferImpl) buffers;
    GENARR(ReniShaderImpl) shaders;
    GENARR(ReniTextureImpl) textures;
    GENARR(ReniSamplerImpl) samplers;
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
    Reni reni = { .instance = wgpuCreateInstance(nullptr) };

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
            .callback = request_device_callback,
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

    return reni;
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
    if (FLAG_HAS(usage, ReniBufferUsage_Undefined)) FLAG_SET(ret, WGPUBufferUsage_None);
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

ReniSurface reni_create_surface(Reni* reni, ReniSurfaceConfig config)
{
    if (!config.mode) config.mode = ReniPresentMode_Fifo;

    ReniSurfaceImpl impl = { .config = config, .surface = _reni_get_surface(reni->instance, config.window) };
    if (!impl.surface) return (ReniSurface){ 0 };

    WGPUSurfaceCapabilities caps; wgpuSurfaceGetCapabilities(impl.surface, reni->adapter, &caps);
    impl.state.format = _reni_texture_format_from_wgpu(caps.formats[0]);

    return (ReniSurface){ .h = genarr_add(reni->surfaces, impl) };
}

void reni_surface_update(Reni* reni, ReniSurface surface, ReniSurfaceState state)
{
    ReniSurfaceImpl* impl = genarr_get(reni->surfaces, surface.h);
    if (!impl) RENI_ERR("Invalid surface handle");

    if (state.width == impl->state.width &&
        state.height == impl->state.height &&
        state.format == impl->state.format
    ) return;

    impl->state = state;
    wgpuSurfaceConfigure(impl->surface,
        &(WGPUSurfaceConfiguration){
            .width = state.width,
            .height = state.height,
            .device = reni->device,
            .usage = WGPUTextureUsage_RenderAttachment,
            .format = _reni_texture_format_to_wgpu(state.format),
            .presentMode = _reni_present_mode_to_wgpu(impl->config.mode),
        }
    );
}

void reni_release_surface(Reni* reni, ReniSurface surface)
{
    ReniSurfaceImpl* impl = genarr_get(reni->surfaces, surface.h);
    if (!impl) RENI_ERR("Invalid surface handle");
    wgpuSurfaceRelease(impl->surface);
    genarr_remove(reni->surfaces, surface.h);
}

ReniTexture reni_create_texture(Reni*, ReniTextureConfig);
const ReniTextureConfig* reni_get_texture_config(Reni*, ReniTexture);
ReniTexture reni_recreate_texture(Reni*, ReniTexture, ReniTextureConfig);
void reni_resize_texture(Reni*, ReniTexture, u32 w, u32 h); // keeps prev config
void reni_release_texture(Reni*, ReniTexture);

ReniTextureFormat reni_get_texture_format(Reni*, ReniTexture);

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
    return;
}

ReniBuffer reni_create_buffer(Reni* reni, ReniBufferConfig config)
{
    ReniBuffer buffer = {
        .h = genarr_add(reni->buffers, (ReniBufferImpl){
            .config.name = config.name,
            .config.usage = config.usage
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

ReniBinding reni_create_binding(Reni*, ReniBinding);
const ReniBinding* reni_get_binding_config(Reni*, ReniBinding);
ReniBinding reni_recreate_binding(Reni*, ReniBinding, ReniBinding);
void reni_release_binding(Reni*, ReniBinding);

ReniShader reni_create_shader(Reni*, ReniShaderConfig, str path, strSlice includes);
const ReniShaderConfig* reni_get_shader_config(Reni*, ReniShader);
ReniShader reni_recreate_shader(Reni*, ReniShader, ReniShaderConfig, str path, strSlice includes);
void reni_release_shader(Reni*, ReniShader);

// id like to figure out a better api than the one webgpu does but that also comes at a later date
ReniRenderpass reni_create_renderpass(Reni*, ReniRenderpassConfig);
void reni_renderpass_set_shader(ReniRenderpass, ReniShader, ReniBindingSlice bindings);

STRUCT(DrawConfig)
{
    ReniBuffer vertex;
    u32 n_vertices;
    ReniBuffer index;
    bool u8_indices;
    u32 n_indices;
    ReniBuffer instance;
    u32 n_instances;
};
void reni_renderpass_draw(ReniRenderpass, DrawConfig);
void reni_submit_renderpass(Reni*, ReniRenderpass);

typedef enum ReniSurfaceStatus {
    ReniSurfaceStatus_Ok,
    ReniSurfaceStatus_Error,
    ReniSurfaceStatus_Reconfigured,
} ReniSurfaceStatus;

STRUCT(ReniSurfaceAcquired) {
    ReniSurfaceStatus status;
    ReniTexture texture;
};

// also bumps the textures inner bind group gen handle so any that were referencing it get re-created
ReniSurfaceAcquired reni_surface_acquire(Reni*, ReniSurface);

void reni_begin(Reni*);
// submits all acquired surfaces
void reni_end(Reni*);
