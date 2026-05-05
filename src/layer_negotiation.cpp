// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "layer.h"
#include "logging.h"

#include <cstring>

} // namespace monoeye

// The core negotiation function - this is the only function the loader calls
// directly from our DLL via GetProcAddress. Everything else goes through
// xrGetInstanceProcAddr.
extern "C" MONOEYE_EXPORT XrResult xrNegotiateLoaderApiLayerInterface(
    const XrNegotiateLoaderInfo* loaderInfo,
    const char* apiLayerName,
    XrNegotiateApiLayerRequest* apiLayerRequest
) {
    MONOEYE_LOG("xrNegotiateLoaderApiLayerInterface called, layerName: %s",
                apiLayerName ? apiLayerName : "(null)");

    // Validate loader info
    if (!loaderInfo) {
        MONOEYE_LOG("ERROR: loaderInfo is null");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    if (loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        loaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION ||
        loaderInfo->structSize != sizeof(XrNegotiateLoaderInfo)) {
        MONOEYE_LOG("ERROR: loaderInfo struct is not valid");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Check API version compatibility
    if (loaderInfo->minApiVersion > XR_CURRENT_API_VERSION ||
        loaderInfo->maxApiVersion < XR_CURRENT_API_VERSION) {
        MONOEYE_LOG("ERROR: loader API version mismatch");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Check interface version compatibility
    if (loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION) {
        MONOEYE_LOG("ERROR: loader interface version mismatch");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Validate apiLayerRequest
    if (!apiLayerRequest ||
        apiLayerRequest->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST ||
        apiLayerRequest->structVersion != XR_API_LAYER_INFO_STRUCT_VERSION ||
        apiLayerRequest->structSize != sizeof(XrNegotiateApiLayerRequest)) {
        MONOEYE_LOG("ERROR: apiLayerRequest is not valid");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Fill in our capabilities
    apiLayerRequest->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
    apiLayerRequest->layerApiVersion = XR_CURRENT_API_VERSION;
    apiLayerRequest->getInstanceProcAddr = monoeye::LayerXrGetInstanceProcAddr;
    apiLayerRequest->createApiLayerInstance = monoeye::LayerXrCreateApiLayerInstance;

    MONOEYE_LOG("Negotiation successful - interface version: %u, API version: %u",
                apiLayerRequest->layerInterfaceVersion,
                apiLayerRequest->layerApiVersion);

    return XR_SUCCESS;
}

