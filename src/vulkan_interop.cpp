// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "vulkan_interop.h"
#include "logging.h"
#include "vulkan_utils.h"

#ifdef _WIN32
#include <vulkan/vulkan_win32.h>
#endif

namespace monoeye {

#ifdef _WIN32

HANDLE create_d3d12_shared_handle(ID3D12Resource* resource) {
    HANDLE sharedHandle = nullptr;
    ID3D12Device* device = nullptr;
    resource->GetDevice(IID_PPV_ARGS(&device));
    
    if (!device) {
        MONOEYE_LOG_ERROR("Failed to get D3D12 device from resource");
        return nullptr;
    }

    HRESULT hr = device->CreateSharedHandle(
        resource,
        nullptr,
        GENERIC_ALL,
        nullptr,
        &sharedHandle
    );

    device->Release();

    if (FAILED(hr)) {
        MONOEYE_LOG_ERROR("Failed to create D3D12 shared handle: 0x%08X", hr);
        return nullptr;
    }

    return sharedHandle;
}

VkResult import_d3d12_texture_to_vulkan(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    ID3D12Resource* d3d12Resource,
    VkFormat vkFormat,
    uint32_t width,
    uint32_t height,
    VkImage* outImage,
    VkDeviceMemory* outMemory
) {
    HANDLE sharedHandle = create_d3d12_shared_handle(d3d12Resource);
    if (!sharedHandle) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // 1. Create External Vulkan Image
    VkExternalMemoryImageCreateInfo externalMemoryImageInfo = {};
    externalMemoryImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryImageInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = &externalMemoryImageInfo;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = vkFormat;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult res = vkCreateImage(device, &imageInfo, nullptr, outImage);
    if (res != VK_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to create external Vulkan image: %d", res);
        CloseHandle(sharedHandle);
        return res;
    }

    // 2. Query Memory Requirements
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, *outImage, &memReqs);

    // 3. Import Memory from Shared Handle
    VkImportMemoryWin32HandleInfoKHR importInfo = {};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
    importInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    importInfo.handle = sharedHandle;

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &importInfo;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = find_memory_type(physicalDevice, memReqs.memoryTypeBits, 0);

    res = vkAllocateMemory(device, &allocInfo, nullptr, outMemory);
    if (res != VK_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to allocate imported memory: %d", res);
        vkDestroyImage(device, *outImage, nullptr);
        CloseHandle(sharedHandle);
        return res;
    }

    // 4. Bind Memory
    res = vkBindImageMemory(device, *outImage, *outMemory, 0);
    if (res != VK_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to bind imported memory: %d", res);
        vkFreeMemory(device, *outMemory, nullptr);
        vkDestroyImage(device, *outImage, nullptr);
        CloseHandle(sharedHandle);
        return res;
    }

    // Handle is owned by the memory object now, but we can close our local copy
    CloseHandle(sharedHandle);
    
    return VK_SUCCESS;
}

#endif

} // namespace monoeye
