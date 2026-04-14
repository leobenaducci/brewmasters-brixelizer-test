#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <Plugins/DirectX/d3dx12.h>

class Texture {
public:
    static ID3D12Resource* CreateTexture(
        ID3D12Device* device, 
        ID3D12GraphicsCommandList* cmdList, 
        const std::string& path, 
        Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer
    );

    static void CreateSrv(
        ID3D12Device* device, 
        ID3D12DescriptorHeap* srvHeap, 
        ID3D12Resource* texRes, 
        int index
    );
};