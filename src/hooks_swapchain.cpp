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
static std::mutex s_session_map_mutex;
static std::unordered_map<XrSession, XrInstance> s_session_map;

XrResult monoeye_xrCreateSwapchain(
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

    if (!dispatch || !dispatch->CreateSwapchain) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    XrResult result = ((PFN_xrCreateSwapchain)dispatch->CreateSwapchain)(session, createInfo, swapchain);

    if (result == XR_SUCCESS) {
        uint32_t imageCount = 0;
        ((PFN_xrEnumerateSwapchainImages)dispatch->EnumerateSwapchainImages)(*swapchain, 0, &imageCount, nullptr);

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
            ((PFN_xrEnumerateSwapchainImages)dispatch->EnumerateSwapchainImages)(
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

XrResult monoeye_xrDestroySwapchain(XrSwapchain swapchain) {
    MONOEYE_LOG("xrDestroySwapchain: %p", (void*)(uintptr_t)swapchain);

    SwapchainTracker::get_instance().untrack_swapchain(swapchain);

    // Find the session/instance for this swapchain via the dispatch table
    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        for (const auto& pair : g_instance_dispatch_map) {
            if (pair.second && pair.second->DestroySwapchain) {
                dispatch = pair.second;
                break;
            }
        }
    }

    if (!dispatch || !dispatch->DestroySwapchain) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return ((PFN_xrDestroySwapchain)dispatch->DestroySwapchain)(swapchain);
}

XrResult monoeye_xrEnumerateSwapchainImages(
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
            if (pair.second && pair.second->EnumerateSwapchainImages) {
                dispatch = pair.second;
                break;
            }
        }
    }

    if (!dispatch || !dispatch->EnumerateSwapchainImages) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return ((PFN_xrEnumerateSwapchainImages)dispatch->EnumerateSwapchainImages)(
        swapchain, swapchainImageCapacityInput,
        swapchainImageCountOutput, swapchainImages
    );
}

XrResult monoeye_xrAcquireSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageAcquireInfo* acquireInfo,
    uint32_t* index
) {
    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        for (const auto& pair : g_instance_dispatch_map) {
            if (pair.second && pair.second->AcquireSwapchainImage) {
                dispatch = pair.second;
                break;
            }
        }
    }

    if (!dispatch || !dispatch->AcquireSwapchainImage) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return ((PFN_xrAcquireSwapchainImage)dispatch->AcquireSwapchainImage)(swapchain, acquireInfo, index);
}

XrResult monoeye_xrWaitSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageWaitInfo* waitInfo
) {
    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        for (const auto& pair : g_instance_dispatch_map) {
            if (pair.second && pair.second->WaitSwapchainImage) {
                dispatch = pair.second;
                break;
            }
        }
    }

    if (!dispatch || !dispatch->WaitSwapchainImage) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return ((PFN_xrWaitSwapchainImage)dispatch->WaitSwapchainImage)(swapchain, waitInfo);
}

XrResult monoeye_xrReleaseSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageReleaseInfo* releaseInfo
) {
    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        for (const auto& pair : g_instance_dispatch_map) {
            if (pair.second && pair.second->ReleaseSwapchainImage) {
                dispatch = pair.second;
                break;
            }
        }
    }

    if (!dispatch || !dispatch->ReleaseSwapchainImage) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return ((PFN_xrReleaseSwapchainImage)dispatch->ReleaseSwapchainImage)(swapchain, releaseInfo);
}

} // namespace monoeye
