#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <Plugins/DirectX/d3dx12.h>

namespace Brixelizer {

using Microsoft::WRL::ComPtr;

// ---------------------------------------------------------------------------
// Minimum G-Buffer for Brixelizer.
//
// RTs:
//   [0] normals   DXGI_FORMAT_R8G8B8A8_UNORM (world-space, [0,1] packed)
// Depth:
//   depth     DXGI_FORMAT_D32_FLOAT
//
// Dummies (constants values):
//   roughness     DXGI_FORMAT_R8_UNORM (= 0, no specular)
//   motionVectors DXGI_FORMAT_R16G16_FLOAT (= 0, no TAA)
//
// History (copies of CPU→GPU (last frames)):
//   historyDepth, historyNormals
// ---------------------------------------------------------------------------
class GBuffer {
public:
	// Main resources.
	ComPtr<ID3D12Resource> normals;
	ComPtr<ID3D12Resource> depth;
	ComPtr<ID3D12Resource> roughness;       // dummy.
	ComPtr<ID3D12Resource> motionVectors;   // dummy.

	// History.
	ComPtr<ID3D12Resource> historyDepth;
	ComPtr<ID3D12Resource> historyNormals;

	// Tracked state.
	D3D12_RESOURCE_STATES normalsState       = D3D12_RESOURCE_STATE_RENDER_TARGET;
	D3D12_RESOURCE_STATES depthState         = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	D3D12_RESOURCE_STATES roughnessState     = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	D3D12_RESOURCE_STATES motionVectorsState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	D3D12_RESOURCE_STATES historyDepthState   = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	D3D12_RESOURCE_STATES historyNormalsState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	// Descriptors.
	D3D12_CPU_DESCRIPTOR_HANDLE normalsRTV {};
	D3D12_CPU_DESCRIPTOR_HANDLE depthDSV  {};

	uint32_t width  = 0;
	uint32_t height = 0;

	void Init(
		ID3D12Device* device,
		uint32_t width, 
		uint32_t height,
		ID3D12DescriptorHeap* rtvHeap, 
		uint32_t rtvOffset, // 1 RTV slot.
		ID3D12DescriptorHeap* dsvHeap, 
		uint32_t dsvOffset  // 1 DSV slot.
	) {
		m_RtvHeapOwned = rtvHeap;
		m_DsvHeapOwned = dsvHeap;

		auto makeRT = [&](DXGI_FORMAT fmt, D3D12_RESOURCE_STATES initState,
						  const wchar_t* name) -> ComPtr<ID3D12Resource>
		{
			D3D12_HEAP_PROPERTIES heap { .Type = D3D12_HEAP_TYPE_DEFAULT };
			D3D12_RESOURCE_DESC   desc {
				.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
				.Width            = width,
				.Height           = height,
				.DepthOrArraySize = 1,
				.MipLevels        = 1,
				.Format           = fmt,
				.SampleDesc       = { 1, 0 },
				.Flags            = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
									D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			};
			D3D12_CLEAR_VALUE clear { .Format = fmt, .Color = {0,0,0,0} };
			ComPtr<ID3D12Resource> res;
			device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
				&desc, initState, &clear, IID_PPV_ARGS(&res));
			res->SetName(name);
			return res;
		};

		auto makeDummy = [&](DXGI_FORMAT fmt, const wchar_t* name) -> ComPtr<ID3D12Resource>
		{
			// 1x1 texture in SRV status, always 0.
			D3D12_HEAP_PROPERTIES heap { .Type = D3D12_HEAP_TYPE_DEFAULT };
			D3D12_RESOURCE_DESC   desc {
				.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
				.Width            = 1,
				.Height           = 1,
				.DepthOrArraySize = 1,
				.MipLevels        = 1,
				.Format           = fmt,
				.SampleDesc       = { 1, 0 },
				.Flags            = D3D12_RESOURCE_FLAG_NONE,
			};
			ComPtr<ID3D12Resource> res;
			device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
				&desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr, IID_PPV_ARGS(&res));
			res->SetName(name);
			return res;
		};

		auto makeHistory = [&](DXGI_FORMAT fmt, const wchar_t* name) -> ComPtr<ID3D12Resource>
		{
			D3D12_HEAP_PROPERTIES heap { .Type = D3D12_HEAP_TYPE_DEFAULT };
			D3D12_RESOURCE_DESC   desc {
				.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
				.Width            = width,
				.Height           = height,
				.DepthOrArraySize = 1,
				.MipLevels        = 1,
				.Format           = fmt,
				.SampleDesc       = { 1, 0 },
				.Flags            = D3D12_RESOURCE_FLAG_NONE,
			};
			ComPtr<ID3D12Resource> res;
			device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
				&desc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				nullptr, IID_PPV_ARGS(&res));
			res->SetName(name);
			return res;
		};

		// Depth (typeless format for reading it as SRV R32_FLOAT).
		{
			D3D12_HEAP_PROPERTIES heap { .Type = D3D12_HEAP_TYPE_DEFAULT };
			D3D12_RESOURCE_DESC desc {
				.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
				.Width            = width,
				.Height           = height,
				.DepthOrArraySize = 1,
				.MipLevels        = 1,
				.Format           = DXGI_FORMAT_R32_TYPELESS,
				.SampleDesc       = { 1, 0 },
				.Flags            = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
			};
			D3D12_CLEAR_VALUE clear { .Format = DXGI_FORMAT_D32_FLOAT };
			clear.DepthStencil = { 1.0f, 0 };
			device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
				&desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear,
				IID_PPV_ARGS(&depth));
			depth->SetName(L"GBuffer_Depth");
		}

		normals       = makeRT    (DXGI_FORMAT_R8G8B8A8_UNORM,   D3D12_RESOURCE_STATE_RENDER_TARGET,              L"GBuffer_Normals");
		roughness     = makeDummy (DXGI_FORMAT_R8_UNORM,          L"GBuffer_Roughness_Dummy");
		motionVectors = makeDummy (DXGI_FORMAT_R16G16_FLOAT,      L"GBuffer_MotionVectors_Dummy");
		historyDepth   = makeHistory(DXGI_FORMAT_R32_FLOAT,       L"GBuffer_HistoryDepth");
		historyNormals = makeHistory(DXGI_FORMAT_R8G8B8A8_UNORM,  L"GBuffer_HistoryNormals");

		// RTVs.
		uint32_t const rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		normalsRTV = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		normalsRTV.ptr += rtvOffset * rtvSize;

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc { .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
												.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D };
		device->CreateRenderTargetView(normals.Get(), &rtvDesc, normalsRTV);

		// Depth Stencil View (DSV).
		uint32_t const dsvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		depthDSV = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		depthDSV.ptr += dsvOffset * dsvSize;

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc {
			.Format        = DXGI_FORMAT_D32_FLOAT,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D
		};
		device->CreateDepthStencilView(depth.Get(), &dsvDesc, depthDSV);
	}

	void BeginGBufferPass(ID3D12GraphicsCommandList* cmd) {
		std::vector<D3D12_RESOURCE_BARRIER> barriers;

		if (normalsState != D3D12_RESOURCE_STATE_RENDER_TARGET) {
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				normals.Get(), normalsState, D3D12_RESOURCE_STATE_RENDER_TARGET));
			normalsState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		}

		if (depthState != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				depth.Get(), depthState, D3D12_RESOURCE_STATE_DEPTH_WRITE));
			depthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

		if (!barriers.empty()) {
			cmd->ResourceBarrier((UINT)barriers.size(), barriers.data());
		}

		float clearColor[] = { 0, 0, 0, 0 };
		cmd->ClearRenderTargetView(normalsRTV, clearColor, 0, nullptr);
		cmd->ClearDepthStencilView(depthDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		cmd->OMSetRenderTargets(1, &normalsRTV, FALSE, &depthDSV);
	}

	void EndGBufferPass(ID3D12GraphicsCommandList* cmd) {
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(normals.Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(depth.Get(),
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
		};
		cmd->ResourceBarrier(_countof(barriers), barriers);
		normalsState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		depthState   = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	}
	

	void CopyToHistory(ID3D12GraphicsCommandList* cmd) {
		// Normals: NON_PIXEL_SHADER_RESOURCE → COPY_SRC.
		D3D12_RESOURCE_BARRIER toSrc[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(normals.Get(),
				normalsState, D3D12_RESOURCE_STATE_COPY_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(depth.Get(),
				depthState, D3D12_RESOURCE_STATE_COPY_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(historyNormals.Get(),
				historyNormalsState, D3D12_RESOURCE_STATE_COPY_DEST),
			CD3DX12_RESOURCE_BARRIER::Transition(historyDepth.Get(),
				historyDepthState, D3D12_RESOURCE_STATE_COPY_DEST),
		};
		cmd->ResourceBarrier(_countof(toSrc), toSrc);

		// Depth is copied as R32_TYPELESS → R32_FLOAT in history.
		{
			D3D12_TEXTURE_COPY_LOCATION src {
				.pResource        = depth.Get(),
				.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
				.SubresourceIndex = 0
			};
			D3D12_TEXTURE_COPY_LOCATION dst {
				.pResource        = historyDepth.Get(),
				.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
				.SubresourceIndex = 0
			};
			cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		}
		cmd->CopyResource(historyNormals.Get(), normals.Get());

		D3D12_RESOURCE_BARRIER toSRVBarriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(normals.Get(),
				D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(depth.Get(),
				D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(historyNormals.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(historyDepth.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
		};
		cmd->ResourceBarrier(_countof(toSRVBarriers), toSRVBarriers);

		normalsState        = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		depthState          = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		historyNormalsState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		historyDepthState   = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	}
private:
	ComPtr<ID3D12DescriptorHeap> m_RtvHeapOwned {};
	ComPtr<ID3D12DescriptorHeap> m_DsvHeapOwned {};
};

}