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

extern "C" XrResult monoeye_xrEnumerateViewConfigurationViews(
    XrInstance instance,
    XrSystemId systemId,
    XrViewConfigurationType viewConfigurationType,
    uint32_t viewCapacityInput,
    uint32_t* viewCountOutput,
    XrViewConfigurationView* views
);

extern "C" XrResult monoeye_xrLocateViews(
    XrSession session,
    const XrViewLocateInfo* viewLocateInfo,
    XrViewState* viewState,
    uint32_t viewCapacityInput,
    uint32_t* viewCountOutput,
    XrView* views
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

extern "C" XrResult monoeye_xrGetVulkanGraphicsRequirements2KHR(
    XrInstance instance,
    XrSystemId systemId,
    XrGraphicsRequirementsVulkanKHR* graphicsRequirements
) {
    monoeye::XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(monoeye::g_instance_dispatch_mutex);
        auto it = monoeye::g_instance_dispatch_map.find(instance);
        if (it != monoeye::g_instance_dispatch_map.end()) {
            dispatch = it->second;
        }
    }

    if (!dispatch || !dispatch->xrGetVulkanGraphicsRequirements2KHR) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return ((PFN_xrGetVulkanGraphicsRequirements2KHR)dispatch->xrGetVulkanGraphicsRequirements2KHR)(instance, systemId, graphicsRequirements);

}


extern "C" XrResult monoeye_xrGetVulkanGraphicsDevice2KHR(
    XrInstance instance,
    const XrVulkanGraphicsDeviceGetInfoKHR* getInfo,
    VkPhysicalDevice* vkPhysicalDevice
) {
    monoeye::XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(monoeye::g_instance_dispatch_mutex);
        auto it = monoeye::g_instance_dispatch_map.find(instance);
        if (it != monoeye::g_instance_dispatch_map.end()) {
            dispatch = it->second;
        }
    }

    if (!dispatch || !dispatch->xrGetVulkanGraphicsDevice2KHR) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return ((PFN_xrGetVulkanGraphicsDevice2KHR)dispatch->xrGetVulkanGraphicsDevice2KHR)(instance, getInfo, vkPhysicalDevice);

}

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
    {"xrGetVulkanGraphicsRequirementsKHR",  (PFN_xrVoidFunction)monoeye_xrGetVulkanGraphicsRequirements2KHR},
    {"xrGetVulkanGraphicsDevice2KHR",       (PFN_xrVoidFunction)monoeye_xrGetVulkanGraphicsDevice2KHR},
    {"xrGetVulkanGraphicsDeviceKHR",        (PFN_xrVoidFunction)monoeye_xrGetVulkanGraphicsDevice2KHR},

    {"xrEnumerateViewConfigurationViews",   (PFN_xrVoidFunction)monoeye_xrEnumerateViewConfigurationViews},
    {"xrLocateViews",                       (PFN_xrVoidFunction)monoeye_xrLocateViews},
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

    // Check if this is a function we hook — return our override immediately
    for (int i = 0; s_hooked_functions[i].name != nullptr; ++i) {
        if (strcmp(name, s_hooked_functions[i].name) == 0) {
            *function = s_hooked_functions[i].function;
            MONOEYE_LOG_DEBUG("  -> returning hooked function");
            return XR_SUCCESS;
        }
    }

    // NOT a hooked function — pass COMPLETELY transparently to the real runtime.
    // We MUST use g_nextGetInstanceProcAddr (the pointer given to us by the loader
    // at layer creation time) rather than anything from our dispatch table, to
    // avoid any chance of routing back to ourselves and causing an infinite loop.
    if (monoeye::g_nextGetInstanceProcAddr) {
        return monoeye::g_nextGetInstanceProcAddr(instance, name, function);
    }

    // Fallback: try the per-instance dispatch table
    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        auto it = g_instance_dispatch_map.find(instance);
        if (it != g_instance_dispatch_map.end()) {
            dispatch = it->second;
        }
    }

    if (dispatch && dispatch->nextGetInstanceProcAddr &&
        dispatch->nextGetInstanceProcAddr != (PFN_xrGetInstanceProcAddr)LayerXrGetInstanceProcAddr) {
        return dispatch->nextGetInstanceProcAddr(instance, name, function);
    }

    MONOEYE_LOG_ERROR("xrGetInstanceProcAddr: no downstream handler for '%s'", name);
    *function = nullptr;
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}


} // namespace monoeye
