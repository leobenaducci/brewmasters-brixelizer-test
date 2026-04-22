#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <iostream>
#include <vector>
#include <Scene/SceneBuilder.hpp>
#include <Plugins/DirectX/d3dx12_core.h>
#include <Plugins/DirectX/d3dx12.h>

#define FFX_CPU
#include <Plugins/FidelityFX/include/FidelityFX/gpu/brixelizer/ffx_brixelizer_host_gpu_shared.h>

#include "DXException.hpp"
#include "DXCShaderCompiler.hpp"

using Microsoft::WRL::ComPtr;

class BrixelizerShader {
public:
    BrixelizerShader(ID3D12Device* device, std::wstring const& path) {
        CreateRootSignature(device);

        ComPtr<ID3DBlob> vsBlob{}, psBlob{};
        CompileShaders(path, &vsBlob, &psBlob);

        CreatePSO(device, vsBlob.Get(), psBlob.Get());
    }

    void Bind(ID3D12GraphicsCommandList* cmdList, 
              D3D12_GPU_VIRTUAL_ADDRESS sceneConstantsAddress,
              D3D12_GPU_VIRTUAL_ADDRESS brixelConstantsAddress,
              ID3D12DescriptorHeap* srvHeap
    ) {
        assert(m_PSO && "PSO is null");

        // Bind Root Signature and PSO.
        cmdList->SetGraphicsRootSignature(m_RootSignature.Get());
        cmdList->SetPipelineState(m_PSO.Get());

        // Bind Constant Buffer Views.
        cmdList->SetGraphicsRootConstantBufferView(0, sceneConstantsAddress);
        cmdList->SetGraphicsRootConstantBufferView(1, brixelConstantsAddress);

        // Bind Descriptor Table with textures t0-t4.
        ID3D12DescriptorHeap* heaps[] = { srvHeap };
        cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
        
        // Bind table on slot 2.
        cmdList->SetGraphicsRootDescriptorTable(2, srvHeap->GetGPUDescriptorHandleForHeapStart());
    }

private:
    void CreateRootSignature(ID3D12Device* device) {
        CD3DX12_ROOT_PARAMETER rootParameters[3];

        // b0: Scene Constants (CBV)
        rootParameters[0].InitAsConstantBufferView(0);

        // b1: Brixelizer Cascade Info (CBV)
        rootParameters[1].InitAsConstantBufferView(1);

        // t0, t1, t2, t3, t4.
        CD3DX12_DESCRIPTOR_RANGE texRange{};
        texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0);
        rootParameters[2].InitAsDescriptorTable(1, &texRange, D3D12_SHADER_VISIBILITY_PIXEL);

        // Linear wrap sampler.
        CD3DX12_STATIC_SAMPLER_DESC wrapSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
        wrapSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        wrapSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        wrapSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        wrapSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        
        // Linear clamp sampler.
        CD3DX12_STATIC_SAMPLER_DESC clampSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
        clampSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        clampSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        clampSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        clampSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC const samplers[2] = { wrapSampler, clampSampler };

        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc{};
        rootSigDesc.Init(_countof(rootParameters), rootParameters, 1, samplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature{}, error{};
        HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        if (FAILED(hr)) {
            if (error) std::cout << "RootSig Error: " << (char*)error->GetBufferPointer() << std::endl;
            DX_THROW(hr);
        }

        DX_THROW(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));
    }

    void CompileShaders(const std::wstring& path, ID3DBlob** vsBlob, ID3DBlob** psBlob) {
        static DXCShaderCompiler dxc{};

        std::vector<std::wstring> includeDirs = {
            L"../Engine/Plugins/FidelityFX/include/FidelityFX/gpu/brixelizer",
            L"../Engine/Plugins/FidelityFX/include/FidelityFX/gpu",
            L"../Engine/Plugins/FidelityFX/include/FidelityFX",
        };

        try {
            *vsBlob = dxc.Compile(path.c_str(), L"VSMain", L"vs_6_6", includeDirs).Detach();
            *psBlob = dxc.Compile(path.c_str(), L"PSMain", L"ps_6_6", includeDirs).Detach();
        } catch (const std::exception& e) {
            std::cout << "Shader compilation error: " << e.what() << '\n';
            DX_THROW(E_FAIL);
        }
    }

    void CreatePSO(ID3D12Device* device, ID3DBlob* vsBlob, ID3DBlob* psBlob) {
        constexpr D3D12_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { layout, _countof(layout) };
        psoDesc.pRootSignature = m_RootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsBlob);
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob);
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;

        HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO));
        if (FAILED(hr)) {
            std::cerr << "[ERROR] Failed to create PSO (0x" << std::hex << hr << ")" << '\n';
            DX_THROW(hr);
        }
    }

private:
    ComPtr<ID3D12RootSignature> m_RootSignature{};
    ComPtr<ID3D12PipelineState> m_PSO{};
};