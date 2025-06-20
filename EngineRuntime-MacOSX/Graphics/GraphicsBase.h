#pragma once

#include "../EngineBase.h"
#include "../Streaming.h"
#include "../ImageCodec/CodecBase.h"
#include "../Graphics/Graphics.h"
#include "../Math/Vector.h"

namespace Engine
{
	namespace UI
	{
		extern double CurrentScaleFactor;

		class Coordinate
		{
		public:
			double Anchor, Zoom;
			int Absolute;

			Coordinate(void) noexcept;
			Coordinate(int shift) noexcept;
			Coordinate(int shift, double zoom, double anchor) noexcept;

			Coordinate friend operator + (const Coordinate & a, const Coordinate & b) noexcept;
			Coordinate friend operator - (const Coordinate & a, const Coordinate & b) noexcept;
			Coordinate friend operator * (const Coordinate & a, double b) noexcept;
			Coordinate friend operator * (double b, const Coordinate & a) noexcept;
			Coordinate friend operator / (const Coordinate & a, double b) noexcept;

			Coordinate & operator += (const Coordinate & a) noexcept;
			Coordinate & operator -= (const Coordinate & a) noexcept;
			Coordinate & operator *= (double a) noexcept;
			Coordinate & operator /= (double a) noexcept;
			Coordinate operator - (void) const noexcept;

			bool friend operator == (const Coordinate & a, const Coordinate & b) noexcept;
			bool friend operator != (const Coordinate & a, const Coordinate & b) noexcept;

			static Coordinate Right() noexcept;
			static Coordinate Bottom() noexcept;

			string ToString(void) const;
			int ToInteger(int size_relative) const noexcept;
		};
		class Rectangle
		{
		public:
			Coordinate Left, Top, Right, Bottom;

			Rectangle(void) noexcept;
			Rectangle(const Coordinate & left, const Coordinate & top, const Coordinate & right, const Coordinate & bottom) noexcept;

			Rectangle friend operator + (const Rectangle & a, const Rectangle & b) noexcept;
			Rectangle friend operator - (const Rectangle & a, const Rectangle & b) noexcept;
			Rectangle friend operator * (const Rectangle & a, double b) noexcept;
			Rectangle friend operator * (double b, const Rectangle & a) noexcept;
			Rectangle friend operator / (const Rectangle & a, double b) noexcept;

			Rectangle & operator += (const Rectangle & a) noexcept;
			Rectangle & operator -= (const Rectangle & a) noexcept;
			Rectangle & operator *= (double a) noexcept;
			Rectangle & operator /= (double a) noexcept;
			Rectangle operator - (void) const noexcept;

			bool friend operator == (const Rectangle & a, const Rectangle & b) noexcept;
			bool friend operator != (const Rectangle & a, const Rectangle & b) noexcept;

			bool IsValid(void) const noexcept;

			static Rectangle Entire() noexcept;
			static Rectangle Invalid() noexcept;

			string ToString(void) const;
		};
	}

	class Point
	{
	public:
		int x, y;

		Point(void) noexcept;
		Point(int _x, int _y) noexcept;
		string ToString(void) const;

		Point & operator += (const Point & a) noexcept;
		Point & operator -= (const Point & a) noexcept;
		Point operator - (void) const noexcept;

		Point friend operator + (const Point & a, const Point & b) noexcept;
		Point friend operator - (const Point & a, const Point & b) noexcept;
		int friend operator * (const Point & a, const Point & b) noexcept;

		bool friend operator == (const Point & a, const Point & b) noexcept;
		bool friend operator != (const Point & a, const Point & b) noexcept;
	};
	class Box
	{
	public:
		int Left, Top, Right, Bottom;

		Box(void) noexcept;
		Box(const UI::Rectangle & source, const Box & outer) noexcept;
		Box(int left, int top, int right, int bottom) noexcept;

		bool friend operator == (const Box & a, const Box & b) noexcept;
		bool friend operator != (const Box & a, const Box & b) noexcept;

		bool IsInside(const Point & p) const noexcept;
		string ToString(void) const;

		static Box Intersect(const Box & a, const Box & b) noexcept;
	};
	class Color
	{
	public:
		union {
			struct { uint8 r, g, b, a; };
			uint32 Value;
		};

		Color(void) noexcept;
		Color(uint8 sr, uint8 sg, uint8 sb, uint8 sa = 0xFF) noexcept;
		Color(int sr, int sg, int sb, int sa = 0xFF) noexcept;
		Color(float sr, float sg, float sb, float sa = 1.0) noexcept;
		Color(double sr, double sg, double sb, double sa = 1.0) noexcept;
		Color(uint32 code) noexcept;

		operator uint32 (void) const noexcept;
		string ToString(void) const;

		bool friend operator == (const Color & a, const Color & b) noexcept;
		bool friend operator != (const Color & a, const Color & b) noexcept;
	};
	class GradientPoint
	{
	public:
		Color Value;
		double Position;

		GradientPoint(void) noexcept;
		GradientPoint(const Color & color) noexcept;
		GradientPoint(const Color & color, double position) noexcept;
		string ToString(void) const;

		bool friend operator == (const GradientPoint & a, const GradientPoint & b) noexcept;
		bool friend operator != (const GradientPoint & a, const GradientPoint & b) noexcept;
	};

	namespace Graphics
	{
		constexpr const widechar * FontWordSerif = L"serif";
		constexpr const widechar * FontWordSans = L"sans";
		constexpr const widechar * FontWordMono = L"mono";

		constexpr const widechar * SystemSerifFont = L"@serif";
		constexpr const widechar * SystemSansSerifFont = L"@sans serif";
		constexpr const widechar * SystemMonoSerifFont = L"@mono serif";
		constexpr const widechar * SystemMonoSansSerifFont = L"@mono sans serif";

		enum DeviceContextFeature {
			DeviceContextFeatureBlurCapable = 0x00000001,
			DeviceContextFeatureInversionCapable = 0x00000002,
			DeviceContextFeaturePolygonCapable = 0x00000004,
			DeviceContextFeatureLayersCapable = 0x00000008,
			DeviceContextFeatureHardware = 0x20000000,
			DeviceContextFeatureGraphicsInteropEnabled = 0x40000000,
			DeviceContextFeatureBitmapTarget = 0x80000000
		};
		enum TextAlignment {
			TextAlignmentLeft = 0,
			TextAlignmentTop = 0,
			TextAlignmentCenter = 1,
			TextAlignmentRight = 2,
			TextAlignmentBottom = 2
		};

		enum class TextureAlphaMode { Ignore = 0, Premultiplied = 1 };

		class I2DDeviceContext;
		class I2DDeviceContextFactory;
		class IBitmapContext;
		class IBitmap;
		class IDeviceBitmap;
		class IBitmapLink;
		class IFont;

		class IBitmap : public Object
		{
		public:
			virtual int GetWidth(void) const noexcept = 0;
			virtual int GetHeight(void) const noexcept = 0;
			virtual bool Reload(Codec::Frame * source) noexcept = 0;
			virtual bool AddDeviceBitmap(IDeviceBitmap * bitmap, I2DDeviceContext * device_for) noexcept = 0;
			virtual bool RemoveDeviceBitmap(I2DDeviceContext * device_for) noexcept = 0;
			virtual IDeviceBitmap * GetDeviceBitmap(I2DDeviceContext * device_for) const noexcept = 0;
			virtual IBitmapLink * GetLinkObject(void) const noexcept = 0;
			virtual Codec::Frame * QueryFrame(void) const noexcept = 0;
		};
		class IDeviceBitmap : public Object
		{
		public:
			virtual int GetWidth(void) const noexcept = 0;
			virtual int GetHeight(void) const noexcept = 0;
			virtual bool Reload(void) noexcept = 0;
			virtual IBitmap * GetParentBitmap(void) const noexcept = 0;
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept = 0;
		};
		class IBitmapLink : public Object
		{
		public:
			virtual IBitmap * GetBitmap(void) const noexcept = 0;
		};
		class IFont : public Object
		{
		public:
			virtual int GetWidth(void) const noexcept = 0;
			virtual int GetHeight(void) const noexcept = 0;
			virtual int GetLineSpacing(void) const noexcept = 0;
			virtual int GetBaselineOffset(void) const noexcept = 0;
		};

		class IBrush : public Object
		{
		public:
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept = 0;
		};
		class IColorBrush : public IBrush {};
		class IBlurEffectBrush : public IBrush {};
		class IInversionEffectBrush : public IBrush {};
		class IBitmapBrush : public IBrush {};
		class ITextBrush : public IBrush
		{
		public:
			virtual void GetExtents(int & width, int & height) noexcept = 0;
			virtual void SetHighlightColor(const Color & color) noexcept = 0;
			virtual void HighlightText(int start, int end) noexcept = 0;
			virtual int TestPosition(int point) noexcept = 0;
			virtual int EndOfChar(int index) noexcept = 0;
			virtual int GetStringLength(void) noexcept = 0;
			virtual void SetCharPalette(const Color * colors, int count) = 0;
			virtual void SetCharColors(const uint8 * indicies, int count) = 0;
			virtual void SetCharAdvances(const double * advances) = 0;
			virtual void GetCharAdvances(double * advances) noexcept = 0;
		};

		class I2DDeviceContext : public Object
		{
		public:
			virtual void GetImplementationInfo(string & tech, uint32 & version_major, uint32 & version_minor) = 0;
			virtual uint32 GetFeatureList(void) noexcept = 0;

			virtual IColorBrush * CreateSolidColorBrush(Color color) noexcept = 0;
			virtual IColorBrush * CreateGradientBrush(Point rel_from, Point rel_to, const GradientPoint * points, int count) noexcept = 0;
			virtual IBlurEffectBrush * CreateBlurEffectBrush(double power) noexcept = 0;
			virtual IInversionEffectBrush * CreateInversionEffectBrush(void) noexcept = 0;
			virtual IBitmapBrush * CreateBitmapBrush(IBitmap * bitmap, const Box & area, bool tile) noexcept = 0;
			virtual IBitmapBrush * CreateTextureBrush(ITexture * texture, TextureAlphaMode mode) noexcept = 0;
			virtual ITextBrush * CreateTextBrush(IFont * font, const string & text, uint32 horizontal_align, uint32 vertical_align, const Color & color) noexcept = 0;
			virtual ITextBrush * CreateTextBrush(IFont * font, const uint32 * ucs, int length, uint32 horizontal_align, uint32 vertical_align, const Color & color) noexcept = 0;
			virtual void ClearInternalCache(void) noexcept = 0;

			virtual void PushClip(const Box & rect) noexcept = 0;
			virtual void PopClip(void) noexcept = 0;
			virtual void BeginLayer(const Box & rect, double opacity) noexcept = 0;
			virtual void EndLayer(void) noexcept = 0;

			virtual void Render(IColorBrush * brush, const Box & at) noexcept = 0;
			virtual void Render(IBitmapBrush * brush, const Box & at) noexcept = 0;
			virtual void Render(ITextBrush * brush, const Box & at, bool clip) noexcept = 0;
			virtual void Render(IBlurEffectBrush * brush, const Box & at) noexcept = 0;
			virtual void Render(IInversionEffectBrush * brush, const Box & at, bool blink) noexcept = 0;

			virtual void RenderPolyline(const Math::Vector2 * points, int count, Color color, double width) noexcept = 0;
			virtual void RenderPolygon(const Math::Vector2 * points, int count, Color color) noexcept = 0;

			virtual void SetAnimationTime(uint32 value) noexcept = 0;
			virtual uint32 GetAnimationTime(void) noexcept = 0;
			virtual void SetCaretReferenceTime(uint32 value) noexcept = 0;
			virtual uint32 GetCaretReferenceTime(void) noexcept = 0;
			virtual void SetCaretBlinkPeriod(uint32 value) noexcept = 0;
			virtual uint32 GetCaretBlinkPeriod(void) noexcept = 0;
			virtual bool IsCaretVisible(void) noexcept = 0;

			virtual IDevice * GetParentDevice(void) noexcept = 0;
			virtual I2DDeviceContextFactory * GetParentFactory(void) noexcept = 0;
		};
		class IBitmapContext : public I2DDeviceContext
		{
		public:
			virtual bool BeginRendering(IBitmap * dest) noexcept = 0;
			virtual bool BeginRendering(IBitmap * dest, Color clear_color) noexcept = 0;
			virtual bool EndRendering(void) noexcept = 0;
		};
		class I2DDeviceContextFactory : public Object
		{
		public:
			virtual IBitmap * CreateBitmap(int width, int height, Color clear_color) noexcept = 0;
			virtual IBitmap * LoadBitmap(Codec::Frame * source) noexcept = 0;
			virtual IFont * LoadFont(const string & face_name, int height, int weight, bool italic, bool underline, bool strikeout) noexcept = 0;
			virtual Array<string> * GetFontFamilies(void) noexcept = 0;
			virtual IBitmapContext * CreateBitmapContext(void) noexcept = 0;
		};
	}
}