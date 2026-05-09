// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vulkan/vulkan.h>

#ifdef _WIN32
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#endif

namespace monoeye {

#ifdef _WIN32

/**
 * @brief Imports a D3D12 texture into Vulkan for zero-copy sharing.
 * 
 * Uses VK_KHR_external_memory_win32 to bind a DXGI NT handle to a Vulkan image.
 */
VkResult import_d3d12_texture_to_vulkan(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    ID3D12Resource* d3d12Resource,
    VkFormat vkFormat,
    uint32_t width,
    uint32_t height,
    VkImage* outImage,
    VkDeviceMemory* outMemory
);

/**
 * @brief Creates a shared handle from a D3D12 resource by creating an intermediate texture.
 */
HANDLE create_d3d12_shared_handle(ID3D12Resource* resource, ID3D12Resource** outIntermediateResource);

/**
 * @brief Creates a D3D12 fence and a Vulkan semaphore that are linked.
 */
VkResult create_shared_fence(
    VkDevice device,
    ID3D12Device* d3d12Device,
    ID3D12Fence** outD3D12Fence,
    HANDLE* outSharedHandle,
    VkSemaphore* outVulkanSemaphore
);

#endif

} // namespace monoeye
