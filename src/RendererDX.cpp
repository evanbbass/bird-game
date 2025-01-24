#include "pch.h"
#include "RendererDX.h"

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
	constexpr uint32_t kTextureWidth = 256;
	constexpr uint32_t kTextureHeight = 256;
	constexpr uint32_t kTexturePixelSize = 4;    // The number of bytes used to represent a pixel in the texture.

	void CheckHResult(HRESULT result)
	{
		if (FAILED(result))
		{
			throw std::exception("borked");
		}
	}

	// Generate a simple black and white checkerboard texture.
	std::vector<uint8_t> GenerateTextureData()
	{
		const uint32_t rowPitch = kTextureWidth * kTexturePixelSize;
		const uint32_t cellPitch = rowPitch >> 3;        // The width of a cell in the checkboard texture.
		const uint32_t cellHeight = kTextureWidth >> 3;    // The height of a cell in the checkerboard texture.
		const uint32_t textureSize = rowPitch * kTextureHeight;

		std::vector<uint8_t> data(textureSize);
		uint8_t* pData = &data[0];

		for (uint32_t n = 0; n < textureSize; n += kTexturePixelSize)
		{
			uint32_t x = n % rowPitch;
			uint32_t y = n / rowPitch;
			uint32_t i = x / cellPitch;
			uint32_t j = y / cellHeight;

			if (i % 2 == j % 2)
			{
				// Black square if row and column are both even or both odd
				pData[n] = 0x00;        // R
				pData[n + 1] = 0x00;    // G
				pData[n + 2] = 0x00;    // B
				pData[n + 3] = 0xff;    // A
			}
			else
			{
				// White square if row and column are different odd/even states
				pData[n] = 0xff;        // R
				pData[n + 1] = 0xff;    // G
				pData[n + 2] = 0xff;    // B
				pData[n + 3] = 0xff;    // A
			}
		}

		return data;
	}
}

#pragma region RendererImpl

namespace BirdGame
{
	class RendererImpl final
	{
	private:
		struct Vertex
		{
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT2 uv;
		};

	public:
		RendererImpl();
		~RendererImpl();

		// Initialization methods
		void LoadPipeline(HWND hwnd, uint32_t width, uint32_t height);
		void LoadAssets();

		// Render methods
		void PopulateCommandList();
		void CloseAndExecuteCommandList();
		void Present();

		// TODO apparently this is bad, look into the frame buffer DX sample project
		void WaitForPreviousFrame();

		void Destroy();

	private:
		void Initialize(uint32_t width, uint32_t height);
		void CreateDevice();
		void CreateCommandQueue();
		void CreateSwapChain(HWND hwnd);

		void LoadShaders();
		void CreateCommandList();
		void CreateVertexBuffer();
		void CreateTexture();
		void CreateFence();

		CD3DX12_VIEWPORT mViewport;
		CD3DX12_RECT mScissorRect;

		ComPtr<ID3D12Device> mDevice;
		ComPtr<ID3D12CommandQueue> mCommandQueue;
		ComPtr<IDXGISwapChain3> mSwapChain;
		ComPtr<ID3D12CommandAllocator> mCommandAllocator;

		ComPtr<ID3D12RootSignature> mRootSignature;

		ComPtr<ID3D12DescriptorHeap> mRtvHeap;
		ComPtr<ID3D12DescriptorHeap> mSrvHeap;
		uint32_t mRtvDescriptorSize;

		ComPtr<ID3D12Resource> mRenderTargets[kNumBufferFrames];
		ComPtr<ID3D12GraphicsCommandList> mCommandList;

		ComPtr<ID3D12PipelineState> mPipelineState;

		// App resources.
		ComPtr<ID3D12Resource> mVertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
		ComPtr<ID3D12Resource> mTexture;

		uint32_t mFrameIndex;
		ComPtr<ID3D12Fence> mFence;
		HANDLE mFenceEvent;
		uint64_t mFenceValue;
	};
}

BirdGame::RendererImpl::RendererImpl() :
	mRtvDescriptorSize(0),
	mVertexBufferView(),
	mFrameIndex(0),
	mFenceEvent(NULL),
	mFenceValue(0)
{
}

BirdGame::RendererImpl::~RendererImpl()
{
	// TODO should we do this?
	// Destroy is called in Renderer::Shutdown() so this might be redundant
	// Destroy();
}

void BirdGame::RendererImpl::LoadPipeline(HWND hwnd, uint32_t width, uint32_t height)
{
	Initialize(width, height);
	CreateDevice();
	CreateCommandQueue();
	CreateSwapChain(hwnd);
}

void BirdGame::RendererImpl::LoadAssets()
{
	LoadShaders();
	CreateCommandList();
	CreateVertexBuffer(); // Set up the vertex buffers here for now since this shader is very basic and not doing anything interesting
	CreateTexture();
	CloseAndExecuteCommandList(); // Close the command list and execute it to begin the initial GPU setup.
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

	// Set necessary state.
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	mCommandList->RSSetViewports(1, &mViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate that the back buffer will be used as a render target.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargets[mFrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mFrameIndex, mRtvDescriptorSize);
	mCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	mCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mCommandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
	mCommandList->DrawInstanced(3, 1, 0, 0);

	// Indicate that the back buffer will now be used to present.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargets[mFrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
}

void BirdGame::RendererImpl::CloseAndExecuteCommandList()
{
	CheckHResult(mCommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
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

void BirdGame::RendererImpl::Destroy()
{
	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	WaitForPreviousFrame();

	CloseHandle(mFenceEvent);
	mFenceEvent = NULL;
}

void BirdGame::RendererImpl::Initialize(uint32_t width, uint32_t height)
{
	// Initialize Viewport
	mViewport.TopLeftX = 0.0f;
	mViewport.TopLeftY = 0.0f;
	mViewport.Width = static_cast<float>(width);
	mViewport.Height = static_cast<float>(height);
	mViewport.MinDepth = D3D12_MIN_DEPTH;
	mViewport.MaxDepth = D3D12_MAX_DEPTH;

	// Initialize ScissorRect
	mScissorRect.left = 0;
	mScissorRect.top = 0;
	mScissorRect.right = static_cast<LONG>(width);
	mScissorRect.bottom = static_cast<LONG>(height);
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

	CheckHResult(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));
}

void BirdGame::RendererImpl::CreateSwapChain(HWND hwnd)
{
	// Create the swap chain
	{
		ComPtr<IDXGIFactory4> factory;
		CheckHResult(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

		// Describe and create the swap chain.
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
		swapChainDesc.BufferCount = kNumBufferFrames;
		swapChainDesc.Width = static_cast<UINT>(mViewport.Width);
		swapChainDesc.Height = static_cast<UINT>(mViewport.Height);
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

		// Describe and create a shader resource view (SRV) heap for the texture.
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 1;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		CheckHResult(mDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvHeap)));

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

void BirdGame::RendererImpl::LoadShaders()
{
	// Create the root signature.
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(mDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		CD3DX12_DESCRIPTOR_RANGE1 ranges[1] = {};
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

		CD3DX12_ROOT_PARAMETER1 rootParameters[1] = {};
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		CheckHResult(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
		CheckHResult(mDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
	}

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		CheckHResult(D3DCompileFromFile(L"assets/shaders/shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		CheckHResult(D3DCompileFromFile(L"assets/shaders/shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = mRootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		CheckHResult(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPipelineState)));
	}
}

void BirdGame::RendererImpl::CreateCommandList()
{
	// Create the command list.
	CheckHResult(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocator.Get(), mPipelineState.Get(), IID_PPV_ARGS(&mCommandList)));
}

void BirdGame::RendererImpl::CreateVertexBuffer()
{
	// Define the geometry for a triangle.
	float aspectRatio = mViewport.Width / mViewport.Height;
	Vertex triangleVertices[] =
	{
        { { 0.0f, 0.25f * aspectRatio, 0.0f }, { 0.5f, 0.0f } },
        { { 0.25f, -0.25f * aspectRatio, 0.0f }, { 1.0f, 1.0f } },
        { { -0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 1.0f } }
	};

	const UINT vertexBufferSize = sizeof(triangleVertices);

	// Note: using upload heaps to transfer static data like vert buffers is not 
	// recommended. Every time the GPU needs it, the upload heap will be marshalled 
	// over. Please read up on Default Heap usage. An upload heap is used here for 
	// code simplicity and because there are very few verts to actually transfer.
	CheckHResult(mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mVertexBuffer)));

	// Copy the triangle data to the vertex buffer.
	uint8_t* pVertexDataBegin = nullptr;
	CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
	CheckHResult(mVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
	mVertexBuffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view.
	mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
	mVertexBufferView.StrideInBytes = sizeof(Vertex);
	mVertexBufferView.SizeInBytes = vertexBufferSize;
}

void BirdGame::RendererImpl::CreateTexture()
{
	// Describe and create a Texture2D
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.Width = kTextureWidth;
	textureDesc.Height = kTextureHeight;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	CheckHResult(mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&mTexture)));

	const uint64_t uploadBufferSize = GetRequiredIntermediateSize(mTexture.Get(), 0, 1);

	// Create the GPU upload buffer
	ID3D12Resource* textureUploadHeap; // Use raw pointer here instead of ComPtr because we want this to exist until it has finished executing on the GPU
	CheckHResult(mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&textureUploadHeap)));

	// Copy data to the intermediate upload heap and then schedule a copy 
	// from the upload heap to the Texture2D.
	std::vector<uint8_t> texture = GenerateTextureData();

	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = &texture[0];
	textureData.RowPitch = static_cast<LONG_PTR>(kTextureWidth) * static_cast<LONG_PTR>(kTexturePixelSize);
	textureData.SlicePitch = textureData.RowPitch * kTextureHeight;

	// This is a helper function in d3dx12.h that copies data to a default heap (used by the texture) via the upload heap using CopyTextureRegion.
	UpdateSubresources(mCommandList.Get(), mTexture.Get(), textureUploadHeap, 0, 0, 1, &textureData);
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	// Describe and create a SRV for the texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	mDevice->CreateShaderResourceView(mTexture.Get(), &srvDesc, mSrvHeap->GetCPUDescriptorHandleForHeapStart());
}

// A fence is a synchronization primitive that we can use to signal that the GPU is done rendering a frame
void BirdGame::RendererImpl::CreateFence()
{
	// Create synchronization objects and wait until assets have been uploaded to the GPU.
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
BirdGame::RendererDX::RendererDX()
{
}

BirdGame::RendererDX::~RendererDX()
{
}

void BirdGame::RendererDX::Initialize(Window& window)
{
	mImpl.reset(new RendererImpl());
	mImpl->LoadPipeline(window.GetHandle(), window.GetWidth(), window.GetHeight());
	mImpl->LoadAssets();

	// Wait for the command list to execute; we are reusing the same command 
	// list in our main loop but for now, we just want to wait for setup to 
	// complete before continuing.
	mImpl->WaitForPreviousFrame();
}

void BirdGame::RendererDX::Shutdown()
{
	mImpl->Destroy();
}

void BirdGame::RendererDX::Render()
{
	mImpl->PopulateCommandList();
	mImpl->CloseAndExecuteCommandList();
	mImpl->Present();
	mImpl->WaitForPreviousFrame();
}
