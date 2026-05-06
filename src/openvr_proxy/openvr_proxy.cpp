#include "openvr_proxy.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <iostream>
#include "../logging.h"

// Pointer to the real OpenVR DLL
static HMODULE s_realOpenVR = nullptr;

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
    if (s_realOpenVR) return true;

    // Load the real openvr_api_real.dll from a different name or path
    s_realOpenVR = LoadLibraryA("openvr_api_real.dll");
    if (!s_realOpenVR) {
        MONOEYE_LOG_ERROR("Failed to load real openvr_api_real.dll");
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

extern "C" __declspec(dllexport) void* VR_CALLTYPE VR_InitInternal(vr::EVRInitError *peError, vr::EVRApplicationType eApplicationType) {
    MONOEYE_LOG("OpenVR Proxy: VR_InitInternal called");
    if (!LoadRealOpenVR()) {
        if (peError) *peError = vr::VRInitError_Init_FileNotFound;
        return nullptr;
    }
    return g_real_VR_InitInternal(peError, eApplicationType);
}

extern "C" __declspec(dllexport) uint32_t VR_CALLTYPE VR_InitInternal2(vr::EVRInitError *peError, vr::EVRApplicationType eApplicationType, const char *pStartupInfo) {
    MONOEYE_LOG("OpenVR Proxy: VR_InitInternal2 called");
    if (!LoadRealOpenVR()) {
        if (peError) *peError = vr::VRInitError_Init_FileNotFound;
        return 0;
    }
    if (!g_real_VR_InitInternal2) return 0;
    return g_real_VR_InitInternal2(peError, eApplicationType, pStartupInfo);
}

extern "C" __declspec(dllexport) void VR_CALLTYPE VR_ShutdownInternal() {
    MONOEYE_LOG("OpenVR Proxy: VR_ShutdownInternal called");
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
    
    MONOEYE_LOG("OpenVR Proxy: Requesting interface %s", pchInterfaceVersion);
    
    void* iface = g_real_VR_GetGenericInterface(pchInterfaceVersion, peError);
    
    // This is where we will wrap IVRCompositor and IVRSystem
    
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

