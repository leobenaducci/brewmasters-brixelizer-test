#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <stdexcept>

#include "DXCShaderCompiler.hpp"

static DXCShaderCompiler& GetGlobalDXC() {
    static DXCShaderCompiler dxc{};
    return dxc;
}

Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
	std::wstring const& filename,
	std::string const& entrypoint,
	std::string const& profile
) {
    std::vector<std::wstring> includeDirs = {
        L"../Engine/Plugins/FidelityFX/include/FidelityFX/gpu/brixelizer",
        L"../Engine/Plugins/FidelityFX/include/FidelityFX/gpu",
        L"../Engine/Plugins/FidelityFX/include/FidelityFX",
    };

    std::wstring entryepointW(entrypoint.begin(), entrypoint.end());
    std::wstring profileW(profile.begin(), profile.end());

    return GetGlobalDXC().Compile(
        filename.c_str(),
        entryepointW.c_str(),
        profileW.c_str(),
        includeDirs
    );
}