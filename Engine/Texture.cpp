#include <Renderer/Util/stb_image.h>
#include <Renderer/Util/stb_image_resize2.h>

#include <cmath>
#include <vector>
#include <iostream>
#include <filesystem>
#include <algorithm>

#include "Texture.hpp"

namespace fs = std::filesystem;

static UINT16 CalculateMipLevels(UINT width, UINT height) {
    return static_cast<UINT16>(std::floor(std::log2(std::max(width, height)))) + 1;
}

ID3D12Resource* Texture::CreateTexture(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const std::string& path,
    Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer
) {
    // --- Corrección de ruta (igual que antes) ---
    fs::path fullPath(path);
    std::string pathStr = fullPath.string();

    size_t objPos = pathStr.find(".obj/");
    if (objPos == std::string::npos) objPos = pathStr.find(".fbx/");
    if (objPos != std::string::npos) {
        fs::path modelFile = pathStr.substr(0, objPos + 4);
        fs::path texturePart = pathStr.substr(objPos + 5);
        fullPath = modelFile.parent_path() / texturePart;
    }
    fullPath = fullPath.make_preferred();
    std::string finalPath = fullPath.string();

    // --- Carga de imagen base ---
    int width, height, channels;
    stbi_uc* baseData = stbi_load(finalPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!baseData) {
        std::cerr << "[Texture] Error cargando: " << finalPath << "\n";
        return nullptr;
    }

    // --- Calcular mip levels ---
    const UINT16 mipLevels = CalculateMipLevels(width, height);

    // --- Generar todos los mips en CPU ---
    // Cada elemento: {datos, ancho, alto}
    struct MipData { std::vector<stbi_uc> pixels; UINT width, height; };
    std::vector<MipData> mips(mipLevels);

    // Mip 0: imagen original
    mips[0].width = width;
    mips[0].height = height;
    mips[0].pixels.assign(baseData, baseData + width * height * 4);
    stbi_image_free(baseData);

    // Mips 1..N: downscale progresivo
    for (UINT16 i = 1; i < mipLevels; ++i) {
        UINT srcW = mips[i - 1].width;
        UINT srcH = mips[i - 1].height;
        UINT dstW = std::max(1u, srcW / 2);
        UINT dstH = std::max(1u, srcH / 2);

        mips[i].width = dstW;
        mips[i].height = dstH;
        mips[i].pixels.resize(dstW * dstH * 4);

        // stbir_resize_uint8_linear: filtro bilineal, correcto para texturas lineales.
        // Usa stbir_resize_uint8_srgb si tu textura es sRGB (albedo/diffuse).
        stbir_resize_uint8_linear(
            mips[i - 1].pixels.data(), srcW, srcH, srcW * 4,
            mips[i].pixels.data(),     dstW, dstH, dstW * 4,
            STBIR_RGBA
        );
    }

    // --- Crear textura en Default Heap con todos los mips ---
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.MipLevels          = mipLevels;        // ← antes era 1
    texDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.Width              = width;
    texDesc.Height             = height;
    texDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;
    texDesc.DepthOrArraySize   = 1;
    texDesc.SampleDesc.Count   = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    ID3D12Resource* textureResource = nullptr;
    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);

    HRESULT hr = device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&textureResource)
    );
    if (FAILED(hr)) return nullptr;

    // SetName
    int sz = MultiByteToWideChar(CP_UTF8, 0, finalPath.c_str(), (int)finalPath.size(), NULL, 0);
    std::wstring wpath(sz, 0);
    MultiByteToWideChar(CP_UTF8, 0, finalPath.c_str(), (int)finalPath.size(), &wpath[0], sz);
    textureResource->SetName(wpath.c_str());

    // --- Upload buffer que cubre TODOS los subrecursos ---
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(textureResource, 0, mipLevels);
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer)
    );

    // --- Preparar subresource data para cada mip ---
    std::vector<D3D12_SUBRESOURCE_DATA> subresources(mipLevels);
    for (UINT16 i = 0; i < mipLevels; ++i) {
        subresources[i].pData      = mips[i].pixels.data();
        subresources[i].RowPitch   = mips[i].width * 4;
        subresources[i].SlicePitch = mips[i].width * mips[i].height * 4;
    }

    // Copia todos los mips de una vez
    UpdateSubresources(cmdList, textureResource, uploadBuffer.Get(),
                       0, 0, mipLevels, subresources.data());

    // --- Barrera de transición ---
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        textureResource,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
    cmdList->ResourceBarrier(1, &barrier);

    return textureResource;
}

// --- SRV actualizado para exponer todos los mips al shader ---
void Texture::CreateSrv(
    ID3D12Device* device,
    ID3D12DescriptorHeap* srvHeap,
    ID3D12Resource* texRes,
    int index
) {
    if (!texRes) {
        return;
    }

    auto desc = texRes->GetDesc();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format                          = desc.Format;
    srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels             = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip       = 0;
    srvDesc.Texture2D.ResourceMinLODClamp   = 0.0f;

    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(srvHeap->GetCPUDescriptorHandleForHeapStart());
    hDescriptor.Offset(index, descriptorSize);

    device->CreateShaderResourceView(texRes, &srvDesc, hDescriptor);
}