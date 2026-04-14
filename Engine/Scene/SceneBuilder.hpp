#pragma once
#include <Camera.hpp>
#include <Renderer/Renderer.hpp> // SceneConstants.

namespace AppConfig {
    constexpr float FOV_DEG    = 45.0f;
    constexpr float NEAR_PLANE = 0.1f;
    constexpr float FAR_PLANE  = 5000.0f;
    constexpr float ASPECT     = 1280.0f / 720.0f;
    constexpr float MODEL_SCALE = 0.01f;
}

class SceneBuilder {
public:
    explicit SceneBuilder(Camera& cam) : m_Camera(cam) {}

    SceneConstants Build() const {
        using namespace DirectX;

        SceneConstants scene{};

        XMStoreFloat4x4(&scene.model, XMMatrixScaling(
            AppConfig::MODEL_SCALE, AppConfig::MODEL_SCALE, AppConfig::MODEL_SCALE
        ));

        XMStoreFloat4x4(&scene.view, m_Camera.GetViewMatrix());
        XMStoreFloat4x4(&scene.proj, XMMatrixPerspectiveFovLH(
            XMConvertToRadians(AppConfig::FOV_DEG),
            AppConfig::ASPECT,
            AppConfig::NEAR_PLANE,
            AppConfig::FAR_PLANE));

        scene.lightPos  = m_Camera.GetPosition();
        scene.cameraPos = m_Camera.GetPosition();
        return scene;
    }

private:
    Camera& m_Camera;
};