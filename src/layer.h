// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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

// OpenXR includes
#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>

// Layer negotiation entry point (must be C-exported)
extern "C" MONOEYE_EXPORT XrResult xrNegotiateLoaderApiLayerInterface(
    const XrNegotiateLoaderInfo* loaderInfo,
    const char* apiLayerName,
    XrNegotiateApiLayerRequest* apiLayerRequest
);

namespace monoeye {

struct InstanceState;

// Internal layer functions (not exported directly)
XrResult LayerXrCreateApiLayerInstance(
    const XrInstanceCreateInfo* info,
    const XrApiLayerCreateInfo* apiLayerInfo,
    XrInstance* instance
);

XrResult LayerXrDestroyInstance(XrInstance instance);

XrResult LayerXrGetInstanceProcAddr(
    XrInstance instance,
    const char* name,
    PFN_xrVoidFunction* function
);

} // namespace monoeye
