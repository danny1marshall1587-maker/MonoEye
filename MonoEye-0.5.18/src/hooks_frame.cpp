// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "layer.h"
#include "logging.h"
#include "dispatch_table.h"
#include "swapchain_tracker.h"
#include "warp_pipeline.h"
#include "config.h"
#include "overlay_manager.h"

#include <vector>
#include <mutex>
#include <unordered_map>

namespace monoeye {

extern std::mutex s_session_map_mutex;
extern std::unordered_map<XrSession, XrInstance> s_session_map;

// Frame state tracking
static std::mutex s_frame_mutex;
static XrFrameState s_last_frame_state = {};
static bool s_frame_state_valid = false;

extern "C" XrResult monoeye_xrBeginFrame(
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

    if (!dispatch || !dispatch->xrBeginFrame) {
        return XR_ERROR_RUNTIME_FAILURE;
    }

    // Reset eye assignments for this new frame
    SwapchainTracker::get_instance().reset_eye_assignments();

    // Wait for any pending warp operations to complete
    WarpPipeline::get_instance().wait_for_completion();

    return ((PFN_xrBeginFrame)dispatch->xrBeginFrame)(session, frameBeginInfo);

}

extern "C" XrResult monoeye_xrEndFrame(
    XrSession session,
    const XrFrameEndInfo* frameEndInfo
) {
    const Config& config = get_config();

    // Find the instance for this session
    XrInstance instance = XR_NULL_HANDLE;
    {
        std::lock_guard<std::mutex> lock(s_session_map_mutex);
        auto it = s_session_map.find(session);
        if (it != s_session_map.end()) {
            instance = it->second;
        }
    }

    if (instance == XR_NULL_HANDLE) return XR_ERROR_SESSION_LOST;

    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        auto it = g_instance_dispatch_map.find(instance);
        if (it != g_instance_dispatch_map.end()) {
            dispatch = it->second;
        }
    }
    if (!dispatch || !dispatch->xrEndFrame) return XR_ERROR_RUNTIME_FAILURE;

    // Passthrough if disabled
    if (!config.enabled || config.bypass_mode) {
        return ((PFN_xrEndFrame)dispatch->xrEndFrame)(session, frameEndInfo);
    }


    // --- MONOEYE DEPTH WARP PIPELINE ---
    
    // We may need to modify the layer submission
    std::vector<const XrCompositionLayerBaseHeader*> modifiedLayers;
    modifiedLayers.reserve(frameEndInfo->layerCount);

    // Track views and layers we allocate per-frame to ensure pointers remain valid until xrEndFrame returns
    static thread_local std::vector<std::vector<XrCompositionLayerProjectionView>> s_view_storage;
    static thread_local std::vector<XrCompositionLayerProjection> s_proj_storage;
    s_view_storage.clear();
    s_proj_storage.clear();

    for (uint32_t i = 0; i < frameEndInfo->layerCount; ++i) {
        const XrCompositionLayerBaseHeader* layer = frameEndInfo->layers[i];
        
        if (layer->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
            const XrCompositionLayerProjection* projLayer = 
                reinterpret_cast<const XrCompositionLayerProjection*>(layer);
            
            // MONO DETECTION: app submitted 2 views but both reference the same swapchain
            // (stereo-compatible mono mode — we told the app views are identical).
            bool isMono = (projLayer->viewCount == 1) ||
                          (projLayer->viewCount >= 2 &&
                           projLayer->views[0].subImage.swapchain == projLayer->views[1].subImage.swapchain);

            if (isMono) {
                // MONO BYPASS DETECTED - Expand to proper Stereo with warp
                MONOEYE_LOG_DEBUG("Mono projection detected (viewCount=%d), expanding to stereo", projLayer->viewCount);

                s_view_storage.emplace_back(2);
                auto& newViews = s_view_storage.back();
                
                // Copy the first view (Left)
                newViews[0] = projLayer->views[0];
                
                // Create the second view (Right)
                newViews[1] = projLayer->views[0]; // Start with left's data
                
                // 1. Get or create the shadow right swapchain
                SwapchainImageInfo* leftInfo = SwapchainTracker::get_instance().get_info(newViews[0].subImage.swapchain);
                if (leftInfo) {
                    XrSwapchain rightSwapchain = SwapchainTracker::get_instance().get_or_create_right_swapchain(
                        session, leftInfo->createInfo, dispatch);
                    
                    if (rightSwapchain != XR_NULL_HANDLE) {
                        newViews[1].subImage.swapchain = rightSwapchain;
                        
                        SwapchainImageInfo* leftDepthInfo = nullptr;
                        SwapchainImageInfo* leftMotionInfo = nullptr;
                        const XrBaseInStructure* next = reinterpret_cast<const XrBaseInStructure*>(newViews[0].next);
                        while (next) {
                            if (next->type == XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR) {
                                const XrCompositionLayerDepthInfoKHR* depthInfo = 
                                    reinterpret_cast<const XrCompositionLayerDepthInfoKHR*>(next);
                                leftDepthInfo = SwapchainTracker::get_instance().get_info(depthInfo->subImage.swapchain);
                            }
#ifdef XR_TYPE_COMPOSITION_LAYER_MOTION_VECTOR_KHR
                            else if (next->type == XR_TYPE_COMPOSITION_LAYER_MOTION_VECTOR_KHR) {
                                const XrCompositionLayerMotionVectorKHR* mvInfo = 
                                    reinterpret_cast<const XrCompositionLayerMotionVectorKHR*>(next);
                                leftMotionInfo = SwapchainTracker::get_instance().get_info(mvInfo->colorSubImage.swapchain);
                            }
#endif
                            next = next->next;
                        }

                        SwapchainImageInfo* rightInfo = SwapchainTracker::get_instance().get_info(rightSwapchain);
                        
                        if (rightInfo) {
                            WarpPipeline::get_instance().execute_warp(
                                leftInfo, leftDepthInfo, leftMotionInfo, rightInfo, frameEndInfo->displayTime);
                        }
                    }
                }

                // 3. Create a modified projection layer
                s_proj_storage.push_back(*projLayer);
                XrCompositionLayerProjection& newLayer = s_proj_storage.back();
                newLayer.viewCount = 2;
                newLayer.views = newViews.data();
                
                modifiedLayers.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&newLayer));
                continue;
            }
        }
        
        // Default: Keep original layer
        modifiedLayers.push_back(layer);
    }


    // 4. Add the overlay layer if visible (top-most layer)
    const XrCompositionLayerBaseHeader* overlayLayer = OverlayManager::get_instance().get_composition_layer();
    if (overlayLayer) {
        modifiedLayers.push_back(overlayLayer);
    }

    // Update the frame info
    XrFrameEndInfo modifiedFrameEndInfo = *frameEndInfo;
    modifiedFrameEndInfo.layerCount = (uint32_t)modifiedLayers.size();
    modifiedFrameEndInfo.layers = modifiedLayers.data();

    return ((PFN_xrEndFrame)dispatch->xrEndFrame)(session, &modifiedFrameEndInfo);

}

} // namespace monoeye
