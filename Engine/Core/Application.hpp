#pragma once
#include <Core/Window.hpp>
#include <Core/DX12Manager.hpp>
#include <Core/Timer.hpp>
#include <ImGuiManager.hpp>
#include <Core/Event.hpp>
#include "Layer.hpp"

namespace Core {
	struct ApplicationInfo {
		std::string Name { "Application" };
		WindowInfo WindowInfo {};
	};

	class Application {
	public:
		explicit Application(ApplicationInfo const& info);
		~Application();

		void Run();
		void Stop();
		void RaiseEvent(Event& event);

		template <typename TLayer>
		requires(std::is_base_of_v<Layer, TLayer>)
		void PushLayer() {
			m_LayerStack.push_back(std::make_unique<TLayer>());
		}

		template <typename TLayer>
		requires(std::is_base_of_v<Layer, TLayer>)
		TLayer* GetLayer() {
			for (auto const& layer : m_LayerStack) {
				if (auto* candidate = dynamic_cast<TLayer*>(layer.get())) {
					return candidate;
				}
			}

			return nullptr;
		}

		void QueueLayerTransition(Layer* from, std::unique_ptr<Layer> to);
		std::vector<std::unique_ptr<Layer>>& GetLayerStack() { return m_LayerStack; }

		DX12Manager&  GetDX()       noexcept { return m_Graphics; }
		ImGuiManager& GetImgui()    noexcept { return m_Imgui; }

		static Application& Get();
		static float GetTime();

	private:
		void DoFrame();

		Window       m_Window;
		DX12Manager  m_Graphics;
		ImGuiManager m_Imgui;
		Timer        m_Timer;

		// Layers.
		std::vector<std::pair<Layer*, std::unique_ptr<Layer>>> m_PendingTransitions;
		std::vector<std::unique_ptr<Layer>> m_LayerStack{};

		// General info.
		ApplicationInfo m_Info{};
		bool m_IsRunning { false };
	};
}