// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "layer.h"
#include "logging.h"
#include "dispatch_table.h"
#include "swapchain_tracker.h"
#include "warp_pipeline.h"
#include "config.h"
#include <openxr/openxr_platform.h>
#include <vulkan/vulkan.h>

#include <mutex>

namespace monoeye {

extern std::mutex s_session_map_mutex;
extern std::unordered_map<XrSession, XrInstance> s_session_map;

XrResult monoeye_xrCreateSession(
    XrInstance instance,
    const XrSessionCreateInfo* createInfo,
    XrSession* session
) {
    MONOEYE_LOG("xrCreateSession called");

    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        auto it = g_instance_dispatch_map.find(instance);
        if (it != g_instance_dispatch_map.end()) {
            dispatch = it->second;
        }
    }

    if (!dispatch || !dispatch->CreateSession) {
        MONOEYE_LOG_ERROR("No dispatch table or CreateSession function");
        return XR_ERROR_RUNTIME_FAILURE;
    }

    // Call down to create the session
    XrResult result = ((PFN_xrCreateSession)dispatch->CreateSession)(instance, createInfo, session);

    if (result != XR_SUCCESS) {
        MONOEYE_LOG_ERROR("xrCreateSession failed: %d", result);
        return result;
    }

    MONOEYE_LOG("Session created: %p", (void*)(uintptr_t)*session);

    // Map session to instance for later lookups
    {
        std::lock_guard<std::mutex> lock(s_session_map_mutex);
        s_session_map[*session] = instance;
    }

    // Initialize the Vulkan warp pipeline if enabled
    const Config& config = get_config();
    if (config.enabled && !config.bypass_mode) {
        if (createInfo && createInfo->next) {
            // Check if this is a Vulkan session
            const XrGraphicsBindingVulkan2KHR* vkBinding =
                reinterpret_cast<const XrGraphicsBindingVulkan2KHR*>(createInfo->next);

            // Walk the next chain to find the Vulkan binding
            const XrBaseInStructure* header = createInfo->next;
            while (header) {
                if (header->type == XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR ||
                    header->type == XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR) {

                    const XrGraphicsBindingVulkan2KHR* vb =
                        reinterpret_cast<const XrGraphicsBindingVulkan2KHR*>(header);

                    MONOEYE_LOG("Vulkan session detected: device=%p, instance=%p, queueFamily=%u",
                        (void*)vb->device, (void*)vb->instance, vb->queueFamilyIndex);

                    // Initialize the warp pipeline with this Vulkan device
                    WarpPipeline::get_instance().initialize(
                        vb->instance,
                        vb->device,
                        vb->queueFamilyIndex
                    );

                    break;
                }
                header = header->next;
            }
        }
    }

    return XR_SUCCESS;
}

XrResult monoeye_xrDestroySession(XrSession session) {
    MONOEYE_LOG("xrDestroySession called: %p", (void*)(uintptr_t)session);

    // Find the instance for this session
    XrInstance instance = XR_NULL_HANDLE;
    {
        std::lock_guard<std::mutex> lock(s_session_map_mutex);
        auto it = s_session_map.find(session);
        if (it != s_session_map.end()) {
            instance = it->second;
            s_session_map.erase(it);
        }
    }

    // Shut down the warp pipeline
    WarpPipeline::get_instance().shutdown();

    XrGeneratedDispatchTable* dispatch = nullptr;
    if (instance != XR_NULL_HANDLE) {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        auto it = g_instance_dispatch_map.find(instance);
        if (it != g_instance_dispatch_map.end()) {
            dispatch = it->second;
        }
    }

    if (!dispatch || !dispatch->DestroySession) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return ((PFN_xrDestroySession)dispatch->DestroySession)(session);
}

} // namespace monoeye
