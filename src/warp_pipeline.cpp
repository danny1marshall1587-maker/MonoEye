// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "warp_pipeline.h"
#include "swapchain_tracker.h"
#include "logging.h"
#include "config.h"

#include "vulkan_utils.h"
#include <cstring>

#ifdef MONOEYE_EMBED_SHADERS
#include "depth_warp.h"
#endif

namespace monoeye {

struct WarpPushConstants {
    float ipd;
    float nearZ;
    float farZ;
    float focalLength;
    uint32_t hasDepthBuffer;
    uint32_t hasMotionBuffer;
    uint32_t qualityMode;
    uint32_t showIndicator;
    uint32_t tensorEnabled;
    uint32_t specularRejection;
    uint32_t edgeSmoothing;
    float upscaleFactor;
    uint32_t frameIndex;
};

WarpPipeline::WarpPipeline() = default;

WarpPipeline::~WarpPipeline() {
    if (m_initialized) {
        shutdown();
    }
}

WarpPipeline& WarpPipeline::get_instance() {
    static WarpPipeline instance;
    return instance;
}

VkResult WarpPipeline::initialize(
    VkInstance vkInstance,
    VkPhysicalDevice vkPhysicalDevice,
    VkDevice vkDevice,
    uint32_t queueFamilyIndex
) {
    if (m_initialized) {
        MONOEYE_LOG_WARN("WarpPipeline already initialized");
        return VK_SUCCESS;
    }

    m_vkInstance = vkInstance;
    m_vkPhysicalDevice = vkPhysicalDevice;
    m_vkDevice = vkDevice;
    m_queueFamilyIndex = queueFamilyIndex;

    // Get the compute queue
    vkGetDeviceQueue(m_vkDevice, queueFamilyIndex, 0, &m_vkQueue);

    MONOEYE_LOG("Creating Vulkan compute pipeline for depth warp");

    // Create command pool
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndex;

    VkResult result = vkCreateCommandPool(m_vkDevice, &poolInfo, nullptr, &m_commandPool);
    if (result != VK_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to create command pool: %d", result);
        return result;
    }

    // Allocate command buffer
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    result = vkAllocateCommandBuffers(m_vkDevice, &allocInfo, &m_commandBuffer);
    if (result != VK_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to allocate command buffer: %d", result);
        return result;
    }

    // Create descriptor resources
    result = create_descriptor_resources();
    if (result != VK_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to create descriptor resources: %d", result);
        return result;
    }

    // Create compute pipeline
    result = create_compute_pipeline();
    if (result != VK_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to create compute pipeline: %d", result);
        return result;
    }

    // Create synchronization objects
    VkSemaphoreCreateInfo semInfo = {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = vkCreateSemaphore(m_vkDevice, &semInfo, nullptr, &m_completionSemaphore);
    if (result != VK_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to create semaphore: %d", result);
        return result;
    }

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled
    result = vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &m_fence);
    if (result != VK_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to create fence: %d", result);
        return result;
    }

    // Check for cooperative matrix support (Universal)
    // We look for VK_KHR_cooperative_matrix (Standard) 
    // or VK_NV_cooperative_matrix (Legacy NVIDIA)
    
    // For now, let's just flag it as available if the extension is enabled in the instance/device
    // (In a full implementation we would query vkGetPhysicalDeviceFeatures2)
    m_hasTensorCores = true; // Placeholder for v0.3.0 alpha
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(m_vkPhysicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(m_vkPhysicalDevice, nullptr, &extensionCount, extensions.data());

    for (const auto& ext : extensions) {
        if (strcmp(ext.extensionName, "VK_KHR_cooperative_matrix") == 0) {
            m_hasTensorCores = true;
            MONOEYE_LOG("Universal AI Accelerators (KHR Cooperative Matrix) detected.");
            break;
        }
        if (strcmp(ext.extensionName, "VK_NV_cooperative_matrix") == 0) {
            m_hasTensorCores = true;
            MONOEYE_LOG("NVIDIA Tensor Cores (NV Cooperative Matrix) detected.");
            break;
        }
    }

    m_initialized = true;
    MONOEYE_LOG("Vulkan warp pipeline initialized successfully %s", 
        m_hasTensorCores ? "with Tensor Core support" : "(standard compute only)");

    return VK_SUCCESS;
}

void WarpPipeline::shutdown() {
    if (!m_initialized) {
        return;
    }

    MONOEYE_LOG("Shutting down Vulkan warp pipeline");

    vkDeviceWaitIdle(m_vkDevice);

    if (m_fence) {
        vkDestroyFence(m_vkDevice, m_fence, nullptr);
        m_fence = VK_NULL_HANDLE;
    }

    if (m_completionSemaphore) {
        vkDestroySemaphore(m_vkDevice, m_completionSemaphore, nullptr);
        m_completionSemaphore = VK_NULL_HANDLE;
    }

    if (m_computePipeline) {
        vkDestroyPipeline(m_vkDevice, m_computePipeline, nullptr);
        m_computePipeline = VK_NULL_HANDLE;
    }

    if (m_pipelineLayout) {
        vkDestroyPipelineLayout(m_vkDevice, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }

    if (m_descriptorSetLayout) {
        vkDestroyDescriptorSetLayout(m_vkDevice, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }

    if (m_descriptorPool) {
        vkDestroyDescriptorPool(m_vkDevice, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    if (m_commandBuffer) {
        vkFreeCommandBuffers(m_vkDevice, m_commandPool, 1, &m_commandBuffer);
        m_commandBuffer = VK_NULL_HANDLE;
    }

    if (m_commandPool) {
        vkDestroyCommandPool(m_vkDevice, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    if (m_temporalView) {
        vkDestroyImageView(m_vkDevice, m_temporalView, nullptr);
        m_temporalView = VK_NULL_HANDLE;
    }

    if (m_temporalImage) {
        vkDestroyImage(m_vkDevice, m_temporalImage, nullptr);
        m_temporalImage = VK_NULL_HANDLE;
    }

    if (m_temporalMemory) {
        vkFreeMemory(m_vkDevice, m_temporalMemory, nullptr);
        m_temporalMemory = VK_NULL_HANDLE;
    }

    m_vkInstance = VK_NULL_HANDLE;
    m_vkDevice = VK_NULL_HANDLE;
    m_vkQueue = VK_NULL_HANDLE;
    m_queueFamilyIndex = 0;
    m_initialized = false;
}

VkResult WarpPipeline::create_descriptor_resources() {
    // Descriptor set layout:
    // Binding 2: right eye output (storage image)
    // Binding 3: previous frame (storage image for accumulation)
    // Binding 4: motion vectors (sampled image)

    VkDescriptorSetLayoutBinding bindings[5] = {};

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 5;
    layoutInfo.pBindings = bindings;

    VkResult result = vkCreateDescriptorSetLayout(m_vkDevice, &layoutInfo, nullptr, &m_descriptorSetLayout);
    if (result != VK_SUCCESS) {
        return result;
    }

    // Create descriptor pool
    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[0].descriptorCount = 3; // Color + Depth + Motion
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 2; // Output + Temporal

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;

    result = vkCreateDescriptorPool(m_vkDevice, &poolInfo, nullptr, &m_descriptorPool);
    if (result != VK_SUCCESS) {
        return result;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;

    result = vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &m_descriptorSet);
    if (result != VK_SUCCESS) {
        return result;
    }

    return VK_SUCCESS;
}

VkResult WarpPipeline::create_compute_pipeline() {
    VkPushConstantRange pushRange = {};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(WarpPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_descriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    VkResult result = vkCreatePipelineLayout(m_vkDevice, &layoutInfo, nullptr, &m_pipelineLayout);
    if (result != VK_SUCCESS) {
        return result;
    }

    // Load SPIR-V shader
#ifdef MONOEYE_EMBED_SHADERS
    VkShaderModuleCreateInfo shaderInfo = {};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = depth_warp_size;
    shaderInfo.pCode = reinterpret_cast<const uint32_t*>(depth_warp_data);

    VkShaderModule shaderModule;
    result = vkCreateShaderModule(m_vkDevice, &shaderInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to create shader module from embedded SPIR-V: %d", result);
        return result;
    }
#else
    MONOEYE_LOG_ERROR("No embedded shaders and no runtime shader loading implemented");
    return VK_ERROR_INITIALIZATION_FAILED;
#endif

    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = m_pipelineLayout;

    result = vkCreateComputePipelines(m_vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_computePipeline);

    vkDestroyShaderModule(m_vkDevice, shaderModule, nullptr);

    if (result != VK_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to create compute pipeline: %d", result);
        return result;
    }

    return VK_SUCCESS;
}

VkResult WarpPipeline::execute_warp(
    SwapchainImageInfo* leftColor,
    SwapchainImageInfo* leftDepth,
    SwapchainImageInfo* leftMotion,
    SwapchainImageInfo* rightColor,
    XrTime displayTime
) {
    (void)displayTime; // Future: use for temporal reprojection

    if (!m_initialized) {
        MONOEYE_LOG_ERROR("WarpPipeline not initialized");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (!leftColor || !rightColor || leftColor->vulkanImages.empty() || rightColor->vulkanImages.empty()) {
        MONOEYE_LOG_WARN("Missing Vulkan images for warp");
        return VK_SUCCESS; // Not an error, just skip
    }

    // Wait for previous warp to complete
    wait_for_completion();

    uint32_t width = rightColor->createInfo.width;
    uint32_t height = rightColor->createInfo.height;

    // Ensure temporal buffer is ready
    if (m_temporalWidth != width || m_temporalHeight != height) {
        ensure_temporal_buffer(width, height);
    }

    MONOEYE_LOG_DEBUG("Executing depth warp: %dx%d", width, height);

    // Get image indices (use first available for now)
    uint32_t leftColorIdx = 0;
    uint32_t rightColorIdx = 0;
    uint32_t leftDepthIdx = leftDepth ? 0 : 0;

    // Create image views
    VkImageView leftColorView = SwapchainTracker::get_instance().get_current_view(leftColor);
    VkImageView leftDepthView = leftDepth ? SwapchainTracker::get_instance().get_current_view(leftDepth) : VK_NULL_HANDLE;
    VkImageView leftMotionView = leftMotion ? SwapchainTracker::get_instance().get_current_view(leftMotion) : VK_NULL_HANDLE;
    VkImageView rightColorView = SwapchainTracker::get_instance().get_current_view(rightColor);

    // Use a format appropriate for the swapchain
    VkFormat colorFormat = VK_FORMAT_R8G8B8A8_SRGB; // Will be overridden based on actual format
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    // For now, create views with assumed formats. In production, derive from createInfo.format
    // OpenXR format -> Vulkan format mapping would be needed here

    if (!leftColorView || !rightColorView) {
        MONOEYE_LOG_ERROR("Failed to create image views for warp");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Update descriptor set
    VkDescriptorImageInfo colorInfo = {};
    colorInfo.imageView = leftColorView;
    colorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo depthInfo = {};
    depthInfo.imageView = leftDepth ? leftDepthView : leftColorView; // Fallback
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo outputInfo = {};
    outputInfo.imageView = rightColorView;
    outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writes[3] = {};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[0].pImageInfo = &colorInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_descriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[1].pImageInfo = &depthInfo;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = m_descriptorSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[2].pImageInfo = &outputInfo;

    VkDescriptorImageInfo temporalInfo = {};
    temporalInfo.imageView = m_temporalView;
    temporalInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet temporalWrite = {};
    temporalWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    temporalWrite.dstSet = m_descriptorSet;
    temporalWrite.dstBinding = 3;
    temporalWrite.descriptorCount = 1;
    temporalWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    temporalWrite.pImageInfo = &temporalInfo;

    vkUpdateDescriptorSets(m_vkDevice, 3, writes, 0, nullptr);
    vkUpdateDescriptorSets(m_vkDevice, 1, &temporalWrite, 0, nullptr);

    if (leftMotionView) {
        VkDescriptorImageInfo motionInfo = {};
        motionInfo.imageView = leftMotionView;
        motionInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        motionInfo.sampler = m_sampler;

        VkWriteDescriptorSet motionWrite = {};
        motionWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        motionWrite.dstSet = m_descriptorSet;
        motionWrite.dstBinding = 4;
        motionWrite.descriptorCount = 1;
        motionWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        motionWrite.pImageInfo = &motionInfo;

        vkUpdateDescriptorSets(m_vkDevice, 1, &motionWrite, 0, nullptr);
    }

    // Record and submit the compute command
    VkResult result = record_compute_command(leftColorView, leftDepthView, leftMotionView, rightColorView, width, height);

    return result;
}

VkResult WarpPipeline::record_compute_command(
    VkImageView leftColorView,
    VkImageView leftDepthView,
    VkImageView leftMotionView,
    VkImageView rightColorView,
    uint32_t width,
    uint32_t height
) {
    (void)leftColorView;
    (void)leftDepthView;
    (void)rightColorView;

    // Begin command buffer recording
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult result = vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        return result;
    }

    // Compute dispatch dimensions (8x8 workgroup)
    uint32_t groupCountX = (width + 7) / 8;
    uint32_t groupCountY = (height + 7) / 8;

    // Bind the compute pipeline
    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);

    // Push constants
    const Config& config = get_config();
    static uint32_t s_frame_index = 0;
    WarpPushConstants pc = {};
    pc.ipd = config.ipd_override > 0.0f ? config.ipd_override : 0.064f;
    pc.nearZ = 0.1f;    // Default near plane
    pc.farZ = 1000.0f;  // Default far plane
    pc.focalLength = 1.0f;
    pc.hasDepthBuffer = leftDepthView ? 1 : 0;
    pc.hasMotionBuffer = leftMotionView ? 1 : 0;
    pc.qualityMode = (uint32_t)config.warp_quality;
    pc.showIndicator = config.show_indicator ? 1 : 0;
    pc.tensorEnabled = (config.tensor_stabilization && m_hasTensorCores) ? 1 : 0;
    pc.specularRejection = config.specular_rejection ? 1 : 0;
    pc.edgeSmoothing = config.edge_smoothing ? 1 : 0;
    pc.upscaleFactor = config.render_width_percent / 100.0f;
    pc.frameIndex = s_frame_index++;

    vkCmdPushConstants(m_commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 
        0, sizeof(WarpPushConstants), &pc);

    // Bind the descriptor set
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

    // Dispatch the compute shader
    vkCmdDispatch(m_commandBuffer, groupCountX, groupCountY, 1);

    // End command buffer
    result = vkEndCommandBuffer(m_commandBuffer);
    if (result != VK_SUCCESS) {
        return result;
    }

    // Submit to the queue
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_completionSemaphore;

    // Reset fence before submit
    vkResetFences(m_vkDevice, 1, &m_fence);

    vkQueueSubmit(m_vkQueue, 1, &submitInfo, m_fence);
    
    // Copy output to temporal buffer for next frame
    // In a production system we might use ping-ponging, but this is simpler
    // We need to wait for the compute to finish before copying, or record it in the same CB
    // Recording in the same CB is better:
    
    return result;
}

VkResult WarpPipeline::ensure_temporal_buffer(uint32_t width, uint32_t height) {
    if (m_temporalView) vkDestroyImageView(m_vkDevice, m_temporalView, nullptr);
    if (m_temporalImage) vkDestroyImage(m_vkDevice, m_temporalImage, nullptr);
    if (m_temporalMemory) vkFreeMemory(m_vkDevice, m_temporalMemory, nullptr);

    m_temporalWidth = width;
    m_temporalHeight = height;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateImage(m_vkDevice, &imageInfo, nullptr, &m_temporalImage);
    if (result != VK_SUCCESS) return result;

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_vkDevice, m_temporalImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(m_vkPhysicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    result = vkAllocateMemory(m_vkDevice, &allocInfo, nullptr, &m_temporalMemory);
    if (result != VK_SUCCESS) return result;

    vkBindImageMemory(m_vkDevice, m_temporalImage, m_temporalMemory, 0);

    return create_image_view(m_temporalImage, VK_FORMAT_R8G8B8A8_UNORM, &m_temporalView);
}

void WarpPipeline::wait_for_completion() {
    if (!m_initialized || !m_fence) {
        return;
    }

    VkResult result = vkWaitForFences(m_vkDevice, 1, &m_fence, VK_TRUE, 16000000); // 16ms timeout
    if (result != VK_SUCCESS && result != VK_TIMEOUT) {
        MONOEYE_LOG_WARN("vkWaitForFences returned: %d", result);
    }
}

VkResult WarpPipeline::create_image_view(VkImage image, VkFormat format, VkImageView* view) {
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    return vkCreateImageView(m_vkDevice, &viewInfo, nullptr, view);
}

} // namespace monoeye
