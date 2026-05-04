// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "overlay_manager.h"
#include "logging.h"
#include "dispatch_table.h"
#include "vulkan_utils.h"

#include <cstring>

namespace monoeye {

void OverlayManager::initialize(XrInstance instance, XrSession session, VkDevice device, VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkQueue queue) {
    if (m_initialized) return;

    m_xrInstance = instance;
    m_xrSession = session;
    m_vkDevice = device;

    MONOEYE_LOG("Initializing OverlayManager...");

    // 1. Create OpenXR Swapchain for UI (1024x1024)
    XrSwapchainCreateInfo swapchainInfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    swapchainInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    swapchainInfo.sampleCount = 1;
    swapchainInfo.width = 1024;
    swapchainInfo.height = 1024;
    swapchainInfo.faceCount = 1;
    swapchainInfo.arraySize = 1;
    swapchainInfo.mipCount = 1;

    XrResult result = xrCreateSwapchain(m_xrSession, &swapchainInfo, &m_swapchain);
    if (result != XR_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to create overlay swapchain: %d", result);
        return;
    }

    // 2. Get swapchain images
    uint32_t imageCount = 0;
    xrEnumerateSwapchainImages(m_swapchain, 0, &imageCount, nullptr);
    std::vector<XrSwapchainImageVulkanKHR> images(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
    xrEnumerateSwapchainImages(m_swapchain, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)images.data());

    for (uint32_t i = 0; i < imageCount; ++i) {
        m_images.push_back(images[i].image);
        
        VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = images[i].image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        
        VkImageView view;
        vkCreateImageView(m_vkDevice, &viewInfo, nullptr, &view);
        m_imageViews.push_back(view);
    }

    // 3. Initialize Quad Layer info
    m_quadLayer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
    m_quadLayer.next = nullptr;
    m_quadLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    m_quadLayer.space = XR_NULL_HANDLE; // Will be set per frame (usually Reference Space)
    m_quadLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    m_quadLayer.subImage.swapchain = m_swapchain;
    m_quadLayer.subImage.imageRect.offset = {0, 0};
    m_quadLayer.subImage.imageRect.extent = {1024, 1024};
    m_quadLayer.subImage.imageArrayIndex = 0;
    
    // Position: 1.5 meters in front of user
    m_quadLayer.pose.orientation = {0, 0, 0, 1};
    m_quadLayer.pose.position = {0, 0, -1.5f};
    m_quadLayer.size = {1.0f, 1.0f}; // 1 meter wide/high

    m_initialized = true;
    m_visible = false; // Start hidden
}

void OverlayManager::shutdown() {
    if (!m_initialized) return;

    for (auto view : m_imageViews) {
        vkDestroyImageView(m_vkDevice, view, nullptr);
    }
    m_imageViews.clear();
    m_images.clear();

    if (m_swapchain) {
        xrDestroySwapchain(m_swapchain);
        m_swapchain = XR_NULL_HANDLE;
    }

    m_initialized = false;
}

void OverlayManager::begin_frame() {
    if (!m_initialized || !m_visible) return;
    // ImGui NewFrame logic will go here
}

void OverlayManager::end_frame(XrTime displayTime) {
    if (!m_initialized || !m_visible) return;

    // 1. Acquire swapchain image
    uint32_t imageIndex;
    xrAcquireSwapchainImage(m_swapchain, nullptr, &imageIndex);
    
    XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
    xrWaitSwapchainImage(m_swapchain, &waitInfo);

    // 2. Render UI to m_imageViews[imageIndex]
    // (Placeholder: Clear to semi-transparent blue for now)
    // In v0.4.0 we'll use ImGui here

    // 3. Release swapchain image
    xrReleaseSwapchainImage(m_swapchain, nullptr);
}

const XrCompositionLayerBaseHeader* OverlayManager::get_composition_layer() {
    if (!m_initialized || !m_visible) return nullptr;
    return reinterpret_cast<const XrCompositionLayerBaseHeader*>(&m_quadLayer);
}

} // namespace monoeye
