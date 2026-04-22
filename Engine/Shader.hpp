#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <iostream>
#include <vector>
#include <Plugins/DirectX/d3dx12_core.h>
#include <Plugins/DirectX/d3dx12.h>
#include <Scene/SceneBuilder.hpp>

#include "DXException.hpp"
#include "DXCShaderCompiler.hpp"

using Microsoft::WRL::ComPtr;

class Shader {
public:
	Shader(ID3D12Device* device, std::wstring const& path) {
		CD3DX12_ROOT_PARAMETER rootParameters[2];

		// cbuffer b0.
		constexpr int bufferID = 0;
		rootParameters[0].InitAsConstants(sizeof(SceneConstants) / 4, bufferID);

		// texture t0.
		CD3DX12_DESCRIPTOR_RANGE texRange{};
		texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		rootParameters[1].InitAsDescriptorTable(1, &texRange, D3D12_SHADER_VISIBILITY_PIXEL);

		// Static sampler. (static because is defined in the root signature).
		CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_ANISOTROPIC);
		sampler.Filter         = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // Trilinear filtering.
		sampler.AddressU       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.MipLODBias     = 0.0f;
		sampler.MaxAnisotropy  = 16;
		sampler.MinLOD         = 0.0f;
		sampler.MaxLOD         = D3D12_FLOAT32_MAX; // Allows all mips.
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace  = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
		rootSigDesc.Init(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature{}, error{};
		HRESULT hrSig = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

		if (FAILED(hrSig)) {
			if (error) {
				std::cout << "RootSig Serialize Error: " << (char*)error->GetBufferPointer() << std::endl;
			}

			DX_THROW(hrSig);
		}

		DX_THROW(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));

		// VS & PS compilation.
		static DXCShaderCompiler dxc{};

		std::vector<std::wstring> includeDirs = {
			L"G:/Brewmasters/brixelizer-test/Engine/Plugins/FidelityFX/include/FidelityFX/gpu/brixelizer",
			L"G:/Brewmasters/brixelizer-test/Engine/Plugins/FidelityFX/include/FidelityFX/gpu",
			L"G:/Brewmasters/brixelizer-test/Engine/Plugins/FidelityFX/include/FidelityFX",
			L"G:/Brewmasters/FidelityFX-SDK-1.1.4/sdk/src/backends/dx12/shaders/brixelizer"
		};

		ComPtr<ID3DBlob> vsBlob, psBlob;
		try {
			vsBlob = dxc.Compile(path.c_str(), L"VSMain", L"vs_5_0", includeDirs);
			psBlob = dxc.Compile(path.c_str(), L"PSMain", L"ps_5_0", includeDirs);
		} catch (const std::exception& e) {
			std::cout << "Shader compilation error: " << e.what() << std::endl;
			DX_THROW(E_FAIL);
		}

		// Input Layout.
		D3D12_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// PSO Desc.
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { layout, _countof(layout) };
		psoDesc.pRootSignature = m_RootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());

		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.FrontCounterClockwise = FALSE; // CW or CCW rendering.

		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleDesc.Count = 1;

		// Create PSO!!
		HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO));

		if (FAILED(hr)) {
			std::cerr << "[CRITICAL] Falló la creación del PSO (0x" << std::hex << hr << ")" << std::endl;
			DX_THROW(hr);
		}
	}

	void Bind(ID3D12GraphicsCommandList* cmdList) {
		assert(m_PSO && "PSO is null, was Shader::Compile() called?");

		if (m_PSO && m_RootSignature) {
			cmdList->SetGraphicsRootSignature(m_RootSignature.Get());
			cmdList->SetPipelineState(m_PSO.Get());
		}
	}

private:
	ComPtr<ID3D12RootSignature> m_RootSignature{};
	ComPtr<ID3D12PipelineState> m_PSO{};
};