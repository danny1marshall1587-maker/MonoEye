// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "swapchain_tracker.h"
#include "warp_pipeline.h"
#include "logging.h"
#include "vulkan_utils.h"
#include "vulkan_interop.h"
#ifdef _WIN32
#include <windows.h>
#include <d3d12.h>
#endif
#include <cstring>
#include <mutex>
#include <vector>
#include <unordered_map>

namespace monoeye {

extern std::mutex s_session_map_mutex;
extern std::unordered_map<XrSession, SessionState> s_session_map;

VkFormat dxgi_to_vulkan_format(int dxgiFormat) {
    switch (dxgiFormat) {
        case 28: // DXGI_FORMAT_R8G8B8A8_UNORM
            return VK_FORMAT_R8G8B8A8_UNORM;
        case 27: // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            return VK_FORMAT_R8G8B8A8_SRGB;
        case 24: // DXGI_FORMAT_R8G8B8A8_TYPELESS
            return VK_FORMAT_R8G8B8A8_UNORM;
        case 87: // DXGI_FORMAT_B8G8R8A8_UNORM
            return VK_FORMAT_B8G8R8A8_UNORM;
        case 91: // DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
            return VK_FORMAT_B8G8R8A8_SRGB;
        case 10: // DXGI_FORMAT_R16G16B16A16_FLOAT
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case 2:  // DXGI_FORMAT_R32G32B32A32_FLOAT
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case 40: // DXGI_FORMAT_D32_FLOAT
            return VK_FORMAT_D32_SFLOAT;
        case 45: // DXGI_FORMAT_D24_UNORM_S8_UINT
            return VK_FORMAT_D24_UNORM_S8_UINT;
        case 20: // DXGI_FORMAT_R32_FLOAT
            return VK_FORMAT_D32_SFLOAT;
        default:
            MONOEYE_LOG_WARN("Unknown DXGI format %d, defaulting to R8G8B8A8_UNORM", dxgiFormat);
            return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

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
    track_swapchain_locked(swapchain, createInfo, imageCount, images);
}

void SwapchainTracker::track_swapchain_locked(
    XrSwapchain swapchain,
    const XrSwapchainCreateInfo& createInfo,
    uint32_t imageCount,
    const XrSwapchainImageBaseHeader* images
) {
    SwapchainImageInfo info;
    info.swapchain = swapchain;
    info.createInfo = createInfo;
    info.imageCount = imageCount;
    info.isDepth = false;
    info.isLeftEye = false;
    info.isRightEye = false;
    info.isVulkan = false;
    info.isD3D11 = false;
    info.isD3D12 = false;

    if (images && images->type == XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR) {
        info.isVulkan = true;
        const XrSwapchainImageVulkanKHR* vkImages =
            reinterpret_cast<const XrSwapchainImageVulkanKHR*>(images);
        VkDevice device = WarpPipeline::get_instance().get_vk_device();
        for (uint32_t i = 0; i < imageCount; ++i) {
            info.vulkanImages.push_back(vkImages[i].image);
            
            uint32_t layers = createInfo.arrayLayers;
            if (layers == 0) layers = 1;
            std::vector<VkImageView> imageLayers;
            
            if (device != VK_NULL_HANDLE) {
                for (uint32_t layer = 0; layer < layers; ++layer) {
                    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
                    viewInfo.image = vkImages[i].image;
                    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                    viewInfo.format = static_cast<VkFormat>(createInfo.format);
                    viewInfo.subresourceRange.aspectMask = (createInfo.usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
                    viewInfo.subresourceRange.baseArrayLayer = layer;
                    viewInfo.subresourceRange.levelCount = 1;
                    viewInfo.subresourceRange.layerCount = 1;
                    
                    VkImageView view;
                    if (vkCreateImageView(device, &viewInfo, nullptr, &view) == VK_SUCCESS) {
                        imageLayers.push_back(view);
                    }
                }
            }
            info.vulkanImageViewsPerLayer.push_back(imageLayers);
        }
    } 
#ifdef _WIN32
    else if (images && images->type == XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
        info.isD3D11 = true;
        const XrSwapchainImageD3D11KHR* d3dImages = reinterpret_cast<const XrSwapchainImageD3D11KHR*>(images);
        for (uint32_t i = 0; i < imageCount; ++i) {
            info.d3dResources.push_back(d3dImages[i].texture);
        }
    } else if (images && images->type == XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR) {
        info.isD3D12 = true;
        const XrSwapchainImageD3D12KHR* d3dImages = reinterpret_cast<const XrSwapchainImageD3D12KHR*>(images);
        VkDevice device = WarpPipeline::get_instance().get_vk_device();
        VkPhysicalDevice physicalDevice = WarpPipeline::get_instance().get_vk_physical_device();

        for (uint32_t i = 0; i < imageCount; ++i) {
            info.d3dResources.push_back(d3dImages[i].texture);

            if (device != VK_NULL_HANDLE && physicalDevice != VK_NULL_HANDLE) {
                VkImage vkImage = VK_NULL_HANDLE;
                VkDeviceMemory vkMemory = VK_NULL_HANDLE;
                ID3D12Resource* intermediate = nullptr;
                VkFormat vkFormat = dxgi_to_vulkan_format(static_cast<int>(createInfo.format));

                VkResult res = import_d3d12_texture_to_vulkan(
                    device, physicalDevice, d3dImages[i].texture, vkFormat,
                    createInfo.width, createInfo.height, &vkImage, &vkMemory, &intermediate
                );

                if (res == VK_SUCCESS) {
                    info.vulkanImages.push_back(vkImage);
                    info.vulkanMemories.push_back(vkMemory);
                    info.intermediateResources.push_back(intermediate);

                    uint32_t layers = createInfo.arrayLayers;
                    if (layers == 0) layers = 1;
                    std::vector<VkImageView> imageLayers;

                    for (uint32_t layer = 0; layer < layers; ++layer) {
                        VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
                        viewInfo.image = vkImage;
                        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                        viewInfo.format = vkFormat;
                        viewInfo.subresourceRange.aspectMask = (createInfo.usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
                        viewInfo.subresourceRange.baseArrayLayer = layer;
                        viewInfo.subresourceRange.levelCount = 1;
                        viewInfo.subresourceRange.layerCount = 1;

                        VkImageView view;
                        if (vkCreateImageView(device, &viewInfo, nullptr, &view) == VK_SUCCESS) {
                            imageLayers.push_back(view);
                        }
                    }
                    info.vulkanImageViewsPerLayer.push_back(imageLayers);
                } else {
                    MONOEYE_LOG_ERROR("Failed to import D3D12 resource to Vulkan: %d", res);
                }
            }
        }
        if (!info.vulkanImageViewsPerLayer.empty()) {
            info.isVulkan = true;
        }
    }
#endif

    if (createInfo.usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        info.isDepth = true;
    }

    m_swapchains[swapchain] = info;
    m_assigned_eye[swapchain] = false;

    MONOEYE_LOG_DEBUG("Tracked swapchain %p: %dx%d, fmt=%d, samples=%d, depth=%s, api=%s",
        (void*)(uintptr_t)swapchain,
        createInfo.width, createInfo.height,
        createInfo.format,
        createInfo.sampleCount,
        info.isDepth ? "yes" : "no",
        info.isVulkan ? "Vulkan" : (info.isD3D11 ? "D3D11" : (info.isD3D12 ? "D3D12" : "Unknown")));
}

void SwapchainTracker::untrack_swapchain(XrSwapchain swapchain) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_swapchains.find(swapchain);
    if (it != m_swapchains.end()) {
        VkDevice device = WarpPipeline::get_instance().get_vk_device();
        if (device != VK_NULL_HANDLE) {
            for (const auto& layers : it->second.vulkanImageViewsPerLayer) {
                for (auto view : layers) {
                    vkDestroyImageView(device, view, nullptr);
                }
            }
            if (it->second.isD3D12 || it->second.isD3D11) {
                for (auto image : it->second.vulkanImages) {
                    vkDestroyImage(device, image, nullptr);
                }
                for (auto mem : it->second.vulkanMemories) {
                    vkFreeMemory(device, mem, nullptr);
                }
#ifdef _WIN32
                for (auto res : it->second.intermediateResources) {
                    if (res) {
                        reinterpret_cast<IUnknown*>(res)->Release();
                    }
                }
                for (auto alloc : it->second.d3d12CommandAllocators) {
                    if (alloc) {
                        reinterpret_cast<IUnknown*>(alloc)->Release();
                    }
                }
                for (auto list : it->second.d3d12CommandLists) {
                    if (list) {
                        reinterpret_cast<IUnknown*>(list)->Release();
                    }
                }
#endif
            }
        }
        m_swapchains.erase(it);
    }
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

void SwapchainTracker::set_active_index(XrSwapchain swapchain, uint32_t index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_swapchains.find(swapchain);
    if (it != m_swapchains.end()) {
        it->second.activeIndex = index;
    }
}

void SwapchainTracker::analyze_frame_views(
    uint32_t layerCount,
    const XrCompositionLayerBaseHeader* const* layers
) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (uint32_t l = 0; l < layerCount; ++l) {
        if (!layers[l]) continue;

        if (layers[l]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
            const XrCompositionLayerProjection* projLayer =
                reinterpret_cast<const XrCompositionLayerProjection*>(layers[l]);

            for (uint32_t v = 0; v < projLayer->viewCount; ++v) {
                const XrCompositionLayerProjectionView& view = projLayer->views[v];

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
        return it->second;
    }

    XrSwapchainCreateInfo createInfo = leftCreateInfo;
    createInfo.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    
    XrSwapchain rightSwapchain = XR_NULL_HANDLE;
    XrResult result = ((PFN_xrCreateSwapchain)dispatch->xrCreateSwapchain)(session, &createInfo, &rightSwapchain);

    if (result != XR_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to create shadow swapchain: %d", result);
        return XR_NULL_HANDLE;
    }

    m_right_swapchains[session] = rightSwapchain;

    uint32_t imageCount = 0;
    ((PFN_xrEnumerateSwapchainImages)dispatch->xrEnumerateSwapchainImages)(rightSwapchain, 0, &imageCount, nullptr);

    if (imageCount > 0) {
        SessionType sessionType = SESSION_UNKNOWN;
        {
            std::lock_guard<std::mutex> lock_session(s_session_map_mutex);
            auto s_it = s_session_map.find(session);
            if (s_it != s_session_map.end()) {
                sessionType = s_it->second.type;
            }
        }

        std::vector<char> buffer;
        XrStructureType imageType = XR_TYPE_UNKNOWN;
        size_t structSize = 0;

        if (sessionType == SESSION_VULKAN) {
            imageType = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
            structSize = sizeof(XrSwapchainImageVulkanKHR);
        } 
#ifdef _WIN32
        else if (sessionType == SESSION_D3D11) {
            imageType = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
            structSize = sizeof(XrSwapchainImageD3D11KHR);
        } else if (sessionType == SESSION_D3D12) {
            imageType = XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR;
            structSize = sizeof(XrSwapchainImageD3D12KHR);
        }
#endif

        if (structSize > 0) {
            buffer.resize(imageCount * structSize);
            for (uint32_t i = 0; i < imageCount; ++i) {
                XrSwapchainImageBaseHeader* header = reinterpret_cast<XrSwapchainImageBaseHeader*>(buffer.data() + (i * structSize));
                header->type = imageType;
                header->next = nullptr;
            }

            uint32_t actualCount = 0;
            ((PFN_xrEnumerateSwapchainImages)dispatch->xrEnumerateSwapchainImages)(
                rightSwapchain,
                imageCount,
                &actualCount,
                reinterpret_cast<XrSwapchainImageBaseHeader*>(buffer.data())
            );

            // Track the shadow swapchain inside the lock (using locked helper)
            track_swapchain_locked(
                rightSwapchain, createInfo, actualCount,
                reinterpret_cast<const XrSwapchainImageBaseHeader*>(buffer.data())
            );

            // Mark it as the right eye
            auto info_it = m_swapchains.find(rightSwapchain);
            if (info_it != m_swapchains.end()) {
                info_it->second.isRightEye = true;
                info_it->second.isLeftEye = false;
            }
        }
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

VkImageView SwapchainTracker::get_current_view(SwapchainImageInfo* info, uint32_t layerIndex) {
    if (!info || !info->isVulkan || info->vulkanImageViewsPerLayer.empty()) {
        return VK_NULL_HANDLE;
    }
    uint32_t activeIdx = info->activeIndex;
    if (activeIdx >= info->vulkanImageViewsPerLayer.size()) {
        return VK_NULL_HANDLE;
    }
    const auto& layers = info->vulkanImageViewsPerLayer[activeIdx];
    if (layerIndex >= layers.size()) {
        return VK_NULL_HANDLE;
    }
    return layers[layerIndex];
}

void SwapchainTracker::mark_as_depth(XrSwapchain swapchain) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_swapchains.find(swapchain);
    if (it != m_swapchains.end()) {
        it->second.isDepth = true;
    }
}

void SwapchainTracker::mark_as_left_eye(XrSwapchain swapchain) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_swapchains.find(swapchain);
    if (it != m_swapchains.end()) {
        it->second.isLeftEye = true;
    }
}

void SwapchainTracker::mark_as_right_eye(XrSwapchain swapchain) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_swapchains.find(swapchain);
    if (it != m_swapchains.end()) {
        it->second.isRightEye = true;
    }
}

} // namespace monoeye
