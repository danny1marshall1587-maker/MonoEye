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
    XrGeneratedDispatchTable* dispatch = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_instance_dispatch_mutex);
        auto it = g_instance_dispatch_map.find(instance);
        if (it != g_instance_dispatch_map.end()) {
            dispatch = it->second;
        }
    }
    if (!dispatch || !dispatch->EnumerateViewConfigurationViews) return XR_ERROR_RUNTIME_FAILURE;

    const Config& config = get_config();

    // Pass through if disabled
    if (!config.enabled || config.bypass_mode) {
        return ((PFN_xrEnumerateViewConfigurationViews)dispatch->EnumerateViewConfigurationViews)(
            instance, systemId, viewConfigurationType, viewCapacityInput, viewCountOutput, views);
    }

    // Call the real runtime to get actual views
    XrResult result = ((PFN_xrEnumerateViewConfigurationViews)dispatch->EnumerateViewConfigurationViews)(
        instance, systemId, viewConfigurationType, viewCapacityInput, viewCountOutput, views);

    if (result != XR_SUCCESS) return result;

    // In mono mode: we keep the REAL view count (typically 2) so strict engines
    // don't assert, but we make both views identical (using left eye props).
    // The actual mono collapse happens in xrEndFrame.
    if (views && viewCountOutput && *viewCountOutput >= 2) {
        MONOEYE_LOG("xrEnumerateViewConfigurationViews: Returning %d views (stereo-compatible mono mode)", *viewCountOutput);
        // Mirror left eye properties into all other views so rendering is symmetric
        for (uint32_t i = 1; i < *viewCountOutput; ++i) {
            views[i] = views[0];
        }
    }

    return XR_SUCCESS;
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
    if (!dispatch || !dispatch->LocateViews) return XR_ERROR_RUNTIME_FAILURE;

    // Pass through if disabled
    if (!config.enabled || config.bypass_mode) {
        return ((PFN_xrLocateViews)dispatch->LocateViews)(
            session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);
    }

    // Call the real runtime with whatever capacity the app gave us
    XrResult result = ((PFN_xrLocateViews)dispatch->LocateViews)(
        session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);

    // If runtime returns more than our guess, retry with correct size
    if (result == XR_ERROR_SIZE_INSUFFICIENT && viewCountOutput) {
        uint32_t needed = *viewCountOutput;
        std::vector<XrView> tmpViews(needed, {XR_TYPE_VIEW});
        result = ((PFN_xrLocateViews)dispatch->LocateViews)(
            session, viewLocateInfo, viewState, needed, viewCountOutput, tmpViews.data());
        if (result == XR_SUCCESS && views && viewCapacityInput >= needed) {
            for (uint32_t i = 0; i < needed; ++i) views[i] = tmpViews[i];
        }
    }

    // Mono mode: mirror left eye pose/fov into all other views so the app
    // gets consistent stereo data but we only render once.
    if (result == XR_SUCCESS && views && viewCountOutput && *viewCountOutput >= 2) {
        for (uint32_t i = 1; i < *viewCountOutput; ++i) {
            views[i].pose = views[0].pose;
            views[i].fov  = views[0].fov;
        }
    }

    return result;
}

} // namespace monoeye
