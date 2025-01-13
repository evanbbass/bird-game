#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

// Windows
#include <Windows.h>
#include <wrl.h>

// DirectX
#include <directx/d3dx12.h> // This helper library has to be included before any SDK headers
#include <d3d12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <dxcapi.h>
#include <dxgi1_4.h>

// Standard libraries
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
