#pragma once
#include <Camera.hpp>

namespace AppConfig {
	constexpr float FOV_DEG     = 45.0f;
	constexpr float NEAR_PLANE  = 0.1f;
	constexpr float FAR_PLANE   = 5000.0f;
	constexpr float ASPECT      = 1280.0f / 720.0f;
	constexpr float MODEL_SCALE = 0.01f;
}

struct SceneConstants {
	DirectX::XMFLOAT4X4 model{};
	DirectX::XMFLOAT4X4 view{};
	DirectX::XMFLOAT4X4 proj{};
	DirectX::XMFLOAT3   lightPosition{};  
	float _padding0{};
	DirectX::XMFLOAT3   cameraPosition{}; 
	float _padding1{};
};

class SceneBuilder {
public:
	explicit SceneBuilder(ID3D12Device *const device) {
		UINT alignedSize = (sizeof(SceneConstants) + 255) & ~255;

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(alignedSize);

		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_SceneConstantBuffer)
		);

		m_SceneConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_SceneConstantBufferMapped));
	}

	~SceneBuilder() {
		m_SceneConstantBuffer->Unmap(0, nullptr);
	}

	D3D12_GPU_VIRTUAL_ADDRESS GetSceneConstantBufferGPUAddress() const noexcept {
		return m_SceneConstantBuffer->GetGPUVirtualAddress();
	}

	SceneConstants Build(Camera const& camera) const noexcept {
		using namespace DirectX;

		SceneConstants scene{};

		XMStoreFloat4x4(&scene.model, XMMatrixScaling(
			AppConfig::MODEL_SCALE, AppConfig::MODEL_SCALE, AppConfig::MODEL_SCALE
		));

		XMStoreFloat4x4(&scene.view, camera.GetViewMatrix());
		XMStoreFloat4x4(&scene.proj, XMMatrixPerspectiveFovLH(
			XMConvertToRadians(AppConfig::FOV_DEG),
			AppConfig::ASPECT,
			AppConfig::NEAR_PLANE,
			AppConfig::FAR_PLANE)
		);

		scene.lightPosition  = camera.GetPosition();
		scene.cameraPosition = camera.GetPosition();

		memcpy(m_SceneConstantBufferMapped, &scene, sizeof(SceneConstants));

		return scene;
	}
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_SceneConstantBuffer{};
	UINT8* m_SceneConstantBufferMapped { nullptr };
};