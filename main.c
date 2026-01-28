#include <stdio.h>
#include <emscripten.h>
#include "webgpu_utils.h"

int main(void)
{
    printf("hello !!\n");
    WGPUInstance instance = wgpuCreateInstance(NULL);
    if (!instance) mrw_error("Failed to create WebGPU instance.");
    else mrw_debug("Successfully created the WebGPU instance!");

    WGPUAdapter adapter = get_adapter(instance, (WGPURequestAdapterOptions){ .powerPreference = WGPUPowerPreference_HighPerformance });
    if (!adapter) mrw_error("Failed to get the adapter!");
    else mrw_debug("Successfully got the adapter!");

    WGPUDevice device = get_device(adapter);
    if (!device) mrw_error("Failed to get the device!");
    else mrw_debug("Succesfully got the device!");

    return 1;
}
