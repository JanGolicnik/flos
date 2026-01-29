#include <emscripten.h>
#include <marrow/marrow.h>
#include <marrow/allocator.h>
#include <marrow/webgpu_utils.h>
#define RIPPLE_IMPLEMENTATION
#include <ripple/ripple.h>
#include <ripple/ripple_widgets.h>

struct {
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurface surface;
    WGPUTextureFormat surface_format;
    u32 width, height;
    RippleContext ripple_context;
} renderer;

struct {
    f32 prev_time;
    f32 dt_accum;
    f32 avg_fps;
    u32 dt_n_samples;

    BumpAllocator frame_allocator;
} game;

void render_debug_info(void)
{
    RIPPLE( FORM( .width = RELATIVE(1.0f, SVT_RELATIVE_CHILD), .height = RELATIVE(1.0f, SVT_RELATIVE_CHILD)), RECTANGLE( .color = { 0x2e2e2e }))
    {
        text(mrw_format("hello! you are running at {} fps.", (Allocator*)&game.frame_allocator, game.avg_fps));
    }
}

void main_loop(void* _)
{
    f32 now = emscripten_get_now() * 0.001;
    f32 dt = now - game.prev_time;
    game.dt_accum += dt;
    game.dt_n_samples += 1;
    if (game.dt_accum > 0.3f)
    {
        game.avg_fps = (f32)game.dt_n_samples / game.dt_accum;
        game.dt_accum -= 0.3f;
        game.dt_n_samples = 0;
    }
    game.prev_time = now;

    render_debug_info();

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
                .clearValue = (WGPUColor){ 9.0f / 255.0f, 99.0f / 255.0f, 126.0f / 255.0f, 1.0f },
                .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED
            }
        });

        wgpuRenderPassEncoderEnd(render_pass);
        wgpuRenderPassEncoderRelease(render_pass);
    }

    ripple_submit(&renderer.ripple_context, renderer.width, renderer.height, (RippleRenderData)
    {
        .queue = renderer.queue,
        .device = renderer.device,
        .encoder = encoder,
        .texture_view = surface_texture_view
    });

    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &(WGPUCommandBufferDescriptor){ .label = WEBGPU_STR("Command buffer") });
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(renderer.queue, 1, &command);
    wgpuCommandBufferRelease(command);

    wgpuTextureViewRelease(surface_texture_view);
    wgpuTextureRelease(surface_texture.texture);

    bump_allocator_reset(&game.frame_allocator);
}

void init_renderer(void)
{
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

    renderer.width = 800;
    renderer.height = 800;

    wgpuSurfaceConfigure(renderer.surface, &(WGPUSurfaceConfiguration)
    {
        .width = renderer.width,
        .height = renderer.height,
        .format = renderer.surface_format,
        .usage = WGPUTextureUsage_RenderAttachment,
        .device = renderer.device,
        .presentMode = WGPUPresentMode_Fifo
    });
    renderer.ripple_context = ripple_initialize((RippleBackendRendererConfig){
        .device = renderer.device,
        .queue = renderer.queue,
        .surface_format = renderer.surface_format
    });
    ripple_make_active_context(&renderer.ripple_context);
    ripple_emscripten_register_callbacks(&renderer.ripple_context, "#canvas");
}

void init_game(void)
{
    game.frame_allocator = bump_allocator_create();
}

int main(void)
{
    init_renderer();
    init_game();

    emscripten_set_main_loop_arg(main_loop, nullptr, 0, true);

    return 1;
}
