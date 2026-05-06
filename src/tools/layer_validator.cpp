#include <openxr/openxr.h>
#include <iostream>
#include <vector>
#include <cstring>

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   MONOEYE LAYER VALIDATOR" << std::endl;
    std::cout << "========================================" << std::endl;

    // 1. Enumerate API Layers
    uint32_t layerCount = 0;
    xrEnumerateApiLayerProperties(0, &layerCount, nullptr);
    std::vector<XrApiLayerProperties> layers(layerCount, {XR_TYPE_API_LAYER_PROPERTIES});
    xrEnumerateApiLayerProperties(layerCount, &layerCount, layers.data());

    bool found = false;
    std::cout << "Found " << layerCount << " OpenXR API Layers:" << std::endl;
    for (const auto& layer : layers) {
        std::cout << " - " << layer.layerName << " (v" << layer.layerVersion << "): " << layer.description << std::endl;
        if (strcmp(layer.layerName, "XR_APILAYER_NOVENDOR_monoeye") == 0) {
            found = true;
        }
    }

    if (!found) {
        std::cout << "\n[!] ERROR: MonoEye layer NOT FOUND in system enumeration." << std::endl;
        std::cout << "    Check registry keys and manifest path." << std::endl;
        return 1;
    }

    std::cout << "\n[OK] MonoEye layer is visible to OpenXR loader." << std::endl;

    // 2. Try to create instance with the layer
    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy(createInfo.applicationInfo.applicationName, "MonoEye Validator");
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    const char* enabledLayers[] = {"XR_APILAYER_NOVENDOR_monoeye"};
    createInfo.enabledApiLayerCount = 1;
    createInfo.enabledApiLayerNames = enabledLayers;

    std::cout << "Attempting to create XrInstance with MonoEye enabled..." << std::endl;
    XrInstance instance = XR_NULL_HANDLE;
    XrResult result = xrCreateInstance(&createInfo, &instance);

    if (XR_SUCCEEDED(result)) {
        std::cout << "[SUCCESS] XrInstance created successfully with MonoEye!" << std::endl;
        xrDestroyInstance(instance);
    } else {
        std::cout << "[FAILURE] xrCreateInstance failed with error code: " << result << std::endl;
        std::cout << "    This usually means the DLL failed to load or crashed during negotiation." << std::endl;
    }

    return (XR_SUCCEEDED(result)) ? 0 : 1;
}
