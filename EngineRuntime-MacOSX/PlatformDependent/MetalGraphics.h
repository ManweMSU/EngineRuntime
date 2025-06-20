#pragma once

#include "../UserInterface/ShapeBase.h"
#include "../Graphics/Graphics.h"
#include "../Interfaces/SystemWindows.h"

#import "Foundation/Foundation.h"
#import "QuartzCore/QuartzCore.h"
#import "AppKit/AppKit.h"
#import "Metal/Metal.h"

namespace Engine
{
	namespace MetalGraphics
	{
		class MetalPresentationEngine : public Windows::I2DPresentationEngine
		{
			NSView * _root_view;
			NSView * _metal_view;
		public:
			MetalPresentationEngine(void);
			virtual ~MetalPresentationEngine(void) override;

			virtual void Attach(Windows::ICoreWindow * window) override;
			virtual void Detach(void) override;
			virtual void Invalidate(void) override;
			virtual void Resize(int width, int height) override;
			virtual Graphics::I2DDeviceContext * GetContext(void) noexcept override;
			virtual bool BeginRenderingPass(void) noexcept override;
			virtual bool EndRenderingPass(void) noexcept override;

			id<MTLDevice> GetDevice(void);
			void SetDevice(id<MTLDevice> device);
			MTLPixelFormat GetPixelFormat(void);
			void SetPixelFormat(MTLPixelFormat format);
			Point GetSize(void);
			void SetSize(Point size);
			void SetAutosize(bool autosize);
			void SetOpaque(bool opaque);
			void SetFramebufferOnly(bool framebuffer_only);
			void SetExtendedDynamicRange(bool edr);
			id<CAMetalDrawable> GetDrawable(void);
		};

		Graphics::IDeviceFactory * CreateMetalDeviceFactory(void);
		Graphics::IDevice * GetMetalCommonDevice(void);

		id<MTLDevice> GetInnerMetalDevice(Graphics::IDevice * wrapper);
		id<MTLCommandQueue> GetInnerMetalQueue(Graphics::IDevice * wrapper);
		id<MTLBuffer> GetInnerMetalBuffer(Graphics::IBuffer * buffer);
		id<MTLTexture> GetInnerMetalTexture(Graphics::ITexture * texture);
		MTLPixelFormat MakeMetalPixelFormat(Graphics::PixelFormat format);

		void QueryMetalVersion(id<MTLDevice> device, uint & major, uint & minor) noexcept;
		bool QueryMetalFormatSupportForShaderRead(id<MTLDevice> device, Graphics::PixelFormat format) noexcept;
		bool QueryMetalFormatSupportForShaderSample(id<MTLDevice> device, Graphics::PixelFormat format) noexcept;
		bool QueryMetalFormatSupportForRenderTarget(id<MTLDevice> device, Graphics::PixelFormat format) noexcept;
		bool QueryMetalFormatSupportForBlendRenderTarget(id<MTLDevice> device, Graphics::PixelFormat format) noexcept;
		bool QueryMetalFormatSupportForDepthStencil(id<MTLDevice> device, Graphics::PixelFormat format) noexcept;
	}
}