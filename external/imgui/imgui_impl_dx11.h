// dear imgui: Renderer Backend for DirectX11
#pragma once
#include "imgui.h"      // IMGUI_IMPL_API

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;

struct ImGui_ImplDX11_RenderState
{
    ID3D11Device*               Device;
    ID3D11DeviceContext*        DeviceContext;
    ID3D11Buffer*               VertexConstantBuffer;
};

IMGUI_IMPL_API bool     ImGui_ImplDX11_Init(ID3D11Device* device, ID3D11DeviceContext* device_context);
IMGUI_IMPL_API void     ImGui_ImplDX11_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplDX11_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplDX11_RenderDrawData(ImDrawData* draw_data);

// Use if you want to reset your rendering state to what it was before calling ImGui_ImplDX11_RenderDrawData()
IMGUI_IMPL_API bool     ImGui_ImplDX11_CreateDeviceObjects();
IMGUI_IMPL_API void     ImGui_ImplDX11_InvalidateDeviceObjects();
