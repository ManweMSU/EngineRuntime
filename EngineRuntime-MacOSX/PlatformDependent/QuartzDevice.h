#pragma once

#include "../UserInterface/ShapeBase.h"
#include "../Miscellaneous/Volumes.h"

namespace Engine
{
	namespace Cocoa
	{
		class QuartzDeviceContext : public Graphics::IBitmapContext
		{
			bool _bitmap_target_allowed;
			void * _context;
			int _width, _height, _scale;
			uint32 _time, _ref_time, _blink_time, _blink_htime;
			Volumes::Stack<Box> _clipping;
			Volumes::ObjectCache<Color, Graphics::IColorBrush> _color_cache;
			SafePointer<Graphics::IInversionEffectBrush> _inversion_cache;
			SafePointer<Graphics::IBitmap> _bitmap_target;
		public:
			QuartzDeviceContext(void);
			virtual ~QuartzDeviceContext(void) override;
			void * GetContext(void) const noexcept;
			void SetContext(void * context, int width, int height, int scale) noexcept;
			void SetBitmapTarget(bool set) noexcept;
			// Context information
			virtual void GetImplementationInfo(string & tech, uint32 & version_major, uint32 & version_minor) override;
			virtual uint32 GetFeatureList(void) noexcept override;
			virtual ImmutableString ToString(void) const override;
			// Creating brushes
			virtual Graphics::IColorBrush * CreateSolidColorBrush(Color color) noexcept override;
			virtual Graphics::IColorBrush * CreateGradientBrush(Point rel_from, Point rel_to, const GradientPoint * points, int count) noexcept override;
			virtual Graphics::IBlurEffectBrush * CreateBlurEffectBrush(double power) noexcept override;
			virtual Graphics::IInversionEffectBrush * CreateInversionEffectBrush(void) noexcept override;
			virtual Graphics::IBitmapBrush * CreateBitmapBrush(Graphics::IBitmap * bitmap, const Box & area, bool tile) noexcept override;
			virtual Graphics::IBitmapBrush * CreateTextureBrush(Graphics::ITexture * texture, Graphics::TextureAlphaMode mode) noexcept override;
			virtual Graphics::ITextBrush * CreateTextBrush(Graphics::IFont * font, const string & text, uint32 horizontal_align, uint32 vertical_align, const Color & color) noexcept override;
			virtual Graphics::ITextBrush * CreateTextBrush(Graphics::IFont * font, const uint32 * ucs, int length, uint32 horizontal_align, uint32 vertical_align, const Color & color) noexcept override;
			virtual void ClearInternalCache(void) noexcept override;
			// Managing clipping and layers
			virtual void PushClip(const Box & rect) noexcept override;
			virtual void PopClip(void) noexcept override;
			virtual void BeginLayer(const Box & rect, double opacity) noexcept override;
			virtual void EndLayer(void) noexcept override;
			// Rendering primitives
			virtual void Render(Graphics::IColorBrush * brush, const Box & at) noexcept override;
			virtual void Render(Graphics::IBitmapBrush * brush, const Box & at) noexcept override;
			virtual void Render(Graphics::ITextBrush * brush, const Box & at, bool clip) noexcept override;
			virtual void Render(Graphics::IBlurEffectBrush * brush, const Box & at) noexcept override;
			virtual void Render(Graphics::IInversionEffectBrush * brush, const Box & at, bool blink) noexcept override;
			// Rendering polygons
			virtual void RenderPolyline(const Math::Vector2 * points, int count, Color color, double width) noexcept override;
			virtual void RenderPolygon(const Math::Vector2 * points, int count, Color color) noexcept override;
			// Controlling animation
			virtual void SetAnimationTime(uint32 value) noexcept override;
			virtual uint32 GetAnimationTime(void) noexcept override;
			virtual void SetCaretReferenceTime(uint32 value) noexcept override;
			virtual uint32 GetCaretReferenceTime(void) noexcept override;
			virtual void SetCaretBlinkPeriod(uint32 value) noexcept override;
			virtual uint32 GetCaretBlinkPeriod(void) noexcept override;
			virtual bool IsCaretVisible(void) noexcept override;
			// Device information
			virtual Graphics::IDevice * GetParentDevice(void) noexcept override;
			virtual Graphics::I2DDeviceContextFactory * GetParentFactory(void) noexcept override;
			// Bitmap target functions
			virtual bool BeginRendering(Graphics::IBitmap * dest) noexcept override;
			virtual bool BeginRendering(Graphics::IBitmap * dest, Color clear_color) noexcept override;
			virtual bool EndRendering(void) noexcept override;
		};
		class QuartzDeviceContextFactory : public Graphics::I2DDeviceContextFactory
		{
		public:
			virtual Graphics::IBitmap * CreateBitmap(int width, int height, Color clear_color) noexcept override;
			virtual Graphics::IBitmap * LoadBitmap(Codec::Frame * source) noexcept override;
			virtual Graphics::IFont * LoadFont(const string & face_name, int height, int weight, bool italic, bool underline, bool strikeout) noexcept override;
			virtual Array<string> * GetFontFamilies(void) noexcept override;
			virtual Graphics::IBitmapContext * CreateBitmapContext(void) noexcept override;
			virtual ImmutableString ToString(void) const override;
		};

		Codec::Frame * GetBitmapSurface(Graphics::IBitmap * bitmap);
		void * GetBitmapCoreImage(Graphics::IBitmap * bitmap);
		QuartzDeviceContextFactory * GetCommonDeviceContextFactory(void);
	}
}