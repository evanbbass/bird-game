#include "Application.h"

_Use_decl_annotations_
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int nShowCmd)
{
	BirdGame::Application::Initialize(hInstance, nShowCmd);
	return BirdGame::Application::Instance().Run();
}
