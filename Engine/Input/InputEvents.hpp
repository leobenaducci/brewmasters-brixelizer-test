#pragma once
#include <Core/Event.hpp>
#include <string>
#include <format>

namespace Core {

class KeyEvent : public Event {
public:
    int GetKeycode() const noexcept { return m_KeyCode; }

protected:
    KeyEvent(int keyCode) : m_KeyCode(keyCode) {}

    int m_KeyCode{};
};

class KeyPressedEvent : public KeyEvent {
public:
    KeyPressedEvent(int keyCode, bool repeat)
        : KeyEvent(keyCode), m_IsRepeat(repeat) {}

    bool IsRepeat() const noexcept { return m_IsRepeat; }

    std::string ToString() const noexcept override {
        return std::format("KeyPressedEvent: {} (repeat={})", m_KeyCode, m_IsRepeat);
    }

    EVENT_CLASS_TYPE(KeyPressed)
    EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Keyboard)

private:
    bool m_IsRepeat{};
};

class KeyReleasedEvent : public KeyEvent {
public:
    KeyReleasedEvent(int keyCode)
        : KeyEvent(keyCode) {}

    std::string ToString() const noexcept override {
        return std::format("KeyReleasedEvent: {}", m_KeyCode);
    }

    EVENT_CLASS_TYPE(KeyReleased)
    EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Keyboard)
};

class MouseButtonEvent : public Event {
public:
    int GetMouseButton() const noexcept { return m_Button; }

protected:
    MouseButtonEvent(int button) : m_Button(button) {}

    int m_Button{};
};

class MouseButtonPressedEvent : public MouseButtonEvent {
public:
    MouseButtonPressedEvent(int button)
        : MouseButtonEvent(button) {}

    std::string ToString() const noexcept override {
        return std::format("MouseButtonPressedEvent: {}", m_Button);
    }

    EVENT_CLASS_TYPE(MouseButtonPressed)
    EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Mouse | EventCategory::MouseButton)
};

class MouseButtonReleasedEvent : public MouseButtonEvent {
public:
    MouseButtonReleasedEvent(int button)
        : MouseButtonEvent(button) {}

    std::string ToString() const noexcept override {
        return std::format("MouseButtonReleasedEvent: {}", m_Button);
    }

    EVENT_CLASS_TYPE(MouseButtonReleased)
    EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Mouse | EventCategory::MouseButton)
};

class MouseMovedEvent : public Event {
public:
    MouseMovedEvent(float x, float y)
        : m_MouseX(x), m_MouseY(y) {}

    float GetX() const noexcept { return m_MouseX; }
    float GetY() const noexcept { return m_MouseY; }

    std::string ToString() const noexcept override {
        return std::format("MouseMovedEvent: {}, {}", m_MouseX, m_MouseY);
    }

    EVENT_CLASS_TYPE(MouseMoved)
    EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Mouse)

private:
    float m_MouseX{};
    float m_MouseY{};
};

class MouseScrolledEvent : public Event {
public:
    MouseScrolledEvent(float xOffset, float yOffset)
        : m_XOffset(xOffset), m_YOffset(yOffset) {}

    float GetXOffset() const noexcept { return m_XOffset; }
    float GetYOffset() const noexcept { return m_YOffset; }

    std::string ToString() const noexcept override {
        return std::format("MouseScrolledEvent: {}, {}", m_XOffset, m_YOffset);
    }

    EVENT_CLASS_TYPE(MouseScrolled)
    EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Mouse)

private:
    float m_XOffset{};
    float m_YOffset{};
};

}