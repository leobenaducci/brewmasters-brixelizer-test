#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <string>

#include <Plugins/DirectX/d3dx12.h>

#include "../Engine/DXException.hpp"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

class DX12Manager {
public:
	static const UINT FrameCount = 2;

	DX12Manager(HWND const windowHandle) {
		std::cout << "[Engine] Initializing DX12 Pipeline with double buffering..." << std::endl;
		try {
			InitPipeline(windowHandle);
			InitAssets();
			CreateDepthBuffer(windowHandle);
			
			DX_THROW(m_CommandList->Close());
			ID3D12CommandList* const commandLists[] = { m_CommandList.Get() };
			m_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
			
			FlushGPU();
			
			std::cout << "[Engine] DX12 Pipeline ready." << std::endl;
		}
		catch (DXException const& e) {
			HandleDeviceRemoved(e.GetErrorCode());
			throw;
		}
	}

	~DX12Manager() {
		if (m_Device) FlushGPU();
		if (m_FenceEvent) CloseHandle(m_FenceEvent);
	}

	void BeginFrame() {
		auto* const currentAllocator = m_CommandAllocators[m_FrameIndex].Get();

		DX_THROW(currentAllocator->Reset());
		DX_THROW(m_CommandList->Reset(currentAllocator, nullptr));

		ResourceBarrier(m_RenderTargets[m_FrameIndex].Get(), 
						D3D12_RESOURCE_STATE_PRESENT, 
						D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	void Present() {
		ResourceBarrier(m_RenderTargets[m_FrameIndex].Get(), 
						D3D12_RESOURCE_STATE_RENDER_TARGET, 
						D3D12_RESOURCE_STATE_PRESENT);
		
		DX_THROW(m_CommandList->Close());
		ID3D12CommandList* const commandLists[] = { m_CommandList.Get() };
		m_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

		HRESULT const hr = m_SwapChain->Present(1, 0);
		if (FAILED(hr)) {
			HandleDeviceRemoved(hr);
			DX_THROW(hr);
		}

		WaitForPreviousFrame();
	}

	uint32_t GetCurrentBackBufferIndex() const noexcept { return m_SwapChain->GetCurrentBackBufferIndex(); }
	ID3D12Device* const GetDevice() const noexcept { return m_Device.Get(); }
	ID3D12GraphicsCommandList* const GetCommandList() const noexcept { return m_CommandList.Get(); }
	ID3D12CommandQueue* const GetCommandQueue() const noexcept { return m_CommandQueue.Get(); }
	ID3D12DescriptorHeap* const GetSrvHeap() const noexcept { return m_SrvDescriptorHeap.Get(); }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() {
		D3D12_CPU_DESCRIPTOR_HANDLE handle = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<SIZE_T>(m_FrameIndex) * m_RtvDescriptorSize;
		return handle;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const noexcept {
		return m_DsvHeap->GetCPUDescriptorHandleForHeapStart();
	}

	void FlushGPU() {
		m_FenceValue++;
		
		DX_THROW(m_CommandQueue->Signal(m_Fence.Get(), m_FenceValue));

		if (m_Fence->GetCompletedValue() < m_FenceValue) {
			DX_THROW(m_Fence->SetEventOnCompletion(m_FenceValue, m_FenceEvent));
			WaitForSingleObject(m_FenceEvent, INFINITE);
		}
	}

private:
	void InitPipeline(HWND const hwnd) {
		UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
			debugController->EnableDebugLayer();
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
#endif

		DX_THROW(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_Factory)));
		
		ComPtr<IDXGIAdapter1> bestAdapter;
		DXGI_ADAPTER_DESC1 bestDesc{};
		SIZE_T maxVram = 0;

		ComPtr<IDXGIAdapter1> adapter;
		for (UINT i = 0; m_Factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			// Omit WARP.
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
				continue;
			} 

			// Select GPU with most VRAM.
			if (desc.DedicatedVideoMemory > maxVram) {
				maxVram = desc.DedicatedVideoMemory;
				bestAdapter = adapter;
				bestDesc = desc;
			}
		}

		HRESULT hr = E_FAIL;
		if (bestAdapter) {
			std::wcout << L"[Engine] Selected GPU: " << bestDesc.Description << std::endl;
			std::cout << "[Engine] VRAM: " << bestDesc.DedicatedVideoMemory / (1024 * 1024) << " MB" << std::endl;
			hr = D3D12CreateDevice(bestAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device));
		}

		// WARP fallback (should not be used tho).
		if (FAILED(hr)) {
			std::cout << "[Engine] Dedicated GPU not found. Using WARP..." << std::endl;
			ComPtr<IDXGIAdapter> warpAdapter;
			DX_THROW(m_Factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
			DX_THROW(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device)));
		}

#if defined(_DEBUG)
		ComPtr<ID3D12InfoQueue> infoQueue;
		if (SUCCEEDED(m_Device.As(&infoQueue))) {
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		}
#endif

		D3D12_COMMAND_QUEUE_DESC queueDesc{};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		DX_THROW(m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue)));
		m_CommandQueue->SetName(L"Main Command Queue");

		RECT rect;
		GetClientRect(hwnd, &rect);
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
		swapChainDesc.BufferCount = FrameCount;
		swapChainDesc.Width = static_cast<UINT>(rect.right - rect.left);
		swapChainDesc.Height = static_cast<UINT>(rect.bottom - rect.top);
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

		ComPtr<IDXGISwapChain1> swapChain{};
		DX_THROW(m_Factory->CreateSwapChainForHwnd(m_CommandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain));
		DX_THROW(swapChain.As(&m_SwapChain));
		m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();

		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		DX_THROW(m_Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_RtvHeap)));
		m_RtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
		srvHeapDesc.NumDescriptors = 1024; 
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		DX_THROW(m_Device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_SrvDescriptorHeap)));
	}

	void InitAssets() {
		for (UINT n = 0; n < FrameCount; n++) {
			DX_THROW(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocators[n])));
			
			std::wstring const name = L"Command Allocator " + std::to_wstring(n);
			m_CommandAllocators[n]->SetName(name.c_str());
		}

		DX_THROW(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_CommandList)));
		m_CommandList->SetName(L"Main Command List");

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
		for (UINT n = 0; n < FrameCount; n++) {
			DX_THROW(m_SwapChain->GetBuffer(n, IID_PPV_ARGS(&m_RenderTargets[n])));
			m_Device->CreateRenderTargetView(m_RenderTargets[n].Get(), nullptr, rtvHandle);
			
			std::wstring const name = L"Render Target " + std::to_wstring(n);
			m_RenderTargets[n]->SetName(name.c_str());
			
			rtvHandle.ptr += m_RtvDescriptorSize;
		}

		DX_THROW(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)));
		m_FenceValue = 1;
		m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	}

	void CreateDepthBuffer(HWND const hwnd) {
		RECT rect;
		GetClientRect(hwnd, &rect);

		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		DX_THROW(m_Device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_DsvHeap)));

		D3D12_RESOURCE_DESC depthDesc = {};
		depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthDesc.Width = std::max(1L, rect.right - rect.left);
		depthDesc.Height = std::max(1L, rect.bottom - rect.top);
		depthDesc.DepthOrArraySize = 1;
		depthDesc.MipLevels = 1;
		depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE depthClearValue = {};
		depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		depthClearValue.DepthStencil.Depth = 1.0f;

		auto const heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		DX_THROW(m_Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &depthDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue, IID_PPV_ARGS(&m_DepthBuffer))
		);
		
		m_DepthBuffer->SetName(L"Depth Stencil Buffer");
		m_Device->CreateDepthStencilView(m_DepthBuffer.Get(), nullptr, m_DsvHeap->GetCPUDescriptorHandleForHeapStart());
	}

	void WaitForPreviousFrame() {
		UINT64 const fenceWaitValue = m_FenceValue;
		DX_THROW(m_CommandQueue->Signal(m_Fence.Get(), fenceWaitValue));
		m_FenceValue++;

		if (m_Fence->GetCompletedValue() < fenceWaitValue) {
			DX_THROW(m_Fence->SetEventOnCompletion(fenceWaitValue, m_FenceEvent));
			WaitForSingleObject(m_FenceEvent, INFINITE);
		}

		m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
	}

	void ResourceBarrier(ID3D12Resource* const resource, D3D12_RESOURCE_STATES const before, D3D12_RESOURCE_STATES const after) {
		if (before == after) {
			std::cerr << "[Renderer] Tried to transition to the same state.\n";
			return;
		}

		auto const barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after);
		m_CommandList->ResourceBarrier(1, &barrier);
	}

	void HandleDeviceRemoved(HRESULT const hr) {
		if (hr == DXGI_ERROR_DEVICE_REMOVED && m_Device) {
			HRESULT const reason = m_Device->GetDeviceRemovedReason();
			std::cerr << "[GPU ERROR] Device Removed. Reason: 0x" << std::hex << reason << std::endl;
		}
	}

private:
	ComPtr<IDXGIFactory4> m_Factory;
	ComPtr<ID3D12Device> m_Device;
	ComPtr<ID3D12CommandQueue> m_CommandQueue;
	ComPtr<IDXGISwapChain3> m_SwapChain;
	ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
	ComPtr<ID3D12Resource> m_RenderTargets[FrameCount];
	
	ComPtr<ID3D12CommandAllocator> m_CommandAllocators[FrameCount];
	
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;
	ComPtr<ID3D12Resource> m_DepthBuffer;
	ComPtr<ID3D12DescriptorHeap> m_DsvHeap;
	ComPtr<ID3D12DescriptorHeap> m_SrvDescriptorHeap;
	
	UINT m_RtvDescriptorSize = 0;
	UINT m_FrameIndex = 0;
	
	HANDLE m_FenceEvent = nullptr;
	ComPtr<ID3D12Fence> m_Fence;
	UINT64 m_FenceValue = 0;
};