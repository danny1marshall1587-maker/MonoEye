// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#ifdef _WIN32
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <unknwn.h>
    #include <d3d11.h>
    #include <d3d12.h>
#endif

#include <vulkan/vulkan.h>

// Platform-specific export macros
#ifdef _WIN32
    #ifdef MONOEYE_EXPORTS
        #define MONOEYE_EXPORT __declspec(dllexport)
    #else
        #define MONOEYE_EXPORT __declspec(dllimport)
    #endif
    #define MONOEYE_CALL __stdcall
#else
    #define MONOEYE_EXPORT __attribute__((visibility("default")))
    #define MONOEYE_CALL
#endif

// OpenXR configuration
#ifndef XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_VULKAN
#endif

#ifdef _WIN32
    #ifndef XR_USE_PLATFORM_WIN32
    #define XR_USE_PLATFORM_WIN32
    #endif
    #ifndef XR_USE_GRAPHICS_API_D3D11
    #define XR_USE_GRAPHICS_API_D3D11
    #endif
    #ifndef XR_USE_GRAPHICS_API_D3D12
    #define XR_USE_GRAPHICS_API_D3D12
    #endif
#endif

// OpenXR includes
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_loader_negotiation.h>

// Layer negotiation entry points (must be C-exported)
extern "C" {
    MONOEYE_EXPORT XRAPI_ATTR XrResult XRAPI_CALL xrNegotiateLoaderApiLayerInterface(
        const XrNegotiateLoaderInfo* loaderInfo,
        const char* apiLayerName,
        XrNegotiateApiLayerRequest* apiLayerRequest
    );

    // Core layer entry points called by the loader via the pointers returned during negotiation
    XRAPI_ATTR XrResult XRAPI_CALL LayerXrCreateApiLayerInstance(
        const XrInstanceCreateInfo* info,
        const XrApiLayerCreateInfo* apiLayerInfo,
        XrInstance* instance
    );

    XRAPI_ATTR XrResult XRAPI_CALL LayerXrGetInstanceProcAddr(
        XrInstance instance,
        const char* name,
        PFN_xrVoidFunction* function
    );

    // Internal layer function called via ProcAddr hook
    XRAPI_ATTR XrResult XRAPI_CALL LayerXrDestroyInstance(XrInstance instance);
}

namespace monoeye {

enum SessionType {
    SESSION_VULKAN,
    SESSION_D3D11,
    SESSION_D3D12,
    SESSION_UNKNOWN
};

struct SessionState {
    XrInstance instance;
    SessionType type;
#ifdef _WIN32
    ID3D12Device* d3d12_device = nullptr;
    ID3D12CommandQueue* d3d12_queue = nullptr;
#endif
};

// Global next GetInstanceProcAddr for NULL-instance calls
extern PFN_xrGetInstanceProcAddr g_nextGetInstanceProcAddr;

struct InstanceState;

} // namespace monoeye
