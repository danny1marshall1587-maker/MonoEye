// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "logging.h"
#include <vulkan/vulkan.h>

#include <cstring>
#include <vector>

namespace monoeye {

// Map OpenXR format to Vulkan format
VkFormat xr_format_to_vk_format(int64_t xrFormat) {
    // OpenXR and Vulkan share the same format enum values for most formats
    return static_cast<VkFormat>(xrFormat);
}

// Map OpenXR format to Vulkan aspect mask
VkImageAspectFlags xr_format_to_aspect_mask(int64_t xrFormat) {
    switch (xrFormat) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

// Check if a format is a depth format
bool is_depth_format(int64_t xrFormat) {
    VkFormat fmt = xr_format_to_vk_format(xrFormat);
    return fmt == VK_FORMAT_D16_UNORM ||
           fmt == VK_FORMAT_D32_SFLOAT ||
           fmt == VK_FORMAT_D16_UNORM_S8_UINT ||
           fmt == VK_FORMAT_D32_SFLOAT_S8_UINT ||
           fmt == VK_FORMAT_X8_D24_UNORM_PACK32;
}

// Check if a format is a stencil format
bool is_stencil_format(int64_t xrFormat) {
    VkFormat fmt = xr_format_to_vk_format(xrFormat);
    return fmt == VK_FORMAT_S8_UINT ||
           fmt == VK_FORMAT_D16_UNORM_S8_UINT ||
           fmt == VK_FORMAT_D24_UNORM_S8_UINT ||
           fmt == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

// Get the number of bytes per pixel for a format
uint32_t get_format_bytes_per_pixel(int64_t xrFormat) {
    VkFormat fmt = xr_format_to_vk_format(xrFormat);
    switch (fmt) {
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SRGB:
            return 4;
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            return 8;
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return 16;
        case VK_FORMAT_D32_SFLOAT:
            return 4;
        case VK_FORMAT_D16_UNORM:
            return 2;
        case VK_FORMAT_R8_UNORM:
            return 1;
        default:
            return 4; // Safe default
    }
}

// Get Vulkan queue family indices for a device
bool find_queue_families(
    VkPhysicalDevice physicalDevice,
    uint32_t* graphicsFamily,
    uint32_t* computeFamily,
    uint32_t* transferFamily
) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    bool foundGraphics = false;
    bool foundCompute = false;
    bool foundTransfer = false;

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        // Graphics queue (also handles compute and transfer)
        if (!foundGraphics && (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            if (graphicsFamily) *graphicsFamily = i;
            foundGraphics = true;
        }

        // Dedicated compute queue
        if (!foundCompute && (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            if (computeFamily) *computeFamily = i;
            foundCompute = true;
        }

        // Dedicated transfer queue
        if (!foundTransfer && (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            !(queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            if (transferFamily) *transferFamily = i;
            foundTransfer = true;
        }
    }

    // If no dedicated compute queue found, use graphics queue
    if (!foundCompute && graphicsFamily) {
        if (computeFamily) *computeFamily = *graphicsFamily;
        foundCompute = true;
    }

    // If no dedicated transfer queue found, use graphics queue
    if (!foundTransfer && graphicsFamily) {
        if (transferFamily) *transferFamily = *graphicsFamily;
        foundTransfer = true;
    }

    return foundGraphics && foundCompute && foundTransfer;
}

// Helper to create a Vulkan buffer
VkResult create_buffer(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer* buffer,
    VkDeviceMemory* bufferMemory
) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, buffer);
    if (result != VK_SUCCESS) {
        return result;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, *buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;

    // Find a memory type that meets the requirements
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    bool found = false;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            allocInfo.memoryTypeIndex = i;
            found = true;
            break;
        }
    }

    if (!found) {
        vkDestroyBuffer(device, *buffer, nullptr);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    result = vkAllocateMemory(device, &allocInfo, nullptr, bufferMemory);
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(device, *buffer, nullptr);
        return result;
    }

    vkBindBufferMemory(device, *buffer, *bufferMemory, 0);

    return VK_SUCCESS;
}

} // namespace monoeye
