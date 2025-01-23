#pragma once

#include <memory>

namespace BirdGame
{
	class Renderer;
	class Window;

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

		std::unique_ptr<Window> mWindow;
		std::unique_ptr<Renderer> mRenderer;

		static std::unique_ptr<Application> mInstance;
	};
}
