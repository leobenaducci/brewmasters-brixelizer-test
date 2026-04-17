#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <memory>

#include <Shader.hpp>
#include <Model.hpp>
#include <Core/Layer.hpp>
#include <Camera.hpp>
#include <Input/UserInput.hpp>
#include <Scene/SceneBuilder.hpp>
#include <Brixelizer/BrixelizerContext.hpp>

#include "../Game/PlayerController.hpp"

class AppLayer : public Core::Layer {
public:
    AppLayer();
    ~AppLayer();

    void OnUpdate(float deltaTime);
    void OnRender();
    void OnEvent(Core::Event& event);

private:
    std::unique_ptr<Shader> m_LightingShader;

    Camera           m_Camera;
    SceneBuilder     m_SceneBuilder;
    Input::UserInput m_Input;
    PlayerController m_PlayerController;
    Model            m_Model;
    std::unique_ptr<Brixelizer::BrixelizerContext> m_BrixelizerContext;
    std::vector<Brixelizer::MeshInstance> m_BrixelizerMeshInstances{};
};