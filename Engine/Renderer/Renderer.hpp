#pragma once
#include <DirectXMath.h>
#include <Core/DX12Manager.hpp>
#include <Shader.hpp>
#include <Model.hpp>

struct SceneConstants {
	DirectX::XMFLOAT4X4 model{};
	DirectX::XMFLOAT4X4 view{};
	DirectX::XMFLOAT4X4 proj{};
	DirectX::XMFLOAT3   lightPos{};  float _pad0{};
	DirectX::XMFLOAT3   cameraPos{}; float _pad1{};
};

class Renderer {
public:
	explicit Renderer(DX12Manager& dx)
		: m_DX(dx)
		, m_DescriptorSize(dx.GetDevice()->GetDescriptorHandleIncrementSize(
			  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
	{}

	void SetShader(Shader* shader) { 
		m_Shader = shader;
	}

	void BeginScene() {
		constexpr float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };

		auto* cmdList = m_DX.GetCommandList();
		auto  rtv = m_DX.GetCurrentRTV();
		auto  dsv = m_DX.GetDepthStencilView();

		cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

		const D3D12_VIEWPORT viewport = { 0, 0, WIDTH, HEIGHT, 0, 1 };
		const D3D12_RECT     scissor = { 0, 0, (LONG)WIDTH, (LONG)HEIGHT };
		cmdList->RSSetViewports(1, &viewport);
		cmdList->RSSetScissorRects(1, &scissor);

		ID3D12DescriptorHeap* heaps[] = { m_DX.GetSrvHeap() };
		cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
		m_Shader->Bind(cmdList);
	}

	void DrawModel(Model const& model, SceneConstants const& scene) {
        auto* const cmdList = m_DX.GetCommandList();
        
        cmdList->SetGraphicsRoot32BitConstants(0, sizeof(SceneConstants) / 4, &scene, 0);
        
		m_Shader->Bind(cmdList);

        model.Draw(cmdList, m_DX.GetSrvHeap(), m_DescriptorSize);
    }

private:
	static constexpr float WIDTH = 1280.0f, HEIGHT = 720.0f;
	DX12Manager& m_DX;
	Shader*      m_Shader{};
	UINT         m_DescriptorSize{};
};