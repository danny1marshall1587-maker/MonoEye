// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "layer.h"
#include "logging.h"
#include "dispatch_table.h"
#include "swapchain_tracker.h"
#include <vulkan/vulkan.h>
#include <openxr/openxr_platform.h>

#include <mutex>
#include <vector>

namespace monoeye {

// Track which instance owns each session
std::mutex s_session_map_mutex;
std::unordered_map<XrSession, XrInstance> s_session_map;

extern "C" XrResult monoeye_xrCreateSwapchain(
    XrSession session,
    const XrSwapchainCreateInfo* createInfo,
    XrSwapchain* swapchain
) {
    MONOEYE_LOG("xrCreateSwapchain: %dx%d, fmt=%d, samples=%d, usage=0x%x",
        createInfo->width, createInfo->height,
        createInfo->format, createInfo->sampleCount,
        createInfo->usageFlags);

    XrInstance instance = XR_NULL_HANDLE;
    {
        std::lock_guard<std::mutex> lock(s_session_map_mutex);
        auto it = s_session_map.find(session);
        if (it != s_session_map.end()) {
            instance = it->second;
        }
    }

    if (instance == XR_NULL_HANDLE) {
        MONOEYE_LOG_ERROR("No instance found for session %p", (void*)(uintptr_t)session);
        return XR_ERROR_SESSION_LOST;
    }

    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        auto it = g_instance_dispatch_map.find(instance);
        if (it != g_instance_dispatch_map.end()) {
            dispatch = it->second;
        }
    }

    if (!dispatch || !dispatch->xrCreateSwapchain) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    XrResult result = ((PFN_xrCreateSwapchain)dispatch->xrCreateSwapchain)(session, createInfo, swapchain);

    if (result == XR_SUCCESS) {
        uint32_t imageCount = 0;
        ((PFN_xrEnumerateSwapchainImages)dispatch->xrEnumerateSwapchainImages)(*swapchain, 0, &imageCount, nullptr);


        if (imageCount > 0) {
            // Allocate Vulkan image array
#ifdef _WIN32
            std::vector<XrSwapchainImageVulkanKHR> vkImages(imageCount);
#else
            std::vector<XrSwapchainImageVulkanKHR> vkImages(imageCount);
#endif
            for (uint32_t i = 0; i < imageCount; ++i) {
                vkImages[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
                vkImages[i].next = nullptr;
                vkImages[i].image = VK_NULL_HANDLE;
            }

            uint32_t actualCount = 0;
            ((PFN_xrEnumerateSwapchainImages)dispatch->xrEnumerateSwapchainImages)(
                *swapchain,
                imageCount,
                &actualCount,
                reinterpret_cast<XrSwapchainImageBaseHeader*>(vkImages.data())
            );


            SwapchainTracker::get_instance().track_swapchain(
                *swapchain, *createInfo, actualCount,
                reinterpret_cast<XrSwapchainImageBaseHeader*>(vkImages.data())
            );
        }
    }

    return result;
}

extern "C" XrResult monoeye_xrDestroySwapchain(XrSwapchain swapchain) {
    MONOEYE_LOG("xrDestroySwapchain: %p", (void*)(uintptr_t)swapchain);

    SwapchainTracker::get_instance().untrack_swapchain(swapchain);

    // Find the session/instance for this swapchain via the dispatch table
    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        for (const auto& pair : g_instance_dispatch_map) {
            if (pair.second && pair.second->xrDestroySwapchain) {
                dispatch = pair.second;
                break;
            }
        }

    }

    if (!dispatch || !dispatch->xrDestroySwapchain) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return ((PFN_xrDestroySwapchain)dispatch->xrDestroySwapchain)(swapchain);

}

extern "C" XrResult monoeye_xrEnumerateSwapchainImages(
    XrSwapchain swapchain,
    uint32_t swapchainImageCapacityInput,
    uint32_t* swapchainImageCountOutput,
    XrSwapchainImageBaseHeader* swapchainImages
) {
    // Find the dispatch table for this swapchain's instance
    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        for (const auto& pair : g_instance_dispatch_map) {
            if (pair.second && pair.second->xrEnumerateSwapchainImages) {
                dispatch = pair.second;
                break;
            }
        }

    }

    if (!dispatch || !dispatch->xrEnumerateSwapchainImages) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return ((PFN_xrEnumerateSwapchainImages)dispatch->xrEnumerateSwapchainImages)(
        swapchain, swapchainImageCapacityInput,
        swapchainImageCountOutput, swapchainImages
    );

}

extern "C" XrResult monoeye_xrAcquireSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageAcquireInfo* acquireInfo,
    uint32_t* index
) {
    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        for (const auto& pair : g_instance_dispatch_map) {
            if (pair.second && pair.second->xrAcquireSwapchainImage) {
                dispatch = pair.second;
                break;
            }
        }

    }

    if (!dispatch || !dispatch->xrAcquireSwapchainImage) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return ((PFN_xrAcquireSwapchainImage)dispatch->xrAcquireSwapchainImage)(swapchain, acquireInfo, index);

}

extern "C" XrResult monoeye_xrWaitSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageWaitInfo* waitInfo
) {
    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        for (const auto& pair : g_instance_dispatch_map) {
            if (pair.second && pair.second->xrWaitSwapchainImage) {
                dispatch = pair.second;
                break;
            }
        }

    }

    if (!dispatch || !dispatch->xrWaitSwapchainImage) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return ((PFN_xrWaitSwapchainImage)dispatch->xrWaitSwapchainImage)(swapchain, waitInfo);

}

extern "C" XrResult monoeye_xrReleaseSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageReleaseInfo* releaseInfo
) {
    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        for (const auto& pair : g_instance_dispatch_map) {
            if (pair.second && pair.second->xrReleaseSwapchainImage) {
                dispatch = pair.second;
                break;
            }
        }

    }

    if (!dispatch || !dispatch->xrReleaseSwapchainImage) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return ((PFN_xrReleaseSwapchainImage)dispatch->xrReleaseSwapchainImage)(swapchain, releaseInfo);

}

} // namespace monoeye
