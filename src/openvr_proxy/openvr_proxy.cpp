#include "openvr_proxy.h"
#include <windows.h>
#include <iostream>
#include "../logging.h"

// Pointer to the real OpenVR DLL
static HMODULE s_realOpenVR = nullptr;

typedef void* (VR_CALLTYPE *PFN_VR_InitInternal)(vr::EVRInitError *peError, vr::EVRApplicationType eApplicationType);
typedef void (VR_CALLTYPE *PFN_VR_ShutdownInternal)();
typedef bool (VR_CALLTYPE *PFN_VR_IsHmdPresent)();
typedef const char* (VR_CALLTYPE *PFN_VR_GetStringForHmdError)(vr::EVRInitError error);
typedef void* (VR_CALLTYPE *PFN_VR_GetGenericInterface)(const char *pchInterfaceVersion, vr::EVRInitError *peError);

static PFN_VR_InitInternal g_real_VR_InitInternal = nullptr;
static PFN_VR_ShutdownInternal g_real_VR_ShutdownInternal = nullptr;
static PFN_VR_IsHmdPresent g_real_VR_IsHmdPresent = nullptr;
static PFN_VR_GetStringForHmdError g_real_VR_GetStringForHmdError = nullptr;
static PFN_VR_GetGenericInterface g_real_VR_GetGenericInterface = nullptr;

bool LoadRealOpenVR() {
    if (s_realOpenVR) return true;

    // Load the real openvr_api.dll from a different name or path
    s_realOpenVR = LoadLibraryA("openvr_api_real.dll");
    if (!s_realOpenVR) {
        MONOEYE_LOG_ERROR("Failed to load real openvr_api_real.dll");
        return false;
    }

    g_real_VR_InitInternal = (PFN_VR_InitInternal)GetProcAddress(s_realOpenVR, "VR_InitInternal");
    g_real_VR_ShutdownInternal = (PFN_VR_ShutdownInternal)GetProcAddress(s_realOpenVR, "VR_ShutdownInternal");
    g_real_VR_IsHmdPresent = (PFN_VR_IsHmdPresent)GetProcAddress(s_realOpenVR, "VR_IsHmdPresent");
    g_real_VR_GetStringForHmdError = (PFN_VR_GetStringForHmdError)GetProcAddress(s_realOpenVR, "VR_GetStringForHmdError");
    g_real_VR_GetGenericInterface = (PFN_VR_GetGenericInterface)GetProcAddress(s_realOpenVR, "VR_GetGenericInterface");

    return true;
}

extern "C" __declspec(dllexport) void* VR_CALLTYPE VR_InitInternal(vr::EVRInitError *peError, vr::EVRApplicationType eApplicationType) {
    MONOEYE_LOG("OpenVR Proxy: VR_InitInternal called");
    if (!LoadRealOpenVR()) {
        if (peError) *peError = vr::VRInitError_Init_FileNotFound;
        return nullptr;
    }
    return g_real_VR_InitInternal(peError, eApplicationType);
}

extern "C" __declspec(dllexport) void VR_CALLTYPE VR_ShutdownInternal() {
    MONOEYE_LOG("OpenVR Proxy: VR_ShutdownInternal called");
    if (g_real_VR_ShutdownInternal) g_real_VR_ShutdownInternal();
}

extern "C" __declspec(dllexport) bool VR_CALLTYPE VR_IsHmdPresent() {
    if (!LoadRealOpenVR()) return false;
    return g_real_VR_IsHmdPresent();
}

extern "C" __declspec(dllexport) const char* VR_CALLTYPE VR_GetStringForHmdError(vr::EVRInitError error) {
    if (!LoadRealOpenVR()) return "MonoEye: Real OpenVR not found";
    return g_real_VR_GetStringForHmdError(error);
}

extern "C" __declspec(dllexport) void* VR_CALLTYPE VR_GetGenericInterface(const char *pchInterfaceVersion, vr::EVRInitError *peError) {
    if (!LoadRealOpenVR()) return nullptr;
    
    MONOEYE_LOG("OpenVR Proxy: Requesting interface %s", pchInterfaceVersion);
    
    void* iface = g_real_VR_GetGenericInterface(pchInterfaceVersion, peError);
    
    // This is where we will wrap IVRCompositor and IVRSystem
    
    return iface;
}
