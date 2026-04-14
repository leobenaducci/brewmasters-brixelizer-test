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
		msg += "Result: " + std::string(what()) + " (0x" + ToHexString(m_Hr) + ")\n";
		msg += "Location: " + m_File + " [Line " + std::to_string(m_Line) + "]\n";
		
		// Error-specific hints
		if (m_Hr == E_INVALIDARG) {
			msg += "\n[POSSIBLE CAUSE]: An invalid parameter was passed during object creation.\n";
			msg += "Check:\n - RTV/DSV formats match the SwapChain.\n";
			msg += " - The Input Layout matches the Vertex Shader.\n";
			msg += " - The Root Signature is compatible with the Shader.\n";
		} else if (m_Hr == DXGI_ERROR_DEVICE_REMOVED || m_Hr == DXGI_ERROR_DEVICE_RESET) {
			msg += "\n[GPU CRASH]: The device was lost. Check the driver's internal error code.\n";
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