#pragma once
#include <windows.h>
#include <d3d12.h>
#include <comdef.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <wrl/client.h>

#define DX_THROW(hr) if (FAILED(hr)) { throw DXException(hr, __FILE__, __LINE__); }

class DXException : public std::runtime_error {
public:
    DXException(HRESULT hr, const char* file, int line) 
        : std::runtime_error(TranslateHResult(hr)), m_Hr(hr), m_File(file), m_Line(line) 
    {
        m_FullMsg = FormatMessage();
    }

    const std::string& GetFullMessage() const { return m_FullMsg; }
    HRESULT GetErrorCode() const { return m_Hr; }

private:
    static std::string TranslateHResult(HRESULT hr) {
        _com_error err(hr);
        const TCHAR* msg = err.ErrorMessage();
#ifdef UNICODE
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, msg, -1, NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, msg, -1, &strTo[0], size_needed, NULL, NULL);
        return strTo;
#else
        return std::string(msg);
#endif
    }

    std::string FormatMessage() const {
        std::string msg = "--- DIRECTX EXCEPTION ---\n";
        msg += "Resultado: " + std::string(what()) + " (0x" + ToHexString(m_Hr) + ")\n";
        msg += "Localización: " + m_File + " [Línea " + std::to_string(m_Line) + "]\n";
        
        // Información específica según el error
        if (m_Hr == E_INVALIDARG) {
            msg += "\n[POSIBLE CAUSA]: Un parámetro en la creación del objeto es inválido.\n";
            msg += "Comprueba:\n - Formatos de RTV/DSV coinciden con el SwapChain.\n";
            msg += " - El Input Layout coincide con el Vertex Shader.\n";
            msg += " - La Root Signature es compatible con el Shader.\n";
        } else if (m_Hr == DXGI_ERROR_DEVICE_REMOVED || m_Hr == DXGI_ERROR_DEVICE_RESET) {
            msg += "\n[GPU CRASH]: El dispositivo se ha perdido. Revisa el código de error interno del driver.\n";
        }

        msg += "--------------------------";
        return msg;
    }

    std::string ToHexString(HRESULT hr) const {
        char buf[16];
        sprintf_s(buf, "%08X", (unsigned int)hr);
        return std::string(buf);
    }

    HRESULT m_Hr;
    std::string m_File;
    int m_Line;
    std::string m_FullMsg;
};