#pragma once
#include <Core/Window.hpp>
#include <Core/DX12Manager.hpp>
#include <Core/Timer.hpp>
#include <ImGuiManager.hpp>
#include <Renderer/Renderer.hpp>
#include <Core/Event.hpp>
#include "Layer.hpp"

namespace Core {
    struct ApplicationInfo {
        std::string Name { "Application" };
        WindowInfo WindowInfo {};
    };

    class Application {
    public:
        Application(ApplicationInfo const& info);
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
            for (auto const& layer : m_LayerStack)
                if (auto* c = dynamic_cast<TLayer*>(layer.get())) return c;
            return nullptr;
        }

        std::vector<std::unique_ptr<Layer>>& GetLayerStack() { return m_LayerStack; }

        DX12Manager&  GetDX()       noexcept { return m_Graphics; }
        Renderer&     GetRenderer() noexcept { return m_Renderer; }
        ImGuiManager& GetImgui()    noexcept { return m_Imgui; }

        static Application& Get();
        static float GetTime();

    private:
        // Solo orquesta el frame, ya no renderiza escena
        void DoFrame();

        Window       m_Window;
        DX12Manager  m_Graphics;
        ImGuiManager m_Imgui;
        Timer        m_Timer;
        Renderer     m_Renderer;

        std::vector<std::unique_ptr<Layer>> m_LayerStack{};
        ApplicationInfo m_Info{};
        bool m_IsRunning { false };
    };
}