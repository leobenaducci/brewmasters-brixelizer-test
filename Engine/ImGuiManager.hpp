#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <wrl/client.h>

class ImGuiManager {
public:
	ImGuiManager(HWND hwnd, ID3D12Device* device, int frameCount) {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 1;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_SrvHeap));

		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX12_Init(
			device, 
			frameCount, 
			DXGI_FORMAT_R8G8B8A8_UNORM, m_SrvHeap.Get(),
			m_SrvHeap->GetCPUDescriptorHandleForHeapStart(), 
			m_SrvHeap->GetGPUDescriptorHandleForHeapStart()
		);
	}

	~ImGuiManager() {
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void NewFrame() {
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void Render(ID3D12GraphicsCommandList* cmdList) {
		ImGui::Render();
		ID3D12DescriptorHeap* heaps[] = { m_SrvHeap.Get() };
		cmdList->SetDescriptorHeaps(1, heaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
	}

private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_SrvHeap;
};