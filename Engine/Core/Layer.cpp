#include "Layer.hpp"
#include <Core/Application.hpp>

namespace Core {
    void Layer::QueueTransition(std::unique_ptr<Layer> toLayer) {
        auto& layerStack = Core::Application::Get().GetLayerStack();

        for (auto& layer : layerStack) {
            if (layer.get() == this) {
                layer = std::move(toLayer);
                return;
            }
        }
    }
}