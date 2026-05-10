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
extern "C" XrResult XRAPI_CALL monoeye_xrBeginFrame(
    XrSession session,
    const XrFrameBeginInfo* frameBeginInfo
);

extern "C" XrResult XRAPI_CALL monoeye_xrEndFrame(
    XrSession session,
    const XrFrameEndInfo* frameEndInfo
);

extern "C" XrResult XRAPI_CALL monoeye_xrCreateSession(
    XrInstance instance,
    const XrSessionCreateInfo* createInfo,
    XrSession* session
);

extern "C" XrResult XRAPI_CALL monoeye_xrDestroySession(
    XrSession session
);

extern "C" XrResult XRAPI_CALL monoeye_xrEnumerateViewConfigurationViews(
    XrInstance instance,
    XrSystemId systemId,
    XrViewConfigurationType viewConfigurationType,
    uint32_t viewCapacityInput,
    uint32_t* viewCountOutput,
    XrViewConfigurationView* views
);

extern "C" XrResult XRAPI_CALL monoeye_xrLocateViews(
    XrSession session,
    const XrViewLocateInfo* viewLocateInfo,
    XrViewState* viewState,
    uint32_t viewCapacityInput,
    uint32_t* viewCountOutput,
    XrView* views
);

extern "C" XrResult XRAPI_CALL monoeye_xrCreateSwapchain(
    XrSession session,
    const XrSwapchainCreateInfo* createInfo,
    XrSwapchain* swapchain
);

extern "C" XrResult XRAPI_CALL monoeye_xrDestroySwapchain(
    XrSwapchain swapchain
);

extern "C" XrResult XRAPI_CALL monoeye_xrLocateSpace(
    XrSpace space,
    XrSpace baseSpace,
    XrTime time,
    XrSpaceLocation* location
);

extern "C" XRAPI_ATTR XrResult XRAPI_CALL LayerXrDestroyInstance(
    XrInstance instance
);

extern "C" XrResult XRAPI_CALL monoeye_xrEnumerateSwapchainImages(
    XrSwapchain swapchain,
    uint32_t swapchainImageCapacityInput,
    uint32_t* swapchainImageCountOutput,
    XrSwapchainImageBaseHeader* swapchainImages
);

extern "C" XrResult XRAPI_CALL monoeye_xrAcquireSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageAcquireInfo* acquireInfo,
    uint32_t* index
);

extern "C" XrResult XRAPI_CALL monoeye_xrWaitSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageWaitInfo* waitInfo
);

extern "C" XrResult XRAPI_CALL monoeye_xrReleaseSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageReleaseInfo* releaseInfo
);

extern "C" XrResult XRAPI_CALL monoeye_xrGetVulkanGraphicsRequirementsKHR(
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

    if (!dispatch || !dispatch->xrGetVulkanGraphicsRequirementsKHR) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return ((PFN_xrGetVulkanGraphicsRequirementsKHR)dispatch->xrGetVulkanGraphicsRequirementsKHR)(instance, systemId, graphicsRequirements);
}

extern "C" XrResult XRAPI_CALL monoeye_xrGetVulkanGraphicsRequirements2KHR(
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


extern "C" XrResult XRAPI_CALL monoeye_xrGetVulkanGraphicsDeviceKHR(
    XrInstance instance,
    XrSystemId systemId,
    VkInstance vkInstance,
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

    if (!dispatch || !dispatch->xrGetVulkanGraphicsDeviceKHR) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return ((PFN_xrGetVulkanGraphicsDeviceKHR)dispatch->xrGetVulkanGraphicsDeviceKHR)(instance, systemId, vkInstance, vkPhysicalDevice);
}

extern "C" XrResult XRAPI_CALL monoeye_xrGetVulkanGraphicsDevice2KHR(
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


#ifdef _WIN32
extern "C" XrResult XRAPI_CALL monoeye_xrGetD3D11GraphicsRequirementsKHR(
    XrInstance instance,
    XrSystemId systemId,
    XrGraphicsRequirementsD3D11KHR* graphicsRequirements
) {
    monoeye::XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(monoeye::g_instance_dispatch_mutex);
        auto it = monoeye::g_instance_dispatch_map.find(instance);
        if (it != monoeye::g_instance_dispatch_map.end()) {
            dispatch = it->second;
        }
    }
    if (!dispatch || !dispatch->xrGetD3D11GraphicsRequirementsKHR) return XR_ERROR_RUNTIME_FAILURE;
    return ((PFN_xrGetD3D11GraphicsRequirementsKHR)dispatch->xrGetD3D11GraphicsRequirementsKHR)(instance, systemId, graphicsRequirements);
}

extern "C" XrResult XRAPI_CALL monoeye_xrGetD3D12GraphicsRequirementsKHR(
    XrInstance instance,
    XrSystemId systemId,
    XrGraphicsRequirementsD3D12KHR* graphicsRequirements
) {
    monoeye::XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(monoeye::g_instance_dispatch_mutex);
        auto it = monoeye::g_instance_dispatch_map.find(instance);
        if (it != monoeye::g_instance_dispatch_map.end()) {
            dispatch = it->second;
        }
    }
    if (!dispatch || !dispatch->xrGetD3D12GraphicsRequirementsKHR) return XR_ERROR_RUNTIME_FAILURE;
    return ((PFN_xrGetD3D12GraphicsRequirementsKHR)dispatch->xrGetD3D12GraphicsRequirementsKHR)(instance, systemId, graphicsRequirements);
}
#endif


using namespace monoeye;

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
    {"xrGetVulkanGraphicsRequirementsKHR",  (PFN_xrVoidFunction)monoeye_xrGetVulkanGraphicsRequirementsKHR},
    {"xrGetVulkanGraphicsDevice2KHR",       (PFN_xrVoidFunction)monoeye_xrGetVulkanGraphicsDevice2KHR},
    {"xrGetVulkanGraphicsDeviceKHR",        (PFN_xrVoidFunction)monoeye_xrGetVulkanGraphicsDeviceKHR},
#ifdef _WIN32
    {"xrGetD3D11GraphicsRequirementsKHR", (PFN_xrVoidFunction)monoeye_xrGetD3D11GraphicsRequirementsKHR},
    {"xrGetD3D12GraphicsRequirementsKHR", (PFN_xrVoidFunction)monoeye_xrGetD3D12GraphicsRequirementsKHR},
#endif
    {"xrEnumerateViewConfigurationViews",   (PFN_xrVoidFunction)monoeye_xrEnumerateViewConfigurationViews},
    {"xrLocateViews",                       (PFN_xrVoidFunction)monoeye_xrLocateViews},
    {"xrDestroyInstance",                   (PFN_xrVoidFunction)LayerXrDestroyInstance},
    {"xrLocateSpace",                       (PFN_xrVoidFunction)monoeye_xrLocateSpace},
    {nullptr, nullptr}
};

extern bool g_process_bypass;

extern "C" XRAPI_ATTR XrResult XRAPI_CALL LayerXrGetInstanceProcAddr(
    XrInstance instance,
    const char* name,
    PFN_xrVoidFunction* function
) {
    if (!name || !function) {
        return XR_ERROR_HANDLE_INVALID;
    }

    MONOEYE_LOG_DEBUG("xrGetInstanceProcAddr: %s", name);

    // 1. Check if this is a function we hook — return our override immediately
    if (!g_process_bypass) {
        for (int i = 0; s_hooked_functions[i].name != nullptr; ++i) {
            if (strcmp(name, s_hooked_functions[i].name) == 0) {
                *function = s_hooked_functions[i].function;
                MONOEYE_LOG_DEBUG("  -> returning hooked function");
                return XR_SUCCESS;
            }
        }
    } else {
        MONOEYE_LOG_DEBUG("  -> bypassing hook due to process filter");
    }

    // 2. Try the per-instance dispatch table. This is the most accurate
    // way to find the "next" function in the chain for a specific instance.
    if (instance != XR_NULL_HANDLE) {
        XrGeneratedDispatchTable* dispatch = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
            auto it = g_instance_dispatch_map.find(instance);
            if (it != g_instance_dispatch_map.end()) {
                dispatch = it->second;
            }
        }

        if (dispatch && dispatch->nextGetInstanceProcAddr) {
            MONOEYE_LOG_DEBUG("  -> forwarding to instance nextGetInstanceProcAddr");
            XrResult res = dispatch->nextGetInstanceProcAddr(instance, name, function);
            if (res == XR_SUCCESS && *function == nullptr) {
                return XR_ERROR_FUNCTION_UNSUPPORTED;
            }
            return res;
        }
    }

    // 3. Fallback to the global pointer (used for NULL instance calls or 
    // if the instance isn't in our map yet).
    if (monoeye::g_nextGetInstanceProcAddr) {
        MONOEYE_LOG_DEBUG("  -> forwarding to global g_nextGetInstanceProcAddr");
        XrResult res = monoeye::g_nextGetInstanceProcAddr(instance, name, function);
        if (res == XR_SUCCESS && *function == nullptr) {
            return XR_ERROR_FUNCTION_UNSUPPORTED;
        }
        return res;
    }

    MONOEYE_LOG_ERROR("xrGetInstanceProcAddr: no downstream handler for '%s' (instance: %p). "
                      "The call chain might be broken.", 
                      name, (void*)instance);
    *function = nullptr;
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}
