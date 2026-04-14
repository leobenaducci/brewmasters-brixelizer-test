#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

ComPtr<ID3DBlob> CompileShader(
	std::wstring const& filename,
	std::string const& entrypoint,
	std::string const& profile)
{
	ComPtr<ID3DBlob> shaderBlob;
	ComPtr<ID3DBlob> errorBlob;

	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;

#if defined(_DEBUG)
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = D3DCompileFromFile(
		filename.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entrypoint.c_str(),
		profile.c_str(),
		flags,
		0,
		&shaderBlob,
		&errorBlob
	);

	if (FAILED(hr)) {
		if (errorBlob) {
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		}

		throw std::runtime_error("Shader compilation failed");
	}

	return shaderBlob;
}