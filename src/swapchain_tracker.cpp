// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "swapchain_tracker.h"
#include "logging.h"
#include "dispatch_table.h"
#include <cstring>

namespace monoeye {

SwapchainTracker& SwapchainTracker::get_instance() {
    static SwapchainTracker instance;
    return instance;
}

void SwapchainTracker::track_swapchain(
    XrSwapchain swapchain,
    const XrSwapchainCreateInfo& createInfo,
    uint32_t imageCount,
    const XrSwapchainImageBaseHeader* images
) {
    std::lock_guard<std::mutex> lock(m_mutex);

    SwapchainImageInfo info;
    info.swapchain = swapchain;
    info.createInfo = createInfo;
    info.imageCount = imageCount;
    info.isDepth = false;
    info.isLeftEye = false;
    info.isRightEye = false;

    // Extract Vulkan image handles if this is a Vulkan swapchain
    info.isVulkan = false;
    if (createInfo.usageFlags & XR_SWAPCHAIN_USAGE_SAMPLED_BIT) {
#ifdef _WIN32
        // Try to interpret as Vulkan images
        const XrSwapchainImageVulkanKHR* vkImages =
            reinterpret_cast<const XrSwapchainImageVulkanKHR*>(images);
        if (images && images->type == XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR) {
            info.isVulkan = true;
            for (uint32_t i = 0; i < imageCount; ++i) {
                info.vulkanImages.push_back(vkImages[i].image);
            }
        }
#else
        const XrSwapchainImageVulkanKHR* vkImages =
            reinterpret_cast<const XrSwapchainImageVulkanKHR*>(images);
        if (images && images->type == XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR) {
            info.isVulkan = true;
            for (uint32_t i = 0; i < imageCount; ++i) {
                info.vulkanImages.push_back(vkImages[i].image);
            }
        }
#endif
    }

    // Check if this is a depth swapchain based on usage flags
    if (createInfo.usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        info.isDepth = true;
    }

    m_swapchains[swapchain] = info;
    m_assigned_eye[swapchain] = false;

    MONOEYE_LOG_DEBUG("Tracked swapchain %p: %dx%d, fmt=%d, samples=%d, depth=%s, vulkan=%s",
        (void*)(uintptr_t)swapchain,
        createInfo.width, createInfo.height,
        createInfo.format,
        createInfo.sampleCount,
        info.isDepth ? "yes" : "no",
        info.isVulkan ? "yes" : "no");
}

void SwapchainTracker::untrack_swapchain(XrSwapchain swapchain) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_swapchains.erase(swapchain);
    m_assigned_eye.erase(swapchain);
    MONOEYE_LOG_DEBUG("Untracked swapchain %p", (void*)(uintptr_t)swapchain);
}

SwapchainImageInfo* SwapchainTracker::get_info(XrSwapchain swapchain) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_swapchains.find(swapchain);
    if (it != m_swapchains.end()) {
        return &it->second;
    }
    return nullptr;
}

void SwapchainTracker::analyze_frame_views(
    uint32_t layerCount,
    const XrCompositionLayerBaseHeader* const* layers
) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Walk through composition layers to find projection layers
    // and identify which swapchains are used for which eye
    for (uint32_t l = 0; l < layerCount; ++l) {
        if (!layers[l]) continue;

        if (layers[l]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
            const XrCompositionLayerProjection* projLayer =
                reinterpret_cast<const XrCompositionLayerProjection*>(layers[l]);

            for (uint32_t v = 0; v < projLayer->viewCount; ++v) {
                const XrCompositionLayerProjectionView& view = projLayer->views[v];

                // Find the color swapchain
                for (auto& pair : m_swapchains) {
                    SwapchainImageInfo& info = pair.second;
                    if (info.swapchain == view.subImage.swapchain && !info.isDepth) {
                        if (v == 0) {
                            info.isLeftEye = true;
                            m_assigned_eye[info.swapchain] = true;
                        } else if (v == 1) {
                            info.isRightEye = true;
                            m_assigned_eye[info.swapchain] = true;
                        }
                    }
                }

                // Find the associated depth swapchain via the depth info extension
                const XrBaseInStructure* depthHeader = reinterpret_cast<const XrBaseInStructure*>(view.next);
                while (depthHeader) {
                    if (depthHeader->type == XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR) {
                        const XrCompositionLayerDepthInfoKHR* depthInfo =
                            reinterpret_cast<const XrCompositionLayerDepthInfoKHR*>(depthHeader);
                        XrSwapchain depthSwapchain = depthInfo->subImage.swapchain;
                        for (auto& dpair : m_swapchains) {
                            if (dpair.second.swapchain == depthSwapchain) {
                                dpair.second.isDepth = true;
                                if (v == 0) {
                                    dpair.second.isLeftEye = true;
                                } else if (v == 1) {
                                    dpair.second.isRightEye = true;
                                }
                            }
                        }
                        break;
                    }
                    depthHeader = depthHeader->next;
                }
            }
        }
    }

    // Log the assignments
    for (const auto& pair : m_swapchains) {
        const SwapchainImageInfo& info = pair.second;
        MONOEYE_LOG_DEBUG("Swapchain %p: left=%s, right=%s, depth=%s",
            (void*)(uintptr_t)info.swapchain,
            info.isLeftEye ? "yes" : "no",
            info.isRightEye ? "yes" : "no",
            info.isDepth ? "yes" : "no");
    }
}

std::vector<SwapchainImageInfo*> SwapchainTracker::get_all() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SwapchainImageInfo*> result;
    for (auto& pair : m_swapchains) {
        result.push_back(&pair.second);
    }
    return result;
}

XrSwapchain SwapchainTracker::get_or_create_right_swapchain(
    XrSession session,
    const XrSwapchainCreateInfo& leftCreateInfo,
    XrGeneratedDispatchTable* dispatch
) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_right_swapchains.find(session);
    if (it != m_right_swapchains.end()) {
        // Future: Check if size/format matches, recreate if not
        return it->second;
    }

    // Create a new swapchain for our internal use
    XrSwapchainCreateInfo createInfo = leftCreateInfo;
    // Ensure it has sampled and storage usage for our compute shader
    createInfo.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    
    XrSwapchain rightSwapchain = XR_NULL_HANDLE;
    XrResult result = ((PFN_xrCreateSwapchain)dispatch->CreateSwapchain)(session, &createInfo, &rightSwapchain);

    if (result != XR_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to create shadow swapchain: %d", result);
        return XR_NULL_HANDLE;
    }

    m_right_swapchains[session] = rightSwapchain;

    // We also need to enumerate images to track it
    uint32_t imageCount = 0;
    ((PFN_xrEnumerateSwapchainImages)dispatch->EnumerateSwapchainImages)(rightSwapchain, 0, &imageCount, nullptr);

    if (imageCount > 0) {
        std::vector<XrSwapchainImageVulkanKHR> vkImages(imageCount);
        for (uint32_t i = 0; i < imageCount; ++i) {
            vkImages[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
            vkImages[i].next = nullptr;
        }

        uint32_t actualCount = 0;
        ((PFN_xrEnumerateSwapchainImages)dispatch->EnumerateSwapchainImages)(
            rightSwapchain,
            imageCount,
            &actualCount,
            reinterpret_cast<XrSwapchainImageBaseHeader*>(vkImages.data())
        );

        // Track it (but don't use the lock since we already have it)
        SwapchainImageInfo info;
        info.swapchain = rightSwapchain;
        info.createInfo = createInfo;
        info.imageCount = actualCount;
        info.isDepth = false;
        info.isLeftEye = false;
        info.isRightEye = true; // Mark as right eye
        info.isVulkan = true;
        for (uint32_t i = 0; i < actualCount; ++i) {
            info.vulkanImages.push_back(vkImages[i].image);
        }

        m_swapchains[rightSwapchain] = info;
    }

    MONOEYE_LOG("Created shadow right swapchain: %p", (void*)(uintptr_t)rightSwapchain);
    return rightSwapchain;
}

void SwapchainTracker::reset_eye_assignments() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : m_swapchains) {
        pair.second.isLeftEye = false;
        pair.second.isRightEye = false;
    }
}

void SwapchainTracker::mark_as_depth(XrSwapchain swapchain) {
    // Called from internal analysis
    auto it = m_swapchains.find(swapchain);
    if (it != m_swapchains.end()) {
        it->second.isDepth = true;
    }
}

void SwapchainTracker::mark_as_left_eye(XrSwapchain swapchain) {
    auto it = m_swapchains.find(swapchain);
    if (it != m_swapchains.end()) {
        it->second.isLeftEye = true;
    }
}

void SwapchainTracker::mark_as_right_eye(XrSwapchain swapchain) {
    auto it = m_swapchains.find(swapchain);
    if (it != m_swapchains.end()) {
        it->second.isRightEye = true;
    }
}

} // namespace monoeye
