#include <openxr/openxr.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vulkan/vulkan.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

void set_env_var(const char* name, const char* value) {
#ifdef _WIN32
    SetEnvironmentVariableA(name, value);
#else
    setenv(name, value, 1);
#endif
}

std::string get_env_var(const char* name) {
#ifdef _WIN32
    char buf[1024];
    DWORD ret = GetEnvironmentVariableA(name, buf, sizeof(buf));
    if (ret > 0 && ret < sizeof(buf)) {
        return std::string(buf, ret);
    }
#else
    char* val = getenv(name);
    if (val) return val;
#endif
    return "";
}

int main() {
    std::ofstream report("monoeye_validation_report.txt");
    auto log = [&](const std::string& msg, bool to_console = true) {
        if (to_console) std::cout << msg << std::endl;
        report << msg << "\n";
    };

    log("========================================");
    log("   MONOEYE ADVANCED LAYER VALIDATOR");
    log("========================================");

    // 1. Force OpenXR loader debug logs
    log("[*] Enabling OpenXR loader debug logging (XR_LOADER_DEBUG=all)...");
    set_env_var("XR_LOADER_DEBUG", "all");

    // 2. Dump Environment Variables
    log("\n[1] Environment Variables:");
    log(" - MONOEYE_ENABLE: " + get_env_var("MONOEYE_ENABLE"));
    log(" - MONOEYE_DISABLE: " + get_env_var("MONOEYE_DISABLE"));
    log(" - XR_API_LAYER_PATH: " + get_env_var("XR_API_LAYER_PATH"));
    log(" - XR_RUNTIME_JSON: " + get_env_var("XR_RUNTIME_JSON"));

    // 3. Test Vulkan availability
    log("\n[2] Testing Vulkan Driver Interface...");
    VkInstanceCreateInfo vkCreateInfo{};
    vkCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkResult vkRes = vkCreateInstance(&vkCreateInfo, nullptr, &vkInstance);
    if (vkRes == VK_SUCCESS) {
        log(" [+] Vulkan instance created successfully!");
        vkDestroyInstance(vkInstance, nullptr);
    } else {
        log(" [!] Vulkan instance creation failed with code: " + std::to_string(vkRes));
        log("     Verify GPU drivers and Vulkan configuration.");
    }

    // 4. Enumerate API Layers
    log("\n[3] Enumerating OpenXR API Layers...");
    uint32_t layerCount = 0;
    xrEnumerateApiLayerProperties(0, &layerCount, nullptr);
    std::vector<XrApiLayerProperties> layers(layerCount, {XR_TYPE_API_LAYER_PROPERTIES});
    xrEnumerateApiLayerProperties(layerCount, &layerCount, layers.data());

    bool found = false;
    log(" Found " + std::to_string(layerCount) + " layers:");
    for (const auto& layer : layers) {
        log("  - " + std::string(layer.layerName) + " (v" + std::to_string(layer.layerVersion) + "): " + layer.description);
        if (strcmp(layer.layerName, "XR_APILAYER_NOVENDOR_monoeye") == 0) {
            found = true;
        }
    }

    if (!found) {
        log("\n[!] ERROR: MonoEye layer NOT FOUND in system enumeration.");
        log("    Make sure the manifest path is registered or XR_API_LAYER_PATH is set correctly.");
        report.close();
        return 1;
    }
    log(" [+] MonoEye layer is visible to OpenXR loader.");

    // 5. Try to create instance WITH MonoEye
    log("\n[4] Attempting XrInstance creation WITH MonoEye enabled...");
    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy(createInfo.applicationInfo.applicationName, "MonoEye Validator");
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    const char* enabledLayers[] = {"XR_APILAYER_NOVENDOR_monoeye"};
    createInfo.enabledApiLayerCount = 1;
    createInfo.enabledApiLayerNames = enabledLayers;

    XrInstance instance = XR_NULL_HANDLE;
    XrResult result = xrCreateInstance(&createInfo, &instance);

    if (result == XR_SUCCESS) {
        log(" [+] SUCCESS: XrInstance created successfully WITH MonoEye!");
        xrDestroyInstance(instance);
    } else {
        log(" [!] FAILURE: xrCreateInstance (WITH MonoEye) failed with: " + std::to_string(result));
        if (result == XR_ERROR_RUNTIME_UNAVAILABLE) {
            log("     -> Error code -51 (XR_ERROR_RUNTIME_UNAVAILABLE)");
            log("        The layer was loaded successfully, but no active VR runtime (SteamVR/Monado) is running.");
        } else if (result == XR_ERROR_INITIALIZATION_FAILED) {
            log("     -> Error code -2 (XR_ERROR_INITIALIZATION_FAILED)");
            log("        The layer DLL may have crashed or failed to negotiate loader interfaces.");
        }
    }

    // 6. Try to create instance WITHOUT MonoEye (Baseline test)
    log("\n[5] Attempting XrInstance creation WITHOUT MonoEye (Baseline test)...");
    createInfo.enabledApiLayerCount = 0;
    createInfo.enabledApiLayerNames = nullptr;
    XrInstance baselineInstance = XR_NULL_HANDLE;
    XrResult baselineResult = xrCreateInstance(&createInfo, &baselineInstance);

    if (baselineResult == XR_SUCCESS) {
        log(" [+] SUCCESS: Baseline XrInstance created successfully (Runtime is functional)!");
        xrDestroyInstance(baselineInstance);
    } else {
        log(" [!] FAILURE: Baseline xrCreateInstance failed with: " + std::to_string(baselineResult));
    }

    log("\n========================================");
    log(" Validation report written to 'monoeye_validation_report.txt'");
    log("========================================");

    report.close();
    return (result == XR_SUCCESS || result == XR_ERROR_RUNTIME_UNAVAILABLE) ? 0 : 1;
}
