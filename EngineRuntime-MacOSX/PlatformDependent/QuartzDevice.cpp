#include "QuartzDevice.h"

#include "CocoaInterop2.h"
#include "../ImageCodec/CodecBase.h"
#include "../Math/Color.h"

#include <ImageIO/ImageIO.h>
#include <CoreText/CoreText.h>

using namespace Engine::Graphics;

namespace Engine
{
	namespace Cocoa
	{
		SafePointer<QuartzDeviceContextFactory> common_factory;

		class QuartzBitmapLink : public IBitmapLink
		{
			IBitmap * _parent;
		public:
			QuartzBitmapLink(IBitmap * parent) : _parent(parent) {}
			virtual ~QuartzBitmapLink(void) override {}
			virtual ImmutableString ToString(void) const override { return L"QuartzBitmapLink"; }
			virtual IBitmap * GetBitmap(void) const noexcept override { return _parent; }
			void Invalidate(void) noexcept { _parent = 0; }
		};
		class QuartzBitmap : public IBitmap
		{
			Volumes::ObjectDictionary<I2DDeviceContext *, IDeviceBitmap> _device;
			SafePointer<QuartzBitmapLink> _link;
			SafePointer<Codec::Frame> _surface;
			CGImageRef _image;
			static void _release_bitmap_data(void * info, const void * data, size_t size) { reinterpret_cast<Codec::Frame *>(info)->Release(); }
			bool _reload(Codec::Frame * source) noexcept
			{
				auto length = source->GetScanLineLength() * source->GetHeight();
				auto rgb_cs = CGColorSpaceCreateDeviceRGB();
				if (!rgb_cs) return false;
				auto provider = CGDataProviderCreateWithData(source, source->GetData(), length, _release_bitmap_data);
				if (!provider) { CGColorSpaceRelease(rgb_cs); return false; }
				source->Retain();
				auto image = CGImageCreate(source->GetWidth(), source->GetHeight(), 8, 32, source->GetScanLineLength(), rgb_cs, kCGImageAlphaPremultipliedLast, provider, 0, false, kCGRenderingIntentDefault);
				CGColorSpaceRelease(rgb_cs);
				CGDataProviderRelease(provider);
				if (image) {
					if (_image) CGImageRelease(_image);
					_surface.SetRetain(source);
					_image = image;
					for (auto & d : _device) d.value->Reload();
					return true;
				} else return false;
			}
		public:
			QuartzBitmap(Codec::Frame * surface) : _image(0) { _link = new QuartzBitmapLink(this); if (!_reload(surface)) throw Exception(); }
			virtual ~QuartzBitmap(void) override { _link->Invalidate(); if (_image) CGImageRelease(_image); }
			virtual int GetWidth(void) const noexcept override { return _surface->GetWidth(); }
			virtual int GetHeight(void) const noexcept override { return _surface->GetHeight(); }
			virtual bool Reload(Codec::Frame * source) noexcept override
			{
				if (!source) return false;
				try {
					SafePointer<Codec::Frame> conv = source->ConvertFormat(Codec::PixelFormat::R8G8B8A8, Codec::AlphaMode::Premultiplied, Codec::ScanOrigin::TopDown);
					return _reload(conv);
				} catch (...) { return false; }
			}
			virtual bool AddDeviceBitmap(IDeviceBitmap * bitmap, I2DDeviceContext * device_for) noexcept override { try { _device.Append(device_for, bitmap); } catch (...) { return false; } return true; }
			virtual bool RemoveDeviceBitmap(I2DDeviceContext * device_for) noexcept override { _device.Remove(device_for); return true; }
			virtual IDeviceBitmap * GetDeviceBitmap(I2DDeviceContext * device_for) const noexcept override { return _device.GetObjectByKey(device_for); }
			virtual IBitmapLink * GetLinkObject(void) const noexcept override { return _link; }
			virtual Codec::Frame * QueryFrame(void) const noexcept override { try { return new Codec::Frame(*_surface); } catch (...) { return 0; } }
			virtual ImmutableString ToString(void) const override { return L"QuartzBitmap"; }
			bool Synchronize(void) noexcept { return _reload(_surface); }
			Codec::Frame * GetSurface(void) const noexcept { return _surface; }
			CGImageRef GetCoreImage(void) const noexcept { return _image; }
		};
		class CoreTextFont : public IFont
		{
			CTFontRef _main, _alt;
			bool _underline, _strikeout;
			double _height, _scaled_height, _width, _scale;
		public:
			CoreTextFont(const string & face, double height, double scale, int weight, bool italic, bool underline, bool strikeout)
			{
				_underline = underline;
				_strikeout = strikeout;
				_height = height;
				_scaled_height = height * 0.75;
				_scale = scale;
				auto main_name = CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const uint8 *>(static_cast<const widechar *>(face)), face.Length() * 4, kCFStringEncodingUTF32LE, false);
				auto alt_name = CFStringCreateWithCString(kCFAllocatorDefault, "Apple Color Emoji", kCFStringEncodingASCII);
				if (!main_name || !alt_name) { CFRelease(main_name); CFRelease(alt_name); throw OutOfMemoryException(); }
				_main = CTFontCreateWithName(main_name, _scaled_height, 0);
				_alt = CTFontCreateWithName(alt_name, _scaled_height, 0);
				CFRelease(main_name); CFRelease(alt_name);
				if (!_main) { CFRelease(_alt); throw Exception(); }
				uint32 flags = 0;
				if (weight >= 500) flags |= kCTFontBoldTrait;
				if (italic) flags |= kCTFontItalicTrait;
				auto font = CTFontCreateCopyWithSymbolicTraits(_main, 0.0, 0, flags, kCTFontItalicTrait | kCTFontBoldTrait);
				if (font) { CFRelease(_main); _main = font; }
				UniChar space = L' ';
				CGGlyph space_glyph;
				CGSize space_size;
				CTFontGetGlyphsForCharacters(_main, &space, &space_glyph, 1);
				if (space_glyph) {
					CTFontGetAdvancesForGlyphs(_main, kCTFontOrientationHorizontal, &space_glyph, &space_size, 1);
					_width = space_size.width;
				} else _width = _height;
			}
			virtual ~CoreTextFont(void) override { if (_main) CFRelease(_main); if (_alt) CFRelease(_alt); }
			virtual int GetWidth(void) const noexcept override { return int(_width); }
			virtual int GetHeight(void) const noexcept override { return int(_height * _scale); }
			virtual int GetLineSpacing(void) const noexcept override { return int(CTFontGetAscent(_main) + CTFontGetDescent(_main) + CTFontGetLeading(_main)); }
			virtual int GetBaselineOffset(void) const noexcept override { return int(CTFontGetAscent(_main)); }
			virtual ImmutableString ToString(void) const override { return L"CoreTextFont"; }
			CTFontRef GetMainFont(void) const noexcept { return _main; }
			CTFontRef GetAltFont(void) const noexcept { return _alt; }
			bool IsUnderline(void) const noexcept { return _underline; }
			bool IsStrikeout(void) const noexcept { return _strikeout; }
			int GetScaledHeight(void) const noexcept { return _scaled_height; }
		};

		IBitmap * QuartzDeviceContextFactory::CreateBitmap(int width, int height, Color clear_color) noexcept
		{
			if (width <= 0 || height <= 0) return 0;
			try {
				Color color_prem;
				color_prem.r = int(clear_color.r) * int(clear_color.a) / 255;
				color_prem.g = int(clear_color.g) * int(clear_color.a) / 255;
				color_prem.b = int(clear_color.b) * int(clear_color.a) / 255;
				color_prem.a = clear_color.a;
				SafePointer<Codec::Frame> surface = new Codec::Frame(width, height, Codec::PixelFormat::R8G8B8A8, Codec::AlphaMode::Premultiplied, Codec::ScanOrigin::TopDown);
				for (int y = 0; y < height; y++) for (int x = 0; x < width; x++) surface->SetPixel(x, y, color_prem.Value);
				return new QuartzBitmap(surface);
			} catch (...) { return 0; }			
		}
		IBitmap * QuartzDeviceContextFactory::LoadBitmap(Codec::Frame * source) noexcept
		{
			if (!source) return 0;
			try {
				SafePointer<Codec::Frame> surface = source->ConvertFormat(Codec::PixelFormat::R8G8B8A8, Codec::AlphaMode::Premultiplied, Codec::ScanOrigin::TopDown);
				return new QuartzBitmap(surface);
			} catch (...) { return 0; }
		}
		IFont * QuartzDeviceContextFactory::LoadFont(const string & face_name, int height, int weight, bool italic, bool underline, bool strikeout) noexcept
		{
			try {
				if (face_name[0] == L'@') {
					bool serif = false;
					bool sans = false;
					bool mono = false;
					auto words = face_name.Fragment(1, -1).Split(L' ');
					for (auto & w : words) if (w.Length()) {
						if (string::CompareIgnoreCase(w, Graphics::FontWordSerif) == 0) serif = true;
						else if (string::CompareIgnoreCase(w, Graphics::FontWordSans) == 0) sans = true;
						else if (string::CompareIgnoreCase(w, Graphics::FontWordMono) == 0) mono = true;
						else return 0;
					}
					if (!serif) return 0;
					if (mono) {
						if (sans) return LoadFont(L"Menlo", height, weight, italic, underline, strikeout);
						else return LoadFont(L"Courier New", height, weight, italic, underline, strikeout);
					} else {
						if (sans) return LoadFont(L"Helvetica Neue", height, weight, italic, underline, strikeout);
						else return LoadFont(L"Times New Roman", height, weight, italic, underline, strikeout);
					}
				} else return new CoreTextFont(face_name, double(height), 1.0, weight, italic, underline, strikeout);
			} catch (...) { return 0; }
		}
		Array<string> * QuartzDeviceContextFactory::GetFontFamilies(void) noexcept
		{
			auto fonts = CTFontManagerCopyAvailableFontFamilyNames();
			if (!fonts) return 0;
			SafePointer< Array<string> > result;
			try {
				auto count = CFArrayGetCount(fonts);
				result = new Array<string>(count);
				for (int i = 0; i < count; i++) result->Append(Cocoa::EngineString(reinterpret_cast<CFStringRef>(CFArrayGetValueAtIndex(fonts, i))));
			} catch (...) { CFRelease(fonts); return 0; }
			CFRelease(fonts);
			result->Retain();
			return result;
		}
		IBitmapContext * QuartzDeviceContextFactory::CreateBitmapContext(void) noexcept { try { auto result = new QuartzDeviceContext; result->SetBitmapTarget(true); return result; } catch (...) { return 0; } }
		ImmutableString QuartzDeviceContextFactory::ToString(void) const { return L"QuartzDeviceContextFactory"; }

		class QuartzColorBrush : public IColorBrush
		{
			QuartzDeviceContext * _parent;
			Math::ColorF _solid_color;
			CGGradientRef _gradient;
			Point _begin, _end;
		public:
			QuartzColorBrush(QuartzDeviceContext * parent, Color color) : _parent(parent), _solid_color(color), _gradient(0), _begin(0, 0), _end(0, 0) {}
			QuartzColorBrush(QuartzDeviceContext * parent, const GradientPoint * points, int count, Point begin, Point end) : _parent(parent), _solid_color(0.0, 0.0, 0.0, 0.0), _begin(begin), _end(end)
			{
				if (count < 2) throw InvalidArgumentException();
				Array<double> values(1);
				values.SetLength(5 * count);
				for (int i = 0; i < count; i++) {
					auto color = Math::ColorF(points[i].Value);
					values[4 * i + 0] = color.r;
					values[4 * i + 1] = color.g;
					values[4 * i + 2] = color.b;
					values[4 * i + 3] = color.a;
					values[4 * count + i] = points[i].Position;
				}
				auto rgb_cs = CGColorSpaceCreateDeviceRGB();
				if (!rgb_cs) throw OutOfMemoryException();
				_gradient = CGGradientCreateWithColorComponents(rgb_cs, values.GetBuffer(), values.GetBuffer() + 4 * count, count);
				CGColorSpaceRelease(rgb_cs);
				if (!_gradient) throw OutOfMemoryException();
			}
			virtual ~QuartzColorBrush(void) override { if (_gradient) CGGradientRelease(_gradient); }
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept override { return _parent; }
			virtual ImmutableString ToString(void) const override { return L"QuartzColorBrush"; }
			const Math::ColorF & GetSolidColor(void) const noexcept { return _solid_color; }
			CGGradientRef GetGradient(void) const noexcept { return _gradient; }
			const Point & GetBegin(void) const noexcept { return _begin; }
			const Point & GetEnd(void) const noexcept { return _end; }
		};
		class QuartzInversionBrush : public IInversionEffectBrush
		{
			QuartzDeviceContext * _parent;
			SafePointer<QuartzBitmap> _bitmap;
		public:
			QuartzInversionBrush(QuartzDeviceContext * parent) : _parent(parent)
			{
				auto fact = _parent->GetParentFactory();
				if (!fact) throw OutOfMemoryException();
				_bitmap = static_cast<QuartzBitmap *>(fact->CreateBitmap(1, 1, Color(255, 255, 255, 255)));
				if (!_bitmap) throw OutOfMemoryException();
			}
			virtual ~QuartzInversionBrush(void) override {}
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept override { return _parent; }
			virtual ImmutableString ToString(void) const override { return L"QuartzInversionBrush"; }
			QuartzBitmap * GetBitmap(void) const noexcept { return _bitmap; }
		};
		class QuartzBitmapBrush : public IBitmapBrush
		{
			QuartzDeviceContext * _parent;
			CGImageRef _fragment;
			Point _size;
			bool _tile;
		public:
			QuartzBitmapBrush(QuartzDeviceContext * parent, CGImageRef image, Point size, bool tile) : _parent(parent), _fragment(image), _size(size), _tile(tile) { CGImageRetain(_fragment); }
			virtual ~QuartzBitmapBrush(void) override { CGImageRelease(_fragment); }
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept override { return _parent; }
			virtual ImmutableString ToString(void) const override { return L"QuartzBitmapBrush"; }
			CGImageRef GetImage(void) const noexcept { return _fragment; }
			bool IsTiled(void) const noexcept { return _tile; }
			const Point & GetSize(void) const noexcept { return _size; }
		};
		class QuartzTextBrush : public ITextBrush
		{
		public:
			struct _text_range { int left, right; Math::ColorF color; CTFontRef font; };
			QuartzDeviceContext * _parent;
			SafePointer<CoreTextFont> _font;
			Array<CGGlyph> _glyphs;
			Array<CGFloat> _advances;
			Array<CGPoint> _positions;
			Array<CTFontRef> _fonts;
			Array<uint32> _flags; // 0x01 - surrogate upper pair, 0x02 - alternative font
			Array<int> _ends; // relative to UTF-32
			Array<_text_range> _ranges;
			Array<Math::ColorF> _x_brushes;
			Math::ColorF _text_color, _back_color;
			CGGlyph _space;
			int _halign, _valign, _baseline, _run_length, _hl_start, _hl_end;
			double _underline_offs, _underline_hw, _strikeout_offs, _strikeout_hw;
		private:
			void _reset_ranges(const uint8 * indicies)
			{
				SafePointer< Array<uint8> > ind;
				if (indicies) {
					ind = new Array<uint8>(1);
					ind->Append(indicies, _ends.Length());
				} else ind = 0;
				if (ind) for (int i = 0; i < _flags.Length(); i++) if (_flags[i] & 1) ind->Insert(ind->ElementAt(i - 1), i);
				_ranges.Clear();
				int cp = 0;
				while (cp < _fonts.Length()) {
					int ep = cp;
					while (ep < _fonts.Length() && _fonts[ep] == _fonts[cp] && (!ind || ind->ElementAt(ep) == ind->ElementAt(cp))) ep++;
					int index = ind ? ind->ElementAt(cp) : 0;
					CTFontRef font_ref_ct = _fonts[cp];
					_ranges << _text_range{ cp, ep, (index == 0) ? _text_color : _x_brushes[index - 1], font_ref_ct };
					cp = ep;
				}
			}
		public:
			QuartzTextBrush(QuartzDeviceContext * parent, Graphics::IFont * font, const uint32 * ucs, int length, uint32 horizontal_align, uint32 vertical_align, const Color & color) :
				_parent(parent), _glyphs(1), _advances(1), _positions(1), _fonts(1), _flags(1), _ends(1), _ranges(0x40), _x_brushes(1)
			{
				_font.SetRetain(static_cast<CoreTextFont *>(font));
				_text_color = color;
				_back_color = Math::ColorF(0.0, 0.0, 0.0, 0.0);
				_hl_start = _hl_end = -1;
				_halign = horizontal_align;
				_valign = vertical_align;
				if (!length) { _run_length = 0; return; }
				_ends.SetLength(length);
				auto utf16_length = MeasureSequenceLength(ucs, length, Encoding::UTF32, Encoding::UTF16);
				Array<uint16> utf16(1);
				Array<CGGlyph> alt_glyph(1);
				utf16.SetLength(utf16_length);
				alt_glyph.SetLength(utf16_length);
				_flags.SetLength(utf16_length);
				_glyphs.SetLength(utf16_length);
				ConvertEncoding(utf16.GetBuffer(), ucs, length, Encoding::UTF32, Encoding::UTF16);
				for (int i = 0; i < utf16_length; i++) _flags[i] = (utf16[i] >= 0xDC00 && utf16[i] < 0xE000) ? 1 : 0;
				CTFontGetGlyphsForCharacters(_font->GetMainFont(), utf16.GetBuffer(), _glyphs.GetBuffer(), utf16_length);
				if (_font->GetAltFont()) CTFontGetGlyphsForCharacters(_font->GetAltFont(), utf16.GetBuffer(), alt_glyph.GetBuffer(), utf16_length);
				else ZeroMemory(alt_glyph.GetBuffer(), sizeof(CGGlyph) * utf16_length);
				CGGlyph alt_space, question; UniChar spc = 32, que = '?';
				CTFontGetGlyphsForCharacters(_font->GetMainFont(), &spc, &_space, 1);
				CTFontGetGlyphsForCharacters(_font->GetMainFont(), &que, &question, 1);
				if (_font->GetAltFont()) CTFontGetGlyphsForCharacters(_font->GetAltFont(), &spc, &alt_space, 1);
				else alt_space = 0;
				for (int i = 0; i < utf16_length; i++) {
					if (utf16[i] == L'\t') { _glyphs[i] = 0; alt_glyph[i] = 0; } else {
						if (!_glyphs[i]) {
							if (alt_glyph[i]) { _glyphs[i] = alt_glyph[i]; _flags[i] |= 2; } else {
								if (_flags[i] & 1) {
									_flags[i] |= _flags[i - 1] & 2;
									_glyphs[i] = (_flags[i] & 2) ? alt_space : _space;
								} else _glyphs[i] = question;
							}
						}
					}
				}
				_baseline = int(CTFontGetDescent(_font->GetMainFont()));
				_underline_offs = CTFontGetUnderlinePosition(_font->GetMainFont());
				_underline_hw = CTFontGetUnderlineThickness(_font->GetMainFont()) / 2.0;
				_strikeout_offs = (CTFontGetAscent(_font->GetMainFont()) - CTFontGetDescent(_font->GetMainFont())) / 2.0;
				_strikeout_hw = _underline_hw;
				_advances.SetLength(utf16_length);
				_positions.SetLength(utf16_length);
				_fonts.SetLength(utf16_length);
				SetCharAdvances(0);
				_reset_ranges(0);
			}
			virtual ~QuartzTextBrush(void) override {}
			virtual void GetExtents(int & width, int & height) noexcept override { width = _run_length; height = _font->GetHeight(); }
			virtual void SetHighlightColor(const Color & color) noexcept override { _back_color = color; }
			virtual void HighlightText(int start, int end) noexcept override { _hl_start = start; _hl_end = end; }
			virtual int TestPosition(int point) noexcept override
			{
				if (point < 0) return 0;
				if (point > _run_length) return _ends.Length();
				int ci = 0;
				for (int i = 0; i < _flags.Length(); i++) {
					if (point <= _positions[i].x + _advances[i]) {
						if (point <= _positions[i].x + _advances[i] / 2.0) return ci;
						else return ci + 1;
						break;
					}
					if ((_flags[i] & 1) == 0) ci++;
				}
				return _ends.Length();
			}
			virtual int EndOfChar(int index) noexcept override { if (index < 0) return 0; return _ends[index]; }
			virtual int GetStringLength(void) noexcept override { return _ends.Length(); }
			virtual void SetCharPalette(const Color * colors, int count) override { _x_brushes.SetLength(count); for (int i = 0; i < count; i++) _x_brushes[i] = colors[i]; }
			virtual void SetCharColors(const uint8 * indicies, int count) override { if (_ends.Length() == count) _reset_ranges(indicies); }
			virtual void SetCharAdvances(const double * advances) override
			{
				if (advances) {
					int r = 0;
					for (int i = 0; i < _advances.Length(); i++) if (_flags[i] & 1) _advances[i] = 0.0; else { _advances[i] = advances[r]; r++; }
				} else {
					Array<CGSize> adv(1);
					adv.SetLength(_glyphs.Length());
					double v = 0.0;
					for (int i = 0; i < adv.Length(); i++) {
						if (_flags[i] & 2) {
							_fonts[i] = _font->GetAltFont();
							if (_flags[i] & 1) adv[i].width = 0.0; else {
								CTFontGetAdvancesForGlyphs(_font->GetAltFont(), kCTFontOrientationHorizontal, _glyphs.GetBuffer() + i, adv.GetBuffer() + i, 1);
								v += adv[i].width;
							}
						} else {
							_fonts[i] = _font->GetMainFont();
							if (_glyphs[i]) {
								if (_flags[i] & 1) adv[i].width = 0.0; else {
									CTFontGetAdvancesForGlyphs(_font->GetMainFont(), kCTFontOrientationHorizontal, _glyphs.GetBuffer() + i, adv.GetBuffer() + i, 1);
									v += adv[i].width;
								}
							} else if (_flags[i] & 1) {
								_glyphs[i] = _space;
							} else {
								_glyphs[i] = _space;
								int w = int(int64(v) + 4 * _font->GetWidth()) / int64(max(int(4 * _font->GetWidth()), 1));
								double lv = v;
								v = double(4 * _font->GetWidth() * w);
								adv[i].width = v - lv;		
							}
						}
					}
					for (int i = 0; i < adv.Length(); i++) _advances[i] = adv[i].width;
				}
				if (_advances.Length()) {
					_positions[0].x = _positions[0].y = 0.0;
					for (int i = 1; i < _advances.Length(); i++) {
						_positions[i].x = _positions[i - 1].x + _advances[i - 1];
						_positions[i].y = 0.0;
					}
					_run_length = int(_positions.LastElement().x + _advances.LastElement());
				} else _run_length = 0;
				int w = 0;
				for (int i = 0; i < _positions.Length(); i++) if ((_flags[i] & 1) == 0) {
					_ends[w] = int(_positions[i].x + _advances[i]);
					w++;
				}
			}
			virtual void GetCharAdvances(double * advances) noexcept override { int w = 0; for (int i = 0; i < _advances.Length(); i++) if ((_flags[i] & 1) == 0) { advances[w] = _advances[i]; w++; } }
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept override { return _parent; }
			virtual ImmutableString ToString(void) const override { return L"QuartzTextBrush"; }
		};

		CGRect QuartzMakeRect(const Box & box, int w, int h, int scale)
		{
			double s = double(scale);
			return CGRectMake(double(box.Left) / s, double(h - box.Bottom) / s, double(box.Right - box.Left) / s, double(box.Bottom - box.Top) / s);
		}

		QuartzDeviceContext::QuartzDeviceContext(void) : _color_cache(0x80), _bitmap_target_allowed(false), _context(0), _width(0), _height(0), _scale(0), _time(0), _ref_time(0), _blink_time(1000), _blink_htime(500) {}
		QuartzDeviceContext::~QuartzDeviceContext(void) { if (_bitmap_target_allowed && _context) CGContextRelease(reinterpret_cast<CGContextRef>(_context)); }
		void * QuartzDeviceContext::GetContext(void) const noexcept { return _context; }
		void QuartzDeviceContext::SetContext(void * context, int width, int height, int scale) noexcept
		{
			_context = context; _width = width; _height = height; _scale = scale;
			CGContextSetInterpolationQuality(reinterpret_cast<CGContextRef>(_context), kCGInterpolationNone);
		}
		void QuartzDeviceContext::SetBitmapTarget(bool set) noexcept { _bitmap_target_allowed = set; }
		void QuartzDeviceContext::GetImplementationInfo(string & tech, uint32 & version_major, uint32 & version_minor) { try { tech = L"Quartz"; } catch (...) {} version_major = 1; version_minor = 0; }
		uint32 QuartzDeviceContext::GetFeatureList(void) noexcept
		{
			uint32 result = DeviceContextFeatureInversionCapable | DeviceContextFeaturePolygonCapable | DeviceContextFeatureLayersCapable;
			if (_bitmap_target_allowed) result |= DeviceContextFeatureBitmapTarget;
			return result;
		}
		ImmutableString QuartzDeviceContext::ToString(void) const { return L"QuartzDeviceContext"; }
		Graphics::IColorBrush * QuartzDeviceContext::CreateSolidColorBrush(Color color) noexcept
		{
			auto cached = _color_cache.GetObjectByKey(color);
			if (cached) { cached->Retain(); return cached; }
			try {
				SafePointer<QuartzColorBrush> result = new QuartzColorBrush(this, color);
				try { _color_cache.Push(color, result); } catch (...) {}
				result->Retain();
				return result;
			} catch (...) { return 0; }
		}
		Graphics::IColorBrush * QuartzDeviceContext::CreateGradientBrush(Point rel_from, Point rel_to, const GradientPoint * points, int count) noexcept
		{
			if (count < 1) return 0;
			if (count == 1) return CreateSolidColorBrush(points[0].Value);
			try { return new QuartzColorBrush(this, points, count, rel_from, rel_to); } catch (...) { return 0; }
		}
		Graphics::IBlurEffectBrush * QuartzDeviceContext::CreateBlurEffectBrush(double power) noexcept { return 0; }
		Graphics::IInversionEffectBrush * QuartzDeviceContext::CreateInversionEffectBrush(void) noexcept
		{
			if (_inversion_cache) { _inversion_cache->Retain(); return _inversion_cache; }
			try {
				_inversion_cache = new QuartzInversionBrush(this);
				_inversion_cache->Retain();
				return _inversion_cache;
			} catch (...) { return 0; }
		}
		Graphics::IBitmapBrush * QuartzDeviceContext::CreateBitmapBrush(Graphics::IBitmap * bitmap, const Box & area, bool tile) noexcept
		{
			if (!bitmap) return 0;
			CGImageRef fragment;
			CGImageRef image = static_cast<QuartzBitmap *>(bitmap)->GetCoreImage();
			Point size = Point(area.Right - area.Left, area.Bottom - area.Top);
			if (size.x <= 0 || size.y <= 0) return 0;
			if (area.Left == 0 && area.Top == 0 && area.Right == bitmap->GetWidth() && area.Bottom == bitmap->GetHeight()) {
				fragment = image;
				CGImageRetain(fragment);
			} else {
				auto rect = CGRectMake(double(area.Left), double(area.Top), double(area.Right - area.Left), double(area.Bottom - area.Top));
				fragment = CGImageCreateWithImageInRect(image, rect);
				if (!fragment) return 0;
			}
			SafePointer<QuartzBitmapBrush> result;
			try { result = new QuartzBitmapBrush(this, fragment, size, tile); } catch (...) {}
			CGImageRelease(fragment);
			if (result) { result->Retain(); return result; } else return 0;
		}
		Graphics::IBitmapBrush * QuartzDeviceContext::CreateTextureBrush(Graphics::ITexture * texture, Graphics::TextureAlphaMode mode) noexcept { return 0; }
		Graphics::ITextBrush * QuartzDeviceContext::CreateTextBrush(Graphics::IFont * font, const string & text, uint32 horizontal_align, uint32 vertical_align, const Color & color) noexcept
		{
			if (!font) return 0;
			try {
				auto norm = text.NormalizedForm();
				return new QuartzTextBrush(this, font, reinterpret_cast<const uint32 *>(static_cast<const widechar *>(norm)), norm.Length(), horizontal_align, vertical_align, color);
			} catch (...) { return 0; }
		}
		Graphics::ITextBrush * QuartzDeviceContext::CreateTextBrush(Graphics::IFont * font, const uint32 * ucs, int length, uint32 horizontal_align, uint32 vertical_align, const Color & color) noexcept
		{
			if (!font) return 0;
			try {
				auto text = string(ucs, length, Encoding::UTF32);
				return CreateTextBrush(font, text, horizontal_align, vertical_align, color);
			} catch (...) { return 0; }
		}
		void QuartzDeviceContext::ClearInternalCache(void) noexcept { _color_cache.Clear(); _inversion_cache.SetReference(0); }
		void QuartzDeviceContext::PushClip(const Box & rect) noexcept
		{
			auto clip = _clipping.IsEmpty() ? rect : Box::Intersect(rect, _clipping.GetLast()->GetValue());
			try { _clipping.InsertLast(clip); } catch (...) { return; }
			CGContextSaveGState(reinterpret_cast<CGContextRef>(_context));
			CGContextClipToRect(reinterpret_cast<CGContextRef>(_context), QuartzMakeRect(clip, _width, _height, _scale));
		}
		void QuartzDeviceContext::PopClip(void) noexcept
		{
			_clipping.RemoveLast();
			CGContextRestoreGState(reinterpret_cast<CGContextRef>(_context));
		}
		void QuartzDeviceContext::BeginLayer(const Box & rect, double opacity) noexcept
		{
			CGContextSaveGState(reinterpret_cast<CGContextRef>(_context));
			CGContextClipToRect(reinterpret_cast<CGContextRef>(_context), QuartzMakeRect(rect, _width, _height, _scale));
			CGContextSetAlpha(reinterpret_cast<CGContextRef>(_context), CGFloat(opacity));
			CGContextBeginTransparencyLayer(reinterpret_cast<CGContextRef>(_context), 0);
		}
		void QuartzDeviceContext::EndLayer(void) noexcept
		{
			CGContextEndTransparencyLayer(reinterpret_cast<CGContextRef>(_context));
			CGContextRestoreGState(reinterpret_cast<CGContextRef>(_context));
		}
		void QuartzDeviceContext::Render(Graphics::IColorBrush * brush, const Box & at) noexcept
		{
			if (!brush || at.Left >= at.Right || at.Top >= at.Bottom) return;
			auto info = static_cast<QuartzColorBrush *>(brush);
			CGGradientRef gradient;
			if (gradient = info->GetGradient()) {
				CGPoint from, to;
				double scale = double(_scale);
				from.x = (info->GetBegin().x + at.Left) / scale;
				from.y = (_height - info->GetBegin().y - at.Top) / scale;
				to.x = (info->GetEnd().x + at.Left) / scale;
				to.y = (_height - info->GetEnd().y - at.Top) / scale;
				PushClip(at);
				CGContextDrawLinearGradient(reinterpret_cast<CGContextRef>(_context), gradient, from, to, kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation);
				PopClip();
			} else {
				auto & color = info->GetSolidColor();
				CGContextSetRGBFillColor(reinterpret_cast<CGContextRef>(_context), color.r, color.g, color.b, color.a);
				CGContextFillRect(reinterpret_cast<CGContextRef>(_context), QuartzMakeRect(at, _width, _height, _scale));
			}
		}
		void QuartzDeviceContext::Render(Graphics::IBitmapBrush * brush, const Box & at) noexcept
		{
			if (!brush || at.Left >= at.Right || at.Top >= at.Bottom) return;
			auto info = static_cast<QuartzBitmapBrush *>(brush);
			if (info->IsTiled()) {
				auto size = info->GetSize();
				auto scale = double(_scale);
				PushClip(at);
				CGContextDrawTiledImage(reinterpret_cast<CGContextRef>(_context), CGRectMake(0.0f, 0.0f, double(size.x) / scale, double(size.y) / scale), info->GetImage());
				PopClip();
			} else CGContextDrawImage(reinterpret_cast<CGContextRef>(_context), QuartzMakeRect(at, _width, _height, _scale), info->GetImage());
		}
		void QuartzDeviceContext::Render(Graphics::ITextBrush * brush, const Box & at, bool clip) noexcept
		{
			if (!brush || at.Left >= at.Right || at.Top >= at.Bottom) return;
			auto info = static_cast<QuartzTextBrush *>(brush);
			if (!info->_advances.Length()) return;
			auto rect = QuartzMakeRect(at, _width, _height, _scale);
			if (clip) {
				CGContextSaveGState(reinterpret_cast<CGContextRef>(_context));
				CGContextClipToRect(reinterpret_cast<CGContextRef>(_context), rect);
			}
			int width, height;
			info->GetExtents(width, height);
			int shift_x = 0;
			int shift_y = (info->_font->GetHeight() - info->_font->GetScaledHeight()) / 2 / double(_scale);
			if (info->_halign == 1) shift_x += (at.Right - at.Left - width) / 2;
			else if (info->_halign == 2) shift_x += (at.Right - at.Left - width);
			if (info->_valign == 1) shift_y += (at.Bottom - at.Top - height) / 2;
			else if (info->_valign == 0) shift_y += (at.Bottom - at.Top - height);
			CGAffineTransform trans;
			trans.a = trans.d = 1.0 / double(_scale); trans.b = trans.c = 0.0;
			trans.tx = rect.origin.x + double(shift_x) / double(_scale);
			trans.ty = rect.origin.y + double(shift_y + info->_baseline) / double(_scale);
			if (info->_hl_start != -1) {
				double start = double(info->EndOfChar(info->_hl_start - 1));
				double end = double(info->EndOfChar(info->_hl_end - 1));
				CGContextSetRGBFillColor(reinterpret_cast<CGContextRef>(_context), info->_back_color.r, info->_back_color.g, info->_back_color.b, info->_back_color.a);
				CGContextFillRect(reinterpret_cast<CGContextRef>(_context), CGRectMake(trans.tx + start / double(_scale),
					rect.origin.y, (end - start) / double(_scale), rect.size.height));
			}
			for (auto & r : info->_ranges) {
				CGContextSetFontSize(reinterpret_cast<CGContextRef>(_context), info->_font->GetScaledHeight());
				CGContextSetTextDrawingMode(reinterpret_cast<CGContextRef>(_context), kCGTextFill);
				CGContextSetTextPosition(reinterpret_cast<CGContextRef>(_context), 0.0, 0.0);
				CGContextSetTextMatrix(reinterpret_cast<CGContextRef>(_context), trans);
				CGContextSetRGBFillColor(reinterpret_cast<CGContextRef>(_context), r.color.r, r.color.g, r.color.b, r.color.a);
				CTFontDrawGlyphs(r.font, info->_glyphs.GetBuffer() + r.left, info->_positions.GetBuffer() + r.left, r.right - r.left, reinterpret_cast<CGContextRef>(_context));
			}
			if (info->_font->IsUnderline()) {
				CGContextSetRGBFillColor(reinterpret_cast<CGContextRef>(_context), info->_text_color.r, info->_text_color.g, info->_text_color.b, info->_text_color.a);
				CGContextFillRect(reinterpret_cast<CGContextRef>(_context), CGRectMake(trans.tx, trans.ty + (info->_underline_offs - info->_underline_hw) / double(_scale),
					double(info->_run_length) / double(_scale), 2.0 * info->_underline_hw / double(_scale)));
			}
			if (info->_font->IsStrikeout()) {
				CGContextSetRGBFillColor(reinterpret_cast<CGContextRef>(_context), info->_text_color.r, info->_text_color.g, info->_text_color.b, info->_text_color.a);
				CGContextFillRect(reinterpret_cast<CGContextRef>(_context), CGRectMake(trans.tx, trans.ty + (info->_strikeout_offs - info->_strikeout_hw) / double(_scale),
					double(info->_run_length) / double(_scale), 2.0 * info->_strikeout_hw / double(_scale)));
			}
			if (clip) CGContextRestoreGState(reinterpret_cast<CGContextRef>(_context));
		}
		void QuartzDeviceContext::Render(Graphics::IBlurEffectBrush * brush, const Box & at) noexcept {}
		void QuartzDeviceContext::Render(Graphics::IInversionEffectBrush * brush, const Box & at, bool blink) noexcept
		{
			if (!brush || at.Left >= at.Right || at.Top >= at.Bottom) return;
			auto info = static_cast<QuartzInversionBrush *>(brush);
			if (!blink || IsCaretVisible()) {
				CGContextSetBlendMode(reinterpret_cast<CGContextRef>(_context), kCGBlendModeDifference);
				CGContextDrawImage(reinterpret_cast<CGContextRef>(_context), QuartzMakeRect(at, _width, _height, _scale), info->GetBitmap()->GetCoreImage());
				CGContextSetBlendMode(reinterpret_cast<CGContextRef>(_context), kCGBlendModeNormal);
			}
		}
		void QuartzDeviceContext::RenderPolyline(const Math::Vector2 * points, int count, Color color, double width) noexcept
		{
			CGContextSetRGBStrokeColor(reinterpret_cast<CGContextRef>(_context), color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
			CGContextSetLineWidth(reinterpret_cast<CGContextRef>(_context), width / double(_scale));
			CGContextSetLineJoin(reinterpret_cast<CGContextRef>(_context), kCGLineJoinRound);
			CGContextBeginPath(reinterpret_cast<CGContextRef>(_context));
			if (count) {
				CGContextMoveToPoint(reinterpret_cast<CGContextRef>(_context), points[0].x / _scale, (double(_height) - points[0].y) / _scale);
				for (int i = 1; i < count; i++) CGContextAddLineToPoint(reinterpret_cast<CGContextRef>(_context), points[i].x / double(_scale), (double(_height) - points[i].y) / double(_scale));
			}
			CGContextStrokePath(reinterpret_cast<CGContextRef>(_context));
		}
		void QuartzDeviceContext::RenderPolygon(const Math::Vector2 * points, int count, Color color) noexcept
		{
			CGContextSetRGBFillColor(reinterpret_cast<CGContextRef>(_context), color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
			CGContextBeginPath(reinterpret_cast<CGContextRef>(_context));
			if (count) {
				CGContextMoveToPoint(reinterpret_cast<CGContextRef>(_context), points[0].x / _scale, (double(_height) - points[0].y) / _scale);
				for (int i = 1; i < count; i++) CGContextAddLineToPoint(reinterpret_cast<CGContextRef>(_context), points[i].x / double(_scale), (double(_height) - points[i].y) / double(_scale));
			}
			CGContextFillPath(reinterpret_cast<CGContextRef>(_context));
		}
		void QuartzDeviceContext::SetAnimationTime(uint32 value) noexcept { _time = value; }
		uint32 QuartzDeviceContext::GetAnimationTime(void) noexcept { return _time; }
		void QuartzDeviceContext::SetCaretReferenceTime(uint32 value) noexcept { _ref_time = value; }
		uint32 QuartzDeviceContext::GetCaretReferenceTime(void) noexcept { return _ref_time; }
		void QuartzDeviceContext::SetCaretBlinkPeriod(uint32 value) noexcept { _blink_time = value; _blink_htime = value / 2; }
		uint32 QuartzDeviceContext::GetCaretBlinkPeriod(void) noexcept { return _blink_time; }
		bool QuartzDeviceContext::IsCaretVisible(void) noexcept { return (((_time - _ref_time) / _blink_htime) & 1) == 0; }
		Graphics::IDevice * QuartzDeviceContext::GetParentDevice(void) noexcept { return 0; }
		Graphics::I2DDeviceContextFactory * QuartzDeviceContext::GetParentFactory(void) noexcept { return GetCommonDeviceContextFactory(); }
		bool QuartzDeviceContext::BeginRendering(Graphics::IBitmap * dest) noexcept
		{
			if (!dest || !_bitmap_target_allowed || _bitmap_target) return false;
			auto surface = GetBitmapSurface(dest);
			auto width = surface->GetWidth();
			auto height = surface->GetHeight();
			auto rgb_cs = CGColorSpaceCreateDeviceRGB();
			if (!rgb_cs) return false;
			auto context = CGBitmapContextCreateWithData(surface->GetData(), width, height, 8, surface->GetScanLineLength(), rgb_cs, kCGImageAlphaPremultipliedLast, 0, 0);
			CGColorSpaceRelease(rgb_cs);
			if (!context) return false;
			_bitmap_target.SetRetain(dest);
			SetContext(context, width, height, 1);
			return true;
		}
		bool QuartzDeviceContext::BeginRendering(Graphics::IBitmap * dest, Color clear_color) noexcept
		{
			if (!dest || !_bitmap_target_allowed || _bitmap_target) return false;
			auto surface = GetBitmapSurface(dest);
			Color color_prem;
			color_prem.r = int(clear_color.r) * int(clear_color.a) / 255;
			color_prem.g = int(clear_color.g) * int(clear_color.a) / 255;
			color_prem.b = int(clear_color.b) * int(clear_color.a) / 255;
			color_prem.a = clear_color.a;
			for (int y = 0; y < surface->GetHeight(); y++) for (int x = 0; x < surface->GetWidth(); x++) surface->SetPixel(x, y, color_prem.Value);
			return BeginRendering(dest);
		}
		bool QuartzDeviceContext::EndRendering(void) noexcept
		{
			if (!_bitmap_target) return false;
			_clipping.Clear();
			CGContextFlush(reinterpret_cast<CGContextRef>(_context));
			CGContextRelease(reinterpret_cast<CGContextRef>(_context));
			_context = 0;
			auto status = static_cast<QuartzBitmap *>(_bitmap_target.Inner())->Synchronize();
			_bitmap_target.SetReference(0);
			return status;
		}

		Codec::Frame * GetBitmapSurface(Graphics::IBitmap * bitmap) { return static_cast<QuartzBitmap *>(bitmap)->GetSurface(); }
		void * GetBitmapCoreImage(Graphics::IBitmap * bitmap) { return static_cast<QuartzBitmap *>(bitmap)->GetCoreImage(); }
		QuartzDeviceContextFactory * GetCommonDeviceContextFactory(void)
		{
			if (!common_factory) try { common_factory = new QuartzDeviceContextFactory; } catch (...) { return 0; }
			return common_factory;
		}
	}
}