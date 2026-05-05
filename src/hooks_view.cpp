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

    // We only want to report 1 view to the application
    if (viewCapacityInput == 0) {
        *viewCountOutput = 1;
        return XR_SUCCESS;
    }

    // Call the runtime to get the real view config for at least 1 view
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
    XrResult result = ((PFN_xrEnumerateViewConfigurationViews)dispatch->xrEnumerateViewConfigurationViews)(
        instance, systemId, viewConfigurationType, viewCapacityInput, &realViewCount, views);


    if (result == XR_SUCCESS) {
        *viewCountOutput = 1; // Lie to the app
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
        *viewCountOutput = 1;
        return XR_SUCCESS;
    }

    // We still need to locate both views internally for IPD calculation, 
    // but we only return the first one to the app.
    std::vector<XrView> realViews(2); // Assume 2 for VR
    for (auto& v : realViews) v.type = XR_TYPE_VIEW;

    uint32_t realViewCount = 0;
    XrResult result = ((PFN_xrLocateViews)dispatch->xrLocateViews)(
        session, viewLocateInfo, viewState, 2, &realViewCount, realViews.data());


    if (result == XR_SUCCESS) {
        // Copy only the first view (Left eye) to the app
        if (viewCapacityInput >= 1) {
            views[0] = realViews[0];
            *viewCountOutput = 1;
        }
    }

    return result;
}

} // namespace monoeye
