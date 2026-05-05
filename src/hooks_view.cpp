// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "layer.h"
#include "logging.h"
#include "dispatch_table.h"
#include "config.h"
#include <vector>
#include <mutex>
#include <unordered_map>

namespace monoeye {

extern std::mutex s_session_map_mutex;
extern std::unordered_map<XrSession, XrInstance> s_session_map;

extern "C" XrResult monoeye_xrEnumerateViewConfigurationViews(
    XrInstance instance,
    XrSystemId systemId,
    XrViewConfigurationType viewConfigurationType,
    uint32_t viewCapacityInput,
    uint32_t* viewCountOutput,
    XrViewConfigurationView* views
) {
    const Config& config = get_config();
    
    // Pass through if disabled
    if (!config.enabled || config.bypass_mode) {
        XrGeneratedDispatchTable* dispatch = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
            auto it = g_instance_dispatch_map.find(instance);
            if (it != g_instance_dispatch_map.end()) {
                dispatch = it->second;
            }
        }
        if (!dispatch || !dispatch->xrEnumerateViewConfigurationViews) return XR_ERROR_RUNTIME_FAILURE;
        return ((PFN_xrEnumerateViewConfigurationViews)dispatch->xrEnumerateViewConfigurationViews)(
            instance, systemId, viewConfigurationType, viewCapacityInput, viewCountOutput, views);

    }

    MONOEYE_LOG("xrEnumerateViewConfigurationViews: Intercepting for mono mode");

    // We lie to the app and say there is only 1 view, but the runtime 
    // might require more (e.g., 2 for stereo).
    if (viewCapacityInput == 0) {
        if (viewCountOutput) *viewCountOutput = 1;
        return XR_SUCCESS;
    }

    // Call the runtime to get the real views using a temporary buffer
    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        auto it = g_instance_dispatch_map.find(instance);
        if (it != g_instance_dispatch_map.end()) {
            dispatch = it->second;
        }
    }
    if (!dispatch || !dispatch->xrEnumerateViewConfigurationViews) return XR_ERROR_RUNTIME_FAILURE;

    uint32_t realViewCount = 0;
    // Get the real count first
    ((PFN_xrEnumerateViewConfigurationViews)dispatch->xrEnumerateViewConfigurationViews)(
        instance, systemId, viewConfigurationType, 0, &realViewCount, nullptr);

    if (realViewCount == 0) return XR_ERROR_RUNTIME_FAILURE;

    std::vector<XrViewConfigurationView> realViews(realViewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    XrResult result = ((PFN_xrEnumerateViewConfigurationViews)dispatch->xrEnumerateViewConfigurationViews)(
        instance, systemId, viewConfigurationType, realViewCount, &realViewCount, realViews.data());

    if (result == XR_SUCCESS) {
        if (views && viewCapacityInput >= 1) {
            views[0] = realViews[0]; // Give the app the first one
        }
        if (viewCountOutput) *viewCountOutput = 1;
    }

    return result;
}

extern "C" XrResult monoeye_xrLocateViews(
    XrSession session,
    const XrViewLocateInfo* viewLocateInfo,
    XrViewState* viewState,
    uint32_t viewCapacityInput,
    uint32_t* viewCountOutput,
    XrView* views
) {
    const Config& config = get_config();

    // Find the instance for this session
    XrInstance instance = XR_NULL_HANDLE;
    {
        std::lock_guard<std::mutex> lock(s_session_map_mutex);
        auto it = s_session_map.find(session);
        if (it != s_session_map.end()) {
            instance = it->second;
        }
    }

    if (instance == XR_NULL_HANDLE) return XR_ERROR_SESSION_LOST;

    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        auto it = g_instance_dispatch_map.find(instance);
        if (it != g_instance_dispatch_map.end()) {
            dispatch = it->second;
        }
    }
    if (!dispatch || !dispatch->xrLocateViews) return XR_ERROR_RUNTIME_FAILURE;

    // Pass through if disabled
    if (!config.enabled || config.bypass_mode) {
        return ((PFN_xrLocateViews)dispatch->xrLocateViews)(
            session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);
    }

    // MONO MODE: Application only sees 1 view
    if (viewCapacityInput == 0) {
        if (viewCountOutput) *viewCountOutput = 1;
        return XR_SUCCESS;
    }

    // We must call the runtime with at least the real number of views it expects.
    // Usually 2 for stereo.
    uint32_t realViewCount = 2; // Default to 2
    std::vector<XrView> realViews(realViewCount, {XR_TYPE_VIEW});

    XrResult result = ((PFN_xrLocateViews)dispatch->xrLocateViews)(
        session, viewLocateInfo, viewState, realViewCount, &realViewCount, realViews.data());

    // If runtime returns more than our guess, retry
    if (result == XR_ERROR_SIZE_INSUFFICIENT) {
        realViews.resize(realViewCount, {XR_TYPE_VIEW});
        result = ((PFN_xrLocateViews)dispatch->xrLocateViews)(
            session, viewLocateInfo, viewState, realViewCount, &realViewCount, realViews.data());
    }

    if (result == XR_SUCCESS) {
        // Copy only the first view to the app
        if (views && viewCapacityInput >= 1) {
            views[0] = realViews[0];
        }
        if (viewCountOutput) *viewCountOutput = 1;
    }

    return result;
}

} // namespace monoeye
