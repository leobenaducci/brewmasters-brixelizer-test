#include <Core/Window.hpp>
#include <Core/Application.hpp>
#include <Core/Event.hpp>
#include <Input/InputEvents.hpp>
#include <windowsx.h>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
        return true;
    }

    switch (msg) {
        case WM_KEYDOWN: {
            Core::KeyPressedEvent e((int)wParam, (lParam & 0x40000000) != 0);
            Core::Application::Get().RaiseEvent(e);
            break;
        }

        case WM_KEYUP: {
            Core::KeyReleasedEvent e((int)wParam);
            Core::Application::Get().RaiseEvent(e);
            break;
        }

        case WM_MOUSEMOVE: {
            float x = (float)GET_X_LPARAM(lParam);
            float y = (float)GET_Y_LPARAM(lParam);

            Core::MouseMovedEvent e(x, y);
            Core::Application::Get().RaiseEvent(e);
            break;
        }

        case WM_RBUTTONDOWN: {
            Core::MouseButtonPressedEvent e(VK_RBUTTON);
            Core::Application::Get().RaiseEvent(e);
            break;
        }

        case WM_RBUTTONUP: {
            Core::MouseButtonReleasedEvent e(VK_RBUTTON);
            Core::Application::Get().RaiseEvent(e);
            break;
        }

        case WM_CLOSE: {
            Core::Application::Get().Stop();
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}