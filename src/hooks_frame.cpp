// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "layer.h"
#include "logging.h"
#include "dispatch_table.h"
#include "swapchain_tracker.h"
#include "warp_pipeline.h"
#include "config.h"
#include "overlay_manager.h"
#include "vulkan_interop.h"

#include <vector>
#include <mutex>
#include <unordered_map>
#include <atomic>

namespace monoeye {

extern std::mutex s_session_map_mutex;
extern std::unordered_map<XrSession, SessionState> s_session_map;

// Frame state tracking
static std::mutex s_frame_mutex;
static XrFrameState s_last_frame_state = {};
static bool s_frame_state_valid = false;
std::atomic<uint64_t> g_frame_count{0};

extern "C" XrResult XRAPI_CALL monoeye_xrBeginFrame(
    XrSession session,
    const XrFrameBeginInfo* frameBeginInfo
) {
    MONOEYE_LOG_DEBUG("xrBeginFrame: session=%p", (void*)session);

    // Find the instance for this session
    XrInstance instance = XR_NULL_HANDLE;
    {
        std::lock_guard<std::mutex> lock(s_session_map_mutex);
        auto it = s_session_map.find(session);
        if (it != s_session_map.end()) {
            instance = it->second.instance;
        }
    }

    if (instance == XR_NULL_HANDLE) {
        MONOEYE_LOG("xrBeginFrame: session %p not found in map — returning SESSION_LOST", (void*)session);
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
        MONOEYE_LOG("xrBeginFrame: no dispatch table for instance %p", (void*)instance);
        return XR_ERROR_RUNTIME_FAILURE;
    }

    // Reset eye assignments for this new frame
    SwapchainTracker::get_instance().reset_eye_assignments();

    // Only wait for warp completion if the pipeline is actually running —
    // calling wait_for_completion() on an uninitialized pipeline can deadlock.
    if (WarpPipeline::get_instance().is_initialized()) {
        WarpPipeline::get_instance().wait_for_completion();
    }

    XrResult result = ((PFN_xrBeginFrame)dispatch->xrBeginFrame)(session, frameBeginInfo);
    MONOEYE_LOG_DEBUG("xrBeginFrame: downstream returned %d", (int)result);
    return result;
}

extern "C" XrResult XRAPI_CALL monoeye_xrEndFrame(
    XrSession session,
    const XrFrameEndInfo* frameEndInfo
) {
    MONOEYE_LOG_DEBUG("xrEndFrame: session=%p, layerCount=%u",
                      (void*)session,
                      frameEndInfo ? frameEndInfo->layerCount : 0u);
    const Config& config = get_config();

    SessionType sessionType = SESSION_UNKNOWN;
#ifdef _WIN32
    ID3D12Device* d3d12_device = nullptr;
    ID3D12CommandQueue* d3d12_queue = nullptr;
#endif

    // Find the instance for this session
    XrInstance instance = XR_NULL_HANDLE;
    {
        std::lock_guard<std::mutex> lock(s_session_map_mutex);
        auto it = s_session_map.find(session);
        if (it != s_session_map.end()) {
            instance = it->second.instance;
            sessionType = it->second.type;
#ifdef _WIN32
            d3d12_device = it->second.d3d12_device;
            d3d12_queue = it->second.d3d12_queue;
#endif
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

#ifdef _WIN32
    static thread_local ID3D12Device* s_lastDevice = nullptr;
    static thread_local ID3D12Fence* s_d3d12Fence = nullptr;
    static thread_local VkSemaphore s_vulkanSemaphore = VK_NULL_HANDLE;
    static thread_local HANDLE s_sharedHandle = nullptr;

    if (sessionType == SESSION_D3D12 && d3d12_device && d3d12_queue) {
        if (s_lastDevice != d3d12_device) {
            if (s_vulkanSemaphore) {
                vkDestroySemaphore(WarpPipeline::get_instance().get_vk_device(), s_vulkanSemaphore, nullptr);
                s_vulkanSemaphore = VK_NULL_HANDLE;
            }
            if (s_sharedHandle) {
                CloseHandle(s_sharedHandle);
                s_sharedHandle = nullptr;
            }
            if (s_d3d12Fence) {
                s_d3d12Fence->Release();
                s_d3d12Fence = nullptr;
            }
            s_lastDevice = d3d12_device;
            
            if (WarpPipeline::get_instance().is_initialized()) {
                create_shared_fence(WarpPipeline::get_instance().get_vk_device(), d3d12_device, &s_d3d12Fence, &s_sharedHandle, &s_vulkanSemaphore);
            }
        }
    }
#endif



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
                                leftMotionInfo = SwapchainTracker::get_instance().get_info(mvInfo->motionVectorSubImage.swapchain);
                            }
#endif
                            next = next->next;
                        }

                        SwapchainImageInfo* rightInfo = SwapchainTracker::get_instance().get_info(rightSwapchain);
                        
                        if (rightInfo) {
                            VkSemaphore externalSem = VK_NULL_HANDLE;
#ifdef _WIN32
                            if (sessionType == SESSION_D3D12 && s_vulkanSemaphore) {
                                // Since we reuse a timeline semaphore, we signal value 1 each time
                                // But timeline semaphores must strictly increase. 
                                // Actually, if we just signal it to 1, wait for 1, then reset it to 0 in D3D12, that works!
                                // Wait, D3D12 Signal(0) resets it.
                                externalSem = s_vulkanSemaphore;
                            }
#endif
                            WarpPipeline::get_instance().execute_warp(
                               leftInfo, leftDepthInfo, leftMotionInfo, rightInfo, frameEndInfo->displayTime, externalSem);
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
        } else if (layer->type == XR_TYPE_COMPOSITION_LAYER_QUAD) {
            MONOEYE_LOG_DEBUG("Flat UI layer (XR_TYPE_COMPOSITION_LAYER_QUAD) detected. Passing through untouched.");
            modifiedLayers.push_back(layer);
            continue;
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

#ifdef _WIN32
    if (sessionType == SESSION_D3D12 && s_d3d12Fence && d3d12_queue) {
        // Wait on the D3D12 command queue for the Vulkan compute shader to signal the fence (value 1)
        d3d12_queue->Wait(s_d3d12Fence, 1);
        
        // Reset the fence back to 0 for the next frame
        d3d12_queue->Signal(s_d3d12Fence, 0);
    }
#endif

    g_frame_count.fetch_add(1, std::memory_order_relaxed);
    return ((PFN_xrEndFrame)dispatch->xrEndFrame)(session, &modifiedFrameEndInfo);

}

} // namespace monoeye
