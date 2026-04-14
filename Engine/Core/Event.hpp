#pragma once
#include <functional>
#include <string>

namespace Core {
	enum class EventType {
		None = 0,
		WindowClose, WindowResize,
		KeyPressed, KeyReleased,
		MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
	};

#define BIT(x) (1 << x)

	enum class EventCategory : uint32_t {
		None                    = 0,
		Application             = BIT(0),
		Input                   = BIT(1),
		Keyboard                = BIT(2),
		Mouse                   = BIT(3),
		MouseButton             = BIT(4)
	};

	inline EventCategory operator|(EventCategory a, EventCategory b) {
		return static_cast<EventCategory>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	}

	inline EventCategory operator&(EventCategory a, EventCategory b) {
		return static_cast<EventCategory>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
	}

	inline EventCategory& operator|=(EventCategory& a, EventCategory b) {
		a = a | b;
		return a;
	}

#define EVENT_CLASS_TYPE(type) static EventType GetStaticType() noexcept { return EventType::type; } \
							   virtual EventType GetEventType() const noexcept override { return GetStaticType(); } \
							   virtual char const* GetName() const noexcept override { return #type; } \

#define EVENT_CLASS_CATEGORY(category) virtual EventCategory GetCategoryFlags() const noexcept override { return category; }

	class Event {
	public:
		bool Handled{ false };

		virtual ~Event() {}

		virtual EventType GetEventType() const noexcept = 0;
		virtual const char* GetName() const noexcept = 0;
		virtual EventCategory GetCategoryFlags() const noexcept = 0;

		virtual std::string ToString() const noexcept { return GetName(); }

		bool IsInCategory(EventCategory category) const noexcept {
			return static_cast<uint32_t>(GetCategoryFlags() & category) != 0;
		}
	};

	class EventDispatcher {
		template <typename T>
		using EventFunction = std::function<bool(T&)>;
	public:
		EventDispatcher(Event& event) : m_Event(event) {}

		template <typename T>
		bool Dispatch(EventFunction<T> function) {
			if (m_Event.GetEventType() == T::GetStaticType() && !m_Event.Handled) {
				m_Event.Handled = function(static_cast<T&>(m_Event));
				return true;
			}

			return false;
		}
	private:
		Event& m_Event;
	};
}