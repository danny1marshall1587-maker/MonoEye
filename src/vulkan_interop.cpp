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

HANDLE create_d3d12_shared_handle(ID3D12Resource* resource, ID3D12Resource** outIntermediateResource) {
    HANDLE sharedHandle = nullptr;
    ID3D12Device* device = nullptr;
    resource->GetDevice(IID_PPV_ARGS(&device));
    
    if (!device) {
        MONOEYE_LOG_ERROR("Failed to get D3D12 device from resource");
        return nullptr;
    }

    D3D12_RESOURCE_DESC desc = resource->GetDesc();
    desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    ID3D12Resource* intermediateResource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_SHARED,
        &desc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&intermediateResource)
    );

    if (FAILED(hr)) {
        MONOEYE_LOG_ERROR("Failed to create intermediate D3D12 resource: 0x%08X", hr);
        device->Release();
        return nullptr;
    }

    hr = device->CreateSharedHandle(
        intermediateResource,
        nullptr,
        GENERIC_ALL,
        nullptr,
        &sharedHandle
    );

    device->Release();

    if (FAILED(hr)) {
        MONOEYE_LOG_ERROR("Failed to create D3D12 shared handle: 0x%08X", hr);
        intermediateResource->Release();
        return nullptr;
    }

    if (outIntermediateResource) {
        *outIntermediateResource = intermediateResource;
    } else {
        intermediateResource->Release();
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
    ID3D12Resource* intermediateResource = nullptr;
    HANDLE sharedHandle = create_d3d12_shared_handle(d3d12Resource, &intermediateResource);
    if (!sharedHandle) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
    // Note: The caller is responsible for copying data from d3d12Resource into intermediateResource
    // using a D3D12 command list during the render loop. If we don't return intermediateResource,
    // we leak it here, but since this function is not actively used in Phase 0, we release it for now
    // to avoid leaks until the full pipeline is implemented.
    if (intermediateResource) {
        intermediateResource->Release();
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

VkResult create_shared_fence(
    VkDevice device,
    ID3D12Device* d3d12Device,
    ID3D12Fence** outD3D12Fence,
    HANDLE* outSharedHandle,
    VkSemaphore* outVulkanSemaphore
) {
    if (!device || !d3d12Device || !outD3D12Fence || !outSharedHandle || !outVulkanSemaphore) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    HRESULT hr = d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(outD3D12Fence));
    if (FAILED(hr)) {
        MONOEYE_LOG_ERROR("Failed to create D3D12 fence: 0x%08X", hr);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    hr = d3d12Device->CreateSharedHandle(
        *outD3D12Fence,
        nullptr,
        GENERIC_ALL,
        nullptr,
        outSharedHandle
    );

    if (FAILED(hr)) {
        MONOEYE_LOG_ERROR("Failed to create shared handle for D3D12 fence: 0x%08X", hr);
        (*outD3D12Fence)->Release();
        *outD3D12Fence = nullptr;
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkExportSemaphoreCreateInfo exportInfo = {};
    exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
    exportInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;

    VkSemaphoreTypeCreateInfo timelineCreateInfo = {};
    timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineCreateInfo.pNext = &exportInfo;
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = 0;

    VkSemaphoreCreateInfo semInfo = {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semInfo.pNext = &timelineCreateInfo;

    VkResult res = vkCreateSemaphore(device, &semInfo, nullptr, outVulkanSemaphore);
    if (res != VK_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to create Vulkan semaphore for external fence: %d", res);
        CloseHandle(*outSharedHandle);
        (*outD3D12Fence)->Release();
        *outD3D12Fence = nullptr;
        return res;
    }

    VkImportSemaphoreWin32HandleInfoKHR importInfo = {};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR;
    importInfo.semaphore = *outVulkanSemaphore;
    importInfo.flags = 0;
    importInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;
    importInfo.handle = *outSharedHandle;
    importInfo.name = nullptr;

    auto pfnImportSemaphoreWin32HandleKHR = (PFN_vkImportSemaphoreWin32HandleKHR)
        vkGetDeviceProcAddr(device, "vkImportSemaphoreWin32HandleKHR");

    if (!pfnImportSemaphoreWin32HandleKHR) {
        MONOEYE_LOG_ERROR("vkImportSemaphoreWin32HandleKHR not found");
        vkDestroySemaphore(device, *outVulkanSemaphore, nullptr);
        CloseHandle(*outSharedHandle);
        (*outD3D12Fence)->Release();
        *outD3D12Fence = nullptr;
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    res = pfnImportSemaphoreWin32HandleKHR(device, &importInfo);
    if (res != VK_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to import D3D12 fence handle to Vulkan semaphore: %d", res);
        vkDestroySemaphore(device, *outVulkanSemaphore, nullptr);
        CloseHandle(*outSharedHandle);
        (*outD3D12Fence)->Release();
        *outD3D12Fence = nullptr;
        return res;
    }

    return VK_SUCCESS;
}

#endif

} // namespace monoeye
