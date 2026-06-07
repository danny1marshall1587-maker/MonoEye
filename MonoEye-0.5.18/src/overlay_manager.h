// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <openxr/openxr.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "layer.h"

// Forward declarations for D3D types must be in global namespace
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D12Device;
struct ID3D12CommandQueue;

namespace monoeye {

class OverlayManager {
public:
    static OverlayManager& get_instance() {
        static OverlayManager instance;
        return instance;
    }

    void initialize(XrInstance instance, XrSession session, VkDevice device, VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkQueue queue);
#ifdef _WIN32
    void initializeD3D11(XrInstance instance, XrSession session, ID3D11Device* device);
    void initializeD3D12(XrInstance instance, XrSession session, ID3D12Device* device, ID3D12CommandQueue* queue);
#endif
    void shutdown();

    // Begin a new UI frame
    void begin_frame();
    
    // End and render the UI frame
    void end_frame(XrTime displayTime);

    // Get the composition layer for the overlay
    const XrCompositionLayerBaseHeader* get_composition_layer();

    bool is_visible() const { return m_visible; }
    void toggle_visibility() { m_visible = !m_visible; }

    VkImageView get_vulkan_image_view() const { return m_uiView; }

private:
    OverlayManager() = default;

    XrInstance m_xrInstance = XR_NULL_HANDLE;
    XrSession m_xrSession = XR_NULL_HANDLE;
    VkInstance m_vkInstance = VK_NULL_HANDLE;
    VkDevice m_vkDevice = VK_NULL_HANDLE;
    VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
    VkQueue m_vkQueue = VK_NULL_HANDLE;
    uint32_t m_queueFamily = 0;

    VkImage m_uiImage = VK_NULL_HANDLE;
    VkDeviceMemory m_uiMemory = VK_NULL_HANDLE;
    VkImageView m_uiView = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    
    struct PerformanceMetrics {
        float frameTimeMs = 0;
        float gpuSavings = 48.5f; // Estimated starting value
        float physicsLoad = 0;
    } m_metrics;

    bool m_initialized = false;
    bool m_showHud = true;
    int m_menuIndex = 0;
    bool m_visible = false;
    
    XrCompositionLayerQuad m_quadLayer;
    struct XrGeneratedDispatchTable* m_dispatch = nullptr;
};


} // namespace monoeye
