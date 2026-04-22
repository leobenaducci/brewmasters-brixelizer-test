#include <fstream>
#include <utility>
#include <cstddef>
#include <vector>
#include <algorithm>
#include <wrl.h>
#include <Plugins\DirectX\d3dcommon.h>

class ShaderInclude : public ID3DInclude {
public:
    ShaderInclude(std::vector<std::string> searchDirs) : m_SearchDirs(std::move(searchDirs)) {}

    STDMETHOD(Open)(
        D3D_INCLUDE_TYPE,
        LPCSTR fileName,
        LPCVOID,
        LPCVOID* data,
        UINT* bytes
    ) override {
        for (auto const& dir : m_SearchDirs) {
            std::string const fullPath = dir + "/" + fileName;

            OutputDebugStringA((std::string("[Shader Compiler]: ") + (fullPath + "\n")).c_str());

            std::ifstream file(fullPath, std::ios::binary);
            if (file) {
                file.seekg(0, std::ios::end);
                size_t const size = file.tellg();
                file.seekg(0, std::ios::beg);

                char* buffer = new char[size];
                file.read(buffer, size);

                *data = buffer;
                *bytes = (UINT)size;
                
                return S_OK;
            }
        }

        OutputDebugStringA((std::string("[Shader Compiler]: Error finding:") + fileName + "\n").c_str());
        return E_FAIL;
    }

    STDMETHOD(Close)(LPCVOID data) override {
        delete[] reinterpret_cast<const char*>(data);
        return S_OK;
    }

private:
    std::vector<std::string> m_SearchDirs{};
};