#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

ComPtr<ID3DBlob> CompileShader(const std::wstring& filename, const std::string& entrypoint, const std::string& profile) {
    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DCompileFromFile(filename.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entrypoint.c_str(), profile.c_str(), 0, 0, &shaderBlob, &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        return nullptr;
    }
    return shaderBlob;
}