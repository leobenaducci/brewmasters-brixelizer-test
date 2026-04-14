#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <memory>

#include <Core/Application.hpp>
#include <Shader.hpp>
#include <LightingShader.hpp>
#include <Model.hpp>
#include <Camera.hpp>
#include <Input/UserInput.hpp>
#include <Scene/SceneBuilder.hpp>

#include "../Game/PlayerController.hpp"
#include <Brixelizer/BrixelizerContext.hpp>
#include <Brixelizer/BrixelizerGIContext.hpp>
#include <Brixelizer/GBuffer.hpp>

class AppLayer {
public:
    AppLayer();
    ~AppLayer();

    void OnUpdate(float deltaTime);
    void OnRender();
    void OnEvent(Core::Event& e);

private:
    // ── Shaders ──────────────────────────────────────────────────────────
    std::unique_ptr<Shader>         m_GBufferShader;  // GBuffer.hlsl  — escribe normals
    std::unique_ptr<LightingShader> m_LightingShader; // Lighting.hlsl — shading + GI

    // ── Scene ────────────────────────────────────────────────────────────
    Camera           m_Camera;
    SceneBuilder     m_SceneBuilder;
    Input::UserInput m_Input;
    PlayerController m_PlayerController;
    Model           m_Model;

    // ── Brixelizer ───────────────────────────────────────────────────────
    std::unique_ptr<Brixelizer::BrixelizerContext>   m_BrixelizerContext;
    std::unique_ptr<Brixelizer::BrixelizerGIContext> m_BrixelizerGIContext;
    std::vector<Brixelizer::MeshInstance>            m_MeshInstances;

    // ── G-Buffer ─────────────────────────────────────────────────────────
    std::unique_ptr<Brixelizer::GBuffer> m_GBuffer;

    // ── GI SRV slot en el heap ───────────────────────────────────────────
    // Las texturas del modelo ocupan slots 0..N-1.
    // DiffuseGI ocupa el slot N (se asigna tras cargar el modelo).
    UINT m_DiffuseGISrvSlot = 0;

    // ── Matrices del frame anterior (para reprojection) ──────────────────
    DirectX::XMFLOAT4X4 m_PrevView{};
    DirectX::XMFLOAT4X4 m_PrevProj{};
    bool m_FirstFrame = true;

    void CreateGBufferSRVs(ID3D12Device* device);
};