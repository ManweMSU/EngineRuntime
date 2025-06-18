#pragma once

#include "../Graphics/GraphicsBase.h"
#include "../Miscellaneous/Volumes.h"
#include "../ImageCodec/CodecBase.h"

#include <d2d1_1.h>
#include <dwrite.h>

#undef LoadBitmap

namespace Engine
{
	namespace Direct2D
	{
		extern ID2D1Factory1 * D2DFactory1;
		extern ID2D1Factory * D2DFactory;
		extern IDWriteFactory * DWriteFactory;
		extern SafePointer<Graphics::I2DDeviceContextFactory> CommonFactory;

		void InitializeFactory(void);
		void ShutdownFactory(void);

		class D2D_DeviceContext : public Graphics::IBitmapContext
		{
			ID2D1RenderTarget * _render_target;
			ID2D1DeviceContext * _render_target_ex;
			Graphics::IDevice * _wrapped_device;
			bool _bitmap_context_enabled;
			uint32 _time, _ref_time, _blink_time, _hblink_time, _bitmap_context_state, _clear_counter;
			Volumes::ObjectCache<Color, Graphics::IColorBrush> _color_cache;
			Volumes::ObjectCache<double, Graphics::IBlurEffectBrush> _blur_cache;
			SafePointer<Graphics::IInversionEffectBrush> _inversion_cache;
			SafePointer<Graphics::IBitmap> _bitmap_target;
			Volumes::Stack<ID2D1Layer *> _layers;
			Volumes::Stack<Box> _clipping;
			Volumes::List< SafePointer<Graphics::IBitmapLink> > _bitmaps;
		public:
			D2D_DeviceContext(void);
			virtual ~D2D_DeviceContext(void) override;
			// Direct2D control API
			void SetBitmapContext(bool set) noexcept;
			void SetRenderTarget(ID2D1RenderTarget * target) noexcept;
			void SetRenderTargetEx(ID2D1DeviceContext * target) noexcept;
			void SetWrappedDevice(Graphics::IDevice * device) noexcept;
			ID2D1RenderTarget * GetRenderTarget(void) const noexcept;
			void ClippingUndo(void) noexcept;
			void ClippingRedo(void) noexcept;
			// Core feature API
			virtual void GetImplementationInfo(string & tech, uint32 & version_major, uint32 & version_minor) override;
			virtual uint32 GetFeatureList(void) noexcept override;
			virtual string ToString(void) const override;
			// Brush factory API
			virtual Graphics::IColorBrush * CreateSolidColorBrush(Color color) noexcept override;
			virtual Graphics::IColorBrush * CreateGradientBrush(Point rel_from, Point rel_to, const GradientPoint * points, int count) noexcept override;
			virtual Graphics::IBlurEffectBrush * CreateBlurEffectBrush(double power) noexcept override;
			virtual Graphics::IInversionEffectBrush * CreateInversionEffectBrush(void) noexcept override;
			virtual Graphics::IBitmapBrush * CreateBitmapBrush(Graphics::IBitmap * bitmap, const Box & area, bool tile) noexcept override;
			virtual Graphics::IBitmapBrush * CreateTextureBrush(Graphics::ITexture * texture, Graphics::TextureAlphaMode mode) noexcept override;
			virtual Graphics::ITextBrush * CreateTextBrush(Graphics::IFont * font, const string & text, uint32 horizontal_align, uint32 vertical_align, const Color & color) noexcept override;
			virtual Graphics::ITextBrush * CreateTextBrush(Graphics::IFont * font, const uint32 * ucs, int length, uint32 horizontal_align, uint32 vertical_align, const Color & color) noexcept override;
			virtual void ClearInternalCache(void) noexcept override;
			// Clipping and layers
			virtual void PushClip(const Box & rect) noexcept override;
			virtual void PopClip(void) noexcept override;
			virtual void BeginLayer(const Box & rect, double opacity) noexcept override;
			virtual void EndLayer(void) noexcept override;
			// Rendering
			virtual void Render(Graphics::IColorBrush * brush, const Box & at) noexcept override;
			virtual void Render(Graphics::IBitmapBrush * brush, const Box & at) noexcept override;
			virtual void Render(Graphics::ITextBrush * brush, const Box & at, bool clip) noexcept override;
			virtual void Render(Graphics::IBlurEffectBrush * brush, const Box & at) noexcept override;
			virtual void Render(Graphics::IInversionEffectBrush * brush, const Box & at, bool blink) noexcept override;
			// Polygons
			virtual void RenderPolyline(const Math::Vector2 * points, int count, Color color, double width) noexcept override;
			virtual void RenderPolygon(const Math::Vector2 * points, int count, Color color) noexcept override;
			// Time control
			virtual void SetAnimationTime(uint32 value) noexcept override;
			virtual uint32 GetAnimationTime(void) noexcept override;
			virtual void SetCaretReferenceTime(uint32 value) noexcept override;
			virtual uint32 GetCaretReferenceTime(void) noexcept override;
			virtual void SetCaretBlinkPeriod(uint32 value) noexcept override;
			virtual uint32 GetCaretBlinkPeriod(void) noexcept override;
			virtual bool IsCaretVisible(void) noexcept override;
			// Interface querying
			virtual Graphics::IDevice * GetParentDevice(void) noexcept override;
			virtual Graphics::I2DDeviceContextFactory * GetParentFactory(void) noexcept override;
			// Bitmap render target function
			virtual bool BeginRendering(Graphics::IBitmap * dest) noexcept override;
			virtual bool BeginRendering(Graphics::IBitmap * dest, Color clear_color) noexcept override;
			virtual bool EndRendering(void) noexcept override;
		};
		class D2D_DeviceContextFactory : public Graphics::I2DDeviceContextFactory
		{
		public:
			// Core feature API
			virtual string ToString(void) const override;
			// Factory API
			virtual Graphics::IBitmap * CreateBitmap(int width, int height, Color clear_color) noexcept override;
			virtual Graphics::IBitmap * LoadBitmap(Codec::Frame * source) noexcept override;
			virtual Graphics::IFont * LoadFont(const string & face_name, int height, int weight, bool italic, bool underline, bool strikeout) noexcept override;
			virtual Array<string> * GetFontFamilies(void) noexcept override;
			virtual Graphics::IBitmapContext * CreateBitmapContext(void) noexcept override;
		};
	}
}