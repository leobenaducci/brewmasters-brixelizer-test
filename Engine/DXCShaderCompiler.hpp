#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <fstream>
#include <dxcapi.h>
#include <d3d12.h>
#include <stdexcept>
#include <process.h>

class DXCShaderCompiler {
public:
DXCShaderCompiler() {
        const char* vulkanSdk = getenv("VULKAN_SDK");
        if (!vulkanSdk) throw std::runtime_error("VULKAN_SDK not set");

        std::string path = std::string(vulkanSdk) + "/bin/dxc.exe";
        m_DxcPath = std::wstring(path.begin(), path.end());
    }

    void CompileToFile(
        const wchar_t* filename,
        const wchar_t* entrypoint,
        const wchar_t* profile,
        const std::wstring& outFile,
        const std::vector<std::wstring>& includeDirs
    ) {
        std::wstring args = L"-E main ";
        args += std::wstring(L"/D") + entrypoint + L"=main";
        args += L" -T ";
        args += profile;
        args += L" -HV 2021";
#if defined(_DEBUG)
        args += L" -Od -Zi -Gis";
#endif
        args += L" -Fo ";
        args += outFile;

        for (const auto& dir : includeDirs) {
            args += L" -I ";
            args += dir;
        }
        args += L" ";
        args += filename;

        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE hOutRead, hOutWrite;
        HANDLE hErrRead, hErrWrite;
        CreatePipe(&hOutRead, &hOutWrite, &sa, 0);
        CreatePipe(&hErrRead, &hErrWrite, &sa, 0);
        SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hErrRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = hOutWrite;
        si.hStdError = hErrWrite;

        PROCESS_INFORMATION pi = {};
        if (!CreateProcessW(m_DxcPath.c_str(), (wchar_t*)args.c_str(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(hOutRead);
            CloseHandle(hOutWrite);
            CloseHandle(hErrRead);
            CloseHandle(hErrWrite);
            throw std::runtime_error("Failed to create dxc.exe process");
        }

        CloseHandle(hOutWrite);
        CloseHandle(hErrWrite);

        char buf[4096];
        DWORD bytes;

        while (ReadFile(hOutRead, buf, sizeof(buf) - 1, &bytes, NULL) && bytes > 0) {
            buf[bytes] = 0;
            OutputDebugStringA(buf);
        }
        while (ReadFile(hErrRead, buf, sizeof(buf) - 1, &bytes, NULL) && bytes > 0) {
            buf[bytes] = 0;
            OutputDebugStringA(buf);
        }

        CloseHandle(hOutRead);
        CloseHandle(hErrRead);

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    Microsoft::WRL::ComPtr<ID3DBlob> Compile(
        const wchar_t* filename,
        const wchar_t* entrypoint,
        const wchar_t* profile,
        const std::vector<std::wstring>& includeDirs
    ) {
        wchar_t tempDir[MAX_PATH];
        GetTempPathW(MAX_PATH, tempDir);
        std::wstring outPath = std::wstring(tempDir) + L"shader_compile.cso";

        CompileToFile(filename, entrypoint, profile, outPath, includeDirs);
        return ReadFileBlob(outPath);
    }

    Microsoft::WRL::ComPtr<ID3DBlob> ReadFileBlob(const std::wstring& path) {
        WIN32_FILE_ATTRIBUTE_DATA attr;
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attr)) {
            throw std::runtime_error("Output file does not exist");
        }

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            throw std::runtime_error("Failed to read file");
        }

        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<char> buffer(size);
        file.read(buffer.data(), size);
        file.close();

        DeleteFileW(path.c_str());

        struct Blob : ID3DBlob {
            char* data = nullptr;
            SIZE_T sz = 0;

            virtual HRESULT QueryInterface(REFIID, void**) { return S_OK; }
            virtual ULONG AddRef() { return 1; }
            virtual ULONG Release() { delete[] data; delete this; return 0; }
            virtual LPVOID GetBufferPointer() { return data; }
            virtual SIZE_T GetBufferSize() { return sz; }
        };

        Blob* b = new Blob();
        b->data = new char[buffer.size()];
        b->sz = buffer.size();
        memcpy(b->data, buffer.data(), buffer.size());

        return Microsoft::WRL::ComPtr<ID3DBlob>(b);
    }

private:
    std::wstring m_DxcPath;
};