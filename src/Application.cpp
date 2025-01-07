#include "Application.h"

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
	mInstance->mWindow.Initialize(L"Bird Game", 640, 480, hInstance, nCmdShow);
	// TODO initialize mRenderer
}

BirdGame::Application& BirdGame::Application::Instance()
{
	assert(mInstance != nullptr);
	return *mInstance;
}

int BirdGame::Application::Run()
{
	while (mWindow.ProcessMessages())
	{
		Update();
		Render();
	}
	return 0;
}

void BirdGame::Application::Update()
{
}

void BirdGame::Application::Render()
{
}
