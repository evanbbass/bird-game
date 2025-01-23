#include "pch.h"
#include "Application.h"

#include "Renderer.h"
#include "Window.h"

#include <assert.h>

std::unique_ptr<BirdGame::Application> BirdGame::Application::mInstance;

BirdGame::Application::Application()
{
}

BirdGame::Application::~Application()
{
}

void BirdGame::Application::Initialize(HINSTANCE hInstance, int nCmdShow)
{
	mInstance.reset(new Application());
	mInstance->mWindow.reset(new Window());
	mInstance->mWindow->Initialize(L"Bird Game", 960, 720, hInstance, nCmdShow);
	mInstance->mRenderer.reset(new Renderer());
	mInstance->mRenderer->Initialize(*mInstance->mWindow);
}

BirdGame::Application& BirdGame::Application::Instance()
{
	assert(mInstance != nullptr);
	return *mInstance;
}

int BirdGame::Application::Run()
{
	while (mWindow->ProcessMessages())
	{
		Update();
		Render();
	}
	return 0;
}

void BirdGame::Application::MouseDown(uint8_t /*param*/)
{
}

void BirdGame::Application::MouseUp(uint8_t /*param*/)
{
}

void BirdGame::Application::Update()
{
}

void BirdGame::Application::Render()
{
	mRenderer->Render();
}
