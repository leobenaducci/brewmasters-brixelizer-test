#include "Layer.hpp"
#include <Core/Application.hpp>

namespace Core {
	void Layer::QueueTransition(std::unique_ptr<Layer> toLayer) {
		auto& app = Core::Application::Get();
		app.QueueLayerTransition(this, std::move(toLayer));
	}
}