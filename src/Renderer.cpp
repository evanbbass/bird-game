#include "pch.h"
#include "Renderer.h"


// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

namespace
{
	constexpr uint32_t kNumBufferFrames = 2;

	void CheckHResult(HRESULT result)
	{
		if (FAILED(result))
		{
			// TODO throw exception maybe?
			assert(false);
		}
	}
}

#pragma region RendererImpl

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
		ComPtr<ID3D12Device> mDevice;
		ComPtr<ID3D12CommandQueue> mCommandQueue;
		ComPtr<IDXGISwapChain3> mSwapChain;
		ComPtr<ID3D12CommandAllocator> mCommandAllocator;

		uint32_t mWidth;
		uint32_t mHeight;

		ComPtr<ID3D12DescriptorHeap> mRtvHeap;
		uint32_t mRtvDescriptorSize;

		ComPtr<ID3D12Resource> mRenderTargets[kNumBufferFrames];
		ComPtr<ID3D12GraphicsCommandList> mCommandList;

		//TexturedTriangleRenderer mTriangleRenderer;
		//TextRenderer mTextRenderer;

		uint32_t mFrameIndex;
		ComPtr<ID3D12Fence> mFence;
		HANDLE mFenceEvent;
		uint64_t mFenceValue;
	};
}

void BirdGame::RendererImpl::CreateDevice()
{
	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			// Enable additional debug layers.
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	// Factory for creating device
	ComPtr<IDXGIFactory4> factory;
	CheckHResult(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	// Get hardware adapter (nullptr for now??? DX sample has a helper method to grab this. Template leaves this null unless using a warp adapter)
	ComPtr<IDXGIAdapter1> adapter = nullptr;

	// Create the device
	CheckHResult(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice)));
}

void BirdGame::RendererImpl::CreateCommandQueue()
{
	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	// TODO replace the assert since this might fail at runtime
	CheckHResult(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));
}

void BirdGame::RendererImpl::CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height)
{
	mWidth = width;
	mHeight = height;

	// Create the swap chain
	{
		ComPtr<IDXGIFactory4> factory;
		CheckHResult(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

		// Describe and create the swap chain.
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
		swapChainDesc.BufferCount = kNumBufferFrames;
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

		ComPtr<IDXGISwapChain1> tempSwapChain;
		CheckHResult(factory->CreateSwapChainForHwnd(mCommandQueue.Get(), // Swap chain needs the queue so that it can force a flush on it.
			hwnd,
			&swapChainDesc,
			nullptr,
			nullptr,
			&tempSwapChain));

		CheckHResult(tempSwapChain->QueryInterface(IID_PPV_ARGS(&mSwapChain)));

		// This sample does not support fullscreen transitions.
		CheckHResult(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
	}

	// Grab the back buffer index
	mFrameIndex = mSwapChain->GetCurrentBackBufferIndex();

	// Create descriptor heaps
	{
		// Describe and create a render target view (RTV) descriptor heap
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = kNumBufferFrames;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		CheckHResult(mDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));

		mRtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// Create frame resources
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mRtvHeap->GetCPUDescriptorHandleForHeapStart();

		// Create a RTV for each frame
		for (UINT n = 0; n < kNumBufferFrames; n++)
		{
			CheckHResult(mSwapChain->GetBuffer(n, IID_PPV_ARGS(&mRenderTargets[n])));
			mDevice->CreateRenderTargetView(mRenderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += mRtvDescriptorSize;
		}
	}

	// Create the command allocator
	CheckHResult(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocator)));
}

#pragma endregion

// ------------------------------------------------------------------------------------------------
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
