#pragma once

#include <openvr.h>
#include <mutex>

namespace monoeye {

class ProxyCompositor : public vr::IVRCompositor {
public:
    ProxyCompositor(vr::IVRCompositor* real) : m_real(real) {}

    // IVRCompositor_026 methods (and up to 029)
    virtual void SetTrackingSpace(vr::ETrackingUniverseOrigin eOrigin) override { m_real->SetTrackingSpace(eOrigin); }
    virtual vr::ETrackingUniverseOrigin GetTrackingSpace() override { return m_real->GetTrackingSpace(); }
    virtual vr::EVRCompositorError WaitGetPoses(vr::TrackedDevicePose_t* pRenderPoseArray, uint32_t unRenderPoseArrayCount,
        vr::TrackedDevicePose_t* pGamePoseArray, uint32_t unGamePoseArrayCount) override {
        return m_real->WaitGetPoses(pRenderPoseArray, unRenderPoseArrayCount, pGamePoseArray, unGamePoseArrayCount);
    }
    virtual vr::EVRCompositorError GetLastPoses(vr::TrackedDevicePose_t* pRenderPoseArray, uint32_t unRenderPoseArrayCount,
        vr::TrackedDevicePose_t* pGamePoseArray, uint32_t unGamePoseArrayCount) override {
        return m_real->GetLastPoses(pRenderPoseArray, unRenderPoseArrayCount, pGamePoseArray, unGamePoseArrayCount);
    }
    virtual vr::EVRCompositorError GetLastPoseForTrackedDeviceIndex(vr::TrackedDeviceIndex_t unDeviceIndex, vr::TrackedDevicePose_t *pOutputPose, vr::TrackedDevicePose_t *pOutputGamePose) override {
        return m_real->GetLastPoseForTrackedDeviceIndex(unDeviceIndex, pOutputPose, pOutputGamePose);
    }
    virtual vr::EVRCompositorError GetSubmitTexture(vr::Texture_t *pOutTexture, bool *pNeedsFlush, vr::EVRCompositorTextureUsage eUsage,
        const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds = 0, vr::EVRSubmitFlags nSubmitFlags = vr::Submit_Default) override {
        return m_real->GetSubmitTexture(pOutTexture, pNeedsFlush, eUsage, pTexture, pBounds, nSubmitFlags);
    }

    // THE HOOK
    virtual vr::EVRCompositorError Submit(vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t* pBounds = 0, vr::EVRSubmitFlags nSubmitFlags = vr::Submit_Default) override;

    virtual vr::EVRCompositorError SubmitWithArrayIndex(vr::EVREye eEye, const vr::Texture_t *pTexture, uint32_t unTextureArrayIndex,
        const vr::VRTextureBounds_t *pBounds = 0, vr::EVRSubmitFlags nSubmitFlags = vr::Submit_Default) override {
        return m_real->SubmitWithArrayIndex(eEye, pTexture, unTextureArrayIndex, pBounds, nSubmitFlags);
    }
    virtual void ClearLastSubmittedFrame() override { m_real->ClearLastSubmittedFrame(); }
    virtual void PostPresentHandoff() override { m_real->PostPresentHandoff(); }
    virtual bool GetFrameTiming(vr::Compositor_FrameTiming *pTiming, uint32_t unFramesAgo = 0) override { return m_real->GetFrameTiming(pTiming, unFramesAgo); }
    virtual uint32_t GetFrameTimings(vr::Compositor_FrameTiming *pTiming, uint32_t nFrames) override { return m_real->GetFrameTimings(pTiming, nFrames); }
    virtual float GetFrameTimeRemaining() override { return m_real->GetFrameTimeRemaining(); }
    virtual void GetCumulativeStats(vr::Compositor_CumulativeStats *pStats, uint32_t nStatsSizeInBytes) override { m_real->GetCumulativeStats(pStats, nStatsSizeInBytes); }
    virtual void FadeToColor(float fSeconds, float fRed, float fGreen, float fBlue, float fAlpha, bool bBackground = false) override { m_real->FadeToColor(fSeconds, fRed, fGreen, fBlue, fAlpha, bBackground); }
    virtual vr::HmdColor_t GetCurrentFadeColor(bool bBackground = false) override { return m_real->GetCurrentFadeColor(bBackground); }
    virtual void FadeGrid(float fSeconds, bool bFadeGridIn) override { m_real->FadeGrid(fSeconds, bFadeGridIn); }
    virtual float GetCurrentGridAlpha() override { return m_real->GetCurrentGridAlpha(); }
    virtual vr::EVRCompositorError SetSkyboxOverride(const vr::Texture_t *pTextures, uint32_t unTextureCount) override { return m_real->SetSkyboxOverride(pTextures, unTextureCount); }
    virtual void ClearSkyboxOverride() override { m_real->ClearSkyboxOverride(); }
    virtual void CompositorBringToFront() override { m_real->CompositorBringToFront(); }
    virtual void CompositorGoToBack() override { m_real->CompositorGoToBack(); }
    virtual void CompositorQuit() override { m_real->CompositorQuit(); }
    virtual bool IsFullscreen() override { return m_real->IsFullscreen(); }
    virtual uint32_t GetCurrentSceneFocusProcess() override { return m_real->GetCurrentSceneFocusProcess(); }
    virtual uint32_t GetLastFrameRenderer() override { return m_real->GetLastFrameRenderer(); }
    virtual bool CanRenderScene() override { return m_real->CanRenderScene(); }
    virtual void ShowMirrorWindow() override { m_real->ShowMirrorWindow(); }
    virtual void HideMirrorWindow() override { m_real->HideMirrorWindow(); }
    virtual bool IsMirrorWindowVisible() override { return m_real->IsMirrorWindowVisible(); }
    virtual void CompositorDumpImages() override { m_real->CompositorDumpImages(); }
    virtual bool ShouldAppRenderWithLowResources() override { return m_real->ShouldAppRenderWithLowResources(); }
    virtual void ForceInterleavedReprojectionOn(bool bOverride) override { m_real->ForceInterleavedReprojectionOn(bOverride); }
    virtual void ForceReconnectProcess() override { m_real->ForceReconnectProcess(); }
    virtual void SuspendRendering(bool bSuspend) override { m_real->SuspendRendering(bSuspend); }
    virtual vr::EVRCompositorError GetMirrorTextureD3D11(vr::EVREye eEye, void *pD3D11DeviceOrResource, void **ppD3D11ShaderResourceView) override { return m_real->GetMirrorTextureD3D11(eEye, pD3D11DeviceOrResource, ppD3D11ShaderResourceView); }
    virtual void ReleaseMirrorTextureD3D11(void *pD3D11ShaderResourceView) override { m_real->ReleaseMirrorTextureD3D11(pD3D11ShaderResourceView); }
    virtual vr::EVRCompositorError GetMirrorTextureGL(vr::EVREye eEye, vr::glUInt_t *pglTextureId, vr::glSharedTextureHandle_t *pglSharedTextureHandle) override { return m_real->GetMirrorTextureGL(eEye, pglTextureId, pglSharedTextureHandle); }
    virtual bool ReleaseSharedGLTexture(vr::glUInt_t glTextureId, vr::glSharedTextureHandle_t glSharedTextureHandle) override { return m_real->ReleaseSharedGLTexture(glTextureId, glSharedTextureHandle); }
    virtual void LockGLSharedTextureForAccess(vr::glSharedTextureHandle_t glSharedTextureHandle) override { m_real->LockGLSharedTextureForAccess(glSharedTextureHandle); }
    virtual void UnlockGLSharedTextureForAccess(vr::glSharedTextureHandle_t glSharedTextureHandle) override { m_real->UnlockGLSharedTextureForAccess(glSharedTextureHandle); }
    virtual uint32_t GetVulkanInstanceExtensionsRequired(char *pchValue, uint32_t unBufferSize) override { return m_real->GetVulkanInstanceExtensionsRequired(pchValue, unBufferSize); }
    virtual uint32_t GetVulkanDeviceExtensionsRequired(VkPhysicalDevice_T *pPhysicalDevice, char *pchValue, uint32_t unBufferSize) override { return m_real->GetVulkanDeviceExtensionsRequired(pPhysicalDevice, pchValue, unBufferSize); }
    virtual void SetExplicitTimingMode(vr::EVRCompositorTimingMode eTimingMode) override { m_real->SetExplicitTimingMode(eTimingMode); }
    virtual vr::EVRCompositorError SubmitExplicitTimingData() override { return m_real->SubmitExplicitTimingData(); }
    virtual bool IsMotionSmoothingEnabled() override { return m_real->IsMotionSmoothingEnabled(); }
    virtual bool IsMotionSmoothingSupported() override { return m_real->IsMotionSmoothingSupported(); }
    virtual bool IsCurrentSceneFocusAppLoading() override { return m_real->IsCurrentSceneFocusAppLoading(); }
    virtual vr::EVRCompositorError SetStageOverride_Async(const char *pchRenderModelPath, const vr::HmdMatrix34_t *pTransform = 0, const vr::Compositor_StageRenderSettings *pRenderSettings = 0, uint32_t nSizeOfRenderSettings = 0) override {
        return m_real->SetStageOverride_Async(pchRenderModelPath, pTransform, pRenderSettings, nSizeOfRenderSettings);
    }
    virtual void ClearStageOverride() override { m_real->ClearStageOverride(); }
    virtual bool GetCompositorBenchmarkResults(vr::Compositor_BenchmarkResults *pBenchmarkResults, uint32_t nSizeOfBenchmarkResults) override { return m_real->GetCompositorBenchmarkResults(pBenchmarkResults, nSizeOfBenchmarkResults); }
    virtual vr::EVRCompositorError GetLastPosePredictionIDs(uint32_t *pRenderPosePredictionID, uint32_t *pGamePosePredictionID) override { return m_real->GetLastPosePredictionIDs(pRenderPosePredictionID, pGamePosePredictionID); }
    virtual vr::EVRCompositorError GetPosesForFrame(uint32_t unPosePredictionID, vr::TrackedDevicePose_t* pPoseArray, uint32_t unPoseArrayCount) override { return m_real->GetPosesForFrame(unPosePredictionID, pPoseArray, unPoseArrayCount); }

private:
    vr::IVRCompositor* m_real;

    // Left-eye state for mono-to-stereo synthesis
    vr::Texture_t          m_lastLeftTexture   = {};
    vr::VRTextureBounds_t  m_lastLeftBounds    = {0.0f, 0.0f, 1.0f, 1.0f};
    vr::EVRSubmitFlags     m_leftFlags         = vr::Submit_Default;
    bool                   m_hasLeftTexture    = false;
};


// VTable definition for older games (like Le Mans Ultimate) requesting IVRCompositor_026
struct IVRCompositor_026_VTable {
    virtual void SetTrackingSpace(vr::ETrackingUniverseOrigin eOrigin) = 0;
    virtual vr::ETrackingUniverseOrigin GetTrackingSpace() = 0;
    virtual vr::EVRCompositorError WaitGetPoses(vr::TrackedDevicePose_t* pRenderPoseArray, uint32_t unRenderPoseArrayCount, vr::TrackedDevicePose_t* pGamePoseArray, uint32_t unGamePoseArrayCount) = 0;
    virtual vr::EVRCompositorError GetLastPoses(vr::TrackedDevicePose_t* pRenderPoseArray, uint32_t unRenderPoseArrayCount, vr::TrackedDevicePose_t* pGamePoseArray, uint32_t unGamePoseArrayCount) = 0;
    virtual vr::EVRCompositorError GetLastPoseForTrackedDeviceIndex(vr::TrackedDeviceIndex_t unDeviceIndex, vr::TrackedDevicePose_t *pOutputPose, vr::TrackedDevicePose_t *pOutputGamePose) = 0;
    virtual vr::EVRCompositorError Submit(vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t* pBounds = 0, vr::EVRSubmitFlags nSubmitFlags = vr::Submit_Default) = 0;
    virtual void ClearLastSubmittedFrame() = 0;
    virtual void PostPresentHandoff() = 0;
    virtual bool GetFrameTiming(vr::Compositor_FrameTiming *pTiming, uint32_t unFramesAgo = 0) = 0;
    virtual uint32_t GetFrameTimings(vr::Compositor_FrameTiming *pTiming, uint32_t nFrames) = 0;
    virtual float GetFrameTimeRemaining() = 0;
    virtual void GetCumulativeStats(vr::Compositor_CumulativeStats *pStats, uint32_t nStatsSizeInBytes) = 0;
    virtual void FadeToColor(float fSeconds, float fRed, float fGreen, float fBlue, float fAlpha, bool bBackground = false) = 0;
    virtual vr::HmdColor_t GetCurrentFadeColor(bool bBackground = false) = 0;
    virtual void FadeGrid(float fSeconds, bool bFadeIn) = 0;
    virtual float GetCurrentGridAlpha() = 0;
    virtual vr::EVRCompositorError SetSkyboxOverride(const vr::Texture_t *pTextures, uint32_t unTextureCount) = 0;
    virtual void ClearSkyboxOverride() = 0;
    virtual void CompositorBringToFront() = 0;
    virtual void CompositorGoToBack() = 0;
    virtual void CompositorQuit() = 0;
    virtual bool IsFullscreen() = 0;
    virtual uint32_t GetCurrentSceneFocusProcess() = 0;
    virtual uint32_t GetLastFrameRenderer() = 0;
    virtual bool CanRenderScene() = 0;
    virtual void ShowMirrorWindow() = 0;
    virtual void HideMirrorWindow() = 0;
    virtual bool IsMirrorWindowVisible() = 0;
    virtual void CompositorDumpImages() = 0;
    virtual bool ShouldAppRenderWithLowResources() = 0;
    virtual void ForceInterleavedReprojectionOn(bool bOverride) = 0;
    virtual void ForceReconnectProcess() = 0;
    virtual void SuspendRendering(bool bSuspend) = 0;
    virtual vr::EVRCompositorError GetMirrorTextureD3D11(vr::EVREye eEye, void *pD3D11DeviceOrResource, void **ppD3D11ShaderResourceView) = 0;
    virtual void ReleaseMirrorTextureD3D11(void *pD3D11ShaderResourceView) = 0;
    virtual vr::EVRCompositorError GetMirrorTextureGL(vr::EVREye eEye, vr::glUInt_t *pglTextureId, vr::glSharedTextureHandle_t *pglSharedTextureHandle) = 0;
    virtual bool ReleaseSharedGLTexture(vr::glUInt_t glTextureId, vr::glSharedTextureHandle_t glSharedTextureHandle) = 0;
    virtual void LockGLSharedTextureForAccess(vr::glSharedTextureHandle_t glSharedTextureHandle) = 0;
    virtual void UnlockGLSharedTextureForAccess(vr::glSharedTextureHandle_t glSharedTextureHandle) = 0;
    virtual uint32_t GetVulkanInstanceExtensionsRequired(char *pchValue, uint32_t unBufferSize) = 0;
    virtual uint32_t GetVulkanDeviceExtensionsRequired(VkPhysicalDevice_T *pPhysicalDevice, char *pchValue, uint32_t unBufferSize) = 0;
    virtual void SetExplicitTimingMode(vr::EVRCompositorTimingMode eTimingMode) = 0;
    virtual vr::EVRCompositorError SubmitExplicitTimingData() = 0;
    virtual bool IsMotionSmoothingEnabled() = 0;
    virtual bool IsMotionSmoothingSupported() = 0;
    virtual bool IsCurrentSceneFocusAppLoading() = 0;
    virtual vr::EVRCompositorError SetStageOverride_Async(const char *pchRenderModelPath, const vr::HmdMatrix34_t *pTransform = 0, const vr::Compositor_StageRenderSettings *pRenderSettings = 0, uint32_t nSizeOfRenderSettings = 0) = 0;
    virtual void ClearStageOverride() = 0;
    virtual bool GetCompositorBenchmarkResults(vr::Compositor_BenchmarkResults *pBenchmarkResults, uint32_t nSizeOfBenchmarkResults) = 0;
    virtual vr::EVRCompositorError GetLastPosePredictionIDs(uint32_t *pRenderPosePredictionID, uint32_t *pGamePosePredictionID) = 0;
    virtual vr::EVRCompositorError GetPosesForFrame(uint32_t unPosePredictionID, vr::TrackedDevicePose_t* pPoseArray, uint32_t unPoseArrayCount) = 0;
};

class ProxyCompositor_026 : public IVRCompositor_026_VTable {
public:
    ProxyCompositor_026(vr::IVRCompositor* real) : m_real(real) {}

    virtual void SetTrackingSpace(vr::ETrackingUniverseOrigin eOrigin) override { m_real->SetTrackingSpace(eOrigin); }
    virtual vr::ETrackingUniverseOrigin GetTrackingSpace() override { return m_real->GetTrackingSpace(); }
    virtual vr::EVRCompositorError WaitGetPoses(vr::TrackedDevicePose_t* pRenderPoseArray, uint32_t unRenderPoseArrayCount, vr::TrackedDevicePose_t* pGamePoseArray, uint32_t unGamePoseArrayCount) override {
        return m_real->WaitGetPoses(pRenderPoseArray, unRenderPoseArrayCount, pGamePoseArray, unGamePoseArrayCount);
    }
    virtual vr::EVRCompositorError GetLastPoses(vr::TrackedDevicePose_t* pRenderPoseArray, uint32_t unRenderPoseArrayCount, vr::TrackedDevicePose_t* pGamePoseArray, uint32_t unGamePoseArrayCount) override {
        return m_real->GetLastPoses(pRenderPoseArray, unRenderPoseArrayCount, pGamePoseArray, unGamePoseArrayCount);
    }
    virtual vr::EVRCompositorError GetLastPoseForTrackedDeviceIndex(vr::TrackedDeviceIndex_t unDeviceIndex, vr::TrackedDevicePose_t *pOutputPose, vr::TrackedDevicePose_t *pOutputGamePose) override {
        return m_real->GetLastPoseForTrackedDeviceIndex(unDeviceIndex, pOutputPose, pOutputGamePose);
    }
    virtual vr::EVRCompositorError Submit(vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t* pBounds = 0, vr::EVRSubmitFlags nSubmitFlags = vr::Submit_Default) override;
    
    virtual void ClearLastSubmittedFrame() override { m_real->ClearLastSubmittedFrame(); }
    virtual void PostPresentHandoff() override { m_real->PostPresentHandoff(); }
    virtual bool GetFrameTiming(vr::Compositor_FrameTiming *pTiming, uint32_t unFramesAgo = 0) override { return m_real->GetFrameTiming(pTiming, unFramesAgo); }
    virtual uint32_t GetFrameTimings(vr::Compositor_FrameTiming *pTiming, uint32_t nFrames) override { return m_real->GetFrameTimings(pTiming, nFrames); }
    virtual float GetFrameTimeRemaining() override { return m_real->GetFrameTimeRemaining(); }
    virtual void GetCumulativeStats(vr::Compositor_CumulativeStats *pStats, uint32_t nStatsSizeInBytes) override { m_real->GetCumulativeStats(pStats, nStatsSizeInBytes); }
    virtual void FadeToColor(float fSeconds, float fRed, float fGreen, float fBlue, float fAlpha, bool bBackground = false) override { m_real->FadeToColor(fSeconds, fRed, fGreen, fBlue, fAlpha, bBackground); }
    virtual vr::HmdColor_t GetCurrentFadeColor(bool bBackground = false) override { return m_real->GetCurrentFadeColor(bBackground); }
    virtual void FadeGrid(float fSeconds, bool bFadeIn) override { m_real->FadeGrid(fSeconds, bFadeIn); }
    virtual float GetCurrentGridAlpha() override { return m_real->GetCurrentGridAlpha(); }
    virtual vr::EVRCompositorError SetSkyboxOverride(const vr::Texture_t *pTextures, uint32_t unTextureCount) override { return m_real->SetSkyboxOverride(pTextures, unTextureCount); }
    virtual void ClearSkyboxOverride() override { m_real->ClearSkyboxOverride(); }
    virtual void CompositorBringToFront() override { m_real->CompositorBringToFront(); }
    virtual void CompositorGoToBack() override { m_real->CompositorGoToBack(); }
    virtual void CompositorQuit() override { m_real->CompositorQuit(); }
    virtual bool IsFullscreen() override { return m_real->IsFullscreen(); }
    virtual uint32_t GetCurrentSceneFocusProcess() override { return m_real->GetCurrentSceneFocusProcess(); }
    virtual uint32_t GetLastFrameRenderer() override { return m_real->GetLastFrameRenderer(); }
    virtual bool CanRenderScene() override { return m_real->CanRenderScene(); }
    virtual void ShowMirrorWindow() override { m_real->ShowMirrorWindow(); }
    virtual void HideMirrorWindow() override { m_real->HideMirrorWindow(); }
    virtual bool IsMirrorWindowVisible() override { return m_real->IsMirrorWindowVisible(); }
    virtual void CompositorDumpImages() override { m_real->CompositorDumpImages(); }
    virtual bool ShouldAppRenderWithLowResources() override { return m_real->ShouldAppRenderWithLowResources(); }
    virtual void ForceInterleavedReprojectionOn(bool bOverride) override { m_real->ForceInterleavedReprojectionOn(bOverride); }
    virtual void ForceReconnectProcess() override { m_real->ForceReconnectProcess(); }
    virtual void SuspendRendering(bool bSuspend) override { m_real->SuspendRendering(bSuspend); }
    virtual vr::EVRCompositorError GetMirrorTextureD3D11(vr::EVREye eEye, void *pD3D11DeviceOrResource, void **ppD3D11ShaderResourceView) override { return m_real->GetMirrorTextureD3D11(eEye, pD3D11DeviceOrResource, ppD3D11ShaderResourceView); }
    virtual void ReleaseMirrorTextureD3D11(void *pD3D11ShaderResourceView) override { m_real->ReleaseMirrorTextureD3D11(pD3D11ShaderResourceView); }
    virtual vr::EVRCompositorError GetMirrorTextureGL(vr::EVREye eEye, vr::glUInt_t *pglTextureId, vr::glSharedTextureHandle_t *pglSharedTextureHandle) override { return m_real->GetMirrorTextureGL(eEye, pglTextureId, pglSharedTextureHandle); }
    virtual bool ReleaseSharedGLTexture(vr::glUInt_t glTextureId, vr::glSharedTextureHandle_t glSharedTextureHandle) override { return m_real->ReleaseSharedGLTexture(glTextureId, glSharedTextureHandle); }
    virtual void LockGLSharedTextureForAccess(vr::glSharedTextureHandle_t glSharedTextureHandle) override { m_real->LockGLSharedTextureForAccess(glSharedTextureHandle); }
    virtual void UnlockGLSharedTextureForAccess(vr::glSharedTextureHandle_t glSharedTextureHandle) override { m_real->UnlockGLSharedTextureForAccess(glSharedTextureHandle); }
    virtual uint32_t GetVulkanInstanceExtensionsRequired(char *pchValue, uint32_t unBufferSize) override { return m_real->GetVulkanInstanceExtensionsRequired(pchValue, unBufferSize); }
    virtual uint32_t GetVulkanDeviceExtensionsRequired(VkPhysicalDevice_T *pPhysicalDevice, char *pchValue, uint32_t unBufferSize) override { return m_real->GetVulkanDeviceExtensionsRequired(pPhysicalDevice, pchValue, unBufferSize); }
    virtual void SetExplicitTimingMode(vr::EVRCompositorTimingMode eTimingMode) override { m_real->SetExplicitTimingMode(eTimingMode); }
    virtual vr::EVRCompositorError SubmitExplicitTimingData() override { return m_real->SubmitExplicitTimingData(); }
    virtual bool IsMotionSmoothingEnabled() override { return m_real->IsMotionSmoothingEnabled(); }
    virtual bool IsMotionSmoothingSupported() override { return m_real->IsMotionSmoothingSupported(); }
    virtual bool IsCurrentSceneFocusAppLoading() override { return m_real->IsCurrentSceneFocusAppLoading(); }
    virtual vr::EVRCompositorError SetStageOverride_Async(const char *pchRenderModelPath, const vr::HmdMatrix34_t *pTransform = 0, const vr::Compositor_StageRenderSettings *pRenderSettings = 0, uint32_t nSizeOfRenderSettings = 0) override {
        return m_real->SetStageOverride_Async(pchRenderModelPath, pTransform, pRenderSettings, nSizeOfRenderSettings);
    }
    virtual void ClearStageOverride() override { m_real->ClearStageOverride(); }
    virtual bool GetCompositorBenchmarkResults(vr::Compositor_BenchmarkResults *pBenchmarkResults, uint32_t nSizeOfBenchmarkResults) override { return m_real->GetCompositorBenchmarkResults(pBenchmarkResults, nSizeOfBenchmarkResults); }
    virtual vr::EVRCompositorError GetLastPosePredictionIDs(uint32_t *pRenderPosePredictionID, uint32_t *pGamePosePredictionID) override { return m_real->GetLastPosePredictionIDs(pRenderPosePredictionID, pGamePosePredictionID); }
    virtual vr::EVRCompositorError GetPosesForFrame(uint32_t unPosePredictionID, vr::TrackedDevicePose_t* pPoseArray, uint32_t unPoseArrayCount) override { return m_real->GetPosesForFrame(unPosePredictionID, pPoseArray, unPoseArrayCount); }

private:
    vr::IVRCompositor* m_real;

    // Left-eye state for mono-to-stereo synthesis
    vr::Texture_t          m_lastLeftTexture   = {};
    vr::VRTextureBounds_t  m_lastLeftBounds    = {0.0f, 0.0f, 1.0f, 1.0f};
    vr::EVRSubmitFlags     m_leftFlags         = vr::Submit_Default;
    bool                   m_hasLeftTexture    = false;
};


} // namespace monoeye
