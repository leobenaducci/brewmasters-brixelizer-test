#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <FidelityFX/gpu/brixelizer/ffx_brixelizer_host_gpu_shared.h>

namespace Brixelizer {

    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateCommittedBuffer(
        ID3D12Device* device, 
        uint64_t size, 
        D3D12_RESOURCE_STATES initialState
    ) {
        D3D12_HEAP_PROPERTIES heap { .Type = D3D12_HEAP_TYPE_DEFAULT };
        D3D12_RESOURCE_DESC   desc {
            .Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Width            = size,
            .Height           = 1,
            .DepthOrArraySize = 1,
            .MipLevels        = 1,
            .SampleDesc       = { 1, 0 },
            .Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags            = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        };

        Microsoft::WRL::ComPtr<ID3D12Resource> resource {};
        device->CreateCommittedResource(
            &heap, 
            D3D12_HEAP_FLAG_NONE,
            &desc, 
            initialState, 
            nullptr, 
            IID_PPV_ARGS(&resource)
        );

        return resource;
    }

    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateSdfAtlas(ID3D12Device* device) {
        uint32_t const size = FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE;

        D3D12_HEAP_PROPERTIES heap { .Type = D3D12_HEAP_TYPE_DEFAULT };
        D3D12_RESOURCE_DESC   desc {
            .Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE3D,
            .Width            = size,
            .Height           = size,
            .DepthOrArraySize = size,
            .MipLevels        = 1,
            .Format           = DXGI_FORMAT_R8_UNORM,
            .SampleDesc       = { 1, 0 },
            .Flags            = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        };

        Microsoft::WRL::ComPtr<ID3D12Resource> resource {};
        device->CreateCommittedResource(
            &heap, 
            D3D12_HEAP_FLAG_NONE,
            &desc, 
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
            nullptr, 
            IID_PPV_ARGS(&resource)
        );

        return resource;
    }
}