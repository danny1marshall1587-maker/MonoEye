// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "overlay_manager.h"
#include "warp_pipeline.h"
#include "logging.h"
#include "dispatch_table.h"
#include "vulkan_utils.h"

#include <cstring>

namespace monoeye {

void OverlayManager::initialize(XrInstance instance, XrSession session, VkDevice device, VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkQueue queue) {
    if (m_initialized) return;

    m_xrInstance = instance;
    m_xrSession = session;
    m_vkInstance = WarpPipeline::get_instance().get_vk_instance();
    m_vkDevice = device;

    // Retrieve dispatch table
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        auto it = g_instance_dispatch_map.find(instance);
        if (it != g_instance_dispatch_map.end()) {
            m_dispatch = it->second;
        }
    }

    if (!m_dispatch) {
        MONOEYE_LOG_ERROR("Failed to find dispatch table for instance %p", (void*)instance);
        return;
    }


    MONOEYE_LOG("Initializing OverlayManager...");

    // 1. Create OpenXR Swapchain for UI (1024x1024)
    XrSwapchainCreateInfo swapchainInfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    swapchainInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    swapchainInfo.sampleCount = 1;
    swapchainInfo.width = 1024;
    swapchainInfo.height = 1024;
    swapchainInfo.faceCount = 1;
    swapchainInfo.arraySize = 1;
    swapchainInfo.mipCount = 1;

    XrResult result = ((PFN_xrCreateSwapchain)m_dispatch->CreateSwapchain)(m_xrSession, &swapchainInfo, &m_swapchain);


    if (result != XR_SUCCESS) {
        MONOEYE_LOG_ERROR("Failed to create overlay swapchain: %d", result);
        return;
    }

    // 2. Get swapchain images
    uint32_t imageCount = 0;
    ((PFN_xrEnumerateSwapchainImages)m_dispatch->EnumerateSwapchainImages)(m_swapchain, 0, &imageCount, nullptr);
    std::vector<XrSwapchainImageVulkanKHR> images(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
    ((PFN_xrEnumerateSwapchainImages)m_dispatch->EnumerateSwapchainImages)(m_swapchain, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)images.data());



    for (uint32_t i = 0; i < imageCount; ++i) {
        m_images.push_back(images[i].image);
        
        VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = images[i].image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        
        VkImageView view;
        vkCreateImageView(m_vkDevice, &viewInfo, nullptr, &view);
        m_imageViews.push_back(view);
    }

    // 2. Create Descriptor Pool for ImGui
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    vkCreateDescriptorPool(m_vkDevice, &pool_info, nullptr, &m_descriptorPool);

    // 3. Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    
    // Customize style for VR (high contrast, large text)
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.ScaleAllSizes(2.0f); // Make it big for VR

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_vkInstance;
    init_info.PhysicalDevice = physicalDevice;
    init_info.Device = m_vkDevice;
    init_info.QueueFamily = queueFamilyIndex;
    init_info.Queue = queue;
    init_info.DescriptorPool = m_descriptorPool;
    init_info.MinImageCount = 2;
    init_info.ImageCount = (uint32_t)m_images.size();
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    
    ImGui_ImplVulkan_Init(&init_info);

    // 4. Initialize Quad Layer info
    m_quadLayer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
    m_quadLayer.next = nullptr;
    m_quadLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    m_quadLayer.space = XR_NULL_HANDLE; 
    m_quadLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    m_quadLayer.subImage.swapchain = m_swapchain;
    m_quadLayer.subImage.imageRect.offset = {0, 0};
    m_quadLayer.subImage.imageRect.extent = {1024, 1024};
    m_quadLayer.subImage.imageArrayIndex = 0;
    
    m_quadLayer.pose.orientation = {0, 0, 0, 1};
    m_quadLayer.pose.position = {0, 0, -1.5f};
    m_quadLayer.size = {1.0f, 1.0f};

    m_initialized = true;
    m_visible = false; 
}

void OverlayManager::shutdown() {
    if (!m_initialized) return;

    for (auto view : m_imageViews) {
        vkDestroyImageView(m_vkDevice, view, nullptr);
    }
    m_imageViews.clear();
    m_images.clear();

    if (m_swapchain && m_dispatch) {
        ((PFN_xrDestroySwapchain)m_dispatch->DestroySwapchain)(m_swapchain);


        m_swapchain = XR_NULL_HANDLE;
    }

    m_initialized = false;
}

void OverlayManager::begin_frame() {
    if (!m_initialized || !m_visible) return;
    // ImGui NewFrame logic will go here
}

#ifdef _WIN32
#include <windows.h>
#endif


void OverlayManager::end_frame(XrTime displayTime) {
    if (!m_initialized) return;

    // 1. Handle Keyboard Input
    static bool homeWasDown = false;
    bool homeIsDown = false;
#ifdef _WIN32
    homeIsDown = (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;
#endif

    
    if (homeIsDown && !homeWasDown) {
        m_visible = !m_visible;
        MONOEYE_LOG("Overlay visibility toggled: %s", m_visible ? "ON" : "OFF");
    }
    homeWasDown = homeIsDown;

    if (!m_visible) return;

    // 2. Handle Menu Navigation (Arrows)
    if (m_visible) {
        static float lastKeyTime = 0;
        float currentTime = (float)displayTime / 1e9f;
        
        if (currentTime - lastKeyTime > 0.15f) { // Cooldown for keys
            bool upPressed = false;
            bool downPressed = false;
            bool leftPressed = false;
            bool rightPressed = false;
#ifdef _WIN32
            upPressed = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
            downPressed = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0;
            leftPressed = (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0;
            rightPressed = (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0;
#endif


            if (upPressed) { m_menuIndex = (m_menuIndex - 1 + 4) % 4; lastKeyTime = currentTime; }
            if (downPressed) { m_menuIndex = (m_menuIndex + 1) % 4; lastKeyTime = currentTime; }
            
            // Left/Right to adjust values (Placeholder logic)
            if (leftPressed) { /* Decrease current setting */ lastKeyTime = currentTime; }
            if (rightPressed) { /* Increase current setting */ lastKeyTime = currentTime; }
        }
    }

    // 3. Render ImGui Menu
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(100, 100));
    ImGui::SetNextWindowSize(ImVec2(824, 824));
    ImGui::Begin("MonoEye Universal Suite v0.4.0", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
    
    ImGui::TextColored(ImVec4(0, 1, 1, 1), "MONOEYE ADVANCED CLARITY");
    ImGui::Separator();
    ImGui::Spacing();

    const char* options[] = { "FSR Scaling", "Specular Rejection", "Edge Smoothing", "Performance HUD" };
    for (int i = 0; i < 4; i++) {
        if (i == m_menuIndex) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "> %s", options[i]);
            
            // Handle Toggle for HUD
            if (i == 3) {
                bool rightPressed = false;
                bool leftPressed = false;
#ifdef _WIN32
                rightPressed = (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0;
                leftPressed = (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0;
#endif
                if (rightPressed) m_showHud = true;
                if (leftPressed) m_showHud = false;
            }
        } else {
            ImGui::Text("  %s", options[i]);
        }
    }

    
    ImGui::End();

    // 4. Render Performance HUD (Always on if enabled)
    if (m_showHud) {
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::Begin("Performance HUD", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground);
        
        static float lastTime = 0;
        float currentTime = (float)displayTime / 1e9f;
        m_metrics.frameTimeMs = (currentTime - lastTime) * 1000.0f;
        lastTime = currentTime;

        ImGui::TextColored(ImVec4(0, 1, 0, 1), "GPU SAVINGS: %.1f%%", m_metrics.gpuSavings);
        ImGui::Text("FRAME TIME: %.2f ms", m_metrics.frameTimeMs);
        ImGui::Text("RENDER LOAD: %.1f%%", (m_metrics.frameTimeMs / 11.1f) * 100.0f); // Assuming 90Hz target
        
        ImGui::End();
    }

    ImGui::Render();

    // 4. Acquire swapchain image and record draw commands
    uint32_t imageIndex;
    ((PFN_xrAcquireSwapchainImage)m_dispatch->AcquireSwapchainImage)(m_swapchain, nullptr, &imageIndex);
    
    XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
    ((PFN_xrWaitSwapchainImage)m_dispatch->WaitSwapchainImage)(m_swapchain, &waitInfo);
    
    // Placeholder: We need a command buffer and a way to submit to the queue
    // For alpha, we just release the image. Full rendering implementation follows in v0.5.1
    // To ensure build passes, we keep it simple but safe.

    ((PFN_xrReleaseSwapchainImage)m_dispatch->ReleaseSwapchainImage)(m_swapchain, nullptr);


}

const XrCompositionLayerBaseHeader* OverlayManager::get_composition_layer() {
    if (!m_initialized || !m_visible) return nullptr;
    return reinterpret_cast<const XrCompositionLayerBaseHeader*>(&m_quadLayer);
}

} // namespace monoeye
