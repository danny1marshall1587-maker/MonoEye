// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <openxr/openxr.h>

#ifdef _WIN32
#include <vulkan/vulkan_win32.h>
#else
#include <vulkan/vulkan_xcb.h>
#endif

#include <vulkan/vulkan.h>

#include <mutex>
#include <vector>
#include <unordered_map>

namespace monoeye {

// Swapchain image tracking info
struct SwapchainImageInfo {
    XrSwapchain swapchain;
    XrSwapchainCreateInfo createInfo;
    uint32_t imageCount;

    // For Vulkan, we need the actual image handles
    bool isVulkan;
    std::vector<VkImage> vulkanImages;

    // Tracking which eye this swapchain belongs to
    bool isDepth;
    bool isLeftEye;
    bool isRightEye;
};

// SwapchainTracker tracks all swapchains created by the application
// and determines which are left-eye, right-eye, or depth swapchains
class SwapchainTracker {
public:
    static SwapchainTracker& get_instance();

    void track_swapchain(
        XrSwapchain swapchain,
        const XrSwapchainCreateInfo& createInfo,
        uint32_t imageCount,
        const XrSwapchainImageBaseHeader* images
    );

    void untrack_swapchain(XrSwapchain swapchain);

    SwapchainImageInfo* get_info(XrSwapchain swapchain);

    // Called from xrEndFrame to analyze which swapchains are used for which eye
    void analyze_frame_views(
        uint32_t layerCount,
        const XrCompositionLayerBaseHeader* const* layers
    );

    // Get all tracked swapchains
    std::vector<SwapchainImageInfo*> get_all();

    // Reset eye assignments between frames
    void reset_eye_assignments();

private:
    SwapchainTracker() = default;

    std::mutex m_mutex;
    std::unordered_map<XrSwapchain, SwapchainImageInfo> m_swapchains;
    std::unordered_map<XrSwapchain, bool> m_assigned_eye;

    void mark_as_depth(XrSwapchain swapchain);
    void mark_as_left_eye(XrSwapchain swapchain);
    void mark_as_right_eye(XrSwapchain swapchain);
};

} // namespace monoeye
