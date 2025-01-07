#pragma once

#include "Window.h"

#include <memory>

namespace BirdGame
{
	class Application
	{
	public:
		~Application();

		static void Initialize(HINSTANCE hInstance, int nCmdShow);
		static Application& Instance();

		int Run();

	private:
		Application();
		Application(const Application&) = delete; // Don't allow copy of App instance

		void Update();
		void Render();

		Window mWindow;

		static std::unique_ptr<Application> mInstance;
	};
}
