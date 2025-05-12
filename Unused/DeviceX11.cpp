#include "DeviceX11.h"
#include "../Miscellaneous/Dictionary.h"

#include <X11/extensions/Xrender.h>
#include <X11/Xft/Xft.h>

namespace Engine
{
	namespace X11
	{
		constexpr Codec::AlphaMode BufferAlphaMode = Codec::AlphaMode::Premultiplied;

		uint16 XColorExpand(uint8 channel) { return uint16(channel) | (uint16(channel) << 8); }
		XRenderColor XMakeColor(UI::Color from, bool prem = true)
		{
			UI::Color conv = Codec::ConvertPixel(from, Codec::PixelFormat::R8G8B8A8, Codec::AlphaMode::Straight, Codec::PixelFormat::R8G8B8A8,
				prem ? BufferAlphaMode : Codec::AlphaMode::Straight);
			XRenderColor result;
			result.red = XColorExpand(conv.r);
			result.green = XColorExpand(conv.g);
			result.blue = XColorExpand(conv.b);
			result.alpha = XColorExpand(conv.a);
			return result;
		}

		class XTexture : public IXTexture
		{
			struct _version { UI::IRenderingDevice * dev; UI::ITexture * tex; };

			Array<_version> _versions;
			SafePointer<XServerConnection> _con;
			SafePointer<Codec::Frame> _surface;
			Pixmap _pixmap;
			int _width, _height;
		public:
			XTexture(XServerConnection * con, Codec::Frame * surface) : _versions(0x10)
			{
				_con.SetRetain(con);
				_surface.SetRetain(surface);
				_pixmap = 0;
				_width = _surface->GetWidth();
				_height = _surface->GetHeight();
			}
			virtual ~XTexture(void) override
			{
				for (auto & ver : _versions) {
					ver.tex->VersionWasDestroyed(this);
					ver.dev->TextureWasDestroyed(this);
				}
				if (_pixmap) XFreePixmap(_con->GetXDisplay(), _pixmap);
			}
			// Core texture API
			virtual int GetWidth(void) const noexcept override { return _width; }
			virtual int GetHeight(void) const noexcept override { return _height; }
			virtual void VersionWasDestroyed(ITexture * texture) noexcept override { for (int i = 0; i < _versions.Length(); i++) if (_versions[i].tex == texture) { _versions.Remove(i); break; } }
			virtual void DeviceWasDestroyed(UI::IRenderingDevice * device) noexcept override { for (int i = 0; i < _versions.Length(); i++) if (_versions[i].dev == device) { _versions.Remove(i); break; } }
			virtual void AddDeviceVersion(UI::IRenderingDevice * device, ITexture * texture) noexcept override
			{
				for (auto & ver : _versions) if (ver.dev == device) return;
				try { _version ver; ver.dev = device; ver.tex = texture; _versions << ver; } catch (...) {}
			}
			virtual bool IsDeviceSpecific(void) const noexcept override { return false; }
			virtual UI::IRenderingDevice * GetParentDevice(void) const noexcept override { return 0; }
			virtual ITexture * GetDeviceVersion(UI::IRenderingDevice * target_device) noexcept override
			{
				if (!target_device) return this;
				for (auto & ver : _versions) if (ver.dev == target_device) return ver.tex;
				return 0;
			}
			virtual void Reload(Codec::Frame * source) override
			{
				try {
					SafePointer<Codec::Frame> frame = source->ConvertFormat(Codec::PixelFormat::B8G8R8A8, BufferAlphaMode, Codec::ScanOrigin::TopDown);
					if (_pixmap) { XFreePixmap(_con->GetXDisplay(), _pixmap); _pixmap = 0; }
					_surface = frame;
					_width = frame->GetWidth();
					_height = frame->GetHeight();
				} catch (...) { return; }
				for (auto & ver : _versions) ver.tex->Reload(this);
			}
			virtual void Reload(ITexture * device_independent) override {}
			// X extension API
			virtual Codec::Frame * GetFrameSurface(void) noexcept override { return _surface; }
			virtual Pixmap GetPixmapSurface(void) noexcept override { if (!_pixmap) _pixmap = LoadPixmap(_con, _surface); return _pixmap; }
			// Object interfaces
			virtual ImmutableString ToString(void) const override { return L"X Texture"; }
		};
		class XFont : public UI::IFont
		{
			friend class XRenderingDevice;
			friend class XTextRenderingInfo;

			SafePointer<XServerConnection> _con;
			XftFont * _font;
			bool _underline;
			bool _strikeout;
			int _ref_height, _ref_width, _ref_baseline, _ref_line;
		public:
			XFont(XServerConnection * con, const string & face_name, int height, int weight, bool italic, bool underline, bool strikeout)
			{
				_con.SetRetain(con);
				_underline = underline;
				_strikeout = strikeout;
				Array<char> name(1);
				name.SetLength(face_name.GetEncodedLength(Encoding::UTF8) + 1);
				face_name.Encode(name.GetBuffer(), Encoding::UTF8, true);
				XftPattern * pattern = XftPatternCreate();
				XftPatternAddString(pattern, XFT_FAMILY, name.GetBuffer());
				XftPatternAddInteger(pattern, XFT_SLANT, italic ? XFT_SLANT_ITALIC : XFT_SLANT_ROMAN);
				XftPatternAddDouble(pattern, XFT_PIXEL_SIZE, double(height));
				if (weight <= 200) XftPatternAddInteger(pattern, XFT_WEIGHT, XFT_WEIGHT_LIGHT);
				else if (weight <= 400) XftPatternAddInteger(pattern, XFT_WEIGHT, XFT_WEIGHT_MEDIUM);
				else if (weight <= 600) XftPatternAddInteger(pattern, XFT_WEIGHT, XFT_WEIGHT_DEMIBOLD);
				else if (weight <= 800) XftPatternAddInteger(pattern, XFT_WEIGHT, XFT_WEIGHT_BOLD);
				else XftPatternAddInteger(pattern, XFT_WEIGHT, XFT_WEIGHT_BLACK);
				XftResult res;
				XftPattern * best = XftFontMatch(con->GetXDisplay(), XDefaultScreen(con->GetXDisplay()), pattern, &res);
				XftPatternDestroy(pattern);
				if (res == XftResultNoMatch) throw Exception();
				_font = XftFontOpenPattern(con->GetXDisplay(), best);
				XftPatternDestroy(best);
				_ref_height = _ref_line = _font->height * 3 / 4;
				XGlyphInfo space_info;
				XftTextExtents8(con->GetXDisplay(), _font, " ", 1, &space_info);
				_ref_width = space_info.xOff;
				_ref_baseline = _font->ascent * 3 / 4;
			}
			virtual ~XFont(void) override { XftFontClose(_con->GetXDisplay(), _font); }
			// Core font API
			virtual int GetWidth(void) const noexcept override { return _ref_width; }
			virtual int GetHeight(void) const noexcept override { return _ref_height; }
			virtual int GetLineSpacing(void) const noexcept override { return _ref_line; }
			virtual int GetBaselineOffset(void) const noexcept override { return _ref_baseline; }
			// Object interfaces
			virtual ImmutableString ToString(void) const override { return L"X Font"; }
		};

		class XBarRenderingInfo : public UI::IBarRenderingInfo
		{
			friend class XTextRenderingInfo;

			bool _solid;
			XRenderPictFormat * _format;
			Picture _brush;
			double _angle;
			Array<XFixed> _stops;
			Array<XRenderColor> _colors;
			XLinearGradient _grad;
			UI::Box _last_box;
			SafePointer<XServerConnection> _con;
		public:
			XBarRenderingInfo(XServerConnection * con, const Array<UI::GradientPoint> & gradient, double angle) : _stops(0x10), _colors(0x10)
			{
				_con.SetRetain(con);
				_solid = false;
				_brush = 0;
				_angle = -angle;
				_grad.p1.x = _grad.p1.y = _grad.p2.x = _grad.p2.y = XDoubleToFixed(-1.0);
				_stops.SetLength(gradient.Length() + 2);
				_colors.SetLength(gradient.Length() + 2);
				for (int i = 0; i < gradient.Length(); i++) {
					_stops[i + 1] = XDoubleToFixed(gradient[i].Position);
					_colors[i + 1] = XMakeColor(gradient[i].Color, false);
				}
				_stops[0] = XDoubleToFixed(0.0);
				_stops.LastElement() = XDoubleToFixed(1.0);
				_colors[0] = _colors[1];
				_colors.LastElement() = _colors[_colors.Length() - 2];
				_last_box = UI::Box(-1, -1, -1, -1);
				_format = XRenderFindStandardFormat(con->GetXDisplay(), PictStandardA8);
			}
			XBarRenderingInfo(XServerConnection * con, UI::Color color) : _stops(0x10), _colors(0x10)
			{
				auto color_trans = XMakeColor(color);
				_con.SetRetain(con);
				_solid = true;
				_brush = XRenderCreateSolidFill(con->GetXDisplay(), &color_trans);
				if (!_brush) throw Exception();
				_format = XRenderFindStandardFormat(con->GetXDisplay(), PictStandardA8);
			}
			virtual ~XBarRenderingInfo(void) override { if (_brush) XRenderFreePicture(_con->GetXDisplay(), _brush); }
			virtual ImmutableString ToString(void) const override { return L"X Bar Rendering Info"; }
			Picture GetBrush(void) noexcept { return _brush; }
			void Render(Display * display, Picture target, const UI::Box & at, int layer_x, int layer_y) noexcept
			{
				if (!_solid && (!_brush || _last_box != at)) {
					if (_brush) { XRenderFreePicture(display, _brush); _brush = 0; }
					_last_box = at;
					double rx = double(at.Right - at.Left) / 2.0;
					double ry = double(at.Bottom - at.Top) / 2.0;
					double dx = cos(_angle);
					double dy = sin(_angle);
					double ti = abs(dx) * rx + abs(dy) * ry;
					dx *= ti; dy *= ti;
					_grad.p1.x = XDoubleToFixed(rx - dx);
					_grad.p1.y = XDoubleToFixed(ry - dy);
					_grad.p2.x = XDoubleToFixed(rx + dx);
					_grad.p2.y = XDoubleToFixed(ry + dy);
					_brush = XRenderCreateLinearGradient(display, &_grad, _stops.GetBuffer(), _colors.GetBuffer(), _colors.Length());
					if (!_brush) return;
				}
				XTrapezoid trap;
				trap.left.p1.y = trap.right.p1.y = trap.top = XDoubleToFixed(at.Top - layer_y);
				trap.left.p2.y = trap.right.p2.y = trap.bottom = XDoubleToFixed(at.Bottom - layer_y);
				trap.left.p1.x = trap.left.p2.x = XDoubleToFixed(at.Left - layer_x);
				trap.right.p1.x = trap.right.p2.x = XDoubleToFixed(at.Right - layer_x);
				XRenderCompositeTrapezoids(display, PictOpOver, _brush, target, _format, 0, 0, &trap, 1);
			}
		};
		class XPseudoInversionInfo : public UI::IInversionEffectRenderingInfo
		{
		public:
			SafePointer<XBarRenderingInfo> Black, White;
			XPseudoInversionInfo(void) {}
			virtual ~XPseudoInversionInfo(void) override {}
			virtual ImmutableString ToString(void) const override { return L"X Pseudo Inversion Info"; }
		};
		class XTextureRenderingInfo : public UI::ITextureRenderingInfo
		{
			SafePointer<XServerConnection> _con;
			SafePointer<UI::ITexture> _tex_retain;
			SafePointer<XBarRenderingInfo> _mask;
			Pixmap _pixmap;
			Picture _picture;
			UI::Box _box;
			int _cax, _cay;
			int _last_vpx, _last_vpy;
			bool _release_pixmap, _pattern;
		public:
			XTextureRenderingInfo(XServerConnection * con, XBarRenderingInfo * mask, UI::ITexture * texture, const UI::Box & take_area, bool fill_pattern)
			{
				_con.SetRetain(con);
				_mask.SetRetain(mask);
				_box = UI::Box::Intersect(UI::Box(0, 0, texture->GetWidth(), texture->GetHeight()), take_area);
				_last_vpx = _last_vpy = -1;
				_cax = _box.Right - _box.Left;
				_cay = _box.Bottom - _box.Top;
				if (fill_pattern) {
					_pattern = true;
					if (take_area.Left == 0 && take_area.Top == 0 && take_area.Right == texture->GetWidth() && take_area.Bottom == texture->GetHeight()) {
						_tex_retain.SetRetain(texture);
						_pixmap = static_cast<XTexture *>(texture)->GetPixmapSurface();
						if (!_pixmap) throw Exception();
						_release_pixmap = false;
					} else {
						auto src = static_cast<XTexture *>(texture)->GetFrameSurface();
						SafePointer<Codec::Frame> subreg = new Codec::Frame(_cax, _cay, Codec::PixelFormat::B8G8R8A8, BufferAlphaMode, Codec::ScanOrigin::TopDown);
						for (int j = 0; j < _cay; j++) for (int i = 0; i < _cax; i++) subreg->SetPixel(i, j, src->GetPixel(i + _box.Left, j + _box.Top));
						_pixmap = LoadPixmap(_con, subreg);
						if (!_pixmap) throw Exception();
						_release_pixmap = true;
					}
					XRenderPictureAttributes attr;
					attr.repeat = RepeatNormal;
					_picture = XRenderCreatePicture(_con->GetXDisplay(), _pixmap, XRenderFindStandardFormat(_con->GetXDisplay(), PictStandardARGB32), CPRepeat, &attr);
					if (!_picture) throw Exception();
					XRenderSetPictureFilter(_con->GetXDisplay(), _picture, FilterNearest, 0, 0);
				} else {
					_tex_retain.SetRetain(texture);
					_pixmap = static_cast<XTexture *>(texture)->GetPixmapSurface();
					if (!_pixmap) throw Exception();
					_picture = XRenderCreatePicture(_con->GetXDisplay(), _pixmap, XRenderFindStandardFormat(_con->GetXDisplay(), PictStandardARGB32), 0, 0);
					if (!_picture) throw Exception();
					XRenderSetPictureFilter(_con->GetXDisplay(), _picture, FilterNearest, 0, 0);
					_release_pixmap = false;
					_pattern = false;
				}
			}
			virtual ~XTextureRenderingInfo(void) override
			{
				if (_picture) XRenderFreePicture(_con->GetXDisplay(), _picture);
				if (_release_pixmap && _pixmap) XFreePixmap(_con->GetXDisplay(), _pixmap);
			}
			virtual ImmutableString ToString(void) const override { return L"X Texture Rendering Info"; }
			void Render(Display * display, Picture target, const UI::Box & at, int layer_x, int layer_y) noexcept
			{
				int vpx = at.Right - at.Left;
				int vpy = at.Bottom - at.Top;
				if (!_pattern) {
					if (_last_vpx != vpx || _last_vpy != vpy) {
						XTransform trans;
						trans.matrix[0][0] = XDoubleToFixed(double(_cax) / double(vpx));
						trans.matrix[0][1] = XDoubleToFixed(0.0);
						trans.matrix[0][2] = XDoubleToFixed(_box.Left);
						trans.matrix[1][0] = XDoubleToFixed(0.0);
						trans.matrix[1][1] = XDoubleToFixed(double(_cay) / double(vpy));
						trans.matrix[1][2] = XDoubleToFixed(_box.Top);
						trans.matrix[2][0] = XDoubleToFixed(0.0);
						trans.matrix[2][1] = XDoubleToFixed(0.0);
						trans.matrix[2][2] = XDoubleToFixed(1.0);
						XRenderSetPictureTransform(display, _picture, &trans);
						_last_vpx = vpx;
						_last_vpy = vpy;
					}
				}
				XRenderComposite(display, PictOpOver, _picture, _mask->GetBrush(), target, 0, 0, 0, 0, at.Left, at.Top, vpx, vpy);
			}
		};
		class XTextRenderingInfo : public UI::ITextRenderingInfo
		{
			SafePointer<XFont> _font;
			SafePointer<IXRenderingDevice> _dev;
			Array<UI::Color> _palette;
			Array<uint32> _chars;
			Array<uint8> _indicies;
			Array<XGlyphInfo> _info;
			Array<XftCharFontSpec> _spec;
			Array<double> _adv;
			Array<int> _offs;
			UI::Color _hlc;
			int _ha, _va;
			int _hls, _hle;
			int _width;
			Picture _color_brush, _highlight_brush;
			UI::Box _last_text_rect;
			Pixmap _cached_surface;
			Picture _cached_picture, _blend_mask;

			void _reset_cached_surface(void) noexcept
			{
				if (_cached_picture) {
					XRenderFreePicture(_font->_con->GetXDisplay(), _cached_picture);
					_cached_picture = 0;
				}
				if (_cached_surface) {
					XFreePixmap(_font->_con->GetXDisplay(), _cached_surface);
					_cached_surface = 0;
				}
			}
			void _set_default_advances(void) noexcept { for (int i = 0; i < _info.Length(); i++) _adv[i] = _info[i].xOff; }
			void _update_highlight(void)
			{
				if (_highlight_brush) {
					XRenderFreePicture(_font->_con->GetXDisplay(), _highlight_brush);
					_highlight_brush = 0;
				}
				auto clr = XMakeColor(_hlc);
				_highlight_brush = XRenderCreateSolidFill(_font->_con->GetXDisplay(), &clr);
			}
			void _update_spec_and_prop(void)
			{
				for (int i = 0; i < _spec.Length(); i++) {
					_spec[i].font = _font->_font;
					_spec[i].ucs4 = _chars[i];
				}
				_update_highlight();
				double offs = 0.0;
				for (int i = 0; i < _adv.Length(); i++) {
					offs += _adv[i];
					_offs[i] = offs;
				}
				_width = offs;
				if (!_width) _width = 1;
				if (_color_brush) {
					XRenderFreePicture(_font->_con->GetXDisplay(), _color_brush);
					_color_brush = 0;
				}
				if (_palette.Length() > 1) {
					Array<XRenderColor> grad_clr(0x20);
					Array<XFixed> grad_ptr(0x20);
					XLinearGradient grad;
					grad.p1.x = XDoubleToFixed(0.0);
					grad.p1.y = XDoubleToFixed(0.0);
					grad.p2.x = XDoubleToFixed(_width);
					grad.p2.y = XDoubleToFixed(0.0);
					int p = 0;
					while (p < _indicies.Length()) {
						int sp = p;
						while (p < _indicies.Length() && _indicies[p] == _indicies[sp]) p++;
						double s = double(EndOfChar(sp - 1)) / double(_width);
						double e = double(EndOfChar(p - 1)) / double(_width);
						auto idx = _indicies[sp];
						if (idx >= _palette.Length()) idx = 0;
						auto clr = XMakeColor(_palette[idx], false);
						grad_clr << clr;
						grad_clr << clr;
						grad_ptr << XDoubleToFixed(s);
						grad_ptr << XDoubleToFixed(e);
					}
					if (!_indicies.Length()) {
						grad_ptr << XDoubleToFixed(0.0);
						grad_clr << XMakeColor(0);
					}
					_color_brush = XRenderCreateLinearGradient(_font->_con->GetXDisplay(), &grad, grad_ptr.GetBuffer(), grad_clr.GetBuffer(), grad_clr.Length());
				} else {
					auto clr = XMakeColor(_palette[0]);
					_color_brush = XRenderCreateSolidFill(_font->_con->GetXDisplay(), &clr);
				}
				if (!_color_brush) throw Exception();
				_reset_cached_surface();
			}
		public:
			XTextRenderingInfo(IXRenderingDevice * device, XFont * font, const uint32 * chars, int length) : _palette(1), _chars(1), _indicies(1), _info(1), _spec(1), _adv(1), _offs(1)
			{
				_last_text_rect = UI::Box(-1, -1, -1, -1);
				_cached_surface = 0;
				_cached_picture = 0;
				_blend_mask = 0;
				_color_brush = _highlight_brush = 0;
				_width = 0;
				_hlc = 0;
				_ha = _va = 0;
				_hls = _hle = -1;
				_font.SetRetain(font);
				_dev.SetRetain(device);
				_chars.SetLength(length);
				_indicies.SetLength(length);
				_info.SetLength(length);
				_spec.SetLength(length);
				_adv.SetLength(length);
				_offs.SetLength(length);
				_palette.Append(0);
				MemoryCopy(_chars.GetBuffer(), chars, sizeof(uint32) * length);
				ZeroMemory(_indicies.GetBuffer(), length);
				for (int i = 0; i < length; i++) XftTextExtents32(_font->_con->GetXDisplay(), _font->_font, chars + i, 1, _info.GetBuffer() + i);
				_set_default_advances();
			}
			virtual ~XTextRenderingInfo(void) override
			{
				if (_color_brush) XRenderFreePicture(_font->_con->GetXDisplay(), _color_brush);
				if (_highlight_brush) XRenderFreePicture(_font->_con->GetXDisplay(), _highlight_brush);
				if (_cached_picture) XRenderFreePicture(_font->_con->GetXDisplay(), _cached_picture);
				if (_blend_mask) XRenderFreePicture(_font->_con->GetXDisplay(), _blend_mask);
				if (_cached_surface) XFreePixmap(_font->_con->GetXDisplay(), _cached_surface);
			}
			virtual void GetExtent(int & width, int & height) noexcept override { width = _width; height = _font->GetHeight(); }
			virtual void SetHighlightColor(const UI::Color & color) noexcept override { _hlc = color; _update_highlight(); }
			virtual void HighlightText(int Start, int End) noexcept override { _hls = Start; _hle = End; }
			virtual int TestPosition(int point) noexcept override
			{
				if (point < 0) return 0;
				if (_offs.Length() && point < _offs[0] / 2) return 0;
				for (int i = 1; i < _offs.Length(); i++) if (point < (_offs[i] + _offs[i - 1]) / 2) return i;
				return _offs.Length();
			}
			virtual int EndOfChar(int Index) noexcept override
			{
				if (Index < 0) return 0;
				if (Index < _offs.Length()) return _offs[Index];
				return _width;
			}
			virtual int GetStringLength(void) noexcept override { return _chars.Length(); }
			virtual void SetCharPalette(const Array<UI::Color> & colors) override
			{
				_palette.SetLength(1 + colors.Length());
				MemoryCopy(_palette.GetBuffer() + 1, colors.GetBuffer(), sizeof(uint32) * colors.Length());
				_update_spec_and_prop();
			}
			virtual void SetCharColors(const Array<uint8> & indicies) override
			{
				if (indicies.Length() != _indicies.Length()) return;
				MemoryCopy(_indicies.GetBuffer(), indicies.GetBuffer(), _indicies.Length());
				_update_spec_and_prop();
			}
			virtual void SetCharAdvances(const double * advances) override
			{
				if (advances) MemoryCopy(_adv.GetBuffer(), advances, sizeof(double) * _adv.Length());
				else _set_default_advances();
				_update_spec_and_prop();
			}
			virtual void GetCharAdvances(double * advances) noexcept override { MemoryCopy(advances, _adv.GetBuffer(), sizeof(double) * _adv.Length()); }
			void SetHAlign(int align) noexcept { _ha = align; }
			void SetVAlign(int align) noexcept { _va = align; }
			void SetRootColor(UI::Color color) noexcept { _palette[0] = color; }
			void Init(void) { _update_spec_and_prop(); }
			void Render(Display * display, Picture target, const UI::Box & at, const UI::Box & clip, int layer_x, int layer_y) noexcept
			{
				int w, h;
				GetExtent(w, h);
				int xorg = 0;
				int yorg = 0;
				if (_ha == 0) xorg = 0;
				else if (_ha == 1) xorg = (at.Right - at.Left - w) / 2;
				else xorg = at.Right - at.Left - w;
				if (_va == 0) yorg = 0;
				else if (_va == 1) yorg = (at.Bottom - at.Top - h) / 2;
				else yorg = at.Bottom - at.Top - h;
				xorg += at.Left - layer_x;
				yorg += at.Top - layer_y;
				if (_hls >= 0 && _highlight_brush && _hls != _hle) {
					int _hlsx = EndOfChar(_hls - 1);
					int _hlex = EndOfChar(_hle - 1);
					XTrapezoid trap;
					trap.left.p1.x = trap.left.p2.x = XDoubleToFixed(xorg + _hlsx);
					trap.right.p1.x = trap.right.p2.x = XDoubleToFixed(xorg + _hlex);
					trap.left.p1.y = trap.right.p1.y = trap.top = XDoubleToFixed(at.Top - layer_y);
					trap.left.p2.y = trap.right.p2.y = trap.bottom = XDoubleToFixed(at.Bottom - layer_y);
					XRenderCompositeTrapezoids(display, PictOpOver, _highlight_brush, target, XRenderFindStandardFormat(display, PictStandardA8), 0, 0, &trap, 1);
				}
				UI::Box text_box(0, 0, w, h);
				UI::Box shiftex_clip(clip.Left - layer_x - xorg, clip.Top - layer_y - yorg, clip.Right - layer_x - xorg, clip.Bottom - layer_y - yorg);
				UI::Box clipped_text_box = UI::Box::Intersect(text_box, shiftex_clip);
				if (_last_text_rect != clipped_text_box) {
					_last_text_rect = clipped_text_box;
					_reset_cached_surface();
				}
				if (!_cached_surface && _last_text_rect.Right > _last_text_rect.Left && _last_text_rect.Bottom > _last_text_rect.Top) {
					_cached_surface = CreatePixmap(_font->_con, _last_text_rect.Right - _last_text_rect.Left, _last_text_rect.Bottom - _last_text_rect.Top, 0);
					if (_cached_surface) _cached_picture = XRenderCreatePicture(display, _cached_surface, XRenderFindStandardFormat(display, PictStandardARGB32), 0, 0);
					if (_cached_picture) {
						if (_spec.Length()) {
							_spec[0].x = -_last_text_rect.Left;
							_spec[0].y = -_last_text_rect.Top + _font->_ref_baseline;
						}
						for (int i = 1; i < _spec.Length(); i++) {
							_spec[i].x = -_last_text_rect.Left + _offs[i - 1];
							_spec[i].y = -_last_text_rect.Top + _font->_ref_baseline;
						}
						XftCharFontSpecRender(display, PictOpOver, _color_brush, _cached_picture, _last_text_rect.Left, 0, _spec.GetBuffer(), _spec.Length());
						if (_font->_underline || _font->_strikeout) {
							int u = max(h / 12, 1);
							XTrapezoid trap;
							trap.left.p1.x = XDoubleToFixed(-_last_text_rect.Left);
							trap.left.p1.y = XDoubleToFixed(-_last_text_rect.Top);
							trap.left.p2.x = XDoubleToFixed(-_last_text_rect.Left);
							trap.left.p2.y = XDoubleToFixed(-_last_text_rect.Top + h);
							trap.right.p1.x = XDoubleToFixed(-_last_text_rect.Left + w);
							trap.right.p1.y = XDoubleToFixed(-_last_text_rect.Top);
							trap.right.p2.x = XDoubleToFixed(-_last_text_rect.Left + w);
							trap.right.p2.y = XDoubleToFixed(-_last_text_rect.Top + h);
							if (_font->_underline) {
								trap.top = XDoubleToFixed(-_last_text_rect.Top + _font->_ref_baseline + u);
								trap.bottom = XDoubleToFixed(-_last_text_rect.Top + _font->_ref_baseline + 2 * u);
								XRenderCompositeTrapezoids(display, PictOpOver, _color_brush, _cached_picture, XRenderFindStandardFormat(display, PictStandardA8), 0, 0, &trap, 1);
							}
							if (_font->_strikeout) {
								int t = -_last_text_rect.Top + h / 2 - u / 2;
								trap.top = XDoubleToFixed(t);
								trap.bottom = XDoubleToFixed(t + u);
								XRenderCompositeTrapezoids(display, PictOpOver, _color_brush, _cached_picture, XRenderFindStandardFormat(display, PictStandardA8), 0, 0, &trap, 1);
							}
						}
					}
				}
				if (!_blend_mask) {
					XRenderColor color;
					color.red = color.green = color.blue = 0;
					color.alpha = 0xFFFF;
					_blend_mask = XRenderCreateSolidFill(display, &color);
				}
				if (_cached_picture && _blend_mask) {
					XRenderComposite(display, PictOpOver, _cached_picture, _blend_mask, target, 0, 0, 0, 0, xorg + _last_text_rect.Left, yorg + _last_text_rect.Top,
						_last_text_rect.Right - _last_text_rect.Left, _last_text_rect.Bottom - _last_text_rect.Top);
				}
			}
		};

		class XRenderingDevice : public IXRenderingDevice
		{
			struct _layer_info
			{
				Drawable surface;
				Picture picture;
				uint layer_x, layer_y;
				uint layer_w, layer_h;
				UI::Box clip;
				bool drop_layer;
				double blend_opacity;
			};

			Dictionary::ObjectCache<UI::Color, XBarRenderingInfo> _solid_brushes;
			SafePointer<XPseudoInversionInfo> _inversion;
			SafePointer<XServerConnection> _con;
			Drawable _render_target;
			Pixmap _pixmap;
			Picture _picture;
			uint _rt_x, _rt_y;
			uint _layer_x, _layer_y;
			uint _timer;
			bool _status;
			UI::Box _current_clip;
			Array<UI::Box> _clipping_stack;
			Array<_layer_info> _layer_stack;

			static void _set_clipping(Display * display, Picture picture, const UI::Box & clip, int layer_x, int layer_y) noexcept
			{
				XRectangle rect;
				rect.x = clip.Left - layer_x;
				rect.y = clip.Top - layer_y;
				rect.width = clip.Right - clip.Left;
				rect.height = clip.Bottom - clip.Top;
				XRenderSetPictureClipRectangles(display, picture, 0, 0, &rect, 1);
			}
			bool _push_layer(bool drop, double opacity) noexcept
			{
				_layer_info info;
				info.surface = _render_target;
				info.picture = _picture;
				info.layer_x = _layer_x;
				info.layer_y = _layer_y;
				info.layer_w = _rt_x;
				info.layer_h = _rt_y;
				info.clip = _current_clip;
				info.drop_layer = drop;
				info.blend_opacity = opacity;
				try {
					_layer_stack << info;
					return true;
				} catch (...) { return false; }
			}
			void _revert_last_layer(void) noexcept
			{
				XRenderFreePicture(_con->GetXDisplay(), _picture);
				XFreePixmap(_con->GetXDisplay(), _render_target);
				auto & layer = _layer_stack.LastElement();
				_picture = layer.picture;
				_render_target = layer.surface;
				_layer_x = layer.layer_x;
				_layer_y = layer.layer_y;
				_rt_x = layer.layer_w;
				_rt_y = layer.layer_h;
				_current_clip = layer.clip;
				_layer_stack.RemoveLast();
			}
			void _revert_all_layers(void) noexcept { while (_layer_stack.Length()) _revert_last_layer(); }
			static void _make_arc(Array<XPointDouble> & points, int idx, int n, double x, double y, double r, double dx, double dy) noexcept
			{
				double ca = asin(dy / sqrt(dx * dx + dy * dy));
				if (dx < 0.0) ca = ENGINE_PI - ca;
				for (int i = 0; i < n; i++) {
					double a = (ca - ENGINE_PI / 2.0) + ENGINE_PI * double(i) / double(n - 1);
					double lx = x + r * cos(a);
					double ly = y + r * sin(a);
					points[idx + i].x = lx;
					points[idx + i].y = ly;
				}
			}
			static void _line_rasterize(Display * display, Picture target, Picture color, double x1, double y1, double x2, double y2, double w) noexcept
			{
				double dx = x2 - x1, dy = y2 - y1;
				if (!dx && !dy) { dx = 1.0; dy = 0.0; }
				double dl = sqrt(dx * dx + dy * dy);
				dx /= dl; dy /= dl;
				int aprx_n = max(int(ENGINE_PI * w), 3);
				Array<XPointDouble> x_points(1);
				try { x_points.SetLength(2 * aprx_n); } catch (...) { return 0; }
				_make_arc(x_points, 0, aprx_n, x1, y1, w / 2.0, -dx, -dy);
				_make_arc(x_points, aprx_n, aprx_n, x2, y2, w / 2.0, dx, dy);
				auto mask_format = XRenderFindStandardFormat(display, PictStandardA8);
				XRenderCompositeDoublePoly(display, PictOpMaximum, color, target, mask_format, 0, 0, 0, 0, x_points.GetBuffer(), x_points.Length(), 0);
			}
		public:
			XRenderingDevice(XServerConnection * con) : _render_target(0), _pixmap(0),_picture(0), _rt_x(0), _rt_y(0), _timer(0), _status(false),
				_solid_brushes(0x100, Dictionary::ExcludePolicy::ExcludeLeastRefrenced), _clipping_stack(0x20), _layer_stack(0x20)
			{
				int u1, u2;
				_con.SetRetain(con);
				if (!XRenderQueryExtension(_con->GetXDisplay(), &u1, &u2)) throw Exception();
			}
			virtual ~XRenderingDevice(void) override
			{
				_revert_all_layers();
				if (_picture) XRenderFreePicture(_con->GetXDisplay(), _picture);
				if (_pixmap) XFreePixmap(_con->GetXDisplay(), _pixmap);
			}
			// Object factory / resource loader
			virtual UI::ITexture * LoadTexture(Codec::Frame * source) noexcept override { return X11::LoadTexture(_con, source); }
			virtual UI::IFont * LoadFont(const string & face_name, int height, int weight, bool italic, bool underline, bool strikeout) noexcept override { return X11::LoadFont(_con, face_name, height, weight, italic, underline, strikeout); }
			virtual Array<string> * GetFontFamilies(void) noexcept override { return X11::GetFontFamilies(_con); }
			virtual UI::ITextureRenderingDevice * CreateTextureRenderingDevice(int width, int height, UI::Color color) noexcept override { return X11::CreateTextureRenderingDevice(_con, width, height, color); }
			virtual UI::ITextureRenderingDevice * CreateTextureRenderingDevice(Codec::Frame * frame) noexcept override { return X11::CreateTextureRenderingDevice(_con, frame); }
			// Rendering device
			virtual void TextureWasDestroyed(UI::ITexture * texture) noexcept override {}
			virtual void GetImplementationInfo(string & tech, uint32 & version) noexcept override { try { tech = L"X Rendering Extension"; version = 1; } catch (...) {} }
			virtual uint32 GetFeatureList(void) noexcept override
			{
				uint32 result = UI::RenderingDeviceFeaturePolygonCapable | UI::RenderingDeviceFeatureLayersCapable;
				if (_pixmap) result |= UI::RenderingDeviceFeatureTextureTarget;
				return result;
			}
			virtual UI::IBarRenderingInfo * CreateBarRenderingInfo(const Array<UI::GradientPoint> & gradient, double angle) noexcept override
			{
				if (!gradient.Length()) return 0;
				if (gradient.Length() == 1) return CreateBarRenderingInfo(gradient[0].Color);
				try { return new XBarRenderingInfo(_con, gradient, angle); } catch (...) { return 0; }
			}
			virtual UI::IBarRenderingInfo * CreateBarRenderingInfo(UI::Color color) noexcept override
			{
				auto element = _solid_brushes[color];
				if (element) { element->Retain(); return element; }
				try {
					SafePointer<XBarRenderingInfo> bar = new XBarRenderingInfo(_con, color);
					try { _solid_brushes.Append(color, bar); } catch (...) {}
					bar->Retain();
					return bar;
				} catch (...) { return 0; }
			}
			virtual UI::IBlurEffectRenderingInfo * CreateBlurEffectRenderingInfo(double power) noexcept override { return 0; }
			virtual UI::IInversionEffectRenderingInfo * CreateInversionEffectRenderingInfo(void) noexcept override
			{
				if (_inversion) { _inversion->Retain(); return _inversion; }
				try {
					SafePointer<XPseudoInversionInfo> info = new XPseudoInversionInfo;
					info->White = new XBarRenderingInfo(_con, UI::Color(0xFF, 0xFF, 0xFF));
					info->Black = new XBarRenderingInfo(_con, UI::Color(0x00, 0x00, 0x00));
					_inversion = info;
					info->Retain();
					return info;
				} catch (...) { return 0; }
			}
			virtual UI::ITextureRenderingInfo * CreateTextureRenderingInfo(UI::ITexture * texture, const UI::Box & take_area, bool fill_pattern) noexcept override
			{
				if (!texture) return 0;
				try {
					SafePointer<UI::IBarRenderingInfo> bar = CreateBarRenderingInfo(0xFF000000);
					return new XTextureRenderingInfo(_con, static_cast<XBarRenderingInfo *>(bar.Inner()), texture, take_area, fill_pattern);
				} catch (...) { return 0; }
			}
			virtual UI::ITextureRenderingInfo * CreateTextureRenderingInfo(Graphics::ITexture * texture) noexcept override { return 0; }
			virtual UI::ITextRenderingInfo * CreateTextRenderingInfo(UI::IFont * font, const string & text, int horizontal_align, int vertical_align, const UI::Color & color) noexcept override
			{
				try {
					Array<uint32> chars(1);
					chars.SetLength(text.GetEncodedLength(Encoding::UTF32));
					text.Encode(chars.GetBuffer(), Encoding::UTF32, false);
					SafePointer<XTextRenderingInfo> info = new XTextRenderingInfo(this, static_cast<XFont *>(font), chars.GetBuffer(), chars.Length());
					info->SetHAlign(horizontal_align);
					info->SetVAlign(vertical_align);
					info->SetRootColor(color);
					info->Init();
					info->Retain();
					return info;
				} catch (...) { return 0; }
			}
			virtual UI::ITextRenderingInfo * CreateTextRenderingInfo(UI::IFont * font, const Array<uint32> & text, int horizontal_align, int vertical_align, const UI::Color & color) noexcept override
			{
				try {
					SafePointer<XTextRenderingInfo> info = new XTextRenderingInfo(this, static_cast<XFont *>(font), text.GetBuffer(), text.Length());
					info->SetHAlign(horizontal_align);
					info->SetVAlign(vertical_align);
					info->SetRootColor(color);
					info->Init();
					info->Retain();
					return info;
				} catch (...) { return 0; }
			}
			virtual Graphics::ITexture * CreateIntermediateRenderTarget(Graphics::PixelFormat format, int width, int height) override { return 0; }
			virtual void RenderBar(UI::IBarRenderingInfo * Info, const UI::Box & At) noexcept override
			{
				if (!Info || !_status) return;
				if (At.Right <= At.Left || At.Bottom <= At.Top) return;
				static_cast<XBarRenderingInfo *>(Info)->Render(_con->GetXDisplay(), _picture, At, _layer_x, _layer_y);
			}
			virtual void RenderTexture(UI::ITextureRenderingInfo * Info, const UI::Box & At) noexcept override
			{
				if (!Info || !_status) return;
				if (At.Right <= At.Left || At.Bottom <= At.Top) return;
				static_cast<XTextureRenderingInfo *>(Info)->Render(_con->GetXDisplay(), _picture, At, _layer_x, _layer_y);
			}
			virtual void RenderText(UI::ITextRenderingInfo * Info, const UI::Box & At, bool Clip) noexcept override
			{
				if (!Info || !_status) return;
				if (At.Right <= At.Left || At.Bottom <= At.Top) return;
				if (Clip) PushClip(At);
				static_cast<XTextRenderingInfo *>(Info)->Render(_con->GetXDisplay(), _picture, At, _current_clip, _layer_x, _layer_y);
				if (Clip) PopClip();
			}
			virtual void ApplyBlur(UI::IBlurEffectRenderingInfo * Info, const UI::Box & At) noexcept override {}
			virtual void ApplyInversion(UI::IInversionEffectRenderingInfo * Info, const UI::Box & At, bool Blink) noexcept override
			{
				if (!Info || !_status) return;
				auto info = static_cast<XPseudoInversionInfo *>(Info);
				if (Blink && !CaretShouldBeVisible()) RenderBar(info->White, At); else RenderBar(info->Black, At);
			}
			virtual void DrawPolygon(const Math::Vector2 * points, int count, UI::Color color, double width) noexcept override
			{
				if (!_status && _current_clip.Right > _current_clip.Left && _current_clip.Bottom > _current_clip.Top) return;
				int cl_x = _current_clip.Left;
				int cl_y = _current_clip.Top;
				int cl_w = _current_clip.Right - _current_clip.Left;
				int cl_h = _current_clip.Bottom - _current_clip.Top;
				auto display = _con->GetXDisplay();
				auto format = XRenderFindStandardFormat(_con->GetXDisplay(), PictStandardARGB32);
				auto canvas = CreatePixmap(_con, cl_w, cl_h, 0);
				if (!canvas) return;
				auto canvas_pict = XRenderCreatePicture(display, canvas, format, 0, 0);
				if (!canvas_pict) {
					XFreePixmap(display, canvas);
					return;
				}
				auto color_trans = XMakeColor(color);
				auto fill = XRenderCreateSolidFill(display, &color_trans);
				if (!fill) {
					XRenderFreePicture(display, canvas_pict);
					XFreePixmap(display, canvas);
					return;
				}
				color_trans.red = color_trans.green = color_trans.blue = 0xFFFF;
				color_trans.alpha = 0xFFFF;
				auto black = XRenderCreateSolidFill(display, &color_trans);
				if (!black) {
					XRenderFreePicture(display, fill);
					XRenderFreePicture(display, canvas_pict);
					XFreePixmap(display, canvas);
					return;
				}
				double sx = -_layer_x - cl_x;
				double sy = -_layer_y - cl_y;
				for (int i = 1; i < count; i++) {
					auto & p1 = points[i - 1];
					auto & p2 = points[i];
					_line_rasterize(display, canvas_pict, black, p1.x + sx, p1.y + sy, p2.x + sx, p2.y + sy, width);
				}
				XRenderFreePicture(display, black);
				XRenderComposite(display, PictOpOver, fill, canvas_pict, _picture, 0, 0, 0, 0, cl_x, cl_y, cl_w, cl_h);
				XRenderFreePicture(display, fill);
				XRenderFreePicture(display, canvas_pict);
				XFreePixmap(display, canvas);
			}
			virtual void FillPolygon(const Math::Vector2 * points, int count, UI::Color color) noexcept override
			{
				if (!_status) return;
				Array<XPointDouble> x_points(1);
				try {
					x_points.SetLength(count);
					for (int i = 0; i < count; i++) {
						x_points[i].x = points[i].x - double(_layer_x);
						x_points[i].y = points[i].y - double(_layer_y);
					}
				} catch (...) { return; }
				auto display = _con->GetXDisplay();
				auto mask_format = XRenderFindStandardFormat(_con->GetXDisplay(), PictStandardA8);
				auto color_trans = XMakeColor(color);
				auto fill = XRenderCreateSolidFill(display, &color_trans);
				if (!fill) return;
				XRenderCompositeDoublePoly(display, PictOpOver, fill, _picture, mask_format, 0, 0, 0, 0, x_points.GetBuffer(), count, 0);
				XRenderFreePicture(display, fill);
			}
			virtual void PushClip(const UI::Box & Rect) noexcept override
			{
				if (!_status) return;
				try {
					_clipping_stack << _current_clip;
					_current_clip = UI::Box::Intersect(_current_clip, Rect);
					_set_clipping(_con->GetXDisplay(), _picture, _current_clip, _layer_x, _layer_y);
				} catch (...) {}
			}
			virtual void PopClip(void) noexcept override
			{
				if (!_status || !_clipping_stack.Length()) return;
				_current_clip = _clipping_stack.LastElement();
				_clipping_stack.RemoveLast();
				_set_clipping(_con->GetXDisplay(), _picture, _current_clip, _layer_x, _layer_y);
			}
			virtual void BeginLayer(const UI::Box & Rect, double Opacity) noexcept override
			{
				if (!_status) return;
				auto act_rect = UI::Box::Intersect(Rect, _current_clip);
				uint width = act_rect.Right - act_rect.Left;
				uint height = act_rect.Bottom - act_rect.Top;
				bool drop = !width || !height;
				if (!width) width = 1;
				if (!height) height = 1;
				if (!_push_layer(drop, Math::saturate(Opacity))) return;
				auto format = XRenderFindStandardFormat(_con->GetXDisplay(), PictStandardARGB32);
				auto pixmap = CreatePixmap(_con, width, height, 0);
				if (!pixmap) { _layer_stack.RemoveLast(); return; }
				auto picture = XRenderCreatePicture(_con->GetXDisplay(), pixmap, format, 0, 0);
				if (!picture) { XFreePixmap(_con->GetXDisplay(), pixmap); _layer_stack.RemoveLast(); return; }
				_layer_x = act_rect.Left;
				_layer_y = act_rect.Top;
				_rt_x = width;
				_rt_y = height;
				_current_clip = act_rect;
				_render_target = pixmap;
				_picture = picture;
			}
			virtual void EndLayer(void) noexcept override
			{
				if (!_status || !_layer_stack.Length()) return;
				auto & layer = _layer_stack.LastElement();
				if (!layer.drop_layer) {
					auto display = _con->GetXDisplay();
					XRenderColor mask_color;
					mask_color.red = mask_color.green = mask_color.blue = 0;
					mask_color.alpha = 0xFFFF * layer.blend_opacity;
					Picture mask = XRenderCreateSolidFill(display, &mask_color);
					if (mask) {
						XRenderComposite(display, PictOpOver, _picture, mask, layer.picture, 0, 0, 0, 0, _layer_x - layer.layer_x, _layer_y - layer.layer_y, _rt_x, _rt_y);
						XRenderFreePicture(display, mask);
					}
				}
				_revert_last_layer();
			}
			virtual void SetTimerValue(uint32 time) noexcept override { _timer = time; }
			virtual uint32 GetCaretBlinkHalfTime(void) noexcept override { return 500; }
			virtual bool CaretShouldBeVisible(void) noexcept override { return _timer % 1000 < 500; }
			virtual void ClearCache(void) noexcept override { _solid_brushes.Clear(); _inversion.SetReference(0); }
			// Texture surface API
			virtual void BeginDraw(void) noexcept override { if (_pixmap) XBeginDraw(); }
			virtual void EndDraw(void) noexcept override { if (_pixmap) XEndDraw(); }
			virtual UI::ITexture * GetRenderTargetAsTexture(void) noexcept override
			{
				if (!_pixmap || _status) return 0;
				SafePointer<XTexture> result;
				try {
					SafePointer<Codec::Frame> frame = QueryPixmapSurface(_con, _pixmap, _rt_x, _rt_y);
					if (!frame) return 0;
					result = new XTexture(_con, frame);
				} catch (...) { return 0; }
				result->Retain();
				return result;
			}
			virtual Codec::Frame * GetRenderTargetAsFrame(void) noexcept override { if (!_pixmap || _status) return 0; return QueryPixmapSurface(_con, _pixmap, _rt_x, _rt_y); }
			// X interfaces
			virtual bool SetRenderTarget(Drawable surface, int width, int height) noexcept override
			{
				auto format = XRenderFindStandardFormat(_con->GetXDisplay(), PictStandardARGB32);
				if (!format) return false;
				return SetRenderTarget(surface, width, height, format);
			}
			virtual bool SetRenderTarget(Drawable surface, int width, int height, XRenderPictFormat * format) noexcept override
			{
				_render_target = surface;
				_rt_x = width;
				_rt_y = height;
				_picture = XRenderCreatePicture(_con->GetXDisplay(), _render_target, format, 0, 0);
				if (!_picture) return false;
				return true;
			}
			virtual bool SetPixmapSurface(Pixmap pixmap) noexcept override { _pixmap = pixmap; return true; }
			virtual void UnsetRenderTarget(void) noexcept override
			{
				_revert_all_layers();
				_render_target = 0;
				if (_picture) { XRenderFreePicture(_con->GetXDisplay(), _picture); _picture = 0; }
			}
			virtual bool XBeginDraw(void) noexcept override
			{
				if (_status) return false;
				_layer_x = _layer_y = 0;
				_current_clip = UI::Box(0, 0, _rt_x, _rt_y);
				_clipping_stack.Clear();
				_layer_stack.Clear();
				_status = true;
				return true;
			}
			virtual bool XEndDraw(void) noexcept override
			{
				if (!_status) return false;
				_revert_all_layers();
				_status = false;
				return false;
			}
			virtual bool GetState(void) noexcept override { return _status; }
			// Object interfaces
			virtual ImmutableString ToString(void) const override { return L"X Rendering Device"; }
		};
		class XObjectFactory : public UI::IObjectFactory
		{
			SafePointer<XServerConnection> _con;
		public:
			XObjectFactory(void)
			{
				int u1, u2;
				_con = XServerConnection::Query();
				if (!_con) throw Exception();
				if (!XRenderQueryExtension(_con->GetXDisplay(), &u1, &u2)) throw Exception();
			}
			virtual ~XObjectFactory(void) override {}
			virtual UI::ITexture * LoadTexture(Codec::Frame * source) noexcept override { return X11::LoadTexture(_con, source); }
			virtual UI::IFont * LoadFont(const string & face_name, int height, int weight, bool italic, bool underline, bool strikeout) noexcept override { return X11::LoadFont(_con, face_name, height, weight, italic, underline, strikeout); }
			virtual Array<string> * GetFontFamilies(void) noexcept override { return X11::GetFontFamilies(_con); }
			virtual UI::ITextureRenderingDevice * CreateTextureRenderingDevice(int width, int height, UI::Color color) noexcept override { return X11::CreateTextureRenderingDevice(_con, width, height, color); }
			virtual UI::ITextureRenderingDevice * CreateTextureRenderingDevice(Codec::Frame * frame) noexcept override { return X11::CreateTextureRenderingDevice(_con, frame); }
		};

		IXRenderingDevice * CreateXDevice(XServerConnection * con) noexcept { try { return new XRenderingDevice(con); } catch (...) { return 0; } }
		Cursor LoadCursor(XServerConnection * con, Codec::Frame * frame) noexcept
		{
			auto display = con->GetXDisplay();
			int u1, u2;
			if (!XRenderQueryExtension(display, &u1, &u2)) return 0;
			Cursor result = 0;
			auto pixmap = LoadPixmap(con, frame);
			if (pixmap) {
				int w, h;
				XQueryBestCursor(display, pixmap, frame->GetWidth(), frame->GetHeight(), &w, &h);
				if (w == frame->GetWidth() && h == frame->GetHeight()) {
					auto format = XRenderFindStandardFormat(display, PictStandardARGB32);
					auto picture = XRenderCreatePicture(display, pixmap, format, 0, 0);
					if (picture) {
						result = XRenderCreateCursor(display, picture, frame->HotPointX, frame->HotPointY);
						XRenderFreePicture(display, picture);
					}
				}
				XFreePixmap(display, pixmap);
			}
			return result;
		}
		Pixmap LoadPixmap(XServerConnection * con, Codec::Frame * frame) noexcept
		{
			SafePointer<Codec::Frame> conv;
			if (frame->GetPixelFormat() == Codec::PixelFormat::B8G8R8A8 && frame->GetAlphaMode() == BufferAlphaMode && frame->GetScanOrigin() == Codec::ScanOrigin::TopDown) {
				conv.SetRetain(frame);
			} else {
				try {
					conv = frame->ConvertFormat(Codec::PixelFormat::B8G8R8A8, BufferAlphaMode, Codec::ScanOrigin::TopDown);
				} catch (...) { return 0; }
			}
			uint size = conv->GetScanLineLength() * conv->GetHeight();
			auto image = reinterpret_cast<XImage *>(malloc(sizeof(XImage)));
			if (!image) return 0;
			image->width = conv->GetWidth();
			image->height = conv->GetHeight();
			image->xoffset = 0;
			image->format = ZPixmap;
			image->data = reinterpret_cast<char *>(malloc(size));
			image->byte_order = LSBFirst;
			image->bitmap_unit = 32;
			image->bitmap_bit_order = LSBFirst;
			image->bitmap_pad = 32;
			image->depth = 32;
			image->bytes_per_line = conv->GetScanLineLength();
			image->bits_per_pixel = 32;
			image->red_mask = 0;
			image->green_mask = 0;
			image->blue_mask = 0;
			image->obdata = 0;
			if (!image->data) { free(image); return 0; }
			MemoryCopy(image->data, conv->GetData(), size);
			if (!XInitImage(image)) { free(image->data); free(image); return 0; }
			auto display = con->GetXDisplay();
			Pixmap result = XCreatePixmap(display, XRootWindow(display, XDefaultScreen(display)), conv->GetWidth(), conv->GetHeight(), 32);
			if (!result) { image->f.destroy_image(image); return 0; }
			GC gc = XCreateGC(display, result, 0, 0);
			if (!gc) { XFreePixmap(display, result); image->f.destroy_image(image); return 0; }
			XPutImage(display, result, gc, image, 0, 0, 0, 0, conv->GetWidth(), conv->GetHeight());
			XFreeGC(display, gc);
			image->f.destroy_image(image);
			return result;
		}
		Pixmap CreatePixmap(XServerConnection * con, int width, int height, UI::Color color) noexcept
		{
			auto display = con->GetXDisplay();
			Pixmap result = XCreatePixmap(display, XRootWindow(display, XDefaultScreen(display)), width, height, 32);
			if (!result) return 0;
			GC gc = XCreateGC(display, result, 0, 0);
			if (!gc) { XFreePixmap(display, result); return 0; }
			auto ctr = Codec::ConvertPixel(color.Value, Codec::PixelFormat::R8G8B8A8, Codec::AlphaMode::Straight, Codec::PixelFormat::B8G8R8A8, BufferAlphaMode);
			XSetForeground(display, gc, ctr);
			XFillRectangle(display, result, gc, 0, 0, width, height);
			XFlushGC(display, gc);
			XFreeGC(display, gc);
			return result;
		}
		Codec::Frame * QueryPixmapSurface(XServerConnection * con, Pixmap pixmap, int width, int height, int xorg, int yorg) noexcept
		{
			SafePointer<Codec::Frame> result;
			auto image = XGetImage(con->GetXDisplay(), pixmap, xorg, yorg, width, height, 0xFFFFFFFF, ZPixmap);
			if (!image) return 0;
			try {
				Codec::PixelFormat pxf;
				if (image->depth == 32) {
					pxf = Codec::PixelFormat::B8G8R8A8;
				} else if (image->depth == 24) {
					if (image->red_mask == 0x0000FF && image->green_mask == 0x00FF00 && image->blue_mask == 0xFF0000) {
						if (image->bits_per_pixel == 32) pxf = Codec::PixelFormat::R8G8B8X8;
						else if (image->bits_per_pixel == 24) pxf = Codec::PixelFormat::R8G8B8;
						else throw Exception();
					} else if (image->red_mask == 0xFF0000 && image->green_mask == 0x00FF00 && image->blue_mask == 0x0000FF) {
						if (image->bits_per_pixel == 32) pxf = Codec::PixelFormat::B8G8R8X8;
						else if (image->bits_per_pixel == 24) pxf = Codec::PixelFormat::B8G8R8;
						else throw Exception();
					} else throw Exception();
				} else throw Exception();
				result = new Codec::Frame(width, height, image->bytes_per_line, pxf, BufferAlphaMode, Codec::ScanOrigin::TopDown);
				MemoryCopy(result->GetData(), image->data, image->bytes_per_line * image->height);
			} catch (...) { image->f.destroy_image(image); return 0; }
			image->f.destroy_image(image);
			result->Retain();
			return result;
		}

		UI::ITexture * LoadTexture(XServerConnection * con, Codec::Frame * source) noexcept
		{
			try {
				SafePointer<Codec::Frame> frame = source->ConvertFormat(Codec::PixelFormat::B8G8R8A8, BufferAlphaMode, Codec::ScanOrigin::TopDown);
				return new XTexture(con, frame);
			} catch (...) { return 0; }
		}
		UI::IFont * LoadFont(XServerConnection * con, const string & face_name, int height, int weight, bool italic, bool underline, bool strikeout) noexcept { try { return new XFont(con, face_name, height, weight, italic, underline, strikeout); } catch (...) { return 0; } }
		Array<string> * GetFontFamilies(XServerConnection * con) noexcept
		{
			SafePointer< Array<string> > result;
			try { result = new Array<string>(0x100); } catch (...) { return 0; }
			auto fs = XftListFonts(con->GetXDisplay(), XDefaultScreen(con->GetXDisplay()), 0, XFT_FAMILY, 0);
			if (!fs) return 0;
			for (int i = 0; i < fs->nfont; i++) {
				auto pattern = fs->fonts[i];
				char * name = 0;
				XftPatternGetString(pattern, XFT_FAMILY, 0, &name);
				if (name) try {
					auto str = string(name, -1, Encoding::UTF8);
					bool present = false;
					for (auto & s : *result) if (s == str) { present = true; break; }
					if (!present) result->Append(str);
				} catch (...) {}
			}
			XftFontSetDestroy(fs);
			result->Retain();
			return result;
		}
		UI::ITextureRenderingDevice * CreateTextureRenderingDevice(XServerConnection * con, int width, int height, UI::Color color) noexcept
		{
			SafePointer<IXRenderingDevice> device = CreateXDevice(con);
			if (!device) return 0;
			Pixmap surface = CreatePixmap(con, width, height, color);
			if (!surface) return 0;
			if (!device->SetRenderTarget(surface, width, height)) { XFreePixmap(con->GetXDisplay(), surface); return 0; }
			if (!device->SetPixmapSurface(surface)) { XFreePixmap(con->GetXDisplay(), surface); return 0; }
			device->Retain();
			return device;
		}
		UI::ITextureRenderingDevice * CreateTextureRenderingDevice(XServerConnection * con, Codec::Frame * frame) noexcept
		{
			SafePointer<IXRenderingDevice> device = CreateXDevice(con);
			if (!device) return 0;
			Pixmap surface = LoadPixmap(con, frame);
			if (!surface) return 0;
			if (!device->SetRenderTarget(surface, frame->GetWidth(), frame->GetHeight())) { XFreePixmap(con->GetXDisplay(), surface); return 0; }
			if (!device->SetPixmapSurface(surface)) { XFreePixmap(con->GetXDisplay(), surface); return 0; }
			device->Retain();
			return device;
		}

		UI::IObjectFactory * CreateGraphicsObjectFactory(void) { try { return new XObjectFactory; } catch (...) { return 0; } }
	}
}