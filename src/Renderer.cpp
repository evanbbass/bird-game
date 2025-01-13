#include "pch.h"
#include "Renderer.h"

#include "Window.h"

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
		RendererImpl();
		~RendererImpl();

		// Initialization methods
		void CreateDevice();
		void CreateCommandQueue();
		void CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height);
		void LoadAssets();

		// Render methods
		void PopulateCommandList();
		void ExecuteCommandList();
		void Present();

		// TODO apparently this is bad, look into the frame buffer DX sample project
		void WaitForPreviousFrame();

	private:
		void CreateCommandList();
		void LoadShaders();
		void CreateFence();

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

		ComPtr<ID3D12PipelineState> mPipelineState;

		uint32_t mFrameIndex;
		ComPtr<ID3D12Fence> mFence;
		HANDLE mFenceEvent;
		uint64_t mFenceValue;

		friend class Renderer;
	};
}

BirdGame::RendererImpl::RendererImpl() :
	mWidth(0), mHeight(0),
	mRtvDescriptorSize(0),
	mFrameIndex(0),
	mFenceEvent(NULL),
	mFenceValue(0)
{
}

BirdGame::RendererImpl::~RendererImpl()
{
	// TODO should we do this?
	// Shutdown();
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

void BirdGame::RendererImpl::LoadAssets()
{
	CreateCommandList();
	LoadShaders();
	CreateFence();
}

void BirdGame::RendererImpl::PopulateCommandList()
{
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	CheckHResult(mCommandAllocator->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	CheckHResult(mCommandList->Reset(mCommandAllocator.Get(), mPipelineState.Get()));

	// Indicate that the back buffer will be used as a render target.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargets[mFrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mFrameIndex, mRtvDescriptorSize);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	mCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// Indicate that the back buffer will now be used to present.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargets[mFrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	CheckHResult(mCommandList->Close());
}

void BirdGame::RendererImpl::ExecuteCommandList()
{
	ID3D12CommandList* ppCommandLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(1, ppCommandLists);
}

void BirdGame::RendererImpl::Present()
{
	// Present the frame.
	CheckHResult(mSwapChain->Present(1, 0));
}

void BirdGame::RendererImpl::WaitForPreviousFrame()
{
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
	// sample illustrates how to use fences for efficient resource usage and to
	// maximize GPU utilization.

	// Signal and increment the fence value.
	const UINT64 fence = mFenceValue;
	CheckHResult(mCommandQueue->Signal(mFence.Get(), fence));
	mFenceValue++;

	// Wait until the previous frame is finished.
	if (mFence->GetCompletedValue() < fence)
	{
		CheckHResult(mFence->SetEventOnCompletion(fence, mFenceEvent));
		WaitForSingleObject(mFenceEvent, INFINITE);
	}

	mFrameIndex = mSwapChain->GetCurrentBackBufferIndex();
}

void BirdGame::RendererImpl::CreateCommandList()
{
	// Create the command list.
	CheckHResult(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&mCommandList)));

	// Command lists are created in the recording state, but there is nothing
	// to record yet. The main loop expects it to be closed, so close it now.
	CheckHResult(mCommandList->Close());
}

void BirdGame::RendererImpl::LoadShaders()
{
	// TODO
}

// A fence is a synchronization primitive that we can use to signal that the GPU is done rendering a frame
void BirdGame::RendererImpl::CreateFence()
{
	CheckHResult(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
	mFenceValue = 1;

	// Create an event handle to use for frame synchronization.
	mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (mFenceEvent == nullptr)
	{
		CheckHResult(HRESULT_FROM_WIN32(GetLastError()));
	}
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
	mImpl = std::make_unique<RendererImpl>();
	mImpl->CreateDevice();
	mImpl->CreateCommandQueue();
	mImpl->CreateSwapChain(window.GetHandle(), window.GetWidth(), window.GetHeight());
	mImpl->LoadAssets();
	//mImpl->LoadShader(); // TODO load shader in LoadAssets()

	// Wait for all the setup work we just did to complete because we are going to re-use the command list
	mImpl->WaitForPreviousFrame();
}

void BirdGame::Renderer::Shutdown()
{
	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	mImpl->WaitForPreviousFrame();

	CloseHandle(mImpl->mFenceEvent);
}

void BirdGame::Renderer::Render()
{
	mImpl->PopulateCommandList();
	mImpl->ExecuteCommandList();
	mImpl->Present();
	mImpl->WaitForPreviousFrame();
}
