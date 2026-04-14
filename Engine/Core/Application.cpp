#include "Application.hpp"
#include <ranges>

namespace Core {
	// Singleton.
	static Application* s_Application { nullptr };

	Application::Application(ApplicationInfo const& info)
		: m_Window(info.WindowInfo)
		, m_Graphics(m_Window.GetWindowHandle())
		, m_Imgui(m_Window.GetWindowHandle(), m_Graphics.GetDevice(), 2)
		, m_Info(info)
	{
		s_Application = this;
	}

	Application::~Application() {
		s_Application = nullptr;
	}

	void Application::Run() {
		m_IsRunning = true;

		MSG msg {};
		while (msg.message != WM_QUIT && m_IsRunning) {
			if (PeekMessage(&msg, nullptr, 0u, 0u, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			} else {
				DoFrame();
			}
		}
	}

	void Application::Stop() {
		m_IsRunning = false;
	}

	void Application::RaiseEvent(Event& event) {
		for (auto& layer : std::views::reverse(m_LayerStack)) {
			layer->OnEvent(event);
			if (event.Handled) break;
		}
	}

	void Application::QueueLayerTransition(Layer *from, std::unique_ptr<Layer> to) {
		m_PendingTransitions.emplace_back(from, std::move(to));
	}

	Application &Application::Get() {
		assert(s_Application);
		return *s_Application;
	}

	float Application::GetTime() {
		return 0.0f;
	}

	void Application::DoFrame() {
		float deltaTime = m_Timer.GetDeltaTime();

		for (auto const& layer : m_LayerStack) {
			layer->OnUpdate(deltaTime);
		}

		m_Graphics.BeginFrame();

		for (auto const& layer : m_LayerStack) {
			layer->OnRender();
		}

		m_Imgui.NewFrame();
		m_Imgui.Render(m_Graphics.GetCommandList());

		m_Graphics.Present();

		// Layers transitioning.
		for (auto& [from, to] : m_PendingTransitions) {
			for (auto& layer : m_LayerStack) {
				if (layer.get() == from) {
					layer = std::move(to);
					break;
				}
			}
		}

		m_PendingTransitions.clear();
	}
}