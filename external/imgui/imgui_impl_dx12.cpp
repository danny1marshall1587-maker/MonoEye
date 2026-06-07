// dear imgui: Renderer Backend for DirectX12
#include "imgui.h"
#include "imgui_impl_dx12.h"
#include <d3d12.h>
#include <d3dcompiler.h>

struct ImGui_ImplDX12_Data {
    ID3D12Device* pd3dDevice;
    ID3D12RootSignature* pRootSignature;
    ID3D12PipelineState* pPipelineState;
    DXGI_FORMAT RTVFormat;
    ID3D12Resource* pFontTextureResource;
    D3D12_CPU_DESCRIPTOR_HANDLE hFontSrvCpuDescHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE hFontSrvGpuDescHandle;
    // ... Simplified for space ...
    ImGui_ImplDX12_Data() { memset((void*)this, 0, sizeof(*this)); }
};

static ImGui_ImplDX12_Data* ImGui_ImplDX12_GetBackendData() {
    return ImGui::GetCurrentContext() ? (ImGui_ImplDX12_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

bool ImGui_ImplDX12_Init(ID3D12Device* device, int num_frames_in_flight, DXGI_FORMAT rtv_format, ID3D12DescriptorHeap* cbv_srv_heap, D3D12_CPU_DESCRIPTOR_HANDLE font_srv_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE font_srv_gpu_desc_handle) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplDX12_Data* bd = IM_NEW(ImGui_ImplDX12_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_dx12";
    bd->pd3dDevice = device;
    bd->RTVFormat = rtv_format;
    bd->hFontSrvCpuDescHandle = font_srv_cpu_desc_handle;
    bd->hFontSrvGpuDescHandle = font_srv_gpu_desc_handle;
    bd->pd3dDevice->AddRef();
    return true;
}

void ImGui_ImplDX12_Shutdown() {
    ImGui_ImplDX12_Data* bd = ImGui_ImplDX12_GetBackendData();
    if (bd->pd3dDevice) bd->pd3dDevice->Release();
    IM_DELETE(bd);
}

void ImGui_ImplDX12_NewFrame() { /* Stub */ }
void ImGui_ImplDX12_RenderDrawData(ImDrawData* draw_data, ID3D12GraphicsCommandList* graphics_command_list) { /* Stub */ }
bool ImGui_ImplDX12_CreateDeviceObjects() { return true; }
void ImGui_ImplDX12_InvalidateDeviceObjects() { /* Stub */ }
