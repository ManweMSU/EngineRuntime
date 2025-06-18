#include "Direct2D.h"

#include <d2d1_1helper.h>
#include <dwrite.h>

#include <math.h>

#include "ComInterop.h"
#include "Direct3D.h"
#include "SystemCodecs.h"

#include "../Math/Color.h"
#include "../Miscellaneous/DynamicString.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dxguid.lib")

#undef max

using namespace Engine::Codec;
using namespace Engine::Streaming;
using namespace Engine::Graphics;

namespace Engine
{
	namespace Direct2D
	{
		ID2D1Factory1 * D2DFactory1 = 0;
		ID2D1Factory * D2DFactory = 0;
		IDWriteFactory * DWriteFactory = 0;
		SafePointer<Graphics::I2DDeviceContextFactory> CommonFactory;

		void InitializeFactory(void)
		{
			if (!D2DFactory) {
				if (D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &D2DFactory1) != S_OK) {
					D2DFactory1 = 0;
					if (D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &D2DFactory) != S_OK) {
						D2DFactory = 0;
						throw Exception();
					}
				} else {
					D2DFactory = D2DFactory1;
				}
			}
			if (!DWriteFactory) {
				if (DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&DWriteFactory)) != S_OK) throw Exception();
			}
			if (!WIC::WICFactory) WIC::CreateWICodec();
			CommonFactory = new D2D_DeviceContextFactory;
		}
		void ShutdownFactory(void)
		{
			if (D2DFactory) { D2DFactory->Release(); D2DFactory = 0; }
			if (DWriteFactory) { DWriteFactory->Release(); DWriteFactory = 0; }
			CommonFactory.SetReference(0);
		}

		class WIC_BitmapLink : public IBitmapLink
		{
			IBitmap * _bitmap;
		public:
			WIC_BitmapLink(IBitmap * bitmap) : _bitmap(bitmap) {}
			void Invalidate(void) noexcept { _bitmap = 0; }
			virtual IBitmap * GetBitmap(void) const noexcept override { return _bitmap; }
			virtual string ToString(void) const override { return L"WIC_BitmapLink"; }
		};
		class WIC_Bitmap : public IBitmap
		{
			SafePointer<WIC_BitmapLink> _link;
			Volumes::ObjectDictionary<I2DDeviceContext *, IDeviceBitmap> _versions;
			IWICBitmap * _bitmap;
			int _w, _h;
		public:
			static IWICBitmap * CreateWICBitmap(Codec::Frame * frame) noexcept
			{
				IWICBitmap * result;
				if (WIC::WICFactory->CreateBitmap(frame->GetWidth(), frame->GetHeight(), GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &result) != S_OK) return 0;
				SafePointer<Frame> conv;
				try {
					if (frame->GetPixelFormat() == Codec::PixelFormat::B8G8R8A8 && frame->GetAlphaMode() == AlphaMode::Premultiplied && frame->GetScanOrigin() == ScanOrigin::TopDown) {
						conv.SetRetain(frame);
					} else {
						conv = frame->ConvertFormat(Codec::PixelFormat::B8G8R8A8, AlphaMode::Premultiplied, ScanOrigin::TopDown);
					}
				} catch (...) {
					result->Release();
					return 0;
				}
				IWICBitmapLock * lock;
				if (result->Lock(0, WICBitmapLockRead | WICBitmapLockWrite, &lock) != S_OK) { result->Release(); return 0; }
				UINT length, stride;
				uint8 * pdata;
				if (lock->GetDataPointer(&length, &pdata) != S_OK) { lock->Release(); result->Release(); return 0; }
				if (lock->GetStride(&stride) != S_OK) { lock->Release(); result->Release(); return 0; }
				for (int y = 0; y < conv->GetHeight(); y++) {
					auto org = conv->GetData() + y * intptr(conv->GetScanLineLength());
					auto dest = pdata + y * intptr(stride);
					MemoryCopy(dest, org, intptr(conv->GetWidth()) * 4);
				}
				lock->Release();
				return result;
			}
			static IWICBitmap * CreateWICBitmap(int width, int height, Color color) noexcept
			{
				IWICBitmap * result;
				if (WIC::WICFactory->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &result) != S_OK) return 0;
				IWICBitmapLock * lock;
				if (result->Lock(0, WICBitmapLockRead | WICBitmapLockWrite, &lock) != S_OK) { result->Release(); return 0; }
				UINT length, stride;
				uint8 * pdata;
				if (lock->GetDataPointer(&length, &pdata) != S_OK) { lock->Release(); result->Release(); return 0; }
				if (lock->GetStride(&stride) != S_OK) { lock->Release(); result->Release(); return 0; }
				color.r = int(color.r) * int(color.a) / 255;
				color.g = int(color.g) * int(color.a) / 255;
				color.b = int(color.b) * int(color.a) / 255;
				swap(color.r, color.b);
				for (int y = 0; y < height; y++) for (int x = 0; x < width; x++) {
					auto dest = pdata + y * intptr(stride) + intptr(x) * 4;
					*reinterpret_cast<uint32 *>(dest) = color.Value;
				}
				lock->Release();
				return result;
			}
			WIC_Bitmap(IWICBitmap * bitmap, int w, int h) : _w(w), _h(h), _bitmap(bitmap) { _link = new WIC_BitmapLink(this); if (_bitmap) _bitmap->AddRef(); }
			virtual ~WIC_Bitmap(void) override { _link->Invalidate(); if (_bitmap) _bitmap->Release(); }
			virtual int GetWidth(void) const noexcept override { return _w; }
			virtual int GetHeight(void) const noexcept override { return _h; }
			virtual bool Reload(Codec::Frame * source) noexcept override
			{
				if (!source) return false;
				auto new_bitmap = CreateWICBitmap(source);
				if (!new_bitmap) return false;
				UpdateSurface(new_bitmap, source->GetWidth(), source->GetHeight());
				new_bitmap->Release();
				for (auto dv : _versions) dv.value->Reload();
				return true;
			}
			virtual bool AddDeviceBitmap(IDeviceBitmap * bitmap, I2DDeviceContext * device_for) noexcept override { try { return _versions.Append(device_for, bitmap); } catch (...) { return false; } }
			virtual bool RemoveDeviceBitmap(I2DDeviceContext * device_for) noexcept override { _versions.Remove(device_for); return true; }
			virtual IDeviceBitmap * GetDeviceBitmap(I2DDeviceContext * device_for) const noexcept override { return _versions.GetObjectByKey(device_for); }
			virtual IBitmapLink * GetLinkObject(void) const noexcept override { return _link; }
			virtual Codec::Frame * QueryFrame(void) const noexcept override
			{
				if (!_bitmap) return 0;
				SafePointer<Codec::Frame> result;
				try { result = new Frame(_w, _h, Codec::PixelFormat::B8G8R8A8, Codec::AlphaMode::Premultiplied, Codec::ScanOrigin::TopDown); } catch (...) { return 0; }
				IWICBitmapLock * lock;
				if (_bitmap->Lock(0, WICBitmapLockRead, &lock) != S_OK) return 0;
				UINT length, stride;
				uint8 * pdata;
				if (lock->GetDataPointer(&length, &pdata) != S_OK) { lock->Release(); return 0; }
				if (lock->GetStride(&stride) != S_OK) { lock->Release(); return 0; }
				for (int y = 0; y < _h; y++) {
					auto dest = result->GetData() + y * intptr(result->GetScanLineLength());
					auto org = pdata + y * intptr(stride);
					MemoryCopy(dest, org, intptr(_w) * 4);
				}
				lock->Release();
				result->Retain();
				return result;
			}
			virtual string ToString(void) const override { return L"WIC_Bitmap"; }
			void UpdateSurface(IWICBitmap * bitmap, int w, int h) noexcept { if (_bitmap) _bitmap->Release(); _bitmap = bitmap; if (_bitmap) _bitmap->AddRef(); _w = w; _h = h; }
			void SyncDeviceVersions(void) noexcept { for (auto dv : _versions) dv.value->Reload(); }
			IWICBitmap * GetSurface(void) const noexcept { return _bitmap; }
		};
		class D2D_Bitmap : public IDeviceBitmap
		{
			WIC_Bitmap * _base;
			D2D_DeviceContext * _context;
			ID2D1Bitmap * _bitmap;
			int _w, _h;
		public:
			D2D_Bitmap(WIC_Bitmap * bitmap, D2D_DeviceContext * context) : _base(bitmap), _context(context), _bitmap(0)
			{
				if (bitmap) { _w = bitmap->GetWidth(); _h = bitmap->GetHeight(); } else _w = _h = 0;
				if (!Reload()) throw Exception();
			}
			D2D_Bitmap(ID2D1Bitmap * bitmap, D2D_DeviceContext * context, int w, int h) : _base(0), _context(context), _bitmap(bitmap)
			{
				_w = w; _h = h;
				if (_bitmap) _bitmap->AddRef();
			}
			virtual ~D2D_Bitmap(void) override { if (_bitmap) _bitmap->Release(); }
			virtual int GetWidth(void) const noexcept override { return _w; }
			virtual int GetHeight(void) const noexcept override { return _h; }
			virtual bool Reload(void) noexcept override
			{
				auto device = _context->GetRenderTarget();
				ID2D1Bitmap * new_bitmap;
				if (!device || !_base) return false;
				if (device->CreateBitmapFromWicBitmap(_base->GetSurface(), &new_bitmap) != S_OK) return false;
				if (_bitmap) _bitmap->Release();
				_bitmap = new_bitmap;
				return true;
			}
			virtual IBitmap * GetParentBitmap(void) const noexcept override { return _base; }
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept override { return _context; }
			virtual string ToString(void) const override { return L"D2D_Bitmap"; }
			ID2D1Bitmap * GetSurface(void) const noexcept { return _bitmap; }
		};
		class DW_Font : public Graphics::IFont
		{
			IDWriteFontFace * _font_face, * _alt_face;
			string _font_name;
			int _height, _weight, _actual_height;
			bool _italic, _underline, _strikeout;
			DWRITE_FONT_METRICS _font_metrics, _alt_metrics;
		public:
			DW_Font(const string & face_name, int height, int weight, bool italic, bool underline, bool strikeout)
			{
				_font_name = face_name;
				_actual_height = height;
				_height = int(double(height) * 72.0 / 96.0);
				_weight = weight;
				_italic = italic;
				_underline = underline;
				_strikeout = strikeout;
				IDWriteFontCollection * collection;
				if (DWriteFactory->GetSystemFontCollection(&collection) != S_OK) throw Exception();
				uint32 index;
				int exists;
				if (collection->FindFamilyName(face_name, &index, &exists) != S_OK) { collection->Release(); throw Exception(); }
				if (!exists) { collection->Release(); throw Exception(); }
				IDWriteFontFamily * family;
				if (collection->GetFontFamily(index, &family) != S_OK) { collection->Release(); throw Exception(); }
				IDWriteFont * source;
				if (family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT(_weight), DWRITE_FONT_STRETCH_NORMAL, _italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL, &source) != S_OK) { family->Release(); collection->Release(); throw Exception(); }
				if (source->CreateFontFace(&_font_face) != S_OK) { source->Release(); family->Release(); collection->Release(); throw Exception(); }
				source->Release();
				family->Release();
				if (_font_face->GetGdiCompatibleMetrics(float(_actual_height), 1.0f, 0, &_font_metrics) != S_OK) { _font_face->Release(); collection->Release(); throw Exception(); }
				if (collection->FindFamilyName(L"Segoe UI Emoji", &index, &exists) == S_OK && exists) {
					if (collection->GetFontFamily(index, &family) == S_OK) {
						if (family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT(_weight), DWRITE_FONT_STRETCH_NORMAL, _italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL, &source) == S_OK) {
							if (source->CreateFontFace(&_alt_face) == S_OK) {
								if (_alt_face->GetGdiCompatibleMetrics(float(_actual_height), 1.0f, 0, &_alt_metrics) != S_OK) _alt_metrics = _font_metrics;
							} else {
								_alt_face = 0;
								_alt_metrics = _font_metrics;
							}
							source->Release();
						} else {
							_alt_face = 0;
							_alt_metrics = _font_metrics;
						}
						family->Release();
					} else {
						_alt_face = 0;
						_alt_metrics = _font_metrics;
					}
				} else {
					_alt_face = 0;
					_alt_metrics = _font_metrics;
				}
				collection->Release();
			}
			virtual ~DW_Font(void) override { if (_font_face) _font_face->Release(); if (_alt_face) _alt_face->Release(); }
			float UnitsToDIP(int units) const noexcept { return float(units) * float(_height) / float(_font_metrics.designUnitsPerEm); }
			float AltUnitsToDIP(int units) const noexcept { return float(units) * float(_height) / float(_alt_metrics.designUnitsPerEm); }
			virtual int GetWidth(void) const noexcept override
			{
				uint16 space_glyph;
				uint32 space = 0x20;
				_font_face->GetGlyphIndicesW(&space, 1, &space_glyph);
				DWRITE_GLYPH_METRICS space_metrics;
				_font_face->GetGdiCompatibleGlyphMetrics(float(_height), 1.0f, 0, TRUE, &space_glyph, 1, &space_metrics);
				return UnitsToDIP(space_metrics.advanceWidth);
			}
			virtual int GetHeight(void) const noexcept override { return _actual_height; }
			virtual int GetLineSpacing(void) const noexcept override { return UnitsToDIP(_font_metrics.ascent + _font_metrics.descent + _font_metrics.lineGap); }
			virtual int GetBaselineOffset(void) const noexcept override { return UnitsToDIP(_font_metrics.ascent); }
			virtual string ToString(void) const override { return L"DW_Font"; }
			bool IsUnderlined(void) const noexcept { return _underline; }
			bool IsStrikedout(void) const noexcept { return _strikeout; }
			int GetUnitHeight(void) const noexcept { return _height; }
			IDWriteFontFace * GetMainFont(void) const noexcept { return _font_face; }
			IDWriteFontFace * GetAltFont(void) const noexcept { return _alt_face; }
			DWRITE_FONT_METRICS & GetMainMetrics(void) noexcept { return _font_metrics; }
			DWRITE_FONT_METRICS & GetAltMetrics(void) noexcept { return _alt_metrics; }
		};

		string D2D_DeviceContextFactory::ToString(void) const { return L"D2D_DeviceContextFactory"; }
		IBitmap * D2D_DeviceContextFactory::CreateBitmap(int width, int height, Color clear_color) noexcept
		{
			auto surface = WIC_Bitmap::CreateWICBitmap(width, height, clear_color);
			if (!surface) return 0;
			auto result = new (std::nothrow) WIC_Bitmap(surface, width, height);
			surface->Release();
			return result;
		}
		IBitmap * D2D_DeviceContextFactory::LoadBitmap(Codec::Frame * source) noexcept
		{
			if (!source) return 0;
			auto surface = WIC_Bitmap::CreateWICBitmap(source);
			if (!surface) return 0;
			auto result = new (std::nothrow) WIC_Bitmap(surface, source->GetWidth(), source->GetHeight());
			surface->Release();
			return result;
		}
		Graphics::IFont * D2D_DeviceContextFactory::LoadFont(const string & face_name, int height, int weight, bool italic, bool underline, bool strikeout) noexcept
		{
			if (face_name[0] == L'@') {
				bool serif = false;
				bool sans = false;
				bool mono = false;
				auto words = face_name.Fragment(1, -1).Split(L' ');
				for (auto & w : words) if (w.Length()) {
					if (string::CompareIgnoreCase(w, FontWordSerif) == 0) serif = true;
					else if (string::CompareIgnoreCase(w, FontWordSans) == 0) sans = true;
					else if (string::CompareIgnoreCase(w, FontWordMono) == 0) mono = true;
					else return 0;
				}
				if (!serif) return 0;
				if (mono) {
					if (sans) return LoadFont(L"Consolas", height, weight, italic, underline, strikeout);
					else return LoadFont(L"Courier New", height, weight, italic, underline, strikeout);
				} else {
					if (sans) return LoadFont(L"Segoe UI", height, weight, italic, underline, strikeout);
					else return LoadFont(L"Times New Roman", height, weight, italic, underline, strikeout);
				}
			} else { try { return new DW_Font(face_name, height, weight, italic, underline, strikeout); } catch (...) { return 0; } }
		}
		Array<string> * D2D_DeviceContextFactory::GetFontFamilies(void) noexcept
		{
			SafePointer<IDWriteFontCollection> collection;
			if (Direct2D::DWriteFactory && Direct2D::DWriteFactory->GetSystemFontCollection(collection.InnerRef()) == S_OK) {
				uint count = collection->GetFontFamilyCount();
				SafePointer< Array<string> > result = new (std::nothrow) Array<string>(int(count));
				if (!result) {
					collection->Release();
					return 0;
				}
				for (uint i = 0; i < count; i++) {
					SafePointer<IDWriteFontFamily> family;
					if (collection->GetFontFamily(i, family.InnerRef()) == S_OK) {
						SafePointer<IDWriteLocalizedStrings> strings;
						if (family->GetFamilyNames(strings.InnerRef()) == S_OK) {
							UINT32 index, length;
							BOOL exists;
							if (strings->FindLocaleName(L"en-us", &index, &exists) != S_OK || !exists) index = 0;
							if (strings->GetStringLength(index, &length) == S_OK) {
								try {
									DynamicString str;
									str.ReserveLength(length + 1);
									strings->GetString(index, str, length + 1);
									result->Append(str.ToString());
								} catch (...) {}
							}
						}
					}
				}
				result->Retain();
				return result;
			}
			return 0;
		}
		IBitmapContext * D2D_DeviceContextFactory::CreateBitmapContext(void) noexcept
		{
			try {
				SafePointer<D2D_DeviceContext> dc = new D2D_DeviceContext;
				dc->SetBitmapContext(true);
				dc->Retain();
				return dc;
			} catch (...) { return 0; }
		}

		class D2D_ColorBrush : public IColorBrush
		{
			I2DDeviceContext * _parent;
			ID2D1Brush * _brush;
			ID2D1LinearGradientBrush * _gradient;
			D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props;
		public:
			D2D_ColorBrush(I2DDeviceContext * parent, ID2D1SolidColorBrush * brush) : _parent(parent), _brush(brush), _gradient(0) { _brush->AddRef(); }
			D2D_ColorBrush(I2DDeviceContext * parent, ID2D1LinearGradientBrush * brush) : _parent(parent), _brush(brush), _gradient(brush) { _brush->AddRef(); }
			virtual ~D2D_ColorBrush(void) override { _brush->Release(); }
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept override { return _parent; }
			virtual string ToString(void) const override { return L"D2D_ColorBrush"; }
			ID2D1Brush * GetBrush(void) const noexcept { return _brush; }
			ID2D1LinearGradientBrush * GetGradient(void) const noexcept { return _gradient; }
			D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES & GetGradientProperties(void) noexcept { return props; }
		};
		class D2D_BitmapBrush : public IBitmapBrush
		{
			I2DDeviceContext * _parent;
			SafePointer<D2D_Bitmap> _bitmap;
			ID2D1BitmapBrush * _brush;
			D2D1_RECT_F _area;
			bool _tile;
		public:
			D2D_BitmapBrush(D2D_DeviceContext * parent, D2D_Bitmap * bitmap, Box area, bool tile) : _parent(parent), _brush(0), _tile(tile)
			{
				_bitmap.SetRetain(bitmap);
				_area = D2D1::RectF(float(area.Left), float(area.Top), float(area.Right), float(area.Bottom));
				if (tile) {
					ID2D1Bitmap * fragment;
					if (area.Left == 0 && area.Top == 0 && area.Right == bitmap->GetWidth() && area.Bottom == bitmap->GetHeight()) {
						fragment = _bitmap->GetSurface();
						fragment->AddRef();
					} else {
						auto size = D2D1::SizeU(uint(area.Right - area.Left), uint(area.Bottom - area.Top));
						auto props = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 0.0f, 0.0f);
						auto rect = D2D1::RectU(area.Left, area.Top, area.Right, area.Bottom);
						if (parent->GetRenderTarget()->CreateBitmap(size, props, &fragment) != S_OK) throw Exception();
						if (fragment->CopyFromBitmap(&D2D1::Point2U(0, 0), _bitmap->GetSurface(), &rect) != S_OK) { fragment->Release(); throw Exception(); }
					}
					auto props = D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_WRAP, D2D1_EXTEND_MODE_WRAP, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
					if (parent->GetRenderTarget()->CreateBitmapBrush(fragment, props, &_brush) != S_OK) { fragment->Release(); throw Exception(); }
					fragment->Release();
				}
			}
			virtual ~D2D_BitmapBrush(void) override { if (_brush) _brush->Release(); }
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept override { return _parent; }
			virtual string ToString(void) const override { return L"D2D_BitmapBrush"; }
			void inline Render(ID2D1RenderTarget * target, const Box & at) const noexcept
			{
				D2D1_RECT_F to = D2D1::RectF(float(at.Left), float(at.Top), float(at.Right), float(at.Bottom));
				if (_tile) target->FillRectangle(to, _brush);
				else target->DrawBitmap(_bitmap->GetSurface(), to, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, _area);
			}
		};
		class D2D_TextBrush : public ITextBrush
		{
			struct _text_range
			{
				int left;
				int right;
				D2D_ColorBrush * brush;
			};

			D2D_DeviceContext * _parent;
			SafePointer<DW_Font> _font;
			int _valign, _halign, _hl_start, _hl_end;
			Array<uint32> _text_ucs;
			Array<uint16> _glyphs;
			Array<float> _advances;
			Array<uint8> _alt_face;
			Array<_text_range> _ranges;
			ID2D1PathGeometry * _geometry;
			uint16 _main_space_glyph, _alt_space_glyph;
			int _baseline, _run_length;
			float _underline_offs, _underline_hw;
			float _strikeout_offs, _strikeout_hw;
			SafePointer<D2D_ColorBrush> _main_brush;
			SafePointer<D2D_ColorBrush> _back_brush;
			ObjectArray<D2D_ColorBrush> _extra_brushes;

			void _fill_glyphs(void)
			{
				uint32 space = L' ';
				if (_font->GetMainFont()->GetGlyphIndicesW(&space, 1, &_main_space_glyph) != S_OK) throw Exception();
				if (_font->GetAltFont()) {
					if (_font->GetAltFont()->GetGlyphIndicesW(&space, 1, &_alt_space_glyph) != S_OK) _alt_space_glyph = _main_space_glyph;
				} else _alt_space_glyph = _main_space_glyph;
				if (_font->GetMainFont()->GetGlyphIndicesW(_text_ucs, _text_ucs.Length(), _glyphs) != S_OK) throw Exception();
				for (int i = 0; i < _alt_face.Length(); i++) {
					if (_text_ucs[i] == L'\t') {
						_glyphs[i] = _main_space_glyph;
						_alt_face[i] = false;
					} else {
						if (!_glyphs[i]) {
							auto af = _font->GetAltFont();
							if (af && af->GetGlyphIndicesW(_text_ucs.GetBuffer() + i, 1, _glyphs.GetBuffer() + i) == S_OK && _glyphs[i]) _alt_face[i] = true;
							else _alt_face[i] = false;
						} else _alt_face[i] = false;
					}
				}
			}
			void _fill_advances(void)
			{
				Array<DWRITE_GLYPH_METRICS> metrics(1);
				metrics.SetLength(_text_ucs.Length());
				auto ff = _font->GetMainFont();
				auto af = _font->GetAltFont();
				if (ff->GetGdiCompatibleGlyphMetrics(float(_font->GetUnitHeight()), 1.0f, 0, TRUE, _glyphs, _glyphs.Length(), metrics) != S_OK) throw Exception();
				if (af) for (int i = 0; i < _glyphs.Length(); i++) if (_alt_face[i]) {
					af->GetGdiCompatibleGlyphMetrics(float(_font->GetUnitHeight()), 1.0f, 0, TRUE, _glyphs.GetBuffer() + i, 1, metrics.GetBuffer() + i);
				}
				for (int i = 0; i < _glyphs.Length(); i++) _advances[i] = _alt_face[i] ? _font->AltUnitsToDIP(metrics[i].advanceWidth) : _font->UnitsToDIP(metrics[i].advanceWidth);
				_baseline = int(_font->UnitsToDIP(_font->GetMainMetrics().ascent));
				float run_length_f = 0.0f;
				int tab_width = _font->GetWidth() * 4;
				for (int i = 0; i < _glyphs.Length(); i++) {
					if (_text_ucs[i] == L'\t') {
						int64 align = ((int64(run_length_f) + tab_width) / tab_width) * tab_width;
						_advances[i] = float(align) - run_length_f;
						run_length_f = float(align);
					} else run_length_f += _advances[i];
				}
				_run_length = int(run_length_f + 0.9f);
			}
			void _build_geometry(void)
			{
				if (_geometry) { _geometry->Release(); _geometry = 0; }
				ID2D1GeometrySink * sink;
				if (D2DFactory->CreatePathGeometry(&_geometry) != S_OK) throw Exception();
				if (_geometry->Open(&sink) != S_OK) { _geometry->Release(); _geometry = 0; throw Exception(); }
				auto ssink = static_cast<ID2D1SimplifiedGeometrySink *>(sink);
				try {
					auto glyphs = _glyphs;
					for (int i = 0; i < glyphs.Length(); i++) if (_alt_face[i]) glyphs[i] = _main_space_glyph;
					if (_font->GetMainFont()->GetGlyphRunOutline(float(_font->GetUnitHeight()), glyphs, _advances, 0, glyphs.Length(), FALSE, FALSE, ssink) != S_OK) throw Exception();
					if (_font->GetAltFont()) {
						for (int i = 0; i < glyphs.Length(); i++) glyphs[i] = _alt_face[i] ? _glyphs[i] : _alt_space_glyph;
						_font->GetAltFont()->GetGlyphRunOutline(float(_font->GetUnitHeight()), glyphs, _advances, 0, glyphs.Length(), FALSE, FALSE, ssink);
					}
				} catch (...) {
					sink->Close();
					sink->Release();
					_geometry->Release();
					_geometry = 0;
					throw;
				}
				sink->Close();
				sink->Release();
				_underline_offs = -_font->UnitsToDIP(int16(_font->GetMainMetrics().underlinePosition));
				_underline_hw = _font->UnitsToDIP(_font->GetMainMetrics().underlineThickness) / 2.0f;
				_strikeout_offs = -_font->UnitsToDIP(int16(_font->GetMainMetrics().strikethroughPosition));
				_strikeout_hw = _font->UnitsToDIP(_font->GetMainMetrics().strikethroughThickness) / 2.0f;
			}
		public:
			D2D_TextBrush(D2D_DeviceContext * parent, DW_Font * font, const uint32 * ucs, int length, uint32 halign, uint32 valign, const Color & color) : _parent(parent),
				_text_ucs(1), _glyphs(1), _advances(1), _alt_face(1), _ranges(0x10), _extra_brushes(0x10), _geometry(0), _hl_start(-1), _hl_end(-1), _run_length(0)
			{
				_valign = valign;
				_halign = halign;
				_font.SetRetain(font);
				_main_brush = static_cast<D2D_ColorBrush *>(_parent->CreateSolidColorBrush(color));
				if (!_main_brush) throw Exception();
				_text_ucs.SetLength(length);
				_glyphs.SetLength(length);
				_advances.SetLength(length);
				_alt_face.SetLength(length);
				MemoryCopy(_text_ucs.GetBuffer(), ucs, length * sizeof(uint32));
				if (length) {
					_fill_glyphs();
					_fill_advances();
					_build_geometry();
				}
			}
			virtual ~D2D_TextBrush(void) override { if (_geometry) _geometry->Release(); }
			virtual void GetExtents(int & width, int & height) noexcept override { width = _run_length; height = _font->GetHeight(); }
			virtual void SetHighlightColor(const Color & color) noexcept override { _back_brush = static_cast<D2D_ColorBrush *>(_parent->CreateSolidColorBrush(color)); }
			virtual void HighlightText(int start, int end) noexcept override { _hl_start = start; _hl_end = end; }
			virtual int TestPosition(int point) noexcept override
			{
				if (point < 0) return 0;
				if (point > _run_length) return _text_ucs.Length();
				float p = float(point), s = 0.0f;
				for (int i = 0; i < _advances.Length(); i++) {
					if (p <= s + _advances[i]) {
						if (p < s + _advances[i] / 2.0f) return i;
						else return i + 1;
					}
					s += _advances[i];
				}
				return _text_ucs.Length();
			}
			virtual int EndOfChar(int index) noexcept override
			{
				if (index < 0) return 0;
				float s = 0.0f;
				for (int i = 0; i <= index; i++) s += _advances[i];
				return int(s);
			}
			virtual int GetStringLength(void) noexcept override { return _text_ucs.Length(); }
			virtual void SetCharPalette(const Color * colors, int count) override
			{
				_ranges.Clear();
				_extra_brushes.Clear();
				for (int i = 0; i < count; i++) {
					SafePointer<D2D_ColorBrush> brush = static_cast<D2D_ColorBrush *>(_parent->CreateSolidColorBrush(colors[i]));
					if (!brush) throw Exception();
					_extra_brushes.Append(brush);
				}
			}
			virtual void SetCharColors(const uint8 * indicies, int count) override
			{
				_ranges.Clear();
				int cp = 0;
				float s = 0.0f;
				while (cp < count) {
					float base = s;
					int ep = cp;
					while (ep < count && indicies[ep] == indicies[cp]) { s += _advances[ep]; ep++; }
					int index = indicies[cp];
					_text_range range;
					range.left = int(base);
					range.right = int(s + 1.0f);
					range.brush = (index == 0) ? _main_brush.Inner() : _extra_brushes.ElementAt(index - 1);
					_ranges << range;
					cp = ep;
				}
			}
			virtual void SetCharAdvances(const double * advances) override
			{
				_ranges.Clear();
				if (advances) {
					double run_length_f = 0.0;
					for (int i = 0; i < _advances.Length(); i++) {
						_advances[i] = advances[i];
						run_length_f += advances[i];
					}
					_run_length = int(run_length_f + 0.9);
				} else _fill_advances();
				_build_geometry();
			}
			virtual void GetCharAdvances(double * advances) noexcept override { for (int i = 0; i < _advances.Length(); i++) advances[i] = _advances[i]; }
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept override { return _parent; }
			virtual string ToString(void) const override { return L"D2D_TextBrush"; }
			bool inline IsEmpty(void) const noexcept { return !_advances.Length(); }
			void inline Render(ID2D1RenderTarget * target, const Box & at) noexcept
			{
				if (!_geometry) return;
				int width, height;
				GetExtents(width, height);
				D2D1_MATRIX_3X2_F transform;
				target->GetTransform(&transform);
				int shift_x = at.Left;
				int shift_y = at.Top + _baseline;
				if (_halign == 1) shift_x += (at.Right - at.Left - width) / 2;
				else if (_halign == 2) shift_x += (at.Right - at.Left - width);
				if (_valign == 1) shift_y += (at.Bottom - at.Top - height) / 2;
				else if (_valign == 2) shift_y += (at.Bottom - at.Top - height);
				if (_hl_start >= 0 && _back_brush) {
					int start = EndOfChar(_hl_start - 1);
					int end = EndOfChar(_hl_end - 1);
					D2D1_RECT_F rect = D2D1::RectF(float(shift_x + start), float(at.Top), float(shift_x + end), float(at.Bottom));
					target->FillRectangle(rect, _back_brush->GetBrush());
				}
				target->SetTransform(D2D1::Matrix3x2F::Translation(D2D1::SizeF(float(shift_x), float(shift_y))));
				if (_ranges.Length()) {
					for (auto & range : _ranges) {
						auto rect = D2D1::RectF(float(range.left), float(at.Top - shift_y), float(range.right), float(at.Bottom - shift_y));
						target->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_ALIASED);
						target->FillGeometry(_geometry, range.brush->GetBrush());
						target->PopAxisAlignedClip();
					}
				} else target->FillGeometry(_geometry, _main_brush->GetBrush());
				if (_font->IsUnderlined()) {
					auto rect = D2D1::RectF(0.0f, _underline_offs - _underline_hw, float(_run_length), _underline_offs + _underline_hw);
					target->FillRectangle(rect, _main_brush->GetBrush());
				}
				if (_font->IsStrikedout()) {
					auto rect = D2D1::RectF(0.0f, _strikeout_offs - _strikeout_hw, float(_run_length), _strikeout_offs + _strikeout_hw);
					target->FillRectangle(rect, _main_brush->GetBrush());
				}
				target->SetTransform(transform);
			}
		};
		class D2D_BlurBrush : public IBlurEffectBrush
		{
			D2D_DeviceContext * _parent;
			ID2D1Effect * _fx;
		public:
			D2D_BlurBrush(D2D_DeviceContext * context, ID2D1DeviceContext * dc, double power) : _parent(context)
			{
				if (dc->CreateEffect(CLSID_D2D1GaussianBlur, &_fx) != S_OK) throw Exception();
				_fx->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, float(power));
				_fx->SetValue(D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED);
				_fx->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
			}
			virtual ~D2D_BlurBrush(void) override { if (_fx) _fx->Release(); }
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept override { return _parent; }
			virtual string ToString(void) const override { return L"D2D_BlurBrush"; }
			void inline Render(ID2D1DeviceContext * dc, const Box & at) const noexcept
			{
				ID2D1Bitmap * fragment;
				Box corrected = at;
				if (corrected.Left < 0) corrected.Left = 0;
				if (corrected.Top < 0) corrected.Top = 0;
				if (corrected.Left >= corrected.Right) return;
				if (corrected.Top >= corrected.Bottom) return;
				auto props = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 0.0f, 0.0f);
				if (dc->CreateBitmap(D2D1::SizeU(corrected.Right - corrected.Left, corrected.Bottom - corrected.Top), props, &fragment) == S_OK) {
					_parent->ClippingUndo();
					fragment->CopyFromRenderTarget(&D2D1::Point2U(0, 0), dc, &D2D1::RectU(corrected.Left, corrected.Top, corrected.Right, corrected.Bottom));
					_parent->ClippingRedo();
					_fx->SetInput(0, fragment);
					dc->PushAxisAlignedClip(D2D1::RectF(corrected.Left, corrected.Top, corrected.Right, corrected.Bottom), D2D1_ANTIALIAS_MODE_ALIASED);
					dc->DrawImage(_fx, D2D1::Point2F(float(corrected.Left), float(corrected.Top)), D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2D1_COMPOSITE_MODE_SOURCE_COPY);
					dc->PopAxisAlignedClip();
					fragment->Release();
				}
			}
		};
		class D2D_InversionBrush : public IInversionEffectBrush
		{
			D2D_DeviceContext * _parent;
			ID2D1Effect * _fx;
			SafePointer<IColorBrush> _dropback_white, _dropback_black;
		public:
			D2D_InversionBrush(D2D_DeviceContext * context, ID2D1DeviceContext * dc) : _parent(context)
			{
				if (dc) {
					if (dc->CreateEffect(CLSID_D2D1ColorMatrix, &_fx) != S_OK) throw Exception();
					_fx->SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, D2D1::Matrix5x4F(
						-1.0f, 0.0f, 0.0f, 0.0f,
						0.0f, -1.0f, 0.0f, 0.0f,
						0.0f, 0.0f, -1.0f, 0.0f,
						0.0f, 0.0f, 0.0f, 1.0f,
						1.0f, 1.0f, 1.0f, 0.0f
					));
				} else {
					_fx = 0;
					_dropback_white = context->CreateSolidColorBrush(0xFFFFFFFF);
					_dropback_black = context->CreateSolidColorBrush(0xFF000000);
					if (!_dropback_white || !_dropback_black) throw Exception();
				}
			}
			virtual ~D2D_InversionBrush(void) override { if (_fx) _fx->Release(); }
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept override { return _parent; }
			virtual string ToString(void) const override { return L"D2D_InversionBrush"; }
			void inline Render(ID2D1DeviceContext * dc, const Box & at, bool vstate) const noexcept
			{
				if (dc) {
					if (vstate) {
						ID2D1Bitmap * fragment;
						Box corrected = at;
						if (corrected.Left < 0) corrected.Left = 0;
						if (corrected.Top < 0) corrected.Top = 0;
						if (corrected.Left >= corrected.Right) return;
						if (corrected.Top >= corrected.Bottom) return;
						auto props = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 0.0f, 0.0f);
						if (dc->CreateBitmap(D2D1::SizeU(corrected.Right - corrected.Left, corrected.Bottom - corrected.Top), props, &fragment) == S_OK) {
							_parent->ClippingUndo();
							fragment->CopyFromRenderTarget(&D2D1::Point2U(0, 0), dc, &D2D1::RectU(corrected.Left, corrected.Top, corrected.Right, corrected.Bottom));
							_parent->ClippingRedo();
							_fx->SetInput(0, fragment);
							dc->PushAxisAlignedClip(D2D1::RectF(corrected.Left, corrected.Top, corrected.Right, corrected.Bottom), D2D1_ANTIALIAS_MODE_ALIASED);
							dc->DrawImage(_fx, D2D1::Point2F(float(corrected.Left), float(corrected.Top)), D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2D1_COMPOSITE_MODE_SOURCE_COPY);
							dc->PopAxisAlignedClip();
							fragment->Release();
						}
					}
				} else {
					if (vstate) _parent->Render(_dropback_black, at);
					else _parent->Render(_dropback_white, at);
				}
			}
		};

		D2D_DeviceContext::D2D_DeviceContext(void) : _render_target(0), _render_target_ex(0), _wrapped_device(0), _bitmap_context_enabled(0), _time(0), _ref_time(0),
			_bitmap_context_state(0), _clear_counter(0), _color_cache(0x100), _blur_cache(0x10) { _hblink_time = GetCaretBlinkTime(); _blink_time = _hblink_time * 2; }
		D2D_DeviceContext::~D2D_DeviceContext(void)
		{
			ClearInternalCache();
			for (auto & layer : _layers) layer->Release();
			if (_render_target) _render_target->Release();
		}
		void D2D_DeviceContext::SetBitmapContext(bool set) noexcept { _bitmap_context_enabled = set; }
		void D2D_DeviceContext::SetRenderTarget(ID2D1RenderTarget * target) noexcept
		{
			if (_render_target) _render_target->Release();
			_render_target = target;
			_render_target_ex = 0;
			if (_render_target) _render_target->AddRef();
		}
		void D2D_DeviceContext::SetRenderTargetEx(ID2D1DeviceContext * target) noexcept
		{
			if (_render_target) _render_target->Release();
			_render_target = target;
			_render_target_ex = target;
			if (_render_target) _render_target->AddRef();
		}
		void D2D_DeviceContext::SetWrappedDevice(Graphics::IDevice * device) noexcept { _wrapped_device = device; }
		ID2D1RenderTarget * D2D_DeviceContext::GetRenderTarget(void) const noexcept { return _render_target; }
		void D2D_DeviceContext::ClippingUndo(void) noexcept { for (auto & c : _clipping) _render_target->PopAxisAlignedClip(); }
		void D2D_DeviceContext::ClippingRedo(void) noexcept { for (auto & c : _clipping) _render_target->PushAxisAlignedClip(D2D1::RectF(float(c.Left), float(c.Top), float(c.Right), float(c.Bottom)), D2D1_ANTIALIAS_MODE_ALIASED); }
		void D2D_DeviceContext::GetImplementationInfo(string & tech, uint32 & version_major, uint32 & version_minor) { try { tech = L"Direct2D"; } catch (...) {} version_major = 1; version_minor = _render_target_ex ? 1 : 0; }
		uint32 D2D_DeviceContext::GetFeatureList(void) noexcept
		{
			uint32 result = DeviceContextFeaturePolygonCapable | DeviceContextFeatureLayersCapable;
			if (_render_target_ex) {
				result |= DeviceContextFeatureBlurCapable;
				result |= DeviceContextFeatureInversionCapable;
			}
			if (_bitmap_context_enabled) result |= DeviceContextFeatureBitmapTarget;
			else result |= DeviceContextFeatureHardware;
			if (_wrapped_device) result |= DeviceContextFeatureGraphicsInteropEnabled;
			return result;
		}
		string D2D_DeviceContext::ToString(void) const { return L"D2D_DeviceContext"; }
		Graphics::IColorBrush * D2D_DeviceContext::CreateSolidColorBrush(Color color) noexcept
		{
			auto cached = _color_cache.GetObjectByKey(color);
			if (cached) {
				cached->Retain();
				return cached;
			}
			ID2D1SolidColorBrush * brush;
			SafePointer<IColorBrush> result;
			if (_render_target->CreateSolidColorBrush(D2D1::ColorF(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f), &brush) != S_OK) return 0;
			try { result = new D2D_ColorBrush(this, brush); } catch (...) { brush->Release(); return 0; }
			brush->Release();
			try { _color_cache.Push(color, result); } catch (...) {}
			result->Retain();
			return result;
		}
		Graphics::IColorBrush * D2D_DeviceContext::CreateGradientBrush(Point rel_from, Point rel_to, const GradientPoint * points, int count) noexcept
		{
			if (count < 1) return 0;
			if (count == 1) return CreateSolidColorBrush(points[0].Value);
			Array<D2D1_GRADIENT_STOP> stops(count);
			try {
				stops.SetLength(count);
				for (int i = 0; i < count; i++) {
					stops[i].color = D2D1::ColorF(points[i].Value.r / 255.0f, points[i].Value.g / 255.0f, points[i].Value.b / 255.0f, points[i].Value.a / 255.0f);
					stops[i].position = float(points[i].Position);
				}
			} catch (...) { return 0; }
			ID2D1GradientStopCollection * collection;
			if (_render_target->CreateGradientStopCollection(stops, count, &collection) != S_OK) return 0;
			D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES brush_props;
			brush_props.startPoint.x = rel_from.x;
			brush_props.startPoint.y = rel_from.y;
			brush_props.endPoint.x = rel_to.x;
			brush_props.endPoint.y = rel_to.y;
			ID2D1LinearGradientBrush * brush;
			if (_render_target->CreateLinearGradientBrush(brush_props, collection, &brush) != S_OK) brush = 0;
			collection->Release();
			if (brush) {
				SafePointer<D2D_ColorBrush> result;
				try { result = new D2D_ColorBrush(this, brush); } catch (...) { brush->Release(); return 0; }
				brush->Release();
				result->GetGradientProperties() = brush_props;
				result->Retain();
				return result;
			} else return 0;
		}
		Graphics::IBlurEffectBrush * D2D_DeviceContext::CreateBlurEffectBrush(double power) noexcept
		{
			auto cached = _blur_cache.GetObjectByKey(power);
			if (cached) {
				cached->Retain();
				return cached;
			}
			if (_render_target_ex) {
				SafePointer<Graphics::IBlurEffectBrush> brush;
				try { brush = new D2D_BlurBrush(this, _render_target_ex, power); } catch (...) { return 0; }
				try { _blur_cache.Push(power, brush); } catch (...) {}
				brush->Retain();
				return brush;
			} else return 0;
		}
		Graphics::IInversionEffectBrush * D2D_DeviceContext::CreateInversionEffectBrush(void) noexcept
		{
			if (!_inversion_cache) {
				try { _inversion_cache = new D2D_InversionBrush(this, _render_target_ex); } catch (...) { return 0; }
			}
			_inversion_cache->Retain();
			return _inversion_cache;
		}
		Graphics::IBitmapBrush * D2D_DeviceContext::CreateBitmapBrush(Graphics::IBitmap * bitmap, const Box & area, bool tile) noexcept
		{
			if (!bitmap) return 0;
			try {
				SafePointer<D2D_Bitmap> device_bitmap;
				auto cached = bitmap->GetDeviceBitmap(this);
				if (!cached) {
					device_bitmap = new D2D_Bitmap(static_cast<WIC_Bitmap *>(bitmap), this);
					SafePointer<IBitmapLink> link;
					link.SetRetain(bitmap->GetLinkObject());
					_bitmaps.InsertLast(link);
					_clear_counter++;
					if (!bitmap->AddDeviceBitmap(device_bitmap, this)) throw OutOfMemoryException();
					if (_clear_counter == 0x80) {
						auto current = _bitmaps.GetFirst();
						while (current) {
							auto next = current->GetNext();
							if (!current->GetValue()->GetBitmap()) _bitmaps.Remove(current);
							current = next;
						}
						_clear_counter = 0;
					}
				} else device_bitmap.SetRetain(static_cast<D2D_Bitmap *>(cached));
				return new D2D_BitmapBrush(this, device_bitmap, area, tile);
			} catch (...) { return 0; }
		}
		Graphics::IBitmapBrush * D2D_DeviceContext::CreateTextureBrush(Graphics::ITexture * texture, Graphics::TextureAlphaMode mode) noexcept
		{
			if (!texture || texture->GetParentDevice() != _wrapped_device) return 0;
			if (!(texture->GetResourceUsage() & Graphics::ResourceUsageShaderRead)) return 0;
			if (texture->GetTextureType() != Graphics::TextureType::Type2D) return 0;
			if (texture->GetPixelFormat() != Graphics::PixelFormat::B8G8R8A8_unorm) return 0;
			if (texture->GetMipmapCount() != 1) return 0;
			auto resource = static_cast<ID3D11Texture2D *>(Direct3D::GetInnerObject(texture));
			IDXGISurface * surface = 0;
			if (resource->QueryInterface(__uuidof(IDXGISurface), reinterpret_cast<void **>(&surface)) != S_OK) return 0;
			D2D1_BITMAP_PROPERTIES props;
			props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
			props.pixelFormat.alphaMode = mode == TextureAlphaMode::Premultiplied ? D2D1_ALPHA_MODE_PREMULTIPLIED : D2D1_ALPHA_MODE_IGNORE;
			props.dpiX = props.dpiY = 0.0f;
			ID2D1Bitmap * bitmap = 0;
			if (_render_target->CreateSharedBitmap(__uuidof(IDXGISurface), surface, &props, &bitmap) != S_OK) { surface->Release(); return 0; }
			surface->Release();
			SafePointer<D2D_Bitmap> device_bitmap;
			try { device_bitmap = new D2D_Bitmap(bitmap, this, texture->GetWidth(), texture->GetHeight()); } catch (...) { bitmap->Release(); return 0; }
			bitmap->Release();
			try { return new D2D_BitmapBrush(this, device_bitmap, Box(0, 0, device_bitmap->GetWidth(), device_bitmap->GetHeight()), false); } catch (...) { return 0; }
		}
		Graphics::ITextBrush * D2D_DeviceContext::CreateTextBrush(Graphics::IFont * font, const string & text, uint32 horizontal_align, uint32 vertical_align, const Color & color) noexcept
		{
			if (!font) return 0;
			try {
				Array<uint32> ucs(1);
				ucs.SetLength(text.GetEncodedLength(Encoding::UTF32));
				text.Encode(ucs.GetBuffer(), Encoding::UTF32, false);
				return new D2D_TextBrush(this, static_cast<DW_Font *>(font), ucs, ucs.Length(), horizontal_align, vertical_align, color);
			} catch (...) { return 0; }
		}
		Graphics::ITextBrush * D2D_DeviceContext::CreateTextBrush(Graphics::IFont * font, const uint32 * ucs, int length, uint32 horizontal_align, uint32 vertical_align, const Color & color) noexcept
		{
			if (!font) return 0;
			try { return new D2D_TextBrush(this, static_cast<DW_Font *>(font), ucs, length, horizontal_align, vertical_align, color); } catch (...) { return 0; }
		}
		void D2D_DeviceContext::ClearInternalCache(void) noexcept
		{
			_color_cache.Clear();
			_blur_cache.Clear();
			_inversion_cache.SetReference(0);
			auto element = _bitmaps.GetFirst();
			while (element) {
				auto bitmap = element->GetValue()->GetBitmap();
				if (bitmap) bitmap->RemoveDeviceBitmap(this);
				element = element->GetNext();
			}
			_bitmaps.Clear();
			_clear_counter = 0;
		}
		void D2D_DeviceContext::PushClip(const Box & rect) noexcept
		{
			try {
				_clipping.Push(rect);
				_render_target->PushAxisAlignedClip(D2D1::RectF(float(rect.Left), float(rect.Top), float(rect.Right), float(rect.Bottom)), D2D1_ANTIALIAS_MODE_ALIASED);
			} catch (...) {}
		}
		void D2D_DeviceContext::PopClip(void) noexcept { _clipping.RemoveLast(); _render_target->PopAxisAlignedClip(); }
		void D2D_DeviceContext::BeginLayer(const Box & rect, double opacity) noexcept
		{
			ID2D1Layer * layer;
			if (_render_target->CreateLayer(0, &layer) != S_OK) return;
			try { _layers.Push(layer); } catch (...) { layer->Release(); return; }
			D2D1_LAYER_PARAMETERS props;
			props.contentBounds = D2D1::RectF(float(rect.Left), float(rect.Top), float(rect.Right), float(rect.Bottom));
			props.geometricMask = 0;
			props.maskAntialiasMode = D2D1_ANTIALIAS_MODE_ALIASED;
			props.maskTransform = D2D1::IdentityMatrix();
			props.opacity = float(opacity);
			props.opacityBrush = 0;
			props.layerOptions = D2D1_LAYER_OPTIONS_NONE;
			_render_target->PushLayer(&props, layer);
		}
		void D2D_DeviceContext::EndLayer(void) noexcept { if (_layers.IsEmpty()) return; _render_target->PopLayer(); _layers.GetLast()->GetValue()->Release(); _layers.RemoveLast(); }
		void D2D_DeviceContext::Render(Graphics::IColorBrush * brush, const Box & at) noexcept
		{
			if (!brush) return;
			if (at.Left >= at.Right || at.Top >= at.Bottom) return;
			auto info = static_cast<D2D_ColorBrush *>(brush);
			auto gradient = info->GetGradient();
			if (gradient) {
				auto & props = info->GetGradientProperties();
				D2D1_POINT_2F s, e;
				s.x = props.startPoint.x + at.Left;
				s.y = props.startPoint.y + at.Top;
				e.x = props.endPoint.x + at.Left;
				e.y = props.endPoint.y + at.Top;
				gradient->SetStartPoint(s);
				gradient->SetEndPoint(e);
			}
			_render_target->FillRectangle(D2D1::RectF(float(at.Left), float(at.Top), float(at.Right), float(at.Bottom)), info->GetBrush());
		}
		void D2D_DeviceContext::Render(Graphics::IBitmapBrush * brush, const Box & at) noexcept
		{
			if (!brush) return;
			if (at.Left >= at.Right || at.Top >= at.Bottom) return;
			static_cast<D2D_BitmapBrush *>(brush)->Render(_render_target, at);
		}
		void D2D_DeviceContext::Render(Graphics::ITextBrush * brush, const Box & at, bool clip) noexcept
		{
			if (!brush) return;
			auto info = static_cast<D2D_TextBrush *>(brush);
			if (info->IsEmpty()) return;
			if (clip) PushClip(at);
			info->Render(_render_target, at);
			if (clip) PopClip();
		}
		void D2D_DeviceContext::Render(Graphics::IBlurEffectBrush * brush, const Box & at) noexcept
		{
			if (!_layers.IsEmpty() || !brush) return;
			static_cast<D2D_BlurBrush *>(brush)->Render(_render_target_ex, at);
		}
		void D2D_DeviceContext::Render(Graphics::IInversionEffectBrush * brush, const Box & at, bool blink) noexcept
		{
			if (!_layers.IsEmpty() || !brush) return;
			static_cast<D2D_InversionBrush *>(brush)->Render(_render_target_ex, at, !blink || IsCaretVisible());
		}
		void D2D_DeviceContext::RenderPolyline(const Math::Vector2 * points, int count, Color color, double width) noexcept
		{
			SafePointer<ID2D1PathGeometry> geometry;
			SafePointer<ID2D1SolidColorBrush> brush;
			if (D2DFactory->CreatePathGeometry(geometry.InnerRef()) != S_OK) return;
			if (count) {
				SafePointer<ID2D1GeometrySink> sink;
				if (geometry->Open(sink.InnerRef()) != S_OK) return;
				sink->BeginFigure(D2D1::Point2F(float(points[0].x), float(points[0].y)), D2D1_FIGURE_BEGIN_FILLED);
				for (int i = 1; i < count; i++) sink->AddLine(D2D1::Point2F(float(points[i].x), float(points[i].y)));
				sink->EndFigure(D2D1_FIGURE_END_OPEN);
				sink->Close();
			}
			if (_render_target->CreateSolidColorBrush(D2D1::ColorF(float(color.r) / 255.0f, float(color.g) / 255.0f, float(color.b) / 255.0f, float(color.a) / 255.0f), brush.InnerRef()) != S_OK) return;
			_render_target->DrawGeometry(geometry, brush, float(width));
		}
		void D2D_DeviceContext::RenderPolygon(const Math::Vector2 * points, int count, Color color) noexcept
		{
			SafePointer<ID2D1PathGeometry> geometry;
			SafePointer<ID2D1SolidColorBrush> brush;
			if (D2DFactory->CreatePathGeometry(geometry.InnerRef()) != S_OK) return;
			if (count) {
				SafePointer<ID2D1GeometrySink> sink;
				if (geometry->Open(sink.InnerRef()) != S_OK) return;
				sink->BeginFigure(D2D1::Point2F(float(points[0].x), float(points[0].y)), D2D1_FIGURE_BEGIN_FILLED);
				for (int i = 1; i < count; i++) sink->AddLine(D2D1::Point2F(float(points[i].x), float(points[i].y)));
				sink->EndFigure(D2D1_FIGURE_END_CLOSED);
				sink->Close();
			}
			if (_render_target->CreateSolidColorBrush(D2D1::ColorF(float(color.r) / 255.0f, float(color.g) / 255.0f, float(color.b) / 255.0f, float(color.a) / 255.0f), brush.InnerRef()) != S_OK) return;
			_render_target->FillGeometry(geometry, brush);
		}
		void D2D_DeviceContext::SetAnimationTime(uint32 value) noexcept { _time = value; }
		uint32 D2D_DeviceContext::GetAnimationTime(void) noexcept { return _time; }
		void D2D_DeviceContext::SetCaretReferenceTime(uint32 value) noexcept { _ref_time = value; }
		uint32 D2D_DeviceContext::GetCaretReferenceTime(void) noexcept { return _ref_time; }
		void D2D_DeviceContext::SetCaretBlinkPeriod(uint32 value) noexcept { _blink_time = value; _hblink_time = _blink_time / 2; }
		uint32 D2D_DeviceContext::GetCaretBlinkPeriod(void) noexcept { return _blink_time; }
		bool D2D_DeviceContext::IsCaretVisible(void) noexcept { return (((_time - _ref_time) / _hblink_time) & 1) == 0; }
		Graphics::IDevice * D2D_DeviceContext::GetParentDevice(void) noexcept { return _wrapped_device; }
		Graphics::I2DDeviceContextFactory * D2D_DeviceContext::GetParentFactory(void) noexcept { return CommonFactory; }
		bool D2D_DeviceContext::BeginRendering(Graphics::IBitmap * dest) noexcept
		{
			if (!_bitmap_context_enabled || !dest || _bitmap_context_state) return false;
			auto bitmap = static_cast<WIC_Bitmap *>(dest)->GetSurface();
			D2D1_RENDER_TARGET_PROPERTIES props;
			props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
			props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
			props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
			props.dpiX = 0.0f;
			props.dpiY = 0.0f;
			props.usage = D2D1_RENDER_TARGET_USAGE_NONE;
			props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
			if (D2DFactory->CreateWicBitmapRenderTarget(bitmap, props, &_render_target) != S_OK) return false;
			_render_target->BeginDraw();
			_bitmap_context_state = 1;
			_bitmap_target.SetRetain(dest);
			return true;
		}
		bool D2D_DeviceContext::BeginRendering(Graphics::IBitmap * dest, Color clear_color) noexcept
		{
			if (!BeginRendering(dest)) return false;
			_render_target->Clear(D2D1::ColorF(clear_color.r / 255.0f, clear_color.g / 255.0f, clear_color.b / 255.0f, clear_color.a / 255.0f));
			return true;
		}
		bool D2D_DeviceContext::EndRendering(void) noexcept
		{
			if (!_bitmap_context_enabled && !_bitmap_context_state) return false;
			auto status = _render_target->EndDraw();
			_render_target->Release();
			_render_target = 0;
			_bitmap_context_state = 0;
			ClearInternalCache();
			if (status == S_OK) static_cast<WIC_Bitmap *>(_bitmap_target.Inner())->SyncDeviceVersions();
			_bitmap_target.SetReference(0);
			return status == S_OK;
		}
	}
}


