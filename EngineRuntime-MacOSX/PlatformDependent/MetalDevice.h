#pragma once

#include "MetalGraphics.h"

namespace Engine
{
	namespace Cocoa
	{
		id<MTLLibrary> CreateMetalRenderingDeviceShaders(id<MTLDevice> device);

		Windows::I2DPresentationEngine * CreateMetalPresentationEngine(void);

		Graphics::I2DDeviceContext * CreateMetalRenderingDevice(Graphics::IDevice * device);
		void PureMetalRenderingDeviceBeginDraw(Graphics::I2DDeviceContext * device, id<MTLCommandBuffer> command, id<MTLTexture> texture, uint width, uint height, MTLLoadAction load, const float * clear_value);
		void PureMetalRenderingDeviceEndDraw(Graphics::I2DDeviceContext * device);
	}
}