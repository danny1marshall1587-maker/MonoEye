// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "layer.h"
#include "logging.h"
#include "dispatch_table.h"
#include "config.h"
#include <cstring>
#include <new>

namespace monoeye {

XrResult LayerXrCreateApiLayerInstance(
    const XrInstanceCreateInfo* info,
    const XrApiLayerCreateInfo* apiLayerInfo,
    XrInstance* instance
) {
    MONOEYE_LOG("LayerXrCreateApiLayerInstance called");

    if (!apiLayerInfo || !info || !instance) {
        MONOEYE_LOG_ERROR("Invalid parameters to LayerXrCreateApiLayerInstance");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Validate the API layer info structure
    if (apiLayerInfo->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_CREATE_INFO ||
        apiLayerInfo->structVersion > XR_API_LAYER_CREATE_INFO_STRUCT_VERSION ||
        apiLayerInfo->structSize < sizeof(XrApiLayerCreateInfo)) {
        MONOEYE_LOG_ERROR("Invalid apiLayerInfo struct");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    if (!apiLayerInfo->nextInfo ||
        apiLayerInfo->nextInfo->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_NEXT_INFO ||
        apiLayerInfo->nextInfo->structVersion > XR_API_LAYER_NEXT_INFO_STRUCT_VERSION ||
        apiLayerInfo->nextInfo->structSize < sizeof(XrApiLayerNextInfo)) {
        MONOEYE_LOG_ERROR("Invalid nextInfo struct");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Load configuration
    Config config = load_config();
    (void)config;

    // Copy the layer info and advance the next pointer to pass to the next layer
    XrApiLayerCreateInfo newApiLayerInfo = *apiLayerInfo;
    newApiLayerInfo.nextInfo = apiLayerInfo->nextInfo->next;

    // Get the next layer's xrCreateApiLayerInstance function pointer
    PFN_xrCreateApiLayerInstance nextCreateInstance =
        apiLayerInfo->nextInfo->nextCreateApiLayerInstance;

    if (!nextCreateInstance) {
        MONOEYE_LOG_ERROR("nextCreateApiLayerInstance is null");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Call down to the next layer/runtime to create the instance
    XrResult result = nextCreateInstance(info, &newApiLayerInfo, instance);

    if (result != XR_SUCCESS) {
        MONOEYE_LOG_ERROR("xrCreateInstance failed with result: %d", result);
        return result;
    }

    MONOEYE_LOG("Instance created successfully: %p", (void*)(uintptr_t)*instance);

    // Capture the real downstream proc addr — this points to the real runtime,
    // NOT back to our layer. Store it globally and per-instance.
    PFN_xrGetInstanceProcAddr nextGetInstanceProcAddr =
        apiLayerInfo->nextInfo->nextGetInstanceProcAddr;

    monoeye::g_nextGetInstanceProcAddr = nextGetInstanceProcAddr;

    if (!nextGetInstanceProcAddr) {
        MONOEYE_LOG_ERROR("nextGetInstanceProcAddr is null");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Build a minimal dispatch table by resolving ONLY the functions MonoEye
    // actually calls internally. We deliberately do NOT call GeneratedXrPopulateDispatchTable
    // as it enumerates 500+ extension functions, each of which triggers a log line
    // and can cause feedback loops through the layer chain.
    auto* nextDispatch = new (std::nothrow) XrGeneratedDispatchTable();
    if (!nextDispatch) {
        MONOEYE_LOG_ERROR("Failed to allocate dispatch table");
        return XR_ERROR_OUT_OF_MEMORY;
    }

    // Zero-initialise then resolve only what we need
    *nextDispatch = {};
    nextDispatch->GetInstanceProcAddr = nextGetInstanceProcAddr;

    // Core functions MonoEye hooks call through to downstream
    nextGetInstanceProcAddr(*instance, "xrDestroyInstance",
        (PFN_xrVoidFunction*)&nextDispatch->DestroyInstance);
    nextGetInstanceProcAddr(*instance, "xrCreateSession",
        (PFN_xrVoidFunction*)&nextDispatch->CreateSession);
    nextGetInstanceProcAddr(*instance, "xrDestroySession",
        (PFN_xrVoidFunction*)&nextDispatch->DestroySession);
    nextGetInstanceProcAddr(*instance, "xrEnumerateViewConfigurationViews",
        (PFN_xrVoidFunction*)&nextDispatch->EnumerateViewConfigurationViews);
    nextGetInstanceProcAddr(*instance, "xrLocateViews",
        (PFN_xrVoidFunction*)&nextDispatch->LocateViews);
    nextGetInstanceProcAddr(*instance, "xrCreateSwapchain",
        (PFN_xrVoidFunction*)&nextDispatch->CreateSwapchain);
    nextGetInstanceProcAddr(*instance, "xrDestroySwapchain",
        (PFN_xrVoidFunction*)&nextDispatch->DestroySwapchain);
    nextGetInstanceProcAddr(*instance, "xrEnumerateSwapchainImages",
        (PFN_xrVoidFunction*)&nextDispatch->EnumerateSwapchainImages);
    nextGetInstanceProcAddr(*instance, "xrAcquireSwapchainImage",
        (PFN_xrVoidFunction*)&nextDispatch->AcquireSwapchainImage);
    nextGetInstanceProcAddr(*instance, "xrWaitSwapchainImage",
        (PFN_xrVoidFunction*)&nextDispatch->WaitSwapchainImage);
    nextGetInstanceProcAddr(*instance, "xrReleaseSwapchainImage",
        (PFN_xrVoidFunction*)&nextDispatch->ReleaseSwapchainImage);
    nextGetInstanceProcAddr(*instance, "xrBeginFrame",
        (PFN_xrVoidFunction*)&nextDispatch->BeginFrame);
    nextGetInstanceProcAddr(*instance, "xrEndFrame",
        (PFN_xrVoidFunction*)&nextDispatch->EndFrame);
    nextGetInstanceProcAddr(*instance, "xrGetVulkanGraphicsRequirements2KHR",
        (PFN_xrVoidFunction*)&nextDispatch->GetVulkanGraphicsRequirements2KHR);
    nextGetInstanceProcAddr(*instance, "xrGetVulkanGraphicsRequirementsKHR",
        (PFN_xrVoidFunction*)&nextDispatch->GetVulkanGraphicsRequirementsKHR);
    nextGetInstanceProcAddr(*instance, "xrGetVulkanGraphicsDevice2KHR",
        (PFN_xrVoidFunction*)&nextDispatch->GetVulkanGraphicsDevice2KHR);
    nextGetInstanceProcAddr(*instance, "xrGetVulkanGraphicsDeviceKHR",
        (PFN_xrVoidFunction*)&nextDispatch->GetVulkanGraphicsDeviceKHR);

    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        g_instance_dispatch_map[*instance] = nextDispatch;
    }

    MONOEYE_LOG("Dispatch table created for instance %p", (void*)(uintptr_t)*instance);

    return XR_SUCCESS;
}

XrResult LayerXrDestroyInstance(XrInstance instance) {
    MONOEYE_LOG("LayerXrDestroyInstance called for instance %p", (void*)(uintptr_t)instance);

    XrGeneratedDispatchTable* nextDispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        auto it = g_instance_dispatch_map.find(instance);
        if (it != g_instance_dispatch_map.end()) {
            nextDispatch = it->second;
            g_instance_dispatch_map.erase(it);
        }
    }

    if (nextDispatch) {
        // Call down to destroy the instance
        if (nextDispatch->xrDestroyInstance) {
            ((PFN_xrDestroyInstance)nextDispatch->xrDestroyInstance)(instance);
        }


        // Clean up the dispatch table
        MonoEyeCleanUpDispatchTable(nextDispatch);
    }

    MONOEYE_LOG("Instance %p destroyed", (void*)(uintptr_t)instance);
    return XR_SUCCESS;
}

} // namespace monoeye
