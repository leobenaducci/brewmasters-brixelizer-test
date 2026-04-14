#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <FidelityFX/host/ffx_brixelizer.h>

#include "FfxContext.hpp"
#include <Mesh.hpp>

namespace Brixelizer {

	struct MeshInstance {
		uint32_t ID {};
		uint32_t VertexBufferID {};
		uint32_t IndexBufferID {};
	};

	class BrixelizerContext {
	public:
		BrixelizerContext(ID3D12Device* device, ID3D12CommandQueue* cmdQueue);
		~BrixelizerContext();

		BrixelizerContext(const BrixelizerContext&)            = delete;
		BrixelizerContext& operator=(const BrixelizerContext&) = delete;

		MeshInstance SubmitMeshInstance(Mesh const& mesh);
		void UnloadMeshInstance(MeshInstance const& meshInstance);

		void Update(
			ID3D12Device* device, 
			uint32_t swapChainFrameIndex,
			DirectX::XMFLOAT3 cameraPosition,
			ID3D12GraphicsCommandList* cmdList
		);

		ID3D12Resource*  GetSdfAtlas()    const noexcept { return m_SdfAtlas.Get(); }
		ID3D12Resource*  GetBrickAABBs()  const noexcept { return m_BrickAABBs.Get(); }
		FfxInterface     GetFfxInterface()      noexcept { return m_FfxContext.GetFfxInterface(); }
		FfxBrixelizerContext* GetFfxBrixelizerContext() noexcept { return &m_BrixelizerContext; }

		ID3D12Resource** GetCascadeAABBTrees() { return nullptr; }
		ID3D12Resource** GetCascadeBrickMaps() { return nullptr; }

	public:
		static constexpr uint32_t MAX_CASCADES  { FFX_BRIXELIZER_MAX_CASCADES / 3 };

	private:
		void UnregisterBuffer(uint32_t bufferID);
		void DeleteMeshInstance(uint32_t meshInstanceId);

		static constexpr FfxSurfaceFormat VERTEX_FORMAT { FFX_SURFACE_FORMAT_R32G32B32_FLOAT };

		// FFX objects.
		FfxContext           m_FfxContext       {};
		FfxBrixelizerContext m_BrixelizerContext {};

		// GPU resources.
		Microsoft::WRL::ComPtr<ID3D12Resource> m_SdfAtlas   {};
		D3D12_RESOURCE_STATES m_SdfAtlasState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		Microsoft::WRL::ComPtr<ID3D12Resource> m_BrickAABBs {};

		struct CascadeResources {
			Microsoft::WRL::ComPtr<ID3D12Resource> aabbTree {};
			Microsoft::WRL::ComPtr<ID3D12Resource> brickMap {};
		};
		CascadeResources m_CascadeResources[FFX_BRIXELIZER_MAX_CASCADES] {};
		uint32_t         m_CascadeResourcesCount {};

		Microsoft::WRL::ComPtr<ID3D12Resource> m_ScratchBuffer    {};
		size_t                                 m_ScratchBufferSize {};
		D3D12_RESOURCE_STATES m_ScratchBufferState = D3D12_RESOURCE_STATE_COMMON;

		FfxBrixelizerUpdateDescription      m_UpdateDesc {};
		FfxBrixelizerBakedUpdateDescription m_BakedDesc  {};
		FfxBrixelizerStats                  m_Stats      {};

	};

}