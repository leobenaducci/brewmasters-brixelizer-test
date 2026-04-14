#pragma once
#include <windows.h>
#include <imgui_impl_win32.h>
#include <string_view>
#include <stdexcept>

extern LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct WindowInfo {
    std::string Title {};
    uint32_t Width { 1280 };
    uint32_t Height { 720 };
    bool IsResizable { true };
    bool UseVsync { true };
};

class Window {
public:
    std::wstring ToWide(std::string_view str) {
        int size = MultiByteToWideChar(
            CP_UTF8, 0,
            str.data(),
            (int)str.size(),
            nullptr, 0
        );

        std::wstring result(size, 0);

        MultiByteToWideChar(
            CP_UTF8, 0,
            str.data(),
            (int)str.size(),
            result.data(),
            size
        );

        return result;
    }

    explicit Window(std::string_view title = "DX12 Engine", int width = 1280, int height = 720) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_CLASSDC;
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = GetModuleHandle(nullptr);
        wc.lpszClassName = L"DX12_Window";
        RegisterClassExW(&wc);

        std::wstring wtitle = ToWide(title);

        m_Handle = CreateWindowW(
            wc.lpszClassName,
            wtitle.c_str(),
            WS_OVERLAPPEDWINDOW,
            100, 100, width, height,
            nullptr, nullptr, wc.hInstance, nullptr
        );

        if (!m_Handle) {
            throw std::runtime_error("CreateWindowW failed");
        }

        ShowWindow(m_Handle, SW_SHOWDEFAULT);
    }

    explicit Window(WindowInfo const& info) : Window(info.Title, info.Width, info.Height) {}

    ~Window() {
        if (m_Handle) {
            DestroyWindow(m_Handle);
        }
    }

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&)                 = delete;
    Window& operator=(Window&&)      = delete;

    HWND GetWindowHandle() const noexcept { return m_Handle; }

private:
    HWND m_Handle = nullptr;
};