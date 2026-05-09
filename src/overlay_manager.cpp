// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "overlay_manager.h"
#include "warp_pipeline.h"
#include "logging.h"
#include "dispatch_table.h"
#include "vulkan_utils.h"
#ifdef _WIN32
#include <d3d11.h>
#include <d3d12.h>
#include "imgui_impl_dx11.h"
#include "imgui_impl_dx12.h"
#endif

#include <cstring>

namespace monoeye {

void OverlayManager::initialize(XrInstance instance, XrSession session, VkDevice device, VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkQueue queue) {
    if (m_initialized) return;

    m_xrInstance = instance;
    m_xrSession = session;
    m_vkInstance = WarpPipeline::get_instance().get_vk_instance();
    m_vkDevice = device;
    m_vkPhysicalDevice = physicalDevice;
    m_vkQueue = queue;
    m_queueFamily = queueFamilyIndex;

    MONOEYE_LOG("Initializing Unified OverlayManager (Vulkan Compute Composite)...");

    // 1. Create Internal UI Texture (1024x1024)
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = 1024;
    imageInfo.extent.height = 1024;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateImage(m_vkDevice, &imageInfo, nullptr, &m_uiImage);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_vkDevice, m_uiImage, &memReqs);

    VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = find_memory_type(m_vkPhysicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(m_vkDevice, &allocInfo, nullptr, &m_uiMemory);
    vkBindImageMemory(m_vkDevice, m_uiImage, m_uiMemory, 0);

    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = m_uiImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(m_vkDevice, &viewInfo, nullptr, &m_uiView);

    // 2. Create Render Pass & Framebuffer
    VkAttachmentDescription attachment = {};
    attachment.format = VK_FORMAT_R8G8B8A8_UNORM;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkRenderPassCreateInfo rpInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &attachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    vkCreateRenderPass(m_vkDevice, &rpInfo, nullptr, &m_renderPass);

    VkFramebufferCreateInfo fbInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbInfo.renderPass = m_renderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &m_uiView;
    fbInfo.width = 1024;
    fbInfo.height = 1024;
    fbInfo.layers = 1;
    vkCreateFramebuffer(m_vkDevice, &fbInfo, nullptr, &m_framebuffer);

    // 3. Create Command Pool & Buffer
    VkCommandPoolCreateInfo cpInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpInfo.queueFamilyIndex = m_queueFamily;
    cpInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(m_vkDevice, &cpInfo, nullptr, &m_commandPool);

    VkCommandBufferAllocateInfo cbInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbInfo.commandPool = m_commandPool;
    cbInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(m_vkDevice, &cbInfo, &m_commandBuffer);

    // 4. Initialize ImGui
    VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};
    VkDescriptorPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = pool_sizes;
    vkCreateDescriptorPool(m_vkDevice, &pool_info, nullptr, &m_descriptorPool);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_vkInstance;
    init_info.PhysicalDevice = m_vkPhysicalDevice;
    init_info.Device = m_vkDevice;
    init_info.Queue = m_vkQueue;
    init_info.DescriptorPool = m_descriptorPool;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&init_info, m_renderPass);

    m_initialized = true;
}

#ifdef _WIN32
void OverlayManager::initializeD3D11(XrInstance instance, XrSession session, ID3D11Device* device) {
    // We now use unified Vulkan UI via GraphicsManager
    GraphicsManager::get_instance().ensure_initialized();
    initialize(instance, session, 
        GraphicsManager::get_instance().get_device(),
        GraphicsManager::get_instance().get_physical_device(),
        GraphicsManager::get_instance().get_queue_family(),
        GraphicsManager::get_instance().get_queue());
}

void OverlayManager::initializeD3D12(XrInstance instance, XrSession session, ID3D12Device* device, ID3D12CommandQueue* queue) {
    initializeD3D11(instance, session, nullptr);
}
#endif

void OverlayManager::shutdown() {
    if (!m_initialized) return;
    vkDeviceWaitIdle(m_vkDevice);
    
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(m_vkDevice, m_descriptorPool, nullptr);
    vkDestroyCommandPool(m_vkDevice, m_commandPool, nullptr);
    vkDestroyFramebuffer(m_vkDevice, m_framebuffer, nullptr);
    vkDestroyRenderPass(m_vkDevice, m_renderPass, nullptr);
    vkDestroyImageView(m_vkDevice, m_uiView, nullptr);
    vkFreeMemory(m_vkDevice, m_uiMemory, nullptr);
    vkDestroyImage(m_vkDevice, m_uiImage, nullptr);
    
    m_initialized = false;
}

void OverlayManager::end_frame(XrTime displayTime) {
    if (!m_initialized) return;

    // Handle Hotkey (HOME)
    static bool homeWasDown = false;
#ifdef _WIN32
    bool homeIsDown = (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;
    if (homeIsDown && !homeWasDown) m_visible = !m_visible;
    homeWasDown = homeIsDown;
#endif

    if (!m_visible) return;

    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();

    // UI Layout
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(924, 924));
    ImGui::Begin("MonoEye Control Panel", nullptr, ImGuiWindowFlags_NoDecoration);
    
    ImGui::TextColored(ImVec4(0, 1, 1, 1), "MONOEYE RECONSTRUCTION v0.6.0");
    ImGui::Separator();
    
    const Config& config = get_config();
    ImGui::Text("Status: %s", config.enabled ? "ACTIVE" : "BYPASS");
    ImGui::Text("Quality: %s", config.warp_quality == 2 ? "Ultra (Bilateral)" : "Standard");
    
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "PERFORMANCE METRICS");
    ImGui::Text("Frame Time: %.2f ms", m_metrics.frameTimeMs);
    ImGui::Text("GPU Savings: %.1f%%", m_metrics.gpuSavings);
    
    ImGui::End();
    ImGui::Render();

    // Record Vulkan Commands
    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(m_commandBuffer, &beginInfo);

    VkRenderPassBeginInfo rpBegin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpBegin.renderPass = m_renderPass;
    rpBegin.framebuffer = m_framebuffer;
    rpBegin.renderArea.extent = {1024, 1024};
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
    rpBegin.clearValueCount = 1;
    rpBegin.pClearValues = &clearColor;

    vkCmdBeginRenderPass(m_commandBuffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffer);
    vkCmdEndRenderPass(m_commandBuffer);

    vkEndCommandBuffer(m_commandBuffer);

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    vkQueueSubmit(m_vkQueue, 1, &submitInfo, VK_NULL_HANDLE);
}

const XrCompositionLayerBaseHeader* OverlayManager::get_composition_layer() {
    return nullptr; // No separate layer, handled in compute
}

} // namespace monoeye
