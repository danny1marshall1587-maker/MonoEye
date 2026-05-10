#include "openvr_proxy.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <iostream>
#include <mutex>
#include "../logging.h"

// Pointer to the real OpenVR DLL
static HMODULE s_realOpenVR = nullptr;
static std::mutex s_load_mutex;

typedef void* (VR_CALLTYPE *PFN_VR_InitInternal)(vr::EVRInitError *peError, vr::EVRApplicationType eApplicationType);
typedef uint32_t (VR_CALLTYPE *PFN_VR_InitInternal2)(vr::EVRInitError *peError, vr::EVRApplicationType eApplicationType, const char *pStartupInfo);
typedef void (VR_CALLTYPE *PFN_VR_ShutdownInternal)();
typedef bool (VR_CALLTYPE *PFN_VR_IsHmdPresent)();
typedef bool (VR_CALLTYPE *PFN_VR_IsRuntimeInstalled)();
typedef const char* (VR_CALLTYPE *PFN_VR_GetStringForHmdError)(vr::EVRInitError error);
typedef const char* (VR_CALLTYPE *PFN_VR_GetVRInitErrorAsSymbol)(vr::EVRInitError error);
typedef const char* (VR_CALLTYPE *PFN_VR_GetVRInitErrorAsEnglishDescription)(vr::EVRInitError error);
typedef void* (VR_CALLTYPE *PFN_VR_GetGenericInterface)(const char *pchInterfaceVersion, vr::EVRInitError *peError);
typedef bool (VR_CALLTYPE *PFN_VR_IsInterfaceVersionValid)(const char *pchInterfaceVersion);
typedef uint32_t (VR_CALLTYPE *PFN_VR_GetInitToken)();

static PFN_VR_InitInternal g_real_VR_InitInternal = nullptr;
static PFN_VR_InitInternal2 g_real_VR_InitInternal2 = nullptr;
static PFN_VR_ShutdownInternal g_real_VR_ShutdownInternal = nullptr;
static PFN_VR_IsHmdPresent g_real_VR_IsHmdPresent = nullptr;
static PFN_VR_IsRuntimeInstalled g_real_VR_IsRuntimeInstalled = nullptr;
static PFN_VR_GetStringForHmdError g_real_VR_GetStringForHmdError = nullptr;
static PFN_VR_GetVRInitErrorAsSymbol g_real_VR_GetVRInitErrorAsSymbol = nullptr;
static PFN_VR_GetVRInitErrorAsEnglishDescription g_real_VR_GetVRInitErrorAsEnglishDescription = nullptr;
static PFN_VR_GetGenericInterface g_real_VR_GetGenericInterface = nullptr;
static PFN_VR_IsInterfaceVersionValid g_real_VR_IsInterfaceVersionValid = nullptr;
static PFN_VR_GetInitToken g_real_VR_GetInitToken = nullptr;

bool LoadRealOpenVR() {
    std::lock_guard<std::mutex> lock(s_load_mutex);
    if (s_realOpenVR) return true;

    // Use the suggested name for the original DLL
    s_realOpenVR = LoadLibraryA("openvr_api_orig.dll");
    if (!s_realOpenVR) {
        // Fallback to the old name just in case
        s_realOpenVR = LoadLibraryA("openvr_api_real.dll");
    }

    if (!s_realOpenVR) {
        // We cannot log here if the logging system isn't ready or depends on this DLL
        return false;
    }

    g_real_VR_InitInternal = (PFN_VR_InitInternal)GetProcAddress(s_realOpenVR, "VR_InitInternal");
    g_real_VR_InitInternal2 = (PFN_VR_InitInternal2)GetProcAddress(s_realOpenVR, "VR_InitInternal2");
    g_real_VR_ShutdownInternal = (PFN_VR_ShutdownInternal)GetProcAddress(s_realOpenVR, "VR_ShutdownInternal");
    g_real_VR_IsHmdPresent = (PFN_VR_IsHmdPresent)GetProcAddress(s_realOpenVR, "VR_IsHmdPresent");
    g_real_VR_IsRuntimeInstalled = (PFN_VR_IsRuntimeInstalled)GetProcAddress(s_realOpenVR, "VR_IsRuntimeInstalled");
    g_real_VR_GetStringForHmdError = (PFN_VR_GetStringForHmdError)GetProcAddress(s_realOpenVR, "VR_GetStringForHmdError");
    g_real_VR_GetVRInitErrorAsSymbol = (PFN_VR_GetVRInitErrorAsSymbol)GetProcAddress(s_realOpenVR, "VR_GetVRInitErrorAsSymbol");
    g_real_VR_GetVRInitErrorAsEnglishDescription = (PFN_VR_GetVRInitErrorAsEnglishDescription)GetProcAddress(s_realOpenVR, "VR_GetVRInitErrorAsEnglishDescription");
    g_real_VR_GetGenericInterface = (PFN_VR_GetGenericInterface)GetProcAddress(s_realOpenVR, "VR_GetGenericInterface");
    g_real_VR_IsInterfaceVersionValid = (PFN_VR_IsInterfaceVersionValid)GetProcAddress(s_realOpenVR, "VR_IsInterfaceVersionValid");
    g_real_VR_GetInitToken = (PFN_VR_GetInitToken)GetProcAddress(s_realOpenVR, "VR_GetInitToken");

    return true;
}

namespace monoeye {

// Static instance of our proxy compositor
static ProxyCompositor* s_proxyCompositor = nullptr;
static ProxyCompositor_026* s_proxyCompositor_026 = nullptr;
static std::mutex s_proxy_mutex;

vr::EVRCompositorError ProxyCompositor::Submit(vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t* pBounds, vr::EVRSubmitFlags nSubmitFlags) {
    // Lazy initialization of the warp pipeline if needed
    static bool s_warp_initialized = false;
    if (!s_warp_initialized && pTexture) {
        MONOEYE_LOG("OpenVR Proxy: First Submit called, initializing warp pipeline...");
        
        // In a real implementation, we would detect the graphics API from pTexture->eType
        // and initialize the WarpPipeline accordingly.
        // For now, we just mark as "initialized" to demonstrate the lazy pattern.
        s_warp_initialized = true;
    }

    if (eEye == vr::Eye_Left) {
        // The game is submitting the primary eye.
        // Proxy should capture this texture, trigger shared Vulkan compute backend 
        // to synthesize the right eye from the left eye's data.
        MONOEYE_LOG("OpenVR Proxy: Intercepted Left Eye. Triggering synthetic right eye generation...");
    } else if (eEye == vr::Eye_Right) {
        // If the game attempts to submit a right eye natively (or if MonoEye is in passthrough),
        // we catch it here. We would submit the synthesized texture to the real compositor.
        MONOEYE_LOG("OpenVR Proxy: Intercepted Right Eye. Suppressing or replacing...");
    }

    return m_real->Submit(eEye, pTexture, pBounds, nSubmitFlags);
}

vr::EVRCompositorError ProxyCompositor_026::Submit(vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t* pBounds, vr::EVRSubmitFlags nSubmitFlags) {
    static bool s_warp_initialized = false;
    if (!s_warp_initialized && pTexture) {
        MONOEYE_LOG("OpenVR Proxy 026: First Submit called, initializing warp pipeline...");
        s_warp_initialized = true;
    }
    if (eEye == vr::Eye_Left) {
        MONOEYE_LOG("OpenVR Proxy 026: Intercepted Left Eye. Triggering synthetic right eye generation...");
    } else if (eEye == vr::Eye_Right) {
        MONOEYE_LOG("OpenVR Proxy 026: Intercepted Right Eye. Suppressing or replacing...");
    }

    return m_real->Submit(eEye, pTexture, pBounds, nSubmitFlags);
}

} // namespace monoeye

extern "C" __declspec(dllexport) void* VR_CALLTYPE VR_InitInternal(vr::EVRInitError *peError, vr::EVRApplicationType eApplicationType) {
    if (!LoadRealOpenVR()) {
        if (peError) *peError = vr::VRInitError_Init_FileNotFound;
        return nullptr;
    }
    return g_real_VR_InitInternal(peError, eApplicationType);
}

extern "C" __declspec(dllexport) uint32_t VR_CALLTYPE VR_InitInternal2(vr::EVRInitError *peError, vr::EVRApplicationType eApplicationType, const char *pStartupInfo) {
    if (!LoadRealOpenVR()) {
        if (peError) *peError = vr::VRInitError_Init_FileNotFound;
        return 0;
    }
    if (!g_real_VR_InitInternal2) return 0;
    return g_real_VR_InitInternal2(peError, eApplicationType, pStartupInfo);
}

extern "C" __declspec(dllexport) void VR_CALLTYPE VR_ShutdownInternal() {
    if (g_real_VR_ShutdownInternal) g_real_VR_ShutdownInternal();
}

extern "C" __declspec(dllexport) bool VR_CALLTYPE VR_IsHmdPresent() {
    if (!LoadRealOpenVR()) return false;
    if (!g_real_VR_IsHmdPresent) return false;
    return g_real_VR_IsHmdPresent();
}

extern "C" __declspec(dllexport) bool VR_CALLTYPE VR_IsRuntimeInstalled() {
    if (!LoadRealOpenVR()) return false;
    if (!g_real_VR_IsRuntimeInstalled) return false;
    return g_real_VR_IsRuntimeInstalled();
}

extern "C" __declspec(dllexport) const char* VR_CALLTYPE VR_GetStringForHmdError(vr::EVRInitError error) {
    if (!LoadRealOpenVR()) return "MonoEye: Real OpenVR not found";
    if (!g_real_VR_GetStringForHmdError) return "Unknown Error";
    return g_real_VR_GetStringForHmdError(error);
}

extern "C" __declspec(dllexport) const char* VR_CALLTYPE VR_GetVRInitErrorAsSymbol(vr::EVRInitError error) {
    if (!LoadRealOpenVR()) return "VRInitError_Unknown";
    if (!g_real_VR_GetVRInitErrorAsSymbol) return "VRInitError_Unknown";
    return g_real_VR_GetVRInitErrorAsSymbol(error);
}

extern "C" __declspec(dllexport) const char* VR_CALLTYPE VR_GetVRInitErrorAsEnglishDescription(vr::EVRInitError error) {
    if (!LoadRealOpenVR()) return "Real OpenVR not found";
    if (!g_real_VR_GetVRInitErrorAsEnglishDescription) return "Unknown Error";
    return g_real_VR_GetVRInitErrorAsEnglishDescription(error);
}

extern "C" __declspec(dllexport) void* VR_CALLTYPE VR_GetGenericInterface(const char *pchInterfaceVersion, vr::EVRInitError *peError) {
    if (!LoadRealOpenVR()) return nullptr;
    if (!g_real_VR_GetGenericInterface) return nullptr;
    
    void* iface = g_real_VR_GetGenericInterface(pchInterfaceVersion, peError);
    if (!iface) return nullptr;

    // Wrap the compositor if requested
    if (strcmp(pchInterfaceVersion, "IVRCompositor_026") == 0) {
        std::lock_guard<std::mutex> lock(monoeye::s_proxy_mutex);
        if (!monoeye::s_proxyCompositor_026) {
            MONOEYE_LOG("OpenVR Proxy: Creating strict 026 compositor wrapper for %s", pchInterfaceVersion);
            monoeye::s_proxyCompositor_026 = new monoeye::ProxyCompositor_026((vr::IVRCompositor*)iface);
        }
        return monoeye::s_proxyCompositor_026;
    }
    else if (strstr(pchInterfaceVersion, "IVRCompositor_")) {
        std::lock_guard<std::mutex> lock(monoeye::s_proxy_mutex);
        if (!monoeye::s_proxyCompositor) {
            MONOEYE_LOG("OpenVR Proxy: Creating compositor wrapper for %s", pchInterfaceVersion);
            monoeye::s_proxyCompositor = new monoeye::ProxyCompositor((vr::IVRCompositor*)iface);
        }
        return monoeye::s_proxyCompositor;
    }
    
    return iface;
}

extern "C" __declspec(dllexport) bool VR_CALLTYPE VR_IsInterfaceVersionValid(const char *pchInterfaceVersion) {
    if (!LoadRealOpenVR()) return false;
    if (!g_real_VR_IsInterfaceVersionValid) return false;
    return g_real_VR_IsInterfaceVersionValid(pchInterfaceVersion);
}

extern "C" __declspec(dllexport) uint32_t VR_CALLTYPE VR_GetInitToken() {
    if (!LoadRealOpenVR()) return 0;
    if (!g_real_VR_GetInitToken) return 0;
    return g_real_VR_GetInitToken();
}
