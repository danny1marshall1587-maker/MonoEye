// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "graphics_manager.h"
#include "logging.h"
#include "vulkan_utils.h"

#include <vector>
#include <cstring>

namespace monoeye {

GraphicsManager& GraphicsManager::get_instance() {
    static GraphicsManager instance;
    return instance;
}

GraphicsManager::~GraphicsManager() {
    shutdown();
}

VkResult GraphicsManager::ensure_initialized() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return VK_SUCCESS;
    
    return create_internal_context();
}

VkResult GraphicsManager::initialize_with_external(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    uint32_t queueFamilyIndex
) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return VK_SUCCESS;

    m_instance = instance;
    m_physicalDevice = physicalDevice;
    m_device = device;
    m_queueFamily = queueFamilyIndex;
    vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);
    
    m_initialized = true;
    m_owns_device = false;
    
    MONOEYE_LOG("GraphicsManager initialized with external Vulkan device");
    return VK_SUCCESS;
}

VkResult GraphicsManager::create_internal_context() {
    MONOEYE_LOG("Creating internal Vulkan context for D3D/Interop...");

    // 1. Create Instance
    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "MonoEye";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "MonoEyeEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    std::vector<const char*> instanceExtensions;
#ifdef _WIN32
    instanceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
    instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif

    VkInstanceCreateInfo createInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    VkResult res = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (res != VK_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to create internal Vulkan instance: %d", res);
        return res;
    }

    // 2. Select Physical Device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) return VK_ERROR_INITIALIZATION_FAILED;

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    m_physicalDevice = devices[0]; // Simplification: pick first device

    // 3. Create Device
    uint32_t graphicsFamily = 0, computeFamily = 0;
    find_queue_families(m_physicalDevice, &graphicsFamily, &computeFamily, nullptr);
    m_queueFamily = computeFamily;

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueCreateInfo.queueFamilyIndex = m_queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    std::vector<const char*> deviceExtensions;
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#ifdef _WIN32
    deviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
#endif

    VkDeviceCreateInfo deviceCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    res = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device);
    if (res != VK_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to create internal Vulkan device: %d", res);
        return res;
    }

    vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);

    m_initialized = true;
    m_owns_device = true;
    MONOEYE_LOG("Internal Vulkan context created successfully");

    return VK_SUCCESS;
}

void GraphicsManager::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;

    if (m_owns_device) {
        vkDestroyDevice(m_device, nullptr);
        vkDestroyInstance(m_instance, nullptr);
    }

    m_device = VK_NULL_HANDLE;
    m_instance = VK_NULL_HANDLE;
    m_initialized = false;
}

} // namespace monoeye
