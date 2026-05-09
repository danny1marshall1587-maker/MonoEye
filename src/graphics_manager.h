// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vulkan/vulkan.h>
#include <mutex>
#include <vector>

namespace monoeye {

/**
 * @brief Manages the shared Vulkan backend for both OpenXR and OpenVR frontends.
 * 
 * This class implements the "Dual-Frontend / Shared-Backend" architecture,
 * providing a centralized Vulkan device for compute operations regardless of 
 * whether the host application uses Vulkan, D3D11, or D3D12.
 */
class GraphicsManager {
public:
    static GraphicsManager& get_instance();

    /**
     * @brief Ensures the internal Vulkan context is initialized.
     * This is called lazily from Submit or xrEndFrame to prevent handshaking deadlocks.
     */
    VkResult ensure_initialized();

    /**
     * @brief Initializes with an existing Vulkan device (e.g. from a Vulkan OpenXR session).
     */
    VkResult initialize_with_external(
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        uint32_t queueFamilyIndex
    );

    bool is_initialized() const { return m_initialized; }

    VkInstance get_vk_instance() { return m_instance; }
    VkPhysicalDevice get_physical_device() { return m_physicalDevice; }
    VkDevice get_device() { return m_device; }
    uint32_t get_queue_family() { return m_queueFamily; }
    VkQueue get_queue() { return m_queue; }

    void shutdown();

private:
    GraphicsManager() = default;
    ~GraphicsManager();

    std::mutex m_mutex;
    bool m_initialized = false;
    bool m_owns_device = false;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    uint32_t m_queueFamily = 0;

    VkResult create_internal_context();
};

} // namespace monoeye
