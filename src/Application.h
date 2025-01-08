#pragma once

#include "Renderer.h"
#include "Window.h"

#include <memory>

namespace BirdGame
{
	class Application final
	{
	public:
		~Application();

		static void Initialize(HINSTANCE hInstance, int nCmdShow);
		static Application& Instance();

		int Run();

		// Windows message handlers
		void MouseDown(uint8_t param);
		void MouseUp(uint8_t param);

	private:
		Application();
		Application(const Application&) = delete; // Don't allow copy of App instance

		void Update();
		void Render();

		Window mWindow;
		Renderer mRenderer;

		static std::unique_ptr<Application> mInstance;
	};
}
