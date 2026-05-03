// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "layer.h"
#include "logging.h"
#include "dispatch_table.h"
#include "swapchain_tracker.h"
#include "warp_pipeline.h"
#include "config.h"

#include <vector>

namespace monoeye {

extern std::mutex s_session_map_mutex;
extern std::unordered_map<XrSession, XrInstance> s_session_map;

// Frame state tracking
static std::mutex s_frame_mutex;
static XrFrameState s_last_frame_state = {};
static bool s_frame_state_valid = false;

XrResult monoeye_xrBeginFrame(
    XrSession session,
    const XrFrameBeginInfo* frameBeginInfo
) {
    // Find the instance for this session
    XrInstance instance = XR_NULL_HANDLE;
    {
        std::lock_guard<std::mutex> lock(s_session_map_mutex);
        auto it = s_session_map.find(session);
        if (it != s_session_map.end()) {
            instance = it->second;
        }
    }

    if (instance == XR_NULL_HANDLE) {
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

    if (!dispatch || !dispatch->BeginFrame) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    // Reset eye assignments for this new frame
    SwapchainTracker::get_instance().reset_eye_assignments();

    // Wait for any pending warp operations to complete
    WarpPipeline::get_instance().wait_for_completion();

    return ((PFN_xrBeginFrame)dispatch->BeginFrame)(session, frameBeginInfo);
}

XrResult monoeye_xrEndFrame(
    XrSession session,
    const XrFrameEndInfo* frameEndInfo
) {
    const Config& config = get_config();

    // If bypass mode is enabled, just pass through
    if (config.bypass_mode || !config.enabled) {
        XrInstance instance = XR_NULL_HANDLE;
        {
            std::lock_guard<std::mutex> lock(s_session_map_mutex);
            auto it = s_session_map.find(session);
            if (it != s_session_map.end()) {
                instance = it->second;
            }
        }

        if (instance == XR_NULL_HANDLE) {
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

        if (!dispatch || !dispatch->EndFrame) {
            return XR_ERROR_RUNTIME_FAILURE;
        }

        return ((PFN_xrEndFrame)dispatch->EndFrame)(session, frameEndInfo);
    }

    // --- MONOEYE DEPTH WARP PIPELINE ---

    MONOEYE_LOG_DEBUG("xrEndFrame: %d layers", frameEndInfo->layerCount);

    // 1. Analyze frame to identify left/right eye swapchains
    // We need to build an array of layer pointers
    std::vector<const XrCompositionLayerBaseHeader*> layers(frameEndInfo->layerCount);
    for (uint32_t i = 0; i < frameEndInfo->layerCount; ++i) {
        layers[i] = frameEndInfo->layers[i];
    }

    SwapchainTracker::get_instance().analyze_frame_views(
        frameEndInfo->layerCount,
        layers.data()
    );

    // 2. Find left eye color and depth swapchains
    SwapchainImageInfo* leftColorInfo = nullptr;
    SwapchainImageInfo* leftDepthInfo = nullptr;
    SwapchainImageInfo* rightColorInfo = nullptr;

    for (auto* info : SwapchainTracker::get_instance().get_all()) {
        if (info->isDepth && info->isLeftEye) {
            leftDepthInfo = info;
        } else if (!info->isDepth && info->isLeftEye) {
            leftColorInfo = info;
        } else if (!info->isDepth && info->isRightEye) {
            rightColorInfo = info;
        }
    }

    // 3. If we have the required inputs, perform the depth warp
    if (leftColorInfo && rightColorInfo) {
        MONOEYE_LOG("MonoEye depth warp: left=%p, right=%p, depth=%s",
            (void*)(uintptr_t)leftColorInfo->swapchain,
            (void*)(uintptr_t)rightColorInfo->swapchain,
            leftDepthInfo ? "present" : "absent");

        // Execute the depth warp compute shader
        // This reads left eye color + depth, writes warped result to right eye
        WarpPipeline::get_instance().execute_warp(
            leftColorInfo,
            leftDepthInfo,
            rightColorInfo,
            frameEndInfo->displayTime
        );
    } else {
        MONOEYE_LOG_WARN("Missing required swapchains for depth warp:");
        MONOEYE_LOG_WARN("  leftColor: %s", leftColorInfo ? "found" : "MISSING");
        MONOEYE_LOG_WARN("  rightColor: %s", rightColorInfo ? "found" : "MISSING");
        MONOEYE_LOG_WARN("  leftDepth: %s", leftDepthInfo ? "found" : "MISSING");
    }

    // 4. Pass through to the runtime
    XrInstance instance = XR_NULL_HANDLE;
    {
        std::lock_guard<std::mutex> lock(s_session_map_mutex);
        auto it = s_session_map.find(session);
        if (it != s_session_map.end()) {
            instance = it->second;
        }
    }

    if (instance == XR_NULL_HANDLE) {
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

    if (!dispatch || !dispatch->EndFrame) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    return ((PFN_xrEndFrame)dispatch->EndFrame)(session, frameEndInfo);
}

} // namespace monoeye
