#pragma once

#include "Direct2D.h"

#include <d3d11_1.h>

namespace Engine
{
	namespace Direct3D
	{
		extern SafePointer<Graphics::IDeviceFactory> CommonFactory;
		extern SafePointer<Graphics::IDevice> CommonDevice;

		bool CreateD2DDeviceContextForWindow(HWND Window, ID2D1DeviceContext ** Context, IDXGISwapChain1 ** SwapChain) noexcept;
		bool CreateSwapChainForWindow(HWND Window, IDXGISwapChain ** SwapChain) noexcept;
		bool CreateSwapChainDevice(IDXGISwapChain * SwapChain, ID2D1RenderTarget ** Target) noexcept;
		bool ResizeRenderBufferForD2DDevice(ID2D1DeviceContext * Context, IDXGISwapChain1 * SwapChain) noexcept;
		bool ResizeRenderBufferForSwapChainDevice(IDXGISwapChain * SwapChain) noexcept;

		void CreateCommonDeviceFactory(void) noexcept;
		void CreateCommonDevice(void) noexcept;

		IDXGIFactory * GetInnerObject(Graphics::IDeviceFactory * factory) noexcept;
		IDXGIFactory1 * GetInnerObject1(Graphics::IDeviceFactory * factory) noexcept;
		ID3D11Device * GetInnerObject(Graphics::IDevice * device) noexcept;
		ID2D1Device * GetInnerObject2D(Graphics::IDevice * device) noexcept;
		ID3D11Resource * GetInnerObject(Graphics::IDeviceResource * resource) noexcept;
		ID3D11Texture2D * GetInnerObject2D(Graphics::ITexture * texture) noexcept;

		DXGI_FORMAT MakeDxgiFormat(Graphics::PixelFormat format) noexcept;
		IUnknown * GetVideoAccelerationDevice(Graphics::IDevice * device) noexcept;
		void SetVideoAccelerationDevice(Graphics::IDevice * device_for, IUnknown * device_set) noexcept;
		IDXGIDevice * QueryDXGIDevice(Graphics::IDevice * device) noexcept;
	}
}