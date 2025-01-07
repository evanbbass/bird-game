#pragma once

#include <cstdint>
#include <Windows.h>

namespace BirdGame
{
	class Window
	{
	public:
		Window();
		~Window();

		void Initialize(const wchar_t* title, int windowWidth, int windowHeight, HINSTANCE hInstance, int nCmdShow);

		void Shutdown();
		bool ProcessMessages();

		HWND GetHandle() const { return mHWND; }
		uint32_t GetWidth() const { return mWidth; }
		uint32_t GetHeight() const { return mHeight; }

	private:
		Window(const Window&) = delete;

		HWND mHWND;
		uint32_t mWidth;
		uint32_t mHeight;
	};
}
