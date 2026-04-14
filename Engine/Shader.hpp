#pragma once
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>
#include <iostream>
#include <vector>
#include <Plugins/DirectX/d3dx12_core.h>
#include <Plugins/DirectX/d3dx12.h>

#include "DXException.hpp"

using Microsoft::WRL::ComPtr;

class Shader {
public:
    Shader(ID3D12Device* device, const std::wstring& path) {
        CD3DX12_DESCRIPTOR_RANGE range;
        range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0

        // 1. Rango para la textura (SRV) en t0
        CD3DX12_DESCRIPTOR_RANGE texRange;
        texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // 1 textura en t0

        CD3DX12_ROOT_PARAMETER rootParameters[2];
        // Parámetro 0: Constantes (lo que ya tenías)
        rootParameters[0].InitAsConstants(56, 0, 0, D3D12_SHADER_VISIBILITY_ALL);
        // Parámetro 1: Tabla de descriptores para la textura
        rootParameters[1].InitAsDescriptorTable(1, &texRange, D3D12_SHADER_VISIBILITY_PIXEL);

        // 2. Sampler Estático (Filtro Anistrópico para Sponza)
        CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_ANISOTROPIC);
        sampler.Filter         = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // trilinear filtering
        sampler.AddressU       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressV       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressW       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.MipLODBias     = 0.0f;
        sampler.MaxAnisotropy  = 16;
        sampler.MinLOD         = 0.0f;
        sampler.MaxLOD         = D3D12_FLOAT32_MAX; // permite todos los mips
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace  = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
        rootSigDesc.Init(_countof(rootParameters), rootParameters, 1, &sampler, 
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature, error;
        HRESULT hrSig = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        
        if (FAILED(hrSig)) {
            if (error) std::cout << "RootSig Serialize Error: " << (char*)error->GetBufferPointer() << std::endl;
            DX_THROW(hrSig);
        }

        DX_THROW(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));

        // 2. Compilación de Shaders (VS y PS)
        ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
        UINT compileFlags = 0;
#if defined(_DEBUG)
        compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        // Vertex Shader
        HRESULT hrVS = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, &vsBlob, &errorBlob);
        if (FAILED(hrVS)) {
            if (errorBlob) std::cout << "VS Error: " << (char*)errorBlob->GetBufferPointer() << std::endl;
            DX_THROW(hrVS);
        }

        // Pixel Shader
        HRESULT hrPS = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &psBlob, &errorBlob);
        if (FAILED(hrPS)) {
            if (errorBlob) std::cout << "PS Error: " << (char*)errorBlob->GetBufferPointer() << std::endl;
            DX_THROW(hrPS);
        }

        // 3. Input Layout (Asegúrate de que coincide con tu struct Vertex en C++)
        D3D12_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // 4. PSO Desc (Pipeline State Object)
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { layout, _countof(layout) };
        psoDesc.pRootSignature = m_RootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
        
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.FrontCounterClockwise = TRUE; // Necesario para Sponza/Assimp
        
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;

        // Intentar crear el PSO
        HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO));
        if (FAILED(hr)) {
            std::cerr << "[CRITICAL] Falló la creación del PSO (0x" << std::hex << hr << ")" << std::endl;
            DX_THROW(hr);
        }
    }

    void Bind(ID3D12GraphicsCommandList* cmdList) {
        if (m_PSO && m_RootSignature) {
            cmdList->SetGraphicsRootSignature(m_RootSignature.Get());
            cmdList->SetPipelineState(m_PSO.Get());
        }
    }

private:
    ComPtr<ID3D12RootSignature> m_RootSignature;
    ComPtr<ID3D12PipelineState> m_PSO;
};