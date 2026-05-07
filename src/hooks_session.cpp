// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "config.h"
#include "dispatch_table.h"
#include "layer.h"
#include "logging.h"
#include "overlay_manager.h"
#include "swapchain_tracker.h"
#include "warp_pipeline.h"
#include <openxr/openxr_platform.h>
#include <vulkan/vulkan.h>

#include <mutex>
#include <unordered_map>

namespace monoeye {

// Track which instance owns each session and its graphics API
extern std::mutex s_session_map_mutex;
extern std::unordered_map<XrSession, SessionState> s_session_map;

extern "C" XrResult XRAPI_CALL
monoeye_xrCreateSession(XrInstance instance,
                        const XrSessionCreateInfo *createInfo,
                        XrSession *session) {
  MONOEYE_LOG("xrCreateSession called");

  XrGeneratedDispatchTable *dispatch = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
    auto it = g_instance_dispatch_map.find(instance);
    if (it != g_instance_dispatch_map.end()) {
      dispatch = it->second;
    }
  }

  if (!dispatch || !dispatch->xrCreateSession) {
    MONOEYE_LOG_ERROR("No dispatch table or CreateSession function");
    return XR_ERROR_RUNTIME_FAILURE;
  }

  // Call down to create the session
  XrResult result = ((PFN_xrCreateSession)dispatch->xrCreateSession)(
      instance, createInfo, session);

  if (result != XR_SUCCESS) {
    MONOEYE_LOG_ERROR("xrCreateSession failed: %d", result);
    return result;
  }

  MONOEYE_LOG("Session created: %p", (void *)(uintptr_t)*session);

  // Map session to instance and detect type
  {
    std::lock_guard<std::mutex> lock(s_session_map_mutex);
    SessionState state = {instance, SESSION_UNKNOWN};
    
    if (createInfo && createInfo->next) {
      const XrBaseInStructure *header =
          reinterpret_cast<const XrBaseInStructure *>(createInfo->next);
      while (header) {
        if (header->type == XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR ||
            header->type == XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR) {
          state.type = SESSION_VULKAN;
          MONOEYE_LOG("Vulkan session detected");
          break;
        } 
#ifdef _WIN32
        else if (header->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR) {
          state.type = SESSION_D3D11;
          MONOEYE_LOG("DirectX 11 session detected (AC Evo / Legacy DX11)");
          break;
        } else if (header->type == XR_TYPE_GRAPHICS_BINDING_D3D12_KHR) {
          state.type = SESSION_D3D12;
          MONOEYE_LOG("DirectX 12 session detected (AC Evo / Modern DX12)");
          break;
        }
#endif
        header = header->next;
      }
    }
    s_session_map[*session] = state;
  }

  // Initialize the Vulkan warp pipeline if enabled
  const Config &config = get_config();
  if (config.enabled && !config.bypass_mode) {
    if (createInfo && createInfo->next) {
      // Walk the next chain to find the Vulkan binding
      const XrBaseInStructure *header =
          reinterpret_cast<const XrBaseInStructure *>(createInfo->next);
      while (header) {
        if (header->type == XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR ||
            header->type == XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR) {

          const XrGraphicsBindingVulkan2KHR *vb =
              reinterpret_cast<const XrGraphicsBindingVulkan2KHR *>(header);

          MONOEYE_LOG(
              "Vulkan session detected: device=%p, instance=%p, queueFamily=%u",
              (void *)vb->device, (void *)vb->instance, vb->queueFamilyIndex);

          // Get the physical device
          VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
          XrVulkanGraphicsDeviceGetInfoKHR getInfo = {
              XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
          getInfo.systemId = createInfo->systemId;
          getInfo.vulkanInstance = vb->instance;

          if (dispatch->xrGetVulkanGraphicsDevice2KHR) {
            ((PFN_xrGetVulkanGraphicsDevice2KHR)
                 dispatch->xrGetVulkanGraphicsDevice2KHR)(instance, &getInfo,
                                                          &physicalDevice);
          } else if (dispatch->xrGetVulkanGraphicsDeviceKHR) {
            ((PFN_xrGetVulkanGraphicsDeviceKHR)
                 dispatch->xrGetVulkanGraphicsDeviceKHR)(
                instance, createInfo->systemId, vb->instance, &physicalDevice);
          }

          // Initialize the warp pipeline with this Vulkan device
          WarpPipeline::get_instance().initialize(
              vb->instance, physicalDevice, vb->device, vb->queueFamilyIndex);

          // Initialize the overlay manager
          OverlayManager::get_instance().initialize(
              instance, *session, vb->device, physicalDevice,
              vb->queueFamilyIndex, WarpPipeline::get_instance().get_queue());

          break;
        }
#ifdef _WIN32
        else if (header->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR ||
                 header->type == XR_TYPE_GRAPHICS_BINDING_D3D12_KHR) {
          
          if (header->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR) {
            const XrGraphicsBindingD3D11KHR *db = reinterpret_cast<const XrGraphicsBindingD3D11KHR *>(header);
            OverlayManager::get_instance().initializeD3D11(instance, *session, db->device);
          } else {
            const XrGraphicsBindingD3D12KHR *db = reinterpret_cast<const XrGraphicsBindingD3D12KHR *>(header);
            OverlayManager::get_instance().initializeD3D12(instance, *session, db->device, db->queue);
          }
          break;
        }
#endif
        header = header->next;
      }
    }
  }

  return XR_SUCCESS;
}

extern "C" XrResult XRAPI_CALL monoeye_xrDestroySession(XrSession session) {
  MONOEYE_LOG("xrDestroySession called: %p", (void *)(uintptr_t)session);

  // Find the instance for this session
  XrInstance instance = XR_NULL_HANDLE;
  {
    std::lock_guard<std::mutex> lock(s_session_map_mutex);
    auto it = s_session_map.find(session);
    if (it != s_session_map.end()) {
      instance = it->second.instance;
      s_session_map.erase(it);
    }
  }

  // Shut down the warp pipeline
  WarpPipeline::get_instance().shutdown();
  OverlayManager::get_instance().shutdown();

  XrGeneratedDispatchTable *dispatch = nullptr;
  if (instance != XR_NULL_HANDLE) {
    std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
    auto it = g_instance_dispatch_map.find(instance);
    if (it != g_instance_dispatch_map.end()) {
      dispatch = it->second;
    }
  }

  if (!dispatch || !dispatch->xrDestroySession) {
    return XR_ERROR_RUNTIME_FAILURE;
  }

  return ((PFN_xrDestroySession)dispatch->xrDestroySession)(session);
}

} // namespace monoeye
