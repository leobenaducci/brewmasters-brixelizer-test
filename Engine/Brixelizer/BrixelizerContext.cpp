#include "BrixelizerContext.hpp"
#include "BrixelizerUtils.hpp"

#include <FidelityFX/host/ffx_brixelizer.h>

#define BRIX_LOG(msg) OutputDebugStringA("[Brixelizer] " msg "\n")
#define BRIX_LOGF(fmt, ...) do { char _buf[256]; sprintf_s(_buf, "[Brixelizer] " fmt "\n", __VA_ARGS__); OutputDebugStringA(_buf); } while(0)

namespace Brixelizer {

	BrixelizerContext::BrixelizerContext(ID3D12Device* const device, ID3D12CommandQueue* const cmdQueue) {
		BRIX_LOG("=== Constructor START ===");

		BRIX_LOG("FfxContext::Initialize...");
		if (!m_FfxContext.Initialize(device, cmdQueue)) {
			BRIX_LOG("FfxContext::Initialize FAILED");
			assert(false);
		}
		BRIX_LOG("FfxContext::Initialize OK");

		FfxBrixelizerContextDescription desc {};
		desc.sdfCenter[0] = 0.0f;
		desc.sdfCenter[1] = 0.0f;
		desc.sdfCenter[2] = 0.0f;
		desc.numCascades  = MAX_CASCADES;
		desc.flags        = static_cast<FfxBrixelizerContextFlags>(0);

		BRIX_LOGF("MAX_CASCADES = %u", MAX_CASCADES);

		uint32_t numCascadeResources {};
		for (uint32_t i {}; i < desc.numCascades; i++) {
			FfxBrixelizerCascadeDescription* const cascadeDesc = &desc.cascadeDescs[i];
			cascadeDesc->flags     = static_cast<FfxBrixelizerCascadeFlag>(FFX_BRIXELIZER_CASCADE_STATIC | FFX_BRIXELIZER_CASCADE_DYNAMIC);
			cascadeDesc->voxelSize = 0.2f * (1 << i);

			switch (cascadeDesc->flags & (FFX_BRIXELIZER_CASCADE_STATIC | FFX_BRIXELIZER_CASCADE_DYNAMIC)) {
				case FFX_BRIXELIZER_CASCADE_STATIC:
				case FFX_BRIXELIZER_CASCADE_DYNAMIC:
					numCascadeResources += 1;
					break;
				case (FFX_BRIXELIZER_CASCADE_STATIC | FFX_BRIXELIZER_CASCADE_DYNAMIC):
					numCascadeResources += 3;
					break;
				default:
					assert(false);
			}

			BRIX_LOGF("  cascade[%u] voxelSize=%.4f flags=%u", i, cascadeDesc->voxelSize, static_cast<uint32_t>(cascadeDesc->flags));
		}

		BRIX_LOGF("numCascadeResources = %u", numCascadeResources);
		BRIX_LOGF("FFX_BRIXELIZER_MAX_CASCADES = %u", FFX_BRIXELIZER_MAX_CASCADES);
		BRIX_LOGF("FFX_BRIXELIZER_CASCADE_AABB_TREE_SIZE = %u bytes (%.2f MB)", 
			FFX_BRIXELIZER_CASCADE_AABB_TREE_SIZE, FFX_BRIXELIZER_CASCADE_AABB_TREE_SIZE / (1024.0f * 1024.0f));
		BRIX_LOGF("FFX_BRIXELIZER_CASCADE_BRICK_MAP_SIZE = %u bytes (%.2f MB)",
			FFX_BRIXELIZER_CASCADE_BRICK_MAP_SIZE, FFX_BRIXELIZER_CASCADE_BRICK_MAP_SIZE / (1024.0f * 1024.0f));
		BRIX_LOGF("FFX_BRIXELIZER_BRICK_AABBS_SIZE       = %u bytes (%.2f MB)",
			FFX_BRIXELIZER_BRICK_AABBS_SIZE, FFX_BRIXELIZER_BRICK_AABBS_SIZE / (1024.0f * 1024.0f));
		BRIX_LOGF("FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE = %u (3D: %.2f MB)",
			FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE,
			static_cast<float>(FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE) *
			FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE *
			FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE / (1024.0f * 1024.0f));

		desc.backendInterface = m_FfxContext.GetFfxInterface();

		BRIX_LOG("ffxBrixelizerContextCreate...");
		FfxErrorCode const error = ffxBrixelizerContextCreate(&desc, &m_BrixelizerContext);
		BRIX_LOGF("ffxBrixelizerContextCreate result = %d (0=OK)", static_cast<int>(error));
		assert(error == FFX_OK);

		BRIX_LOG("CreateSdfAtlas...");
		m_SdfAtlas = CreateSdfAtlas(device);
		BRIX_LOGF("CreateSdfAtlas %s", m_SdfAtlas ? "OK" : "FAILED (null)");

		BRIX_LOG("CreateBrickAABBs...");
		m_BrickAABBs = CreateCommittedBuffer(device, FFX_BRIXELIZER_BRICK_AABBS_SIZE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		if (m_BrickAABBs) m_BrickAABBs->SetName(L"Brixelizer_BrickAABBs");
		BRIX_LOGF("CreateBrickAABBs %s", m_BrickAABBs ? "OK" : "FAILED (null)");

		m_CascadeResourcesCount = numCascadeResources;

		BRIX_LOGF("Creating cascade resources for all %u cascades...", FFX_BRIXELIZER_MAX_CASCADES);
		for (uint32_t i = 0; i < FFX_BRIXELIZER_MAX_CASCADES; ++i) {
			BRIX_LOGF("  cascade[%u] aabbTree...", i);
			m_CascadeResources[i].aabbTree = CreateCommittedBuffer(device, FFX_BRIXELIZER_CASCADE_AABB_TREE_SIZE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			if (m_CascadeResources[i].aabbTree) m_CascadeResources[i].aabbTree->SetName(L"Brixelizer_Cascade_AABBTree");
			
			BRIX_LOGF("  cascade[%u] brickMap...", i);
			m_CascadeResources[i].brickMap = CreateCommittedBuffer(device, FFX_BRIXELIZER_CASCADE_BRICK_MAP_SIZE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			if (m_CascadeResources[i].brickMap) m_CascadeResources[i].brickMap->SetName(L"Brixelizer_Cascade_BrickMap");
		}

		BRIX_LOG("=== Constructor END ===");
	}

	BrixelizerContext::~BrixelizerContext() {
		FfxErrorCode const error = ffxBrixelizerContextDestroy(&m_BrixelizerContext);
		assert(error == FFX_OK);
	}

	[[nodiscard]]
	MeshInstance BrixelizerContext::SubmitMeshInstance(Mesh const& mesh) {
		FfxBrixelizerInstanceDescription instanceDesc {};
		FfxBrixelizerInstanceID instanceID = FFX_BRIXELIZER_INVALID_ID;

		instanceDesc.maxCascade = MAX_CASCADES;
		AABB const& aabb = mesh.GetAABB();
		instanceDesc.aabb = { { aabb.min[0], aabb.min[1], aabb.min[2] },
							  { aabb.max[0], aabb.max[1], aabb.max[2] } };

		constexpr FfxFloat32x3x4 modelMatrix {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f
		};
		memcpy(instanceDesc.transform, modelMatrix, sizeof(FfxFloat32x3x4));

		uint32_t vertexBufferID = FFX_BRIXELIZER_INVALID_ID;
		FfxBrixelizerBufferDescription brixelizerVertexBufferDesc {};
		FfxResourceDescription const vertexBufferDesc {
			.type   = FFX_RESOURCE_TYPE_BUFFER,
			.format = VERTEX_FORMAT,
			.size   = mesh.GetVertexCount() * mesh.GetVertexStride(),
			.stride = mesh.GetVertexStride(),
			.usage  = FFX_RESOURCE_USAGE_READ_ONLY,
		};
		brixelizerVertexBufferDesc.buffer   = ffxGetResourceDX12(mesh.GetVertexBuffer(), vertexBufferDesc, L"Brix_Vertex_Buffer", FFX_RESOURCE_STATE_COMPUTE_READ);
		brixelizerVertexBufferDesc.outIndex = &vertexBufferID;

		uint32_t indexBufferID = FFX_BRIXELIZER_INVALID_ID;
		FfxBrixelizerBufferDescription brixelizerIndexBufferDesc {};
		FfxResourceDescription const indexBufferDesc {
			.type   = FFX_RESOURCE_TYPE_BUFFER,
			.format = FFX_SURFACE_FORMAT_UNKNOWN,
			.size   = mesh.GetIndexCount() * mesh.GetIndexStride(),
			.stride = mesh.GetIndexStride(),
			.usage  = FFX_RESOURCE_USAGE_READ_ONLY,
		};
		brixelizerIndexBufferDesc.buffer   = ffxGetResourceDX12(mesh.GetIndexBuffer(), indexBufferDesc, L"Brix_Index_Buffer", FFX_RESOURCE_STATE_COMPUTE_READ);
		brixelizerIndexBufferDesc.outIndex = &indexBufferID;

		FfxBrixelizerBufferDescription bufferDescs[2] = { brixelizerVertexBufferDesc, brixelizerIndexBufferDesc };
		FfxErrorCode const error = ffxBrixelizerRegisterBuffers(&m_BrixelizerContext, bufferDescs, 2u);
		assert(error == FFX_OK);

		instanceDesc.vertexBuffer       = vertexBufferID;
		instanceDesc.vertexStride       = mesh.GetVertexStride();
		instanceDesc.vertexBufferOffset = 0;
		instanceDesc.vertexCount        = mesh.GetVertexCount();
		instanceDesc.vertexFormat       = VERTEX_FORMAT;
		instanceDesc.indexFormat        = FFX_INDEX_TYPE_UINT32;
		instanceDesc.indexBuffer        = indexBufferID;
		instanceDesc.indexBufferOffset  = 0;
		instanceDesc.triangleCount      = mesh.GetTriangleCount();
		instanceDesc.flags              = FFX_BRIXELIZER_INSTANCE_FLAG_NONE;
		instanceDesc.outInstanceID      = &instanceID;

		FfxErrorCode const error2 = ffxBrixelizerCreateInstances(&m_BrixelizerContext, &instanceDesc, 1u);
		assert(error2 == FFX_OK);

		return MeshInstance { instanceID, vertexBufferID, indexBufferID };
	}

	void BrixelizerContext::UnloadMeshInstance(MeshInstance const& meshInstance) {
		DeleteMeshInstance(meshInstance.ID);
		UnregisterBuffer(meshInstance.VertexBufferID);
		UnregisterBuffer(meshInstance.IndexBufferID);
	}

	void BrixelizerContext::UnregisterBuffer(uint32_t const bufferID) {
		uint32_t const buffersIDs[] = { bufferID };
		FfxErrorCode const error = ffxBrixelizerUnregisterBuffers(&m_BrixelizerContext, buffersIDs, 1u);
		assert(error == FFX_OK);
	}

	void BrixelizerContext::DeleteMeshInstance(uint32_t const meshInstanceId) {
		FfxErrorCode const error = ffxBrixelizerDeleteInstances(&m_BrixelizerContext, &meshInstanceId, 1u);
		assert(error == FFX_OK);
	}

	void BrixelizerContext::Update(
		ID3D12Device* const device,
		uint32_t const swapChainFrameIndex,
		Camera const& camera,
		ID3D12GraphicsCommandList* const cmdList,
		ID3D12Resource* renderTarget
	) {
		m_Stats      = FfxBrixelizerStats{};
		m_UpdateDesc = {};
		m_BakedDesc  = {};
		size_t scratchSize {};

		DirectX::XMFLOAT3 cameraPosition = camera.GetPosition();
		m_UpdateDesc.frameIndex       = swapChainFrameIndex;
		m_UpdateDesc.sdfCenter[0]     = cameraPosition.x;
		m_UpdateDesc.sdfCenter[1]     = cameraPosition.y;
		m_UpdateDesc.sdfCenter[2]     = cameraPosition.z;
		m_UpdateDesc.maxReferences    = 32 * (1 << 20);
		m_UpdateDesc.triangleSwapSize = 300 * (1 << 20);
		m_UpdateDesc.maxBricksPerBake = 1 << 14;

		FfxResourceDescription const atlasDesc = {
			.type   = FFX_RESOURCE_TYPE_TEXTURE3D,
			.format = FFX_SURFACE_FORMAT_R8_UNORM,
			.width  = FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE,
			.height = FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE,
			.depth  = FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE,
			.usage  = FFX_RESOURCE_USAGE_UAV
		};
		m_UpdateDesc.resources.sdfAtlas = ffxGetResourceDX12(m_SdfAtlas.Get(), atlasDesc, L"Brix_Atlas", FFX_RESOURCE_STATE_UNORDERED_ACCESS);

		FfxResourceDescription const brickAABBsDesc = {
			.type   = FFX_RESOURCE_TYPE_BUFFER,
			.size   = FFX_BRIXELIZER_BRICK_AABBS_SIZE,
			.stride = FFX_BRIXELIZER_BRICK_AABBS_STRIDE,
			.usage  = FFX_RESOURCE_USAGE_UAV
		};
		m_UpdateDesc.resources.brickAABBs = ffxGetResourceDX12(m_BrickAABBs.Get(), brickAABBsDesc, L"Brix_BrickAABBs", FFX_RESOURCE_STATE_UNORDERED_ACCESS);

		for (uint32_t i {}; i < FFX_BRIXELIZER_MAX_CASCADES; ++i) {
			FfxResourceDescription const aabbTreeDesc = {
				.type   = FFX_RESOURCE_TYPE_BUFFER,
				.size   = FFX_BRIXELIZER_CASCADE_AABB_TREE_SIZE,
				.stride = FFX_BRIXELIZER_CASCADE_AABB_TREE_STRIDE,
				.usage  = FFX_RESOURCE_USAGE_UAV
			};
			m_UpdateDesc.resources.cascadeResources[i].aabbTree = ffxGetResourceDX12(m_CascadeResources[i].aabbTree.Get(), aabbTreeDesc, L"Brix_AABBTree", FFX_RESOURCE_STATE_UNORDERED_ACCESS);

			FfxResourceDescription const brickMapDesc = {
				.type   = FFX_RESOURCE_TYPE_BUFFER,
				.size   = FFX_BRIXELIZER_CASCADE_BRICK_MAP_SIZE,
				.stride = FFX_BRIXELIZER_CASCADE_BRICK_MAP_STRIDE,
				.usage  = FFX_RESOURCE_USAGE_UAV
			};
			m_UpdateDesc.resources.cascadeResources[i].brickMap = ffxGetResourceDX12(m_CascadeResources[i].brickMap.Get(), brickMapDesc, L"Brix_BrickMap", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		m_UpdateDesc.outScratchBufferSize = &scratchSize;
		m_UpdateDesc.outStats             = &m_Stats;

		FfxErrorCode const bakeErr = ffxBrixelizerBakeUpdate(&m_BrixelizerContext, &m_UpdateDesc, &m_BakedDesc);
		assert(bakeErr == FFX_OK);
		
		auto debugVisualizationDesc = BuildDebugVisualization(camera, cmdList, renderTarget);
		m_UpdateDesc.debugVisualizationDesc = &debugVisualizationDesc;
		m_UpdateDesc.populateDebugAABBsFlags = (FfxBrixelizerPopulateDebugAABBsFlags)(FFX_BRIXELIZER_POPULATE_AABBS_STATIC_INSTANCES);

		bool const scratchReallocated = (scratchSize > m_ScratchBufferSize);
		if (scratchReallocated) {
			BRIX_LOGF("Scratch realloc: %zu -> %zu bytes", m_ScratchBufferSize, scratchSize);
			m_ScratchBuffer      = CreateCommittedBuffer(device, scratchSize, D3D12_RESOURCE_STATE_COMMON);
			m_ScratchBufferSize  = scratchSize;
			m_ScratchBufferState = D3D12_RESOURCE_STATE_COMMON;

			if (m_ScratchBuffer) {
				m_ScratchBuffer->SetName(L"Brixelizer_Scratch");
			}
		}

		{
			std::vector<D3D12_RESOURCE_BARRIER> barriers;

			if (m_SdfAtlasState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
				barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
					m_SdfAtlas.Get(),
					m_SdfAtlasState,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
			}

			if (m_ScratchBufferState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
				barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
					m_ScratchBuffer.Get(),
					m_ScratchBufferState,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
			} else {
				barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(m_ScratchBuffer.Get()));
			}

			if (!barriers.empty()) {
				cmdList->ResourceBarrier(static_cast<uint32_t>(barriers.size()), barriers.data());
			}
		}

		FfxResourceDescription const scratchDesc = {
			.type   = FFX_RESOURCE_TYPE_BUFFER,
			.size   = static_cast<uint32_t>(m_ScratchBufferSize),
			.stride = 1,
			.usage  = FFX_RESOURCE_USAGE_UAV
		};

		FfxResource const scratch = ffxGetResourceDX12(m_ScratchBuffer.Get(), scratchDesc, L"Brix_Scratch_Ffx", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
		FfxErrorCode const updateErr = ffxBrixelizerUpdate(&m_BrixelizerContext, &m_BakedDesc, scratch, ffxGetCommandListDX12(cmdList));
		assert(updateErr == FFX_OK);

		{
			D3D12_RESOURCE_BARRIER outBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
				m_SdfAtlas.Get(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			cmdList->ResourceBarrier(1, &outBarrier);
		}

		m_SdfAtlasState      = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		m_ScratchBufferState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	FfxBrixelizerDebugVisualizationDescription BrixelizerContext::BuildDebugVisualization(
		Camera const& camera,
		ID3D12GraphicsCommandList* const cmdList,
		ID3D12Resource* renderTarget
	) noexcept {
		FfxBrixelizerDebugVisualizationDescription debugVisDesc{};
		
		debugVisDesc.debugState = FFX_BRIXELIZER_TRACE_DEBUG_MODE_DISTANCE; // SDF Debug.
		
		DirectX::XMMATRIX const& inverseView = camera.GetInverseViewMatrix();
		DirectX::XMMATRIX const& inverseProjection = camera.GetInverseProjectionMatrix();

		memcpy(&debugVisDesc.inverseViewMatrix, &inverseView, sizeof(debugVisDesc.inverseViewMatrix));
    	memcpy(&debugVisDesc.inverseProjectionMatrix, &inverseProjection, sizeof(debugVisDesc.inverseProjectionMatrix));
		
		debugVisDesc.tMin        = m_TMin;
		debugVisDesc.tMax        = m_TMax;
		debugVisDesc.sdfSolveEps = m_SdfSolveEps;
		
		debugVisDesc.startCascadeIndex = m_StartCascadeIdx; // Static instances.
    	debugVisDesc.endCascadeIndex   = m_EndCascadeIdx;

		debugVisDesc.renderWidth  = 1280; // DEBUG
    	debugVisDesc.renderHeight = 720;

		debugVisDesc.commandList = ffxGetCommandListDX12(cmdList);
		debugVisDesc.cascadeDebugAABB[2 * FFX_BRIXELIZER_MAX_CASCADES + (FFX_BRIXELIZER_MAX_CASCADES - 1)] = FFX_BRIXELIZER_CASCADE_DEBUG_AABB_AABB_TREE;

		FfxResourceDescription renderTargetDesc = ffxGetResourceDescriptionDX12(renderTarget, FFX_RESOURCE_USAGE_UAV);

		debugVisDesc.output = ffxGetResourceDX12(
			renderTarget, 
			renderTargetDesc,
			L"Brixelizer Debug Visualization Output", 
			FFX_RESOURCE_STATE_UNORDERED_ACCESS
		);

		return debugVisDesc;
	}

} // namespace Brixelizer