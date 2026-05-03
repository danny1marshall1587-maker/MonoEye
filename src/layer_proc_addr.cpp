// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "layer.h"
#include "logging.h"
#include "dispatch_table.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <vulkan/vulkan.h>
#include <openxr/openxr_platform.h>
#include <cstring>

// Forward declarations of our hooked functions
extern "C" XrResult monoeye_xrBeginFrame(
    XrSession session,
    const XrFrameBeginInfo* frameBeginInfo
);

extern "C" XrResult monoeye_xrEndFrame(
    XrSession session,
    const XrFrameEndInfo* frameEndInfo
);

extern "C" XrResult monoeye_xrCreateSession(
    XrInstance instance,
    const XrSessionCreateInfo* createInfo,
    XrSession* session
);

extern "C" XrResult monoeye_xrDestroySession(
    XrSession session
);

extern "C" XrResult monoeye_xrCreateSwapchain(
    XrSession session,
    const XrSwapchainCreateInfo* createInfo,
    XrSwapchain* swapchain
);

extern "C" XrResult monoeye_xrDestroySwapchain(
    XrSwapchain swapchain
);

extern "C" XrResult monoeye_xrEnumerateSwapchainImages(
    XrSwapchain swapchain,
    uint32_t swapchainImageCapacityInput,
    uint32_t* swapchainImageCountOutput,
    XrSwapchainImageBaseHeader* swapchainImages
);

extern "C" XrResult monoeye_xrAcquireSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageAcquireInfo* acquireInfo,
    uint32_t* index
);

extern "C" XrResult monoeye_xrWaitSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageWaitInfo* waitInfo
);

extern "C" XrResult monoeye_xrReleaseSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageReleaseInfo* releaseInfo
);

// Vulkan-specific hooks
extern "C" XrResult monoeye_xrGetVulkanGraphicsRequirements2KHR(
    XrInstance instance,
    XrSystemId systemId,
    XrGraphicsRequirementsVulkan2KHR* graphicsRequirements
);

extern "C" XrResult monoeye_xrGetVulkanGraphicsDevice2KHR(
    XrInstance instance,
    const XrGraphicsBindingVulkan2KHR* graphicsBinding,
    VkPhysicalDevice* vkPhysicalDevice
);

namespace monoeye {

struct HookedFunction {
    const char* name;
    PFN_xrVoidFunction function;
};

// Table of functions we hook
static const HookedFunction s_hooked_functions[] = {
    {"xrBeginFrame",            (PFN_xrVoidFunction)monoeye_xrBeginFrame},
    {"xrEndFrame",              (PFN_xrVoidFunction)monoeye_xrEndFrame},
    {"xrCreateSession",         (PFN_xrVoidFunction)monoeye_xrCreateSession},
    {"xrDestroySession",        (PFN_xrVoidFunction)monoeye_xrDestroySession},
    {"xrCreateSwapchain",       (PFN_xrVoidFunction)monoeye_xrCreateSwapchain},
    {"xrDestroySwapchain",      (PFN_xrVoidFunction)monoeye_xrDestroySwapchain},
    {"xrEnumerateSwapchainImages", (PFN_xrVoidFunction)monoeye_xrEnumerateSwapchainImages},
    {"xrAcquireSwapchainImage", (PFN_xrVoidFunction)monoeye_xrAcquireSwapchainImage},
    {"xrWaitSwapchainImage",    (PFN_xrVoidFunction)monoeye_xrWaitSwapchainImage},
    {"xrReleaseSwapchainImage", (PFN_xrVoidFunction)monoeye_xrReleaseSwapchainImage},
    {"xrGetVulkanGraphicsRequirements2KHR", (PFN_xrVoidFunction)monoeye_xrGetVulkanGraphicsRequirements2KHR},
    {"xrGetVulkanGraphicsDevice2KHR",       (PFN_xrVoidFunction)monoeye_xrGetVulkanGraphicsDevice2KHR},
    {nullptr, nullptr}
};

XrResult LayerXrGetInstanceProcAddr(
    XrInstance instance,
    const char* name,
    PFN_xrVoidFunction* function
) {
    if (!name || !function) {
        return XR_ERROR_HANDLE_INVALID;
    }

    MONOEYE_LOG_DEBUG("xrGetInstanceProcAddr: %s", name);

    // Check if this is a function we hook
    for (int i = 0; s_hooked_functions[i].name != nullptr; ++i) {
        if (strcmp(name, s_hooked_functions[i].name) == 0) {
            *function = s_hooked_functions[i].function;
            MONOEYE_LOG_DEBUG("  -> returning hooked function");
            return XR_SUCCESS;
        }
    }

    // Not a hooked function - get it from the next layer/runtime
    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        auto it = g_instance_dispatch_map.find(instance);
        if (it != g_instance_dispatch_map.end()) {
            dispatch = it->second;
        }
    }

    if (!dispatch) {
        MONOEYE_LOG_ERROR("No dispatch table found for instance %p", (void*)(uintptr_t)instance);
        return XR_ERROR_HANDLE_INVALID;
    }

    *function = nullptr;
    return XR_SUCCESS; // The loader handles passthrough for non-hooked functions
}

} // namespace monoeye
