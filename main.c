#include <stdio.h>
#include <emscripten.h>
#include "webgpu_utils.h"

struct {
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurface surface;
    WGPUTextureFormat surface_format;
} renderer;

void main_loop(void* _)
{
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
                .clearValue = (WGPUColor){ .09f, .192f, .243f, 1.0f },
                .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED
            }
        });

        wgpuRenderPassEncoderEnd(render_pass);
        wgpuRenderPassEncoderRelease(render_pass);
    }

    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &(WGPUCommandBufferDescriptor){ .label = WEBGPU_STR("Command buffer") });
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(renderer.queue, 1, &command);
    wgpuCommandBufferRelease(command);

    wgpuTextureViewRelease(surface_texture_view);
    wgpuTextureRelease(surface_texture.texture);
}

int main(void)
{
    printf("hello !!\n");
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

    renderer.surface = get_surface(renderer.instance);
    if (!renderer.surface) mrw_error("Failed to get the surface");
    else mrw_debug("Succefully got the surface!");

    WGPUSurfaceCapabilities caps;
    wgpuSurfaceGetCapabilities(renderer.surface, renderer.adapter, &caps);
    renderer.surface_format = caps.formats[0];
    mrw_debug("Preferred surface format is {}", (u32)renderer.surface_format);

    wgpuSurfaceConfigure(renderer.surface, &(WGPUSurfaceConfiguration)
    {
        .width = 640,
        .height = 480,
        .format = renderer.surface_format,
        .usage = WGPUTextureUsage_RenderAttachment,
        .device = renderer.device,
        .presentMode = WGPUPresentMode_Fifo
    });

    emscripten_set_main_loop_arg(main_loop, nullptr, 0, true);

    return 1;
}
