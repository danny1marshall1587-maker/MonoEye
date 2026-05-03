// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vulkan/vulkan.h>

#include <openxr/openxr.h>

#include <mutex>
#include <vector>

namespace monoeye {

struct SwapchainImageInfo;

// Vulkan warp pipeline: depth-warp left eye -> right eye
class WarpPipeline {
public:
    static WarpPipeline& get_instance();

    // Initialize with Vulkan device from the OpenXR session
    VkResult initialize(
        VkInstance vkInstance,
        VkDevice vkDevice,
        uint32_t queueFamilyIndex
    );

    // Shut down and release all Vulkan resources
    void shutdown();

    // Execute the depth warp
    // Reads leftColor + leftDepth, writes to rightColor
    VkResult execute_warp(
        SwapchainImageInfo* leftColor,
        SwapchainImageInfo* leftDepth,
        SwapchainImageInfo* rightColor,
        XrTime displayTime
    );

    // Wait for any pending warp operations to complete
    void wait_for_completion();

    // Check if initialized
    bool is_initialized() const { return m_initialized; }

private:
    WarpPipeline();
    ~WarpPipeline();

    // Non-copyable
    WarpPipeline(const WarpPipeline&) = delete;
    WarpPipeline& operator=(const WarpPipeline&) = delete;

    // Create the compute pipeline and shader
    VkResult create_compute_pipeline();

    // Create descriptor set layout and pool
    VkResult create_descriptor_resources();

    // Record the compute command buffer
    VkResult record_compute_command(
        VkImageView leftColorView,
        VkImageView leftDepthView,
        VkImageView rightColorView,
        uint32_t width,
        uint32_t height
    );

    // Create a Vulkan image view for a VkImage
    VkResult create_image_view(VkImage image, VkFormat format, VkImageView* view);

    // Vulkan objects
    VkInstance m_vkInstance = VK_NULL_HANDLE;
    VkDevice m_vkDevice = VK_NULL_HANDLE;
    VkQueue m_vkQueue = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;

    // Compute pipeline
    VkPipeline m_computePipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    // Descriptor resources
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    // Synchronization
    VkSemaphore m_completionSemaphore = VK_NULL_HANDLE;
    VkFence m_fence = VK_NULL_HANDLE;

    // State
    bool m_initialized = false;
    uint32_t m_queueFamilyIndex = 0;
};

} // namespace monoeye
