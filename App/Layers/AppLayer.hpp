#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <memory>

#include <Shader.hpp>
#include <LightingShader.hpp>
#include <Model.hpp>
#include <Core/Layer.hpp>
#include <Camera.hpp>
#include <Input/UserInput.hpp>
#include <Scene/SceneBuilder.hpp>

#include "../Game/PlayerController.hpp"
#include <Brixelizer/BrixelizerContext.hpp>
#include <Brixelizer/BrixelizerGIContext.hpp>
#include <Brixelizer/GBuffer.hpp>

class AppLayer : public Core::Layer {
public:
	AppLayer();
	~AppLayer();

	void OnEvent(Core::Event& event);
	void OnUpdate(float deltaTime);
	void OnRender();

private:
	void CreateGBufferSRVs(ID3D12Device* device);
	void UpdateDiffuseGISRV(ID3D12Device* device);

private:
	// Shaders.
	std::unique_ptr<Shader>         m_GBufferShader;  // GBuffer.hlsl, write normals.
	std::unique_ptr<LightingShader> m_LightingShader; // Lighting.hlsl, shading + GI.
	
	// Scene.
	Camera           m_Camera;
	SceneBuilder     m_SceneBuilder;
	Input::UserInput m_Input;
	PlayerController m_PlayerController;
	Model            m_Model;

	// Brixerlizer.
	std::unique_ptr<Brixelizer::BrixelizerContext>   m_BrixelizerContext;
	std::unique_ptr<Brixelizer::BrixelizerGIContext> m_BrixelizerGIContext;
	std::vector<Brixelizer::MeshInstance>            m_MeshInstances;

	// G-Buffer.
	std::unique_ptr<Brixelizer::GBuffer> m_GBuffer;

	// Textures use slots 0...N-1.
	// DiffuseGI uses slot N (assigned after loading the model).
	UINT m_DiffuseGISrvSlot = 0;

	// Previous frame matrices (for reprojection).
	// This allows stabilize noise and accumulate GI.
	DirectX::XMFLOAT4X4 m_PrevView{};
	DirectX::XMFLOAT4X4 m_PrevProjection{};
	bool m_FirstFrame = true;
};