#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <vector>
#include <cassert>

#include <FidelityFX/host/ffx_brixelizer.h>
#include <FidelityFX/host/backends/dx12/ffx_dx12.h>
#include <FidelityFX/host/ffx_brixelizergi.h>

#include "BrixelizerUtils.hpp"
#include "GBuffer.hpp"

namespace Brixelizer {

    using Microsoft::WRL::ComPtr;

    // ---------------------------------------------------------------------------
    // Crea una blue noise texture 128x128 R8G8 en CPU y la sube a la GPU.
    // Brixelizer GI la necesita para el ray jitter.
    // ---------------------------------------------------------------------------
    static ComPtr<ID3D12Resource> CreateNoiseTexture(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmd,
        ComPtr<ID3D12Resource>& uploadBuffer   // keep alive until cmd executes
    ) {
        constexpr uint32_t N = 128;
        constexpr uint32_t pixelSize = 2; // R8G8

        // Simple LCG blue-noise approximation — reemplaza con una textura real si tienes
        std::vector<uint8_t> data(N * N * pixelSize);
        uint32_t state = 0x12345678u;
        for (auto& b : data) {
            state = state * 1664525u + 1013904223u;
            b = static_cast<uint8_t>(state >> 24);
        }

        // GPU texture
        D3D12_HEAP_PROPERTIES heap { .Type = D3D12_HEAP_TYPE_DEFAULT };
        D3D12_RESOURCE_DESC desc {
            .Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Width            = N,
            .Height           = N,
            .DepthOrArraySize = 1,
            .MipLevels        = 1,
            .Format           = DXGI_FORMAT_R8G8_UNORM,
            .SampleDesc       = { 1, 0 },
            .Flags            = D3D12_RESOURCE_FLAG_NONE,
        };
        ComPtr<ID3D12Resource> tex;
        device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tex));
        tex->SetName(L"BrixelizerGI_NoiseTexture");

        // Upload buffer
        uint64_t uploadSize = 0;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint {};
        device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &uploadSize);

        D3D12_HEAP_PROPERTIES uploadHeap { .Type = D3D12_HEAP_TYPE_UPLOAD };
        D3D12_RESOURCE_DESC   uploadDesc {
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Width     = uploadSize,
            .Height = 1, .DepthOrArraySize = 1, .MipLevels = 1,
            .SampleDesc = { 1, 0 },
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        };
        device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer));

        // Copy pixel data into upload buffer respecting row pitch
        void* mapped = nullptr;
        uploadBuffer->Map(0, nullptr, &mapped);
        uint8_t* dst = reinterpret_cast<uint8_t*>(mapped);
        for (uint32_t row = 0; row < N; ++row) {
            memcpy(dst + row * footprint.Footprint.RowPitch,
                data.data() + row * N * pixelSize,
                N * pixelSize);
        }
        uploadBuffer->Unmap(0, nullptr);

        // CopyTextureRegion
        D3D12_TEXTURE_COPY_LOCATION dstLoc { .pResource = tex.Get(),
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, .SubresourceIndex = 0 };
        D3D12_TEXTURE_COPY_LOCATION srcLoc { .pResource = uploadBuffer.Get(),
            .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, .PlacedFootprint = footprint };
        cmd->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            tex.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &barrier);

        return tex;
    }

    // ---------------------------------------------------------------------------
    class BrixelizerGIContext {
    public:
        // -----------------------------------------------------------------------
        void Init(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* uploadCmd,   // para subir noise texture
            FfxInterface backendInterface,          // compartido con BrixelizerContext
            uint32_t displayWidth,
            uint32_t displayHeight
        ) {
            m_DisplayWidth  = displayWidth;
            m_DisplayHeight = displayHeight;

            // Noise texture (upload en este cmd, ejecutarlo antes del primer frame)
            m_NoiseTexture = CreateNoiseTexture(device, uploadCmd, m_NoiseUploadBuffer);

            // Outputs: diffuseGI y specularGI (UAV + SRV)
            auto makeOutput = [&](const wchar_t* name) -> ComPtr<ID3D12Resource> {
                D3D12_HEAP_PROPERTIES heap { .Type = D3D12_HEAP_TYPE_DEFAULT };
                D3D12_RESOURCE_DESC desc {
                    .Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                    .Width            = displayWidth,
                    .Height           = displayHeight,
                    .DepthOrArraySize = 1,
                    .MipLevels        = 1,
                    .Format           = DXGI_FORMAT_R16G16B16A16_FLOAT,
                    .SampleDesc       = { 1, 0 },
                    .Flags            = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                };
                ComPtr<ID3D12Resource> res;
                device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&res));
                res->SetName(name);
                return res;
            };

            m_DiffuseGI  = makeOutput(L"BrixelizerGI_DiffuseGI");
            m_SpecularGI = makeOutput(L"BrixelizerGI_SpecularGI");

            // Prev lit output (historia de GI del frame anterior)
            {
                D3D12_HEAP_PROPERTIES heap { .Type = D3D12_HEAP_TYPE_DEFAULT };
                D3D12_RESOURCE_DESC desc {
                    .Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                    .Width            = displayWidth,
                    .Height           = displayHeight,
                    .DepthOrArraySize = 1,
                    .MipLevels        = 1,
                    .Format           = DXGI_FORMAT_R16G16B16A16_FLOAT,
                    .SampleDesc       = { 1, 0 },
                    .Flags            = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                };
                device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr,
                    IID_PPV_ARGS(&m_PrevLitOutput));
                m_PrevLitOutput->SetName(L"BrixelizerGI_PrevLitOutput");
            }

            // GI Context
            FfxBrixelizerGIContextDescription giDesc {};
            giDesc.flags              = static_cast<FfxBrixelizerGIFlags>(0);
            giDesc.internalResolution = FFX_BRIXELIZER_GI_INTERNAL_RESOLUTION_50_PERCENT;
            giDesc.displaySize        = { displayWidth, displayHeight };
            giDesc.backendInterface   = backendInterface;

            FfxErrorCode const err = ffxBrixelizerGIContextCreate(&m_GIContext, &giDesc);
            assert(err == FFX_OK);
        }

        // -----------------------------------------------------------------------
        void Destroy() {
            FfxErrorCode const err = ffxBrixelizerGIContextDestroy(&m_GIContext);
            assert(err == FFX_OK);
        }

        // -----------------------------------------------------------------------
        // Despacha Brixelizer GI. Llama después de BrixelizerContext::Update().
        //
        // Requisitos de estado al entrar:
        //   gbuffer.depth/normals → NON_PIXEL_SHADER_RESOURCE  (EndGBufferPass lo hace)
        //   sdfAtlas              → NON_PIXEL_SHADER_RESOURCE  (BrixelizerContext::Update lo deja así)
        //   brickAABBs/cascades   → UAV                        (FFX los deja en UAV)
        // -----------------------------------------------------------------------
        void Dispatch(
            ID3D12GraphicsCommandList* cmd,
            uint32_t frameIndex,
            // Matrices row-major (XMStoreFloat4x4 desde XMMATRIX)
            DirectX::XMFLOAT4X4 const& view,
            DirectX::XMFLOAT4X4 const& proj,
            DirectX::XMFLOAT4X4 const& prevView,
            DirectX::XMFLOAT4X4 const& prevProj,
            DirectX::XMFLOAT3 const& cameraPos,
            // G-Buffer
            GBuffer const& gbuffer,
            // Recursos de BrixelizerContext
            ID3D12Resource* sdfAtlas,
            ID3D12Resource* brickAABBs,
            ID3D12Resource* cascadeAABBTrees[FFX_BRIXELIZER_MAX_CASCADES],
            ID3D12Resource* cascadeBrickMaps[FFX_BRIXELIZER_MAX_CASCADES],
            FfxBrixelizerContext* brixelizerContext,
            uint32_t startCascade,
            uint32_t endCascade
        ) {
            // --- Barreras de entrada ---
            // outputs a UAV, prevLitOutput a SRV
            {
                D3D12_RESOURCE_BARRIER barriers[] = {
                    CD3DX12_RESOURCE_BARRIER::Transition(m_DiffuseGI.Get(),
                        m_DiffuseGIState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
                    CD3DX12_RESOURCE_BARRIER::Transition(m_SpecularGI.Get(),
                        m_SpecularGIState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
                };
                cmd->ResourceBarrier(_countof(barriers), barriers);
                m_DiffuseGIState  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                m_SpecularGIState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            }

            // --- Dispatch description ---
            FfxBrixelizerGIDispatchDescription desc {};

            memcpy(&desc.view,           &view,     sizeof(desc.view));
            memcpy(&desc.projection,     &proj,     sizeof(desc.projection));
            memcpy(&desc.prevView,       &prevView, sizeof(desc.prevView));
            memcpy(&desc.prevProjection, &prevProj, sizeof(desc.prevProjection));

            desc.cameraPosition[0] = cameraPos.x;
            desc.cameraPosition[1] = cameraPos.y;
            desc.cameraPosition[2] = cameraPos.z;

            desc.startCascade = startCascade;
            desc.endCascade   = endCascade;

            desc.rayPushoff          = 0.25f;
            desc.sdfSolveEps         = 0.5f;
            desc.specularRayPushoff  = 0.25f;
            desc.specularSDFSolveEps = 0.5f;
            desc.tMin                = 0.0f;
            desc.tMax                = 10000.0f;

            // Normals: packed [0,1] en R8G8B8A8 → unpack con * 2 - 1
            desc.normalsUnpackMul      = 2.0f;
            desc.normalsUnpackAdd      = -1.0f;
            desc.isRoughnessPerceptual = false;
            desc.roughnessChannel      = 0;
            desc.roughnessThreshold    = 0.9f;
            desc.environmentMapIntensity = 0.0f; // sin environment map
            desc.motionVectorScale     = { 1.0f, 1.0f };

            // G-Buffer inputs
            {
                FfxResourceDescription depthResDesc {
                    .type   = FFX_RESOURCE_TYPE_TEXTURE2D,
                    .format = FFX_SURFACE_FORMAT_R32_FLOAT,
                    .width  = gbuffer.width,
                    .height = gbuffer.height,
                    .usage  = FFX_RESOURCE_USAGE_READ_ONLY,
                };
                FfxResourceDescription normResDesc {
                    .type   = FFX_RESOURCE_TYPE_TEXTURE2D,
                    .format = FFX_SURFACE_FORMAT_R8G8B8A8_UNORM,
                    .width  = gbuffer.width,
                    .height = gbuffer.height,
                    .usage  = FFX_RESOURCE_USAGE_READ_ONLY,
                };
                FfxResourceDescription roughResDesc {
                    .type = FFX_RESOURCE_TYPE_TEXTURE2D,
                    .format = FFX_SURFACE_FORMAT_R8_UNORM,
                    .width = 1, .height = 1,
                    .usage = FFX_RESOURCE_USAGE_READ_ONLY,
                };
                FfxResourceDescription mvResDesc {
                    .type = FFX_RESOURCE_TYPE_TEXTURE2D,
                    .format = FFX_SURFACE_FORMAT_R16G16_FLOAT,
                    .width = 1, .height = 1,
                    .usage = FFX_RESOURCE_USAGE_READ_ONLY,
                };
                FfxResourceDescription noiseResDesc {
                    .type = FFX_RESOURCE_TYPE_TEXTURE2D,
                    .format = FFX_SURFACE_FORMAT_R8G8_UNORM,
                    .width = 128, .height = 128,
                    .usage = FFX_RESOURCE_USAGE_READ_ONLY,
                };

                desc.depth         = ffxGetResourceDX12(gbuffer.depth.Get(),          depthResDesc,  L"GI_Depth",         FFX_RESOURCE_STATE_COMPUTE_READ);
                desc.normal        = ffxGetResourceDX12(gbuffer.normals.Get(),        normResDesc,   L"GI_Normals",       FFX_RESOURCE_STATE_COMPUTE_READ);
                desc.roughness     = ffxGetResourceDX12(gbuffer.roughness.Get(),      roughResDesc,  L"GI_Roughness",     FFX_RESOURCE_STATE_COMPUTE_READ);
                desc.motionVectors = ffxGetResourceDX12(gbuffer.motionVectors.Get(),  mvResDesc,     L"GI_MotionVectors", FFX_RESOURCE_STATE_COMPUTE_READ);
                desc.noiseTexture  = ffxGetResourceDX12(m_NoiseTexture.Get(),         noiseResDesc,  L"GI_Noise",         FFX_RESOURCE_STATE_COMPUTE_READ);

                desc.historyDepth  = ffxGetResourceDX12(gbuffer.historyDepth.Get(),   depthResDesc,  L"GI_HistDepth",     FFX_RESOURCE_STATE_COMPUTE_READ);
                desc.historyNormal = ffxGetResourceDX12(gbuffer.historyNormals.Get(), normResDesc,   L"GI_HistNormals",   FFX_RESOURCE_STATE_COMPUTE_READ);
                desc.prevLitOutput = ffxGetResourceDX12(m_PrevLitOutput.Get(),
                    FfxResourceDescription { .type = FFX_RESOURCE_TYPE_TEXTURE2D,
                        .format = FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT,
                        .width = m_DisplayWidth, .height = m_DisplayHeight,
                        .usage = FFX_RESOURCE_USAGE_READ_ONLY },
                    L"GI_PrevLitOutput", FFX_RESOURCE_STATE_COMPUTE_READ);

                // Environment map: dummy (textura negra 1x1)
                desc.environmentMap = ffxGetResourceDX12(gbuffer.roughness.Get(), roughResDesc, L"GI_EnvMap", FFX_RESOURCE_STATE_COMPUTE_READ);
            }

            // Brixelizer resources (todos en COMPUTE_READ: FFX los deja en UAV pero
            // GI los lee como SRV — le indicamos el estado real actual)
            {
                FfxResourceDescription atlasDesc {
                    .type = FFX_RESOURCE_TYPE_TEXTURE3D, .format = FFX_SURFACE_FORMAT_R8_UNORM,
                    .width  = FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE,
                    .height = FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE,
                    .depth  = FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE,
                    .usage  = FFX_RESOURCE_USAGE_READ_ONLY,
                };
                desc.sdfAtlas   = ffxGetResourceDX12(sdfAtlas, atlasDesc, L"GI_SdfAtlas", FFX_RESOURCE_STATE_COMPUTE_READ);

                FfxResourceDescription brickDesc {
                    .type = FFX_RESOURCE_TYPE_BUFFER,
                    .size = FFX_BRIXELIZER_BRICK_AABBS_SIZE, .stride = FFX_BRIXELIZER_BRICK_AABBS_STRIDE,
                    .usage = FFX_RESOURCE_USAGE_READ_ONLY,
                };
                desc.bricksAABBs = ffxGetResourceDX12(brickAABBs, brickDesc, L"GI_BrickAABBs", FFX_RESOURCE_STATE_COMPUTE_READ);

                for (uint32_t i = 0; i < FFX_BRIXELIZER_MAX_CASCADES; ++i) {
                    FfxResourceDescription treeDesc {
                        .type = FFX_RESOURCE_TYPE_BUFFER,
                        .size = FFX_BRIXELIZER_CASCADE_AABB_TREE_SIZE, .stride = FFX_BRIXELIZER_CASCADE_AABB_TREE_STRIDE,
                        .usage = FFX_RESOURCE_USAGE_READ_ONLY,
                    };
                    FfxResourceDescription mapDesc {
                        .type = FFX_RESOURCE_TYPE_BUFFER,
                        .size = FFX_BRIXELIZER_CASCADE_BRICK_MAP_SIZE, .stride = FFX_BRIXELIZER_CASCADE_BRICK_MAP_STRIDE,
                        .usage = FFX_RESOURCE_USAGE_READ_ONLY,
                    };
                    desc.cascadeAABBTrees[i] = ffxGetResourceDX12(cascadeAABBTrees[i], treeDesc, L"GI_AABBTree", FFX_RESOURCE_STATE_COMPUTE_READ);
                    desc.cascadeBrickMaps[i] = ffxGetResourceDX12(cascadeBrickMaps[i], mapDesc,  L"GI_BrickMap", FFX_RESOURCE_STATE_COMPUTE_READ);
                }
            }

            // Outputs
            {
                FfxResourceDescription outDesc {
                    .type = FFX_RESOURCE_TYPE_TEXTURE2D, .format = FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT,
                    .width = m_DisplayWidth, .height = m_DisplayHeight,
                    .usage = FFX_RESOURCE_USAGE_UAV,
                };
                desc.outputDiffuseGI  = ffxGetResourceDX12(m_DiffuseGI.Get(),  outDesc, L"GI_OutDiffuse",  FFX_RESOURCE_STATE_UNORDERED_ACCESS);
                desc.outputSpecularGI = ffxGetResourceDX12(m_SpecularGI.Get(), outDesc, L"GI_OutSpecular", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
            }

            // Raw Brixelizer context pointer
            FfxBrixelizerRawContext* rawCtx = nullptr;
            ffxBrixelizerGetRawContext(brixelizerContext, &rawCtx);
            desc.brixelizerContext = rawCtx;

            FfxErrorCode const err = ffxBrixelizerGIContextDispatch(
                &m_GIContext, &desc, ffxGetCommandListDX12(cmd));
            assert(err == FFX_OK);

            // --- Barreras de salida ---
            // Outputs a SRV para que el lighting pass los lea
            // prevLitOutput: copiamos diffuseGI como nueva historia
            {
                D3D12_RESOURCE_BARRIER barriers[] = {
                    CD3DX12_RESOURCE_BARRIER::Transition(m_DiffuseGI.Get(),
                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                    CD3DX12_RESOURCE_BARRIER::Transition(m_SpecularGI.Get(),
                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                    CD3DX12_RESOURCE_BARRIER::Transition(m_PrevLitOutput.Get(),
                        m_PrevLitOutputState, D3D12_RESOURCE_STATE_COPY_DEST),
                    CD3DX12_RESOURCE_BARRIER::Transition(m_DiffuseGI.Get(),    // ya está en SRV → CopySrc
                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                        D3D12_RESOURCE_STATE_COPY_SOURCE),
                };
                // Separamos para evitar conflicto en DiffuseGI
                cmd->ResourceBarrier(3, barriers);
                m_DiffuseGIState  = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
                m_SpecularGIState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

                // Copy diffuseGI → prevLitOutput
                D3D12_RESOURCE_BARRIER toCopySrc = CD3DX12_RESOURCE_BARRIER::Transition(
                    m_DiffuseGI.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_COPY_SOURCE);
                cmd->ResourceBarrier(1, &toCopySrc);
                cmd->CopyResource(m_PrevLitOutput.Get(), m_DiffuseGI.Get());

                D3D12_RESOURCE_BARRIER fromCopy[] = {
                    CD3DX12_RESOURCE_BARRIER::Transition(m_DiffuseGI.Get(),
                        D3D12_RESOURCE_STATE_COPY_SOURCE,
                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                    CD3DX12_RESOURCE_BARRIER::Transition(m_PrevLitOutput.Get(),
                        D3D12_RESOURCE_STATE_COPY_DEST,
                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                };
                cmd->ResourceBarrier(_countof(fromCopy), fromCopy);
                m_DiffuseGIState     = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
                m_PrevLitOutputState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            }
        }

        // Accesores para usar los outputs en el lighting pass
        ID3D12Resource* GetDiffuseGI()  const { return m_DiffuseGI.Get();  }
        ID3D12Resource* GetSpecularGI() const { return m_SpecularGI.Get(); }
        D3D12_RESOURCE_STATES GetDiffuseGIState()  const { return m_DiffuseGIState;  }
        D3D12_RESOURCE_STATES GetSpecularGIState() const { return m_SpecularGIState; }

    private:
        FfxBrixelizerGIContext m_GIContext {};

        ComPtr<ID3D12Resource> m_DiffuseGI;
        ComPtr<ID3D12Resource> m_SpecularGI;
        ComPtr<ID3D12Resource> m_PrevLitOutput;
        ComPtr<ID3D12Resource> m_NoiseTexture;
        ComPtr<ID3D12Resource> m_NoiseUploadBuffer; // liberado después del primer execute

        D3D12_RESOURCE_STATES m_DiffuseGIState     = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        D3D12_RESOURCE_STATES m_SpecularGIState     = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        D3D12_RESOURCE_STATES m_PrevLitOutputState  = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

        uint32_t m_DisplayWidth  = 0;
        uint32_t m_DisplayHeight = 0;
    };

} // namespace Brixelizer