// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <mutex>
#include <vector>
#include <unordered_map>

#include "dispatch_table.h"

namespace monoeye {

struct SwapchainImageInfo {
    XrSwapchain swapchain;
    XrSwapchainCreateInfo createInfo;
    uint32_t imageCount;

    // For Vulkan, we need the actual image handles and views
    bool isVulkan;
    std::vector<VkImage> vulkanImages;
    std::vector<VkImageView> vulkanImageViews;

    // For DirectX 11/12
    bool isD3D11;
    bool isD3D12;
    std::vector<void*> d3dResources;

    // Tracking which eye this swapchain belongs to
    bool isDepth;
    bool isLeftEye;
    bool isRightEye;
};


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
    VkImageView get_current_view(SwapchainImageInfo* info);

    void analyze_frame_views(
        uint32_t layerCount,
        const XrCompositionLayerBaseHeader* const* layers
    );

    std::vector<SwapchainImageInfo*> get_all();

    XrSwapchain get_or_create_right_swapchain(
        XrSession session,
        const XrSwapchainCreateInfo& leftCreateInfo,
        XrGeneratedDispatchTable* dispatch
    );

    void reset_eye_assignments();

private:
    SwapchainTracker() = default;

    std::mutex m_mutex;
    std::unordered_map<XrSwapchain, SwapchainImageInfo> m_swapchains;
    std::unordered_map<XrSession, XrSwapchain> m_right_swapchains;
    std::unordered_map<XrSwapchain, bool> m_assigned_eye;

    void mark_as_depth(XrSwapchain swapchain);
    void mark_as_left_eye(XrSwapchain swapchain);
    void mark_as_right_eye(XrSwapchain swapchain);
};

} // namespace monoeye
