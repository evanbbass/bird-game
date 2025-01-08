#include "pch.h"
#include "Window.h"

#include "Application.h"

#include <assert.h>

const std::wstring BirdGame::Window::sWindowClassName = L"BirdGame";
const std::wstring BirdGame::Window::sWindowTitle = L"Bird Game";

BirdGame::Window::Window() :
	mHWND(NULL),
	mWidth(100),
	mHeight(100)
{
}

BirdGame::Window::~Window()
{
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
        //case WM_MBUTTONDOWN:
        //{
        //    return 0;
        //}
        //case WM_MBUTTONUP:
        //{
        //    return 0;
        //}
        default:
        {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
}

void BirdGame::Window::Initialize(const wchar_t* title, int windowWidth, int windowHeight, HINSTANCE hInstance, int nCmdShow)
{
    mWidth  = static_cast<uint32_t>(windowWidth);
    mHeight = static_cast<uint32_t>(windowHeight);

	// Create the window class
    WNDCLASSEX windowClass = { 0 };
	windowClass.cbSize = sizeof(windowClass);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	windowClass.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
	windowClass.lpszClassName = sWindowClassName.c_str();

	// Get the window's coordinates
	RECT windowRectangle = { 0, 0, windowWidth, windowHeight };
	AdjustWindowRect(&windowRectangle, WS_OVERLAPPEDWINDOW, FALSE);

    // Register the window class
    RegisterClassEx(&windowClass);

	// Get the coordinates to center the window
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	int centeredX = (screenWidth - windowWidth) / 2;
	int centeredY = (screenHeight - windowHeight) / 2;

	// Create the window
	mHWND = CreateWindow(sWindowClassName.c_str(),                      // name of the window class
                         sWindowTitle.c_str(),                          // title of the window
                         WS_OVERLAPPEDWINDOW,                           // window style
                         centeredX,                                     // x-position of the window
                         centeredY,                                     // y-position of the window
                         windowRectangle.right - windowRectangle.left,  // width of the window
                         windowRectangle.bottom - windowRectangle.top,  // height of the window
                         nullptr,                                       // no parent window
                         nullptr,                                       // no menus
                         hInstance,                                     // application handle
                         nullptr);                                      // no multiple windows

    assert(mHWND != NULL);

    ShowWindow(mHWND, nCmdShow);
}

void BirdGame::Window::Shutdown()
{
    DestroyWindow(mHWND);
    mHWND = NULL;
}

bool BirdGame::Window::ProcessMessages()
{
    bool running = true;
    MSG msg = { 0 };
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            running = false;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

	return running;
}
