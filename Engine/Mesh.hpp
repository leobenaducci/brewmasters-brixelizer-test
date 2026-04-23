#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <algorithm>
#include <vector>
#include <limits>
#include <Plugins/DirectX/d3dx12.h>

using Microsoft::WRL::ComPtr;

struct Vertex {
	float position[3];
	float normal[3];
	float texCoord[2];
};

struct AABB {
	float min[3];
	float max[3];
};

class Mesh {
public:
	Mesh(ID3D12Device* const device, std::vector<Vertex> const& vertices, std::vector<uint32_t> const& indices, int const textureIndex)
		: m_TextureIndex(textureIndex)
		, m_VertexCount(static_cast<UINT>(vertices.size()))
		, m_IndexCount(static_cast<UINT>(indices.size()))
	{
		CreateVertexBuffer(device, vertices);
		CreateIndexBuffer(device, indices);
		ComputeAABB(vertices);
	}

	void Draw(ID3D12GraphicsCommandList* const cmdList,
			  ID3D12DescriptorHeap* const srvHeap,
			  UINT const descriptorSize
	) const {
		if (m_TextureIndex != -1) {
			// Obtain memory address of descriptors.
			CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(srvHeap->GetGPUDescriptorHandleForHeapStart());

			// Offset to mesh texture.
			srvHandle.Offset(m_TextureIndex, descriptorSize);
			cmdList->SetGraphicsRootDescriptorTable(2, srvHandle); 
		}

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->IASetVertexBuffers(0, 1, &m_VertexView);
		cmdList->IASetIndexBuffer(&m_IndexView);

		cmdList->DrawIndexedInstanced(m_IndexCount, 1, 0, 0, 0);
	}

	// Mesh data needed in Brixelizer.
	AABB const&     GetAABB()          const noexcept { return m_AABB; }
	ID3D12Resource* GetVertexBuffer()  const noexcept { return m_VertexBuffer.Get(); }
	ID3D12Resource* GetIndexBuffer()   const noexcept { return m_IndexBuffer.Get(); }
	UINT            GetVertexCount()   const noexcept { return m_VertexCount; }
	UINT            GetVertexStride()  const noexcept { return sizeof(Vertex); }
	UINT            GetIndexCount()    const noexcept { return m_IndexCount; }
	UINT            GetIndexStride()   const noexcept { return sizeof(uint32_t); }
	UINT            GetTriangleCount() const noexcept { return m_IndexCount / 3; }

private:
	void CreateVertexBuffer(ID3D12Device* const device, std::vector<Vertex> const& vertices) {
		UINT const dataSize = m_VertexCount * sizeof(Vertex);

		auto const heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto const resDesc   = CD3DX12_RESOURCE_DESC::Buffer(dataSize);

		// We use D3D12_RESOURCE_STATE_GENERIC_READ for upload buffers (Upload heap)
		// The Debug Layer may sometimes report an error if another state is used for this type of heap.
		device->CreateCommittedResource(
			&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&m_VertexBuffer)
		);

		void* mapped = nullptr;
		m_VertexBuffer->Map(0, nullptr, &mapped);
		memcpy(mapped, vertices.data(), dataSize);
		m_VertexBuffer->Unmap(0, nullptr);

		m_VertexView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
		m_VertexView.StrideInBytes  = sizeof(Vertex);
		m_VertexView.SizeInBytes    = dataSize;
	}

	void CreateIndexBuffer(ID3D12Device* const device, std::vector<uint32_t> const& indices) {
		UINT const dataSize = m_IndexCount * sizeof(uint32_t);

		auto const heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto const resDesc   = CD3DX12_RESOURCE_DESC::Buffer(dataSize);

		device->CreateCommittedResource(
			&heapProps, 
			D3D12_HEAP_FLAG_NONE, 
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, 
			nullptr, // pOptimizedClearValue.
			IID_PPV_ARGS(&m_IndexBuffer)
		);

		void* mapped = nullptr;
		m_IndexBuffer->Map(0, nullptr, &mapped);
		memcpy(mapped, indices.data(), dataSize);
		m_IndexBuffer->Unmap(0, nullptr);

		m_IndexView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
		m_IndexView.Format         = DXGI_FORMAT_R32_UINT;
		m_IndexView.SizeInBytes    = dataSize;
	}

	void ComputeAABB(std::vector<Vertex> const& vertices) {
		constexpr float floatMax = std::numeric_limits<float>::max();

		m_AABB = {
			.min = {  floatMax,  floatMax,  floatMax },
			.max = { -floatMax, -floatMax, -floatMax }
		};

		for (auto const& vertex : vertices) {
			for (int i = 0; i < 3; ++i) {
				m_AABB.min[i] = std::min(m_AABB.min[i], vertex.position[i]);
				m_AABB.max[i] = std::max(m_AABB.max[i], vertex.position[i]);
			}
		}
	}

private:
	ComPtr<ID3D12Resource>   m_VertexBuffer {};
	ComPtr<ID3D12Resource>   m_IndexBuffer  {};
	D3D12_VERTEX_BUFFER_VIEW m_VertexView   {};
	D3D12_INDEX_BUFFER_VIEW  m_IndexView    {};
	UINT                     m_VertexCount  {};
	UINT                     m_IndexCount   {};
	int                      m_TextureIndex {};
	AABB                     m_AABB         {};
};