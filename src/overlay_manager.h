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

private:
    OverlayManager() = default;

    XrInstance m_xrInstance = XR_NULL_HANDLE;
    XrSession m_xrSession = XR_NULL_HANDLE;
    VkInstance m_vkInstance = VK_NULL_HANDLE;
    VkDevice m_vkDevice = VK_NULL_HANDLE;
    
#ifdef _WIN32
    ID3D11Device* m_d3d11Device = nullptr;
    ID3D11DeviceContext* m_d3d11Context = nullptr;
    ID3D12Device* m_d3d12Device = nullptr;
#endif

    SessionType m_sessionType = SESSION_UNKNOWN;
    
    XrSwapchain m_swapchain = XR_NULL_HANDLE;
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    
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
