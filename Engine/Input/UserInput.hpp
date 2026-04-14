#pragma once

#include <DirectXMath.h>
#include <Core/Event.hpp>
#include <Input/InputEvents.hpp>

namespace Input {

    class UserInput {
    public:

        void OnEvent(Core::Event& e) {
            Core::EventDispatcher dispatcher(e);

            dispatcher.Dispatch<Core::KeyPressedEvent>(
                [this](Core::KeyPressedEvent& event) {
                    int key = event.GetKeycode();
                    if (key >= 0 && key < 256)
                        m_Keys[key] = true;
                    return false;
                });

            dispatcher.Dispatch<Core::KeyReleasedEvent>(
                [this](Core::KeyReleasedEvent& event) {
                    int key = event.GetKeycode();
                    if (key >= 0 && key < 256)
                        m_Keys[key] = false;
                    return false;
                });

            dispatcher.Dispatch<Core::MouseButtonPressedEvent>(
                [this](Core::MouseButtonPressedEvent& event) {
                    int button = event.GetMouseButton();
                    if (button < 8)
                        m_MouseButtons[button] = true;
                    return false;
                });

            dispatcher.Dispatch<Core::MouseButtonReleasedEvent>(
                [this](Core::MouseButtonReleasedEvent& event) {
                    int button = event.GetMouseButton();
                    if (button < 8)
                        m_MouseButtons[button] = false;
                    return false;
                });

            dispatcher.Dispatch<Core::MouseMovedEvent>(
                [this](Core::MouseMovedEvent& event) {
                    float x = event.GetX();
                    float y = event.GetY();

                    m_MouseDelta.x = x - m_MousePosition.x;
                    m_MouseDelta.y = y - m_MousePosition.y;

                    m_MousePosition = { x, y };

                    return false;
                });
        }

        void EndFrame() {
            m_MouseDelta = {0.f, 0.f};
        }

    public:
        bool IsKeyHeld(int keycode) const noexcept {
            if (keycode < 0 || keycode >= 256) {
                return false;
            }

            return m_Keys[keycode];
        }

        bool IsMouseButtonHeld(int button) const noexcept {
            if (button < 0 || button >= 8) {
                return false;
            }

            return m_MouseButtons[button];
        }

        DirectX::XMFLOAT2 GetMouseDelta() const noexcept {
            return m_MouseDelta;
        }

        DirectX::XMFLOAT2 GetMousePosition() const noexcept {
            return m_MousePosition;
        }

    private:
        bool m_Keys[256] {};
        bool m_MouseButtons[8] {};

        DirectX::XMFLOAT2 m_MousePosition {};
        DirectX::XMFLOAT2 m_MouseDelta {};
    };

}