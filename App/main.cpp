#include <Core/Application.hpp>
#include <iostream>

#include "Layers/AppLayer.hpp"

int main() {
	Core::ApplicationInfo appInfo {
		.WindowInfo = {
			.Title = "DirectX12 Renderer",
			.Width = 1280,
			.Height = 720,
		}
	};

	try {
		Core::Application app(appInfo);
		
		app.PushLayer<AppLayer>();
		app.Run();
	} catch (DXException const& e) {
		std::cerr << "[CRITICAL ERROR] " << e.GetFullMessage() << std::endl;
		MessageBoxA(NULL, e.GetFullMessage().c_str(), "DirectX Error", MB_ICONERROR);
		return -1;
	} catch (std::exception const& e) {
		std::cerr << "[ERROR] " << e.what() << std::endl;
		return -1;
	}

	std::cout << "[Engine] Bye bye." << std::endl;
	return 0;
}