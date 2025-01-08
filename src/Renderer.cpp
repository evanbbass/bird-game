#include "pch.h"
#include "Renderer.h"

namespace BirdGame
{
	class RendererImpl
	{
	public:
		RendererImpl() = default;
		~RendererImpl() = default;

		void CreateDevice();
		void CreateCommandQueue();
		void CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height);
		void CreateCommandList();
		void CreateFence();

		void InitializeTriangleRenderer();

		void PopulateCommandListAndSubmit();
		void Present();
		void WaitForPreviousFram();

		void AddDebugText(std::string_view text, int32_t x, int32_t y);

	private:
		ID3D12Device* mDevice;
		ID3D12CommandQueue* mCommandQueue;
		//IDXGISwapChain3* mSwapChain;
		ID3D12CommandAllocator* mCommandAllocator;

		uint32_t mWidth;
		uint32_t mHeight;
	};
}

BirdGame::Renderer::Renderer()
{
}

BirdGame::Renderer::~Renderer()
{
}

void BirdGame::Renderer::Initialize(Window& window)
{
}

void BirdGame::Renderer::Shutdown()
{
}

void BirdGame::Renderer::Render()
{
}

void BirdGame::Renderer::AddDebugText(std::string_view text, int32_t x, int32_t y)
{
}
