#pragma once
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>
#include <iostream>
#include <Plugins/DirectX/d3dx12.h>
#include "DXException.hpp"

using Microsoft::WRL::ComPtr;

// ---------------------------------------------------------------------------
// Root signature del Lighting pass:
//
//  [0] b0 — SceneConstants  (56 x 32-bit constants, VS+PS)
//  [1] b1 — GIConstants     (4  x 32-bit constants, PS)
//  [2]     — Descriptor table: t0 (albedo, PS)
//  [3]     — Descriptor table: t1 (diffuseGI, PS)
//
// Sampler estático s0 — trilinear wrap
// ---------------------------------------------------------------------------
class LightingShader {
public:
    LightingShader(ID3D12Device* device, const std::wstring& path) {
        // ── Root parameters ──────────────────────────────────────────────
        CD3DX12_DESCRIPTOR_RANGE albedoRange, giRange;
        albedoRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0
        giRange    .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1

        CD3DX12_ROOT_PARAMETER params[4];
        params[0].InitAsConstants(56, 0, 0, D3D12_SHADER_VISIBILITY_ALL);  // b0 SceneConstants
        params[1].InitAsConstants( 4, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL); // b1 GIConstants
        params[2].InitAsDescriptorTable(1, &albedoRange, D3D12_SHADER_VISIBILITY_PIXEL); // t0
        params[3].InitAsDescriptorTable(1, &giRange,     D3D12_SHADER_VISIBILITY_PIXEL); // t1

        // ── Static sampler (igual que Shader.hpp) ────────────────────────
        CD3DX12_STATIC_SAMPLER_DESC sampler(0);
        sampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.MipLODBias       = 0.0f;
        sampler.MaxAnisotropy    = 16;
        sampler.MinLOD           = 0.0f;
        sampler.MaxLOD           = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister   = 0;
        sampler.RegisterSpace    = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
        rsDesc.Init(_countof(params), params, 1, &sampler,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> sig, err;
        HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
        if (FAILED(hr)) {
            if (err) std::cerr << "LightingShader RootSig error: " << (char*)err->GetBufferPointer() << "\n";
            DX_THROW(hr);
        }
        DX_THROW(device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(),
            IID_PPV_ARGS(&m_RootSignature)));

        // ── Compile shaders ───────────────────────────────────────────────
        UINT flags = 0;
#if defined(_DEBUG)
        flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;

        hr = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "VSMain", "vs_5_0", flags, 0, &vsBlob, &errBlob);
        if (FAILED(hr)) {
            if (errBlob) std::cerr << "LightingShader VS: " << (char*)errBlob->GetBufferPointer() << "\n";
            DX_THROW(hr);
        }

        hr = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "PSMain", "ps_5_0", flags, 0, &psBlob, &errBlob);
        if (FAILED(hr)) {
            if (errBlob) std::cerr << "LightingShader PS: " << (char*)errBlob->GetBufferPointer() << "\n";
            DX_THROW(hr);
        }

        // ── Input layout (idéntico a Shader.hpp) ─────────────────────────
        D3D12_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        // ── PSO ───────────────────────────────────────────────────────────
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso {};
        pso.InputLayout           = { layout, _countof(layout) };
        pso.pRootSignature        = m_RootSignature.Get();
        pso.VS                    = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
        pso.PS                    = CD3DX12_SHADER_BYTECODE(psBlob.Get());
        pso.RasterizerState       = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        pso.RasterizerState.FrontCounterClockwise = TRUE; // Sponza/Assimp
        pso.BlendState            = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        pso.DepthStencilState     = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        pso.SampleMask            = UINT_MAX;
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets      = 1;
        pso.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM; // backbuffer
        pso.DSVFormat             = DXGI_FORMAT_D32_FLOAT;
        pso.SampleDesc.Count      = 1;

        hr = device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_PSO));
        if (FAILED(hr)) {
            std::cerr << "[CRITICAL] LightingShader PSO falló (0x" << std::hex << hr << ")\n";
            DX_THROW(hr);
        }
    }

    // Bindea PSO + root signature. Después de esto el caller setea los params.
    void Bind(ID3D12GraphicsCommandList* cmd) const {
        cmd->SetGraphicsRootSignature(m_RootSignature.Get());
        cmd->SetPipelineState(m_PSO.Get());
    }

    // Helpers para setear cada slot desde AppLayer::OnRender
    static void SetSceneConstants(ID3D12GraphicsCommandList* cmd, void const* data, UINT count32 = 56) {
        cmd->SetGraphicsRoot32BitConstants(0, count32, data, 0);
    }
    static void SetGIConstants(ID3D12GraphicsCommandList* cmd, void const* data, UINT count32 = 4) {
        cmd->SetGraphicsRoot32BitConstants(1, count32, data, 0);
    }
    static void SetAlbedoTable(ID3D12GraphicsCommandList* cmd, D3D12_GPU_DESCRIPTOR_HANDLE handle) {
        cmd->SetGraphicsRootDescriptorTable(2, handle);
    }
    static void SetDiffuseGITable(ID3D12GraphicsCommandList* cmd, D3D12_GPU_DESCRIPTOR_HANDLE handle) {
        cmd->SetGraphicsRootDescriptorTable(3, handle);
    }

private:
    ComPtr<ID3D12RootSignature> m_RootSignature;
    ComPtr<ID3D12PipelineState> m_PSO;
};