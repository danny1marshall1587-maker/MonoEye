// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "layer.h"
#include "logging.h"
#include "dispatch_table.h"
#include "config.h"
#include <cstring>
#include <new>
#include <mutex>
#include <string>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace monoeye;

bool g_process_bypass = false;


extern "C" XRAPI_ATTR XrResult XRAPI_CALL LayerXrCreateApiLayerInstance(
    const XrInstanceCreateInfo* info,
    const XrApiLayerCreateInfo* apiLayerInfo,
    XrInstance* instance
) {
    MONOEYE_LOG("LayerXrCreateApiLayerInstance called");

#ifdef _WIN32
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
        std::string exeName = exePath;
        std::transform(exeName.begin(), exeName.end(), exeName.begin(), ::tolower);
        if (exeName.find("cef") != std::string::npos ||
            exeName.find("steamwebhelper") != std::string::npos) {
            g_process_bypass = true;
            MONOEYE_LOG_WARN("Sandboxed UI process detected (%s). MonoEye entering pure passthrough mode.", exePath);
        }
    }
#endif

    if (!apiLayerInfo || !info || !instance) {
        MONOEYE_LOG_ERROR("Invalid parameters to LayerXrCreateApiLayerInstance");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Validate the API layer info structure
    if (!apiLayerInfo || apiLayerInfo->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_CREATE_INFO ||
        apiLayerInfo->structVersion > XR_API_LAYER_CREATE_INFO_STRUCT_VERSION ||
        apiLayerInfo->structSize < sizeof(XrApiLayerCreateInfo)) {
        MONOEYE_LOG_ERROR("LayerXrCreateApiLayerInstance: Invalid apiLayerInfo struct");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    if (!apiLayerInfo->nextInfo ||
        apiLayerInfo->nextInfo->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_NEXT_INFO ||
        apiLayerInfo->nextInfo->structVersion > XR_API_LAYER_NEXT_INFO_STRUCT_VERSION ||
        apiLayerInfo->nextInfo->structSize < sizeof(XrApiLayerNextInfo)) {
        MONOEYE_LOG_ERROR("LayerXrCreateApiLayerInstance: Invalid nextInfo struct");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Ensure we have the necessary function pointers from the loader
    if (!apiLayerInfo->nextInfo->nextCreateApiLayerInstance ||
        !apiLayerInfo->nextInfo->nextGetInstanceProcAddr) {
        MONOEYE_LOG_ERROR("LayerXrCreateApiLayerInstance: Missing required function pointers from loader");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Load configuration
    Config config = load_config();
    (void)config;

    // Copy the layer info and advance the next pointer to pass to the next layer
    XrApiLayerCreateInfo newApiLayerInfo = *apiLayerInfo;
    newApiLayerInfo.nextInfo = apiLayerInfo->nextInfo->next;

    // Get the next layer's xrCreateApiLayerInstance function pointer
    PFN_xrCreateApiLayerInstance nextCreateInstance =
        apiLayerInfo->nextInfo->nextCreateApiLayerInstance;


    if (!nextCreateInstance) {
        MONOEYE_LOG_ERROR("nextCreateApiLayerInstance is null");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Call down to the next layer/runtime to create the instance
    XrResult result = nextCreateInstance(info, &newApiLayerInfo, instance);

    if (result != XR_SUCCESS) {
        MONOEYE_LOG_ERROR("xrCreateInstance failed with result: %d", result);
        return result;
    }

    MONOEYE_LOG("Instance created successfully: %p", (void*)(uintptr_t)*instance);

    // Capture the real downstream proc addr — this points to the real runtime,
    // NOT back to our layer. Store it globally and per-instance.
    PFN_xrGetInstanceProcAddr nextGetInstanceProcAddr =
        apiLayerInfo->nextInfo->nextGetInstanceProcAddr;

    monoeye::g_nextGetInstanceProcAddr = nextGetInstanceProcAddr;

    if (!nextGetInstanceProcAddr) {
        MONOEYE_LOG_ERROR("nextGetInstanceProcAddr is null");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Build the full dispatch table using the auto-generated function
    // to ensure all downstream extensions are captured correctly,
    // avoiding null pointers on unhooked queries.
    auto* nextDispatch = new (std::nothrow) XrGeneratedDispatchTable();
    if (!nextDispatch) {
        MONOEYE_LOG_ERROR("Failed to allocate dispatch table");
        return XR_ERROR_OUT_OF_MEMORY;
    }

    *nextDispatch = {};
    nextDispatch->nextGetInstanceProcAddr = nextGetInstanceProcAddr;

    // Fully populate the dispatch table to prevent AC Evo initialization crashes
    monoeye::GeneratedXrPopulateDispatchTable(nextDispatch, *instance, nextGetInstanceProcAddr);

    {
        std::lock_guard<std::mutex> lock(monoeye::g_instance_dispatch_mutex);
        monoeye::g_instance_dispatch_map[*instance] = nextDispatch;
    }

    MONOEYE_LOG("Dispatch table created for instance %p", (void*)(uintptr_t)*instance);

    return XR_SUCCESS;
}

extern "C" XrResult XRAPI_CALL LayerXrDestroyInstance(XrInstance instance) {
    MONOEYE_LOG("LayerXrDestroyInstance called for instance %p", (void*)(uintptr_t)instance);

    monoeye::XrGeneratedDispatchTable* nextDispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(monoeye::g_instance_dispatch_mutex);
        auto it = monoeye::g_instance_dispatch_map.find(instance);
        if (it != monoeye::g_instance_dispatch_map.end()) {
            nextDispatch = it->second;
            monoeye::g_instance_dispatch_map.erase(it);
        }
    }

    XrResult result = XR_SUCCESS;
    if (nextDispatch) {
        // Call down to destroy the instance
        if (nextDispatch->xrDestroyInstance) {
            result = ((PFN_xrDestroyInstance)nextDispatch->xrDestroyInstance)(instance);
        }

        // Clean up the dispatch table
        monoeye::MonoEyeCleanUpDispatchTable(nextDispatch);
    }

    MONOEYE_LOG("Instance %p destroyed with result: %d", (void*)(uintptr_t)instance, result);
    return result;
}
