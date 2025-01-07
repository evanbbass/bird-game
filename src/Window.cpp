#include "Window.h"

#include <assert.h>

BirdGame::Window::Window() :
	mHWND(NULL),
	mWidth(640),
	mHeight(480)
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
        case WM_MBUTTONDOWN:
        {
            return 0;
        }
        case WM_MBUTTONUP:
        {
            return 0;
        }
        default:
        {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
}

void BirdGame::Window::Initialize(const wchar_t* title, int windowWidth, int windowHeight, HINSTANCE hInstance, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"BirdGame";

    mWidth  = static_cast<uint32_t>(windowWidth);
    mHeight = static_cast<uint32_t>(windowHeight);

    // Register the window class
    WNDCLASS windowClass = { 0 };
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = CLASS_NAME;
    RegisterClass(&windowClass);

    // Create the window
    mHWND = CreateWindowEx(0,
        CLASS_NAME,
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth, windowHeight,
        NULL,
        NULL,
        hInstance,
        NULL);

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
