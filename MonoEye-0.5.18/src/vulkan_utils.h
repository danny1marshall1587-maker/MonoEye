// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

namespace monoeye {

VkFormat xr_format_to_vk_format(int64_t xrFormat);
VkImageAspectFlags xr_format_to_aspect_mask(int64_t xrFormat);
bool is_depth_format(int64_t xrFormat);
bool is_stencil_format(int64_t xrFormat);
uint32_t get_format_bytes_per_pixel(int64_t xrFormat);

bool find_queue_families(
    VkPhysicalDevice physicalDevice,
    uint32_t* graphicsFamily,
    uint32_t* computeFamily,
    uint32_t* transferFamily
);

uint32_t find_memory_type(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

VkResult create_buffer(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer* buffer,
    VkDeviceMemory* bufferMemory
);

} // namespace monoeye
