#include <webgpu/webgpu.h>
#include <marrow/marrow.h>

#define WEBGPU_STR_EXACT(str) (WGPUStringView) { .data = str, .length = sizeof(str) - 1 }
#define WEBGPU_STR(str) (WGPUStringView) { .data = str, .length = WGPU_STRLEN }

STRUCT(RequestAdapterUserData) {
    WGPUAdapter adapter;
    int request_done;
};

STRUCT(RequestDeviceUserData) {
    WGPUDevice device;
    int request_done;
};

static void request_adapter_callback(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2)
{
    if (status != WGPURequestAdapterStatus_Success)
        mrw_error("Failed to get WebGPU adapter");

    RequestAdapterUserData* callback_user_data = userdata1;
    callback_user_data->adapter = adapter;
    callback_user_data->request_done = 1;
}

static WGPUAdapter get_adapter(WGPUInstance instance, WGPURequestAdapterOptions request_adapter_options)
{
    RequestAdapterUserData request_adapter_user_data = { 0 };
    wgpuInstanceRequestAdapter(instance, &request_adapter_options, (WGPURequestAdapterCallbackInfo) {
        .mode = WGPUCallbackMode_AllowSpontaneous ,
        .callback = request_adapter_callback,
        .userdata1 = &request_adapter_user_data
    });
    while(!request_adapter_user_data.request_done) emscripten_sleep(100);
    return request_adapter_user_data.adapter;
}

static void request_device_callback(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* userdata2)
{
    if (status != WGPURequestDeviceStatus_Success)
    {
        mrw_debug("Failed to get WebGPU device");
    }

    RequestDeviceUserData* callback_user_data = userdata1;
    callback_user_data->device = device;
    callback_user_data->request_done = 1;
}

static void device_uncaptured_error_callback(WGPUDevice const* device, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2)
{
    if (message.length == WGPU_STRLEN)
    {
        mrw_error("Uncaptured device error ({}): {}", (u32)type, message.data);
    }
    else
    {
        char data[message.length + 1];
        buf_copy(data, message.data, message.length + 1);
        data[message.length] = 0;
        mrw_error("Uncaptured device error ({}): {}", (u32)type, data);
    }
}

static WGPUDevice get_device(WGPUAdapter adapter)
{
    WGPUDeviceDescriptor device_descriptor = {
        .label = WEBGPU_STR("Device :D"),
        .defaultQueue.label = WEBGPU_STR("Default queue"),
        .uncapturedErrorCallbackInfo = (WGPUUncapturedErrorCallbackInfo) {
            .callback = device_uncaptured_error_callback,
        }
    };
    RequestDeviceUserData request_device_user_data = { 0 };
    wgpuAdapterRequestDevice(adapter, &device_descriptor,
        (WGPURequestDeviceCallbackInfo){
            .mode = WGPUCallbackMode_AllowSpontaneous,
            .callback = request_device_callback,
            .userdata1 = &request_device_user_data
        }
    );
    while(!request_device_user_data.request_done) emscripten_sleep(100);
    return request_device_user_data.device;
}

static WGPUSurface get_surface(WGPUInstance instance)
{
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector fromCanvasHTMLSelector = { 0 };
    fromCanvasHTMLSelector.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    fromCanvasHTMLSelector.selector = (WGPUStringView){ "canvas", WGPU_STRLEN };
    WGPUSurfaceDescriptor surfaceDescriptor;
    surfaceDescriptor.nextInChain = &fromCanvasHTMLSelector.chain;
    surfaceDescriptor.label = (WGPUStringView){ NULL, WGPU_STRLEN };
    return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
}
