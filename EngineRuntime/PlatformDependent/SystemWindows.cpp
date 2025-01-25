#include "../Interfaces/SystemWindows.h"
#include "../Interfaces/KeyCodes.h"
#include "../Interfaces/Assembly.h"
#include "../Interfaces/SystemGraphics.h"
#include "../Miscellaneous/DynamicString.h"
#include "../PlatformSpecific/WindowsRegistry.h"
#include "Direct3D.h"
#include "Direct2D.h"
#include "EffectPlugin.h"

#include <Windows.h>
#include <ShlObj.h>
#include <ShObjIdl.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "msimg32.lib")

#undef CreateWindow
#undef LoadCursor
#undef MessageBox
#undef CreateFile
#undef InsertMenuItem

#define ENGINE_MAIN_WINDOW_CLASS		L"EngineRuntimeMain"
#define ENGINE_DIALOG_WINDOW_CLASS		L"EngineRuntimeDialog"
#define ENGINE_POPUP_WINDOW_CLASS		L"EngineRuntimePopup"
#define ENGINE_TRAY_WINDOW_CLASS		L"EngineRuntimeTray"
#define ENGINE_HOST_WINDOW_CLASS		L"EngineRuntimeHost"
#define ENGINE_DDE_CLIENT_WINDOW_CLASS	L"EngineRuntimeDDEClient"
#define ENGINE_DDE_SERVER_WINDOW_CLASS	L"EngineRuntimeDDEServer"
#define ENGINE_DDE_TOPIC				L"EngineRuntimeDDE"
#define ENGINE_DDE_FORMAT				L"EngineRuntimeDDE"

#define ERTM_EXECUTE	(WM_USER + 1)
#define ERTM_TRAYEVENT	(WM_USER + 2)

namespace Engine
{
	namespace Windows
	{
		// Accessory
		typedef void (* func_RenderLayerCallback) (IPresentationEngine * engine, IWindow * window);
		HBITMAP _create_bitmap(Codec::Frame * frame, bool premultiplied)
		{
			SafePointer<Codec::Frame> data;
			Codec::AlphaMode alpha_mode = premultiplied ? Codec::AlphaMode::Premultiplied : Codec::AlphaMode::Straight;
			if (frame->GetPixelFormat() == Codec::PixelFormat::B8G8R8A8 && frame->GetAlphaMode() == alpha_mode &&
				frame->GetScanOrigin() == Codec::ScanOrigin::BottomUp && frame->GetScanLineLength() == frame->GetWidth() * 4) {
				data.SetRetain(frame);
			} else {
				data = frame->ConvertFormat(Codec::PixelFormat::B8G8R8A8, alpha_mode, Codec::ScanOrigin::BottomUp, frame->GetWidth() * 4);
			}
			BITMAPINFOHEADER header;
			ZeroMemory(&header, sizeof(header));
			header.biSize = sizeof(header);
			header.biWidth = frame->GetWidth();
			header.biHeight = frame->GetHeight();
			header.biBitCount = 32;
			header.biPlanes = 1;
			header.biSizeImage = 4 * frame->GetWidth() * frame->GetHeight();
			HDC dc = GetDC(0);
			HBITMAP bitmap = CreateDIBitmap(dc, &header, CBM_INIT, data->GetData(), reinterpret_cast<LPBITMAPINFO>(&header), DIB_RGB_COLORS);
			ReleaseDC(0, dc);
			if (!bitmap) throw Exception();
			return bitmap;
		}
		HICON _create_icon(Codec::Frame * frame)
		{
			HBITMAP and_bitmap = 0;
			HBITMAP xor_bitmap = 0;
			HICON result = 0;
			try {
				SafePointer<Codec::Frame> and_mask = new Codec::Frame(frame->GetWidth(), frame->GetHeight(), frame->GetWidth() * 4,
					Codec::PixelFormat::B8G8R8A8, Codec::AlphaMode::Straight, Codec::ScanOrigin::BottomUp);
				and_bitmap = _create_bitmap(and_mask, false);
				xor_bitmap = _create_bitmap(frame, false);
				ICONINFO icon;
				icon.fIcon = TRUE;
				icon.hbmColor = xor_bitmap;
				icon.hbmMask = and_bitmap;
				icon.xHotspot = 0;
				icon.yHotspot = 0;
				result = CreateIconIndirect(&icon);
			} catch (...) {}
			if (and_bitmap) DeleteObject(and_bitmap);
			if (xor_bitmap) DeleteObject(xor_bitmap);
			if (!result) throw Exception();
			return result;
		}
		HCURSOR _create_cursor(Codec::Frame * frame)
		{
			HBITMAP and_bitmap = 0;
			HBITMAP xor_bitmap = 0;
			HICON result = 0;
			try {
				SafePointer<Codec::Frame> and_mask = new Codec::Frame(frame->GetWidth(), frame->GetHeight(), frame->GetWidth() * 4,
					Codec::PixelFormat::B8G8R8A8, Codec::AlphaMode::Straight, Codec::ScanOrigin::BottomUp);
				and_bitmap = _create_bitmap(and_mask, false);
				xor_bitmap = _create_bitmap(frame, false);
				ICONINFO icon;
				icon.fIcon = FALSE;
				icon.hbmColor = xor_bitmap;
				icon.hbmMask = and_bitmap;
				icon.xHotspot = frame->HotPointX;
				icon.yHotspot = frame->HotPointY;
				result = CreateIconIndirect(&icon);
			} catch (...) {}
			if (and_bitmap) DeleteObject(and_bitmap);
			if (xor_bitmap) DeleteObject(xor_bitmap);
			if (!result) throw Exception();
			return result;
		}
		void _make_window_style_for_flags(uint flags, bool has_parent, DWORD & style, DWORD & ex_style, LPCWSTR & wnd_class)
		{
			style = ex_style = 0;
			if (flags & WindowFlagPopup) {
				style |= WS_POPUP;
				ex_style |= WS_EX_NOACTIVATE | WS_EX_TOPMOST;
				wnd_class = ENGINE_POPUP_WINDOW_CLASS;
			} else {
				style |= WS_OVERLAPPED | WS_SYSMENU;
				ex_style |= WS_EX_DLGMODALFRAME;
				if (has_parent) wnd_class = ENGINE_DIALOG_WINDOW_CLASS;
				else wnd_class = ENGINE_MAIN_WINDOW_CLASS;
				if (flags & WindowFlagHasTitle) style |= WS_CAPTION; else style |= WS_POPUP;
				if (flags & WindowFlagHelpButton) ex_style |= WS_EX_CONTEXTHELP;
				if (flags & WindowFlagMinimizeButton) style |= WS_MINIMIZEBOX;
				if (flags & WindowFlagMaximizeButton) style |= WS_MAXIMIZEBOX;
				if (flags & WindowFlagToolWindow) ex_style |= WS_EX_TOOLWINDOW;
				if (flags & WindowFlagSizeble) style |= WS_THICKFRAME;
			}
			if (flags & WindowFlagNonOpaque) ex_style |= WS_EX_LAYERED;
		}
		void _get_window_render_flags(IWindow * window, int * fx_flags, Color * clear_color, MARGINS ** margins);
		void _set_window_render_callback(IWindow * window, func_RenderLayerCallback callback);
		void _set_window_user_render_callback(ICoreWindow * window) { _set_window_render_callback(static_cast<IWindow *>(window), 0); }
		void _get_window_layers(ICoreWindow * window, HLAYERS * layers, double * factor);
		void _set_window_layers(ICoreWindow * window, HLAYERS layers);

		// Screen API
		class SystemScreen : public IScreen
		{
			HMONITOR monitor;
		public:
			SystemScreen(HMONITOR _monitor) : monitor(_monitor) {}
			virtual ~SystemScreen(void) override {}
			virtual string GetName(void) override
			{
				MONITORINFOEXW info;
				info.cbSize = sizeof(info);
				if (GetMonitorInfoW(monitor, &info)) {
					return info.szDevice;
				} else throw Exception();
			}
			virtual Box GetScreenRectangle(void) noexcept override
			{
				MONITORINFOEXW info;
				info.cbSize = sizeof(info);
				if (GetMonitorInfoW(monitor, &info)) {
					return Box(info.rcMonitor.left, info.rcMonitor.top, info.rcMonitor.right, info.rcMonitor.bottom);
				} else return Box(0, 0, 0, 0);
			}
			virtual Box GetUserRectangle(void) noexcept override
			{
				MONITORINFOEXW info;
				info.cbSize = sizeof(info);
				if (GetMonitorInfoW(monitor, &info)) {
					return Box(info.rcWork.left, info.rcWork.top, info.rcWork.right, info.rcWork.bottom);
				} else return Box(0, 0, 0, 0);
			}
			virtual Point GetResolution(void) noexcept override
			{
				MONITORINFOEXW info;
				info.cbSize = sizeof(info);
				if (GetMonitorInfoW(monitor, &info)) {
					return Point(info.rcMonitor.right - info.rcMonitor.left, info.rcMonitor.bottom - info.rcMonitor.top);
				} else return Point(0, 0);
			}
			virtual double GetDpiScale(void) noexcept override
			{
				HDC dc = CreateDCW(GetName(), 0, 0, 0);
				if (!dc) return 0.0;
				int dpi = GetDeviceCaps(dc, LOGPIXELSX);
				DeleteDC(dc);
				return double(dpi) / 96.0;
			}
			virtual Codec::Frame * Capture(void) noexcept override
			{
				HDC dc = CreateDCW(GetName(), 0, 0, 0);
				if (!dc) return 0;
				int w = GetDeviceCaps(dc, HORZRES), h = GetDeviceCaps(dc, VERTRES);
				if (!w || !h) { DeleteDC(dc); return 0; }
				HDC blit_dc = CreateCompatibleDC(dc);
				if (!blit_dc) { DeleteDC(dc); return 0; }
				HBITMAP blit_bitmap = CreateCompatibleBitmap(dc, w, h);
				if (!blit_bitmap) { DeleteDC(dc); DeleteDC(blit_dc); return 0; }
				HGDIOBJ prev_bitmap = SelectObject(blit_dc, blit_bitmap);
				BitBlt(blit_dc, 0, 0, w, h, dc, 0, 0, SRCCOPY);
				DeleteDC(dc);
				SelectObject(blit_dc, prev_bitmap);
				BITMAPINFOHEADER hdr;
				ZeroMemory(&hdr, sizeof(hdr));
				hdr.biSize = sizeof(hdr);
				hdr.biWidth = w;
				hdr.biHeight = h;
				hdr.biPlanes = 1;
				hdr.biBitCount = 32;
				hdr.biSizeImage = w * h * 4;
				SafePointer<Codec::Frame> result;
				try {
					result = new Codec::Frame(w, h, -1, Codec::PixelFormat::B8G8R8A8, Codec::AlphaMode::Straight, Codec::ScanOrigin::BottomUp);
					GetDIBits(blit_dc, blit_bitmap, 0, h, result->GetData(), reinterpret_cast<LPBITMAPINFO>(&hdr), DIB_RGB_COLORS);
				} catch (...) { result.SetReference(0); }
				DeleteDC(blit_dc);
				DeleteObject(blit_bitmap);
				if (result) result->Retain();
				return result;
			}
			HMONITOR GetHandle(void) const noexcept { return monitor; }
		};
		BOOL WINAPI ScreenEnumerator(HMONITOR monitor, HDC dc, LPRECT rect, LPARAM user)
		{
			try {
				SafePointer<SystemScreen> screen = new SystemScreen(monitor);
				reinterpret_cast<ObjectArray<IScreen> *>(user)->Append(screen);
			} catch (...) {}
			return TRUE;
		}
		BOOL WINAPI PrimaryScreenEnumerator(HMONITOR monitor, HDC dc, LPRECT rect, LPARAM user)
		{
			MONITORINFO info;
			info.cbSize = sizeof(info);
			if (GetMonitorInfoW(monitor, &info)) {
				if (info.dwFlags & MONITORINFOF_PRIMARY) {
					*reinterpret_cast<HMONITOR *>(user) = monitor;
					return FALSE;
				}
			}
			return TRUE;
		}
		HMONITOR _get_monitor_handle(IScreen * screen) { return static_cast<SystemScreen *>(screen)->GetHandle(); }
		ObjectArray<IScreen> * GetActiveScreens(void)
		{
			SafePointer< ObjectArray<IScreen> > result = new (std::nothrow) ObjectArray<IScreen>(0x10);
			if (!result) return 0;
			EnumDisplayMonitors(0, 0, ScreenEnumerator, reinterpret_cast<LPARAM>(result.Inner()));
			result->Retain();
			return result;
		}
		IScreen * GetDefaultScreen(void)
		{
			HMONITOR monitor = 0;
			EnumDisplayMonitors(0, 0, PrimaryScreenEnumerator, reinterpret_cast<LPARAM>(&monitor));
			if (monitor) return new SystemScreen(monitor); else return 0;
		}
		
		// Theme API
		class SystemTheme : public ITheme
		{
		public:
			virtual ThemeClass GetClass(void) noexcept override
			{
				auto color = GetColor(ThemeColor::WindowText);
				if (int(color.r) + int(color.g) + int(color.b) > 384) return ThemeClass::Dark;
				else return ThemeClass::Light;
			}
			virtual Color GetColor(ThemeColor color) noexcept override
			{
				if (color == ThemeColor::Accent) {
					Color result;
					BOOL opaque;
					if (DwmGetColorizationColor(reinterpret_cast<LPDWORD>(&result), &opaque) != S_OK) return GetSysColor(COLOR_ACTIVECAPTION) | 0xFF000000;
					swap(result.r, result.b);
					return result;
				} else if (color == ThemeColor::WindowBackgroup) return GetSysColor(COLOR_BTNFACE) | 0xFF000000;
				else if (color == ThemeColor::WindowText) return GetSysColor(COLOR_BTNTEXT) | 0xFF000000;
				else if (color == ThemeColor::SelectedBackground) return GetSysColor(COLOR_HIGHLIGHT) | 0xFF000000;
				else if (color == ThemeColor::SelectedText) return GetSysColor(COLOR_HIGHLIGHTTEXT) | 0xFF000000;
				else if (color == ThemeColor::MenuBackground) return GetSysColor(COLOR_MENU) | 0xFF000000;
				else if (color == ThemeColor::MenuText) return GetSysColor(COLOR_MENUTEXT) | 0xFF000000;
				else if (color == ThemeColor::MenuHotBackground) return GetSysColor(COLOR_HIGHLIGHT) | 0xFF000000;
				else if (color == ThemeColor::MenuHotText) return GetSysColor(COLOR_HIGHLIGHTTEXT) | 0xFF000000;
				else if (color == ThemeColor::GrayedText) return GetSysColor(COLOR_GRAYTEXT) | 0xFF000000;
				else if (color == ThemeColor::Hyperlink) return GetSysColor(COLOR_HOTLIGHT) | 0xFF000000;
				else return 0;
			}
		};
		ITheme * GetCurrentTheme(void) { return new (std::nothrow) SystemTheme; }

		// Windows API
		class SystemBackbufferedPresentationEngine : public IPresentationEngine
		{
			Color _filling;
			SafePointer<Codec::Frame> _image;
			ImageRenderMode _mode;
			IWindow * _window_object;
			HWND _window_handle;
			HBITMAP _bitmap;

			static void Render(IPresentationEngine * _self, IWindow * window)
			{
				auto self = static_cast<SystemBackbufferedPresentationEngine *>(_self);
				MARGINS * margins;
				int fx_flags;
				Color color;
				_get_window_render_flags(window, &fx_flags, &color, &margins);
				RECT rect;
				GetClientRect(self->_window_handle, &rect);
				HDC dc = GetDC(self->_window_handle);
				if (fx_flags & 4) {
					if (color.a) {
						HBRUSH clear = CreateSolidBrush(color.Value & 0xFFFFFF);
						FillRect(dc, &rect, clear);
						DeleteObject(clear);
					} else FillRect(dc, &rect, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
				}
				bool fill_back = true;
				if (fx_flags & 2) fill_back = false;
				if (fx_flags & 1) {
					if (margins->cxLeftWidth < 0 || margins->cyTopHeight < 0 || margins->cxRightWidth < 0 || margins->cyBottomHeight < 0) fill_back = false;
					else {
						rect.left += margins->cxLeftWidth;
						rect.top += margins->cyTopHeight;
						rect.right -= margins->cxRightWidth;
						rect.bottom -= margins->cyBottomHeight;
					}
				}
				if (rect.right > rect.left && rect.bottom > rect.top) {
					if (fill_back) {
						HBRUSH brush = CreateSolidBrush(self->_filling.Value & 0xFFFFFF);
						FillRect(dc, &rect, brush);
						DeleteObject(brush);
					}
					if (self->_bitmap) {
						HDC bdc = CreateCompatibleDC(dc);
						HGDIOBJ prev_bitmap = SelectObject(bdc, self->_bitmap);
						BLENDFUNCTION blend;
						blend.BlendOp = AC_SRC_OVER;
						blend.BlendFlags = 0;
						blend.SourceConstantAlpha = 255;
						blend.AlphaFormat = AC_SRC_ALPHA;
						int w = self->_image->GetWidth(), h = self->_image->GetHeight();
						if (self->_mode == ImageRenderMode::Blit) {
							IntersectClipRect(dc, rect.left, rect.top, rect.right, rect.bottom);
							int xc = (rect.right + rect.left) / 2;
							int yc = (rect.bottom + rect.top) / 2;
							int wl = w / 2, hl = h / 2;
							AlphaBlend(dc, xc - wl, yc - hl, w, h, bdc, 0, 0, w, h, blend);
						} else if (self->_mode == ImageRenderMode::Stretch) {
							AlphaBlend(dc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bdc, 0, 0, w, h, blend);
						} else if (self->_mode == ImageRenderMode::FitKeepAspectRatio) {
							auto image_aspect = double(w) / double(h);
							auto rect_aspect = double(rect.right - rect.left) / double(rect.bottom - rect.top);
							int xc = (rect.right + rect.left) / 2;
							int yc = (rect.bottom + rect.top) / 2;
							if (image_aspect < rect_aspect) {
								auto aw = int((rect.bottom - rect.top) * image_aspect);
								auto hawl = aw / 2;
								AlphaBlend(dc, xc - hawl, rect.top, aw, rect.bottom - rect.top, bdc, 0, 0, w, h, blend);
							} else {
								auto ah = int((rect.right - rect.left) / image_aspect);
								auto hahl = ah / 2;
								AlphaBlend(dc, rect.left, yc - hahl, rect.right, ah, bdc, 0, 0, w, h, blend);
							}
						} else if (self->_mode == ImageRenderMode::CoverKeepAspectRatio) {
							IntersectClipRect(dc, rect.left, rect.top, rect.right, rect.bottom);
							auto image_aspect = double(w) / double(h);
							auto rect_aspect = double(rect.right - rect.left) / double(rect.bottom - rect.top);
							int xc = (rect.right + rect.left) / 2;
							int yc = (rect.bottom + rect.top) / 2;
							if (image_aspect > rect_aspect) {
								auto aw = int((rect.bottom - rect.top) * image_aspect);
								auto hawl = aw / 2;
								AlphaBlend(dc, xc - hawl, rect.top, aw, rect.bottom - rect.top, bdc, 0, 0, w, h, blend);
							} else {
								auto ah = int((rect.right - rect.left) / image_aspect);
								auto hahl = ah / 2;
								AlphaBlend(dc, rect.left, yc - hahl, rect.right, ah, bdc, 0, 0, w, h, blend);
							}
						}
						SelectObject(bdc, prev_bitmap);
						DeleteDC(bdc);
					}
				}
				ReleaseDC(self->_window_handle, dc);
			}
			void UpdateBitmap(void)
			{
				if (_bitmap) DeleteObject(_bitmap);
				try { _bitmap = _create_bitmap(_image, true); } catch (...) { _bitmap = 0; }
			}
		public:
			SystemBackbufferedPresentationEngine(Color filling, Codec::Frame * image, ImageRenderMode mode) : _filling(filling), _mode(mode),
				_window_object(0), _window_handle(0), _bitmap(0) { _image.SetRetain(image); }
			virtual ~SystemBackbufferedPresentationEngine(void) override { if (_bitmap) DeleteObject(_bitmap); }
			virtual void Attach(ICoreWindow * window) override
			{
				_window_object = static_cast<IWindow *>(window);
				_window_handle = reinterpret_cast<HWND>(window->GetOSHandle());
				_set_window_render_callback(_window_object, Render);
			}
			virtual void Detach(void) override { if (_bitmap) DeleteObject(_bitmap); _bitmap = 0; _window_object = 0; _window_handle = 0; }
			virtual void Invalidate(void) override { UpdateBitmap(); InvalidateRect(_window_handle, 0, FALSE); }
			virtual void Resize(int width, int height) override {}
		};
		class System2DRenderingDevice : public I2DPresentationEngine
		{
			HWND _window;
			IWindow * _object;
			SafePointer<IDXGISwapChain> _swap_chain;
			SafePointer<IDXGISwapChain1> _swap_chain_ex;
			SafePointer<ID2D1HwndRenderTarget> _render_target;
			SafePointer<ID2D1RenderTarget> _render_target_d3d;
			SafePointer<ID2D1DeviceContext> _render_target_ex;
			SafePointer<Graphics::ITexture> _surface;
			SafePointer<ID3D11Texture2D> _layer_surface;
			uint orgx, orgy;
			SafePointer<Direct2D::D2D_DeviceContext> _device;
			bool _crashed;
			DeviceClass _device_class;
			HLAYERS _layers;

			bool _init_d2d_ex(void)
			{
				try {
					if (!Direct3D::CreateD2DDeviceContextForWindow(_window, _render_target_ex.InnerRef(), _swap_chain_ex.InnerRef())) return false;
					_device = new Direct2D::D2D_DeviceContext;
					_device->SetRenderTargetEx(_render_target_ex);
					_device->SetWrappedDevice(Direct3D::WrappedDevice);
					_crashed = false;
					return true;
				} catch (...) { return false; }
			}
			bool _init_d3d(void)
			{
				try {
					if (!Direct3D::CreateSwapChainForWindow(_window, _swap_chain.InnerRef())) return false;
					if (!Direct3D::CreateSwapChainDevice(_swap_chain, _render_target_d3d.InnerRef())) { _swap_chain.SetReference(0); return false; }
					_device = new Direct2D::D2D_DeviceContext;
					_device->SetRenderTarget(_render_target_d3d);
					_device->SetWrappedDevice(Direct3D::WrappedDevice);
					_crashed = false;
					return true;
				} catch (...) { return false; }
			}
			bool _init_d2d(void)
			{
				try {
					if (!Direct2D::D2DFactory) return false;
					RECT rect;
					GetClientRect(_window, &rect);
					D2D1_RENDER_TARGET_PROPERTIES target_props;
					D2D1_HWND_RENDER_TARGET_PROPERTIES wnd_props;
					target_props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
					target_props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
					target_props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
					target_props.dpiX = 0.0f;
					target_props.dpiY = 0.0f;
					target_props.usage = D2D1_RENDER_TARGET_USAGE_NONE;
					target_props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
					wnd_props.hwnd = _window;
					wnd_props.pixelSize.width = max(rect.right, 1);
					wnd_props.pixelSize.height = max(rect.bottom, 1);
					wnd_props.presentOptions = D2D1_PRESENT_OPTIONS_IMMEDIATELY;
					if (Direct2D::D2DFactory->CreateHwndRenderTarget(&target_props, &wnd_props, _render_target.InnerRef()) != S_OK) return false;
					_device = new Direct2D::D2D_DeviceContext;
					_device->SetRenderTarget(_render_target);
					_crashed = false;
					return true;
				} catch (...) { return false; }
			}
		public:
			System2DRenderingDevice(DeviceClass device_class) : _device_class(device_class), _window(0), _object(0), _crashed(true), _layers(0) {}
			virtual ~System2DRenderingDevice(void) override { if (_layers) Effect::ReleaseLayers(_layers); }
			virtual void Attach(ICoreWindow * window) override
			{
				_window = reinterpret_cast<HWND>(window->GetOSHandle());
				_object = static_cast<IWindow *>(window);
				Codec::InitializeDefaultCodecs();
				Direct2D::InitializeFactory();
				Direct3D::CreateDevices();
				if (_object->GetBackgroundFlags() & WindowFlagTransparent) {
					HLAYERS current;
					double factor;
					_get_window_layers(_object, &current, &factor);
					auto size = _object->GetClientSize();
					Effect::CreateEngineEffectLayersDesc desc;
					desc.window = _window;
					desc.device = Direct3D::D3DDevice;
					desc.layer_flags = Effect::CreateEngineEffectTransparentBackground;
					if (_object->GetBackgroundFlags() & WindowFlagBlurBehind) desc.layer_flags |= Effect::CreateEngineEffectBlurBehind;
					desc.width = size.x;
					desc.height = size.y;
					desc.deviation = factor;
					_layers = Effect::CreateLayers(&desc);
					if (_layers) {
						_set_window_layers(_object, _layers);
						Resize(size.x, size.y);
					}
				} else {
					if (_device_class == DeviceClass::DontCare) {
						if (!_init_d2d_ex()) if (!_init_d3d()) _init_d2d();
					} else if (_device_class == DeviceClass::Hardware) {
						if (!_init_d2d_ex()) _init_d3d();
					} else if (_device_class == DeviceClass::Basic) {
						_init_d2d();
					}
				}
				_set_window_render_callback(_object, 0);
			}
			virtual void Detach(void) override
			{
				_swap_chain.SetReference(0);
				_swap_chain_ex.SetReference(0);
				_render_target.SetReference(0);
				_render_target_d3d.SetReference(0);
				_render_target_ex.SetReference(0);
				_device.SetReference(0);
				_surface.SetReference(0);
				_layer_surface.SetReference(0);
				_window = 0; _object = 0;
				if (_layers) {
					Effect::ReleaseLayers(_layers);
					_layers = 0;
				}
			}
			virtual void Invalidate(void) override { InvalidateRect(_window, 0, FALSE); }
			virtual void Resize(int width, int height) override
			{
				if (_layers) {
					Effect::ResizeLayers(_layers, width, height);
					Graphics::TextureDesc desc;
					ZeroMemory(&desc, sizeof(desc));
					desc.Type = Graphics::TextureType::Type2D;
					desc.Format = Graphics::PixelFormat::B8G8R8A8_unorm;
					desc.Width = width;
					desc.Height = height;
					desc.MipmapCount = 1;
					desc.Usage = Graphics::ResourceUsageRenderTarget | Graphics::ResourceUsageShaderRead;
					desc.MemoryPool = Graphics::ResourceMemoryPool::Default;
					_surface = Direct3D::WrappedDevice->CreateTexture(desc);
					if (_surface && !_render_target_ex) {
						Direct3D::D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, _render_target_ex.InnerRef());
						_device = new Direct2D::D2D_DeviceContext;
						_device->SetRenderTargetEx(_render_target_ex);
						_device->SetWrappedDevice(Direct3D::WrappedDevice);
						_crashed = false;
					}
					if (_surface && _render_target_ex) {
						IDXGISurface * dxgi;
						if (Direct3D::GetD3D11Texture2D(_surface)->QueryInterface(IID_PPV_ARGS(&dxgi)) == S_OK) {
							D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
								D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 0.0f, 0.0f);
							ID2D1Bitmap1 * bitmap;
							if (_render_target_ex->CreateBitmapFromDxgiSurface(dxgi, props, &bitmap) == S_OK) {
								_render_target_ex->SetTarget(bitmap);
								bitmap->Release();
							}
							dxgi->Release();
						}
					}
				} else {
					if (_render_target_ex) {
						Direct3D::ResizeRenderBufferForD2DDevice(_render_target_ex, _swap_chain_ex);
					} else if (_render_target_d3d) {
						_device->SetRenderTarget(0);
						_render_target_d3d.SetReference(0);
						Direct3D::ResizeRenderBufferForSwapChainDevice(_swap_chain);
						if (Direct3D::CreateSwapChainDevice(_swap_chain, _render_target_d3d.InnerRef())) {
							_device->SetRenderTarget(_render_target_d3d);
						} else {
							_swap_chain.SetReference(0);
							_render_target_d3d.SetReference(0);
						}
					} else if (_render_target) {
						_render_target->Resize(D2D1::SizeU(max(width, 1), max(height, 1)));
					}
				}
			}
			virtual Graphics::I2DDeviceContext * GetContext(void) noexcept override { return _device; }
			virtual bool BeginRenderingPass(void) noexcept override
			{
				ID2D1RenderTarget * target = 0;
				if (_render_target_ex) target = _render_target_ex;
				else if (_render_target_d3d) target = _render_target_d3d;
				else if (_render_target) target = _render_target;
				if (!target || _crashed) return false;
				_layer_surface.SetReference(0);
				if (_layers && !Effect::BeginDraw(_layers, _layer_surface.InnerRef(), &orgx, &orgy)) return false;
				target->SetDpi(96.0f, 96.0f);
				target->BeginDraw();
				int render_flags;
				Color clear_color;
				_get_window_render_flags(_object, &render_flags, &clear_color, 0);
				if (render_flags & 4) target->Clear(D2D1::ColorF(clear_color.r / 255.0f, clear_color.g / 255.0f, clear_color.b / 255.0f, clear_color.a / 255.0f));
				return true;
			}
			virtual bool EndRenderingPass(void) noexcept override
			{
				if (_layers) {
					if (_render_target_ex && _render_target_ex->EndDraw() != S_OK) { _crashed = true; return false; }
					ID3D11DeviceContext * context;
					Direct3D::D3DDevice->GetImmediateContext(&context);
					D3D11_BOX box;
					box.left = box.top = box.front = 0;
					box.right = _surface->GetWidth();
					box.bottom = _surface->GetHeight();
					box.back = 1;
					context->CopySubresourceRegion(_layer_surface, 0, orgx, orgy, 0, Direct3D::GetD3D11Texture2D(_surface), 0, &box);
					context->Flush();
					_layer_surface.SetReference(0);
					Effect::EndDraw(_layers);
				} else {
					if (_render_target_ex) {
						if (_render_target_ex->EndDraw() != S_OK) { _crashed = true; return false; } else if (_swap_chain_ex->Present(1, 0) != S_OK) { _crashed = true; return false; }
					} else if (_render_target_d3d) {
						if (_render_target_d3d->EndDraw() != S_OK) { _crashed = true; return false; } else if (_swap_chain->Present(1, 0) != S_OK) { _crashed = true; return false; }
					} else if (_render_target) {
						if (_render_target->EndDraw() != S_OK) { _crashed = true; return false; }
					}
				}
				return true;
			}
		};
		class SystemLayeredPresentationEngine : public System2DRenderingDevice
		{
			SafePointer<Codec::Frame> _frame;
			SafePointer<Graphics::IBitmap> _texture;
			SafePointer<Graphics::IBitmapBrush> _info;
			SafePointer<Graphics::IColorBrush> _bar;
			Color _filling;
			ImageRenderMode _mode;

			static void _render_layer_callback(IPresentationEngine * engine, IWindow * window)
			{
				auto & self = *static_cast<SystemLayeredPresentationEngine *>(engine);
				auto device = self.GetContext();
				if (self.BeginRenderingPass()) {
					if (self._filling.a && !self._bar) {
						self._bar = device->CreateSolidColorBrush(self._filling);
					}
					if (self._frame && !self._texture) {
						self._texture = device->GetParentFactory()->LoadBitmap(self._frame);
					}
					if (self._texture && !self._info) {
						int w = self._texture->GetWidth();
						int h = self._texture->GetHeight();
						self._info = device->CreateBitmapBrush(self._texture, Box(0, 0, w, h), false);
					}
					auto size = window->GetClientSize();
					auto at = Box(0, 0, size.x, size.y);
					if (self._bar) device->Render(self._bar, at);
					if (self._info) {
						int w = self._texture->GetWidth();
						int h = self._texture->GetHeight();
						if (self._mode == ImageRenderMode::Stretch) {
							device->Render(self._info, at);
						} else if (self._mode == ImageRenderMode::Blit) {
							int xc = size.x / 2;
							int yc = size.y / 2;
							at.Left = xc - w / 2;
							at.Top = yc - h / 2;
							at.Right = at.Left + w;
							at.Bottom = at.Top + h;
							device->Render(self._info, at);
						} else {
							int xc = size.x / 2;
							int yc = size.y / 2;
							double fasp = double(w) / double(h);
							double sasp = double(size.x) / double(size.y);
							bool fit_horz = false;
							if (self._mode == ImageRenderMode::CoverKeepAspectRatio) fit_horz = fasp < sasp;
							else if (self._mode == ImageRenderMode::FitKeepAspectRatio) fit_horz = fasp > sasp;
							if (fit_horz) {
								at.Left = 0;
								at.Right = size.x;
								double sf = double(size.x) / double(w);
								int sh = max(int(h * sf), 1);
								at.Top = yc - sh / 2;
								at.Bottom = at.Top + sh;
							} else {
								at.Top = 0;
								at.Bottom = size.y;
								double sf = double(size.y) / double(h);
								int sw = max(int(w * sf), 1);
								at.Left = xc - sw / 2;
								at.Right = at.Left + sw;
							}
							device->Render(self._info, at);
						}
					}
					self.EndRenderingPass();
				}
			}
		public:
			SystemLayeredPresentationEngine(Color filling, Codec::Frame * image, ImageRenderMode mode) : System2DRenderingDevice(DeviceClass::Hardware)
			{
				_frame.SetRetain(image);
				_filling = filling;
				_mode = mode;
			}
			virtual ~SystemLayeredPresentationEngine(void) override {}
			virtual void Attach(ICoreWindow * window) override
			{
				System2DRenderingDevice::Attach(window);
				_set_window_render_callback(static_cast<IWindow *>(window), _render_layer_callback);
			}
			virtual void Detach(void) override
			{
				System2DRenderingDevice::Detach();
				_texture.SetReference(0);
				_info.SetReference(0);
				_bar.SetReference(0);
			}
			virtual void Invalidate(void) override
			{
				_texture.SetReference(0);
				_info.SetReference(0);
				System2DRenderingDevice::Invalidate();
			}
		};
		class SystemCursor : public ICursor
		{
			HCURSOR _cursor;
			bool _destroy_on_release;
		public:
			SystemCursor(HCURSOR cursor, bool take_own) : _cursor(cursor), _destroy_on_release(take_own) {}
			virtual ~SystemCursor(void) override { if (_destroy_on_release) DestroyCursor(_cursor); }
			virtual handle GetOSHandle(void) noexcept override { return _cursor; }
		};
		class SystemMenuItem : public IMenuItem
		{
			friend class SystemMenu;

			IMenuItemCallback * _callback;
			void * _user;
			SafePointer<IMenu> _submenu;
			int _id;
			string _text, _right;
			bool _enabled, _checked, _separator;
			Graphics::I2DDeviceContext * _device;
			ID2D1DCRenderTarget * _target;
		public:
			SystemMenuItem(void) : _callback(0), _user(0), _id(0), _enabled(true), _checked(false), _separator(false), _device(0), _target(0) {}
			virtual ~SystemMenuItem(void) override { if (_callback) _callback->MenuItemDisposed(this); }
			virtual void SetCallback(IMenuItemCallback * callback) override { _callback = callback; }
			virtual IMenuItemCallback * GetCallback(void) override { return _callback; }
			virtual void SetUserData(void * data) override { _user = data; }
			virtual void * GetUserData(void)override { return _user; }
			virtual void SetSubmenu(IMenu * menu) override { _submenu.SetRetain(menu); }
			virtual IMenu * GetSubmenu(void) override { return _submenu; }
			virtual void SetID(int id) override { _id = id; }
			virtual int GetID(void) override { return _id; }
			virtual void SetText(const string & text) override { _text = text; }
			virtual string GetText(void) override { return _text; }
			virtual void SetSideText(const string & text) override { _right = text; }
			virtual string GetSideText(void) override { return _right; }
			virtual void SetIsSeparator(bool separator) override { _separator = separator; }
			virtual bool IsSeparator(void) override { return _separator; }
			virtual void Enable(bool enable) override { _enabled = enable; }
			virtual bool IsEnabled(void) override { return _enabled; }
			virtual void Check(bool check) override { _checked = check; }
			virtual bool IsChecked(void) override { return _checked; }
		};
		class SystemMenu : public IMenu
		{
			friend class SystemWindow;
			friend class WindowSystem;

			ObjectArray<SystemMenuItem> _children;

			static LRESULT _menu_measure_item(HWND wnd, WPARAM wparam, LPARAM lparam)
			{
				LPMEASUREITEMSTRUCT mis = reinterpret_cast<LPMEASUREITEMSTRUCT>(lparam);
				if (mis->CtlType == ODT_MENU) {
					auto item = reinterpret_cast<SystemMenuItem *>(mis->itemData);
					Point size(0, 0);
					if (item->_callback) size = item->_callback->MeasureMenuItem(item, item->_device);
					mis->itemWidth = size.x;
					mis->itemHeight = size.y;
				}
				return 1;
			}
			static LRESULT _menu_draw_item(HWND wnd, WPARAM wparam, LPARAM lparam)
			{
				LPDRAWITEMSTRUCT dis = reinterpret_cast<LPDRAWITEMSTRUCT>(lparam);
				if (dis->CtlType == ODT_MENU) {
					auto item = reinterpret_cast<SystemMenuItem *>(dis->itemData);
					auto box = Box(0, 0, dis->rcItem.right - dis->rcItem.left, dis->rcItem.bottom - dis->rcItem.top);;
					item->_target->BindDC(dis->hDC, &dis->rcItem);
					item->_target->BeginDraw();
					if (item->_callback) item->_callback->RenderMenuItem(item, item->_device, box, (dis->itemState & ODS_SELECTED) ? true : false);
					item->_target->EndDraw();
				}
				return 1;
			}
			bool _needs_custom_callback(void)
			{
				for (auto & child : _children) {
					if (child.GetCallback()) return true;
					auto sub = child.GetSubmenu();
					if (sub && static_cast<SystemMenu *>(sub)->_needs_custom_callback()) return true;
				}
				return false;
			}
			void _set_device(Graphics::I2DDeviceContext * device, ID2D1DCRenderTarget * target)
			{
				for (auto & child : _children) {
					if (!device && child._callback) child._callback->MenuClosed(&child);
					child._device = device;
					child._target = target;
					if (child._submenu) static_cast<SystemMenu *>(child._submenu.Inner())->_set_device(device, target);
				}
			}
			HMENU _create_menu_handle(void)
			{
				HMENU menu = CreatePopupMenu();
				if (!menu) return 0;
				for (auto & child : _children) {
					HMENU submenu = 0;
					UINT flags = 0;
					UINT_PTR id_menu = 0;
					LPCWSTR data = 0;
					string text;
					if (child._submenu) {
						submenu = static_cast<SystemMenu *>(child._submenu.Inner())->_create_menu_handle();
						if (!submenu) {
							DestroyMenu(menu);
							return 0;
						}
						flags |= MF_POPUP;
						id_menu = reinterpret_cast<UINT_PTR>(submenu);
					} else {
						id_menu = child._id;
						if (child._separator) flags |= MF_SEPARATOR;
					}
					if (child._callback) {
						flags |= MF_OWNERDRAW;
						data = reinterpret_cast<LPCWSTR>(&child);
					} else {
						try { text = child._text.Replace(L'&', L"&&") + L"\t" + child._right.Replace(L'&', L"&&"); }
						catch (...) {
							if (submenu) DestroyMenu(submenu);
							DestroyMenu(menu);
							return 0;
						}
						data = text;
					}
					if (!child._enabled) flags |= MF_DISABLED | MF_GRAYED;
					if (child._checked) flags |= MF_CHECKED;
					if (!AppendMenuW(menu, flags, id_menu, data)) {
						if (submenu) DestroyMenu(submenu);
						DestroyMenu(menu);
						return 0;
					}
				}
				return menu;
			}
			int _run(HWND owner, Point at)
			{
				Direct2D::InitializeFactory();
				bool needs_device = _needs_custom_callback();
				auto menu = _create_menu_handle();
				if (!menu) return -1;
				ID2D1DCRenderTarget * render_target = 0;
				Direct2D::D2D_DeviceContext * device = 0;
				if (needs_device) {
					D2D1_RENDER_TARGET_PROPERTIES props;
					props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
					props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
					props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
					props.dpiX = 0.0f;
					props.dpiY = 0.0f;
					props.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
					props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
					if (Direct2D::D2DFactory->CreateDCRenderTarget(&props, &render_target) != S_OK) { DestroyMenu(menu); return -1; }
					try { device = new Direct2D::D2D_DeviceContext; } catch (...) { render_target->Release(); DestroyMenu(menu); return -1; }
					device->SetRenderTarget(render_target);
					device->SetAnimationTime(0);
					_set_device(device, render_target);
				}
				int result = TrackPopupMenuEx(menu, TPM_RETURNCMD, at.x, at.y, owner, 0);
				DestroyMenu(menu);
				if (needs_device) {
					_set_device(0, 0);
					device->Release();
					render_target->Release();
				}
				return result;
			}
		public:
			SystemMenu(void) : _children(0x10) {}
			virtual ~SystemMenu(void) override {}
			virtual void AppendMenuItem(IMenuItem * item) noexcept override { try { _children.Append(static_cast<SystemMenuItem *>(item)); } catch (...) { return; } }
			virtual void InsertMenuItem(IMenuItem * item, int at) noexcept override { try { _children.Insert(static_cast<SystemMenuItem *>(item), at); } catch (...) { return; } }
			virtual void RemoveMenuItem(int at) noexcept override { _children.Remove(at); }
			virtual IMenuItem * ElementAt(int at) noexcept override { return _children.ElementAt(at); }
			virtual int Length(void) noexcept override { return _children.Length(); }
			virtual IMenuItem * FindMenuItem(int id) noexcept override
			{
				for (auto & child : _children) {
					if (child.GetID() == id) return &child;
					auto submenu = child.GetSubmenu();
					if (submenu) {
						auto item = submenu->FindMenuItem(id);
						if (item) return item;
					}
				}
				return 0;
			}
			virtual int Run(IWindow * owner, Point at) noexcept override { return _run(reinterpret_cast<HWND>(owner->GetOSHandle()), at); }
			virtual handle GetOSHandle(void) noexcept override { return 0; }
		};
		class SystemWindow : public IWindow
		{
			friend class WindowSystem;
			friend void _get_window_render_flags(IWindow * window, int * fx_flags, Color * clear_color, MARGINS ** margins);
			friend void _set_window_render_callback(IWindow * window, func_RenderLayerCallback callback);
			friend void _get_window_layers(ICoreWindow * window, HLAYERS * layers, double * factor);
			friend void _set_window_layers(ICoreWindow * window, HLAYERS layers);

			HWND _window;
			ITaskbarList3 * _taskbar;
			IWindowCallback * _callback;
			SafePointer<IPresentationEngine> _layer;
			HLAYERS _layered;
			SystemWindow * _parent;
			Array<SystemWindow *> _children;
			Point _min_constraints;
			Point _max_constraints;
			MARGINS _margins;
			bool _is_modal;
			int _effect_flags; // 1 - set margins, 2 - set blur behind, 4 - clear background on render, 8 - set transparent background
			int _effective_flags;
			int _last_x, _last_y;
			uint32 _surrogate;
			double _blur_factor;
			Color _clear_color;
			int _layer_flags; // 1 - draw using user's callback, 2 - draw using layer's service
			bool _first_time_render;
			func_RenderLayerCallback _render_callback;

			void _dwm_reset(void)
			{
				_effective_flags = 0;
				auto clear_color = _clear_color;
				BOOL dwm_enabled;
				if (DwmIsCompositionEnabled(&dwm_enabled) != S_OK) return;
				if (dwm_enabled) {
					if (_effect_flags & 0xA) {
						if (Effect::Init()) {
							if (_effect_flags & 0x2) _effective_flags |= WindowFlagBlurBehind | WindowFlagBlurFactor;
							if (_effect_flags & 0x8) _effective_flags |= WindowFlagTransparent;
						} else {
							DWM_BLURBEHIND blur;
							blur.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
							blur.fEnable = (_effect_flags & 0x2) ? TRUE : FALSE;
							blur.hRgnBlur = 0;
							DwmEnableBlurBehindWindow(_window, &blur);
							if (_effect_flags & 0x2) _effective_flags |= WindowFlagBlurBehind;
						}
					}
					if (_effect_flags & 0x1) DwmExtendFrameIntoClientArea(_window, &_margins);
					_clear_color = Color(0, 0, 0, 0);
				} else {
					auto rgb = GetSysColor(COLOR_BTNFACE);
					_clear_color = Color(uint8(GetRValue(rgb)), uint8(GetGValue(rgb)), uint8(GetBValue(rgb)), uint(255));
				}
				if (clear_color != _clear_color) InvalidateRect(_window, 0, FALSE);
			}
		public:
			static LRESULT WINAPI _core_window_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
			{
				auto self = reinterpret_cast<SystemWindow *>(GetWindowLongPtrW(wnd, 0));
				if (msg == WM_CREATE) {
					LPCREATESTRUCTW cs = reinterpret_cast<LPCREATESTRUCTW>(lparam);
					SetWindowLongPtrW(wnd, 0, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
					return 0;
				} else if (self) {
					if (msg == WM_PAINT) {
						ValidateRect(wnd, 0);
						if (self->_layer_flags & 1) if (self->_callback) self->_callback->RenderWindow(self);
						if (self->_layer_flags & 2) self->_render_callback(self->_layer, self);
						if (self->_first_time_render) {
							self->_first_time_render = true;
							GetWindowSystem()->SubmitTask(CreateFunctionalTask([w = wnd]() { InvalidateRect(w, 0, FALSE); }));
						}
						return 0;
					} else if (msg == WM_SIZE) {
						auto size = self->GetClientSize();
						if (self->_callback) self->_callback->WindowSize(self);
						if (self->_layer) self->_layer->Resize(size.x, size.y);
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_MOVE) {
						auto position = MAKEPOINTS(lparam);
						if (self->_last_x != 0x80000000) {
							int dx = position.x - self->_last_x;
							int dy = position.y - self->_last_y;
							for (auto & child : self->_children) {
								RECT rect;
								GetWindowRect(child->_window, &rect);
								SetWindowPos(child->_window, 0, rect.left + dx, rect.top + dy, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
							}
						}
						self->_last_x = position.x;
						self->_last_y = position.y;
						if (self->_callback) self->_callback->WindowMove(self);
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_ENDSESSION) {
						return 0;
					} else if (msg == WM_TIMER) {
						if (self->_callback) self->_callback->Timer(self, wparam);
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_SYSCOMMAND) {
						if (wparam == SC_CONTEXTHELP) {
							if (self->_callback) self->_callback->WindowHelp(self);
							return 0;
						} else {
							if (wparam == SC_MAXIMIZE && self->_callback) self->_callback->WindowMaximize(self);
							else if (wparam == SC_MINIMIZE && self->_callback) self->_callback->WindowMinimize(self);
							else if (wparam == SC_RESTORE && self->_callback) self->_callback->WindowRestore(self);
							return DefWindowProcW(wnd, msg, wparam, lparam);
						}
					} else if (msg == WM_ACTIVATE) {
						if (self->_callback) {
							if (wparam == WA_INACTIVE) self->_callback->WindowDeactivate(self);
							else self->_callback->WindowActivate(self);
						}
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_MOUSEACTIVATE) {
						if (GetWindowLongPtr(wnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE) return MA_NOACTIVATE;
						else return MA_ACTIVATE;
					} else if (msg == WM_CLOSE) {
						if (self->_callback) self->_callback->WindowClose(self);
						return 0;
					} else if (msg == WM_GETMINMAXINFO) {
						LPMINMAXINFO mmi = reinterpret_cast<LPMINMAXINFO>(lparam);
						if (self->_min_constraints.x || self->_min_constraints.y) {
							mmi->ptMinTrackSize.x = self->_min_constraints.x;
							mmi->ptMinTrackSize.y = self->_min_constraints.y;
						}
						if (self->_max_constraints.x || self->_max_constraints.y) {
							mmi->ptMaxTrackSize.x = self->_max_constraints.x;
							mmi->ptMaxTrackSize.y = self->_max_constraints.y;
						}
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_MEASUREITEM) {
						return SystemMenu::_menu_measure_item(wnd, wparam, lparam);
					} else if (msg == WM_DRAWITEM) {
						return SystemMenu::_menu_draw_item(wnd, wparam, lparam);
					} else if (msg == WM_ENABLE) {
						for (auto & child : self->_children) if (!child->_is_modal) EnableWindow(child->_window, wparam ? true : false);
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_SHOWWINDOW) {
						if (self->_callback) self->_callback->Shown(self, wparam != 0);
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_DWMCOMPOSITIONCHANGED || msg == WM_DWMCOLORIZATIONCOLORCHANGED || msg == WM_SYSCOLORCHANGE) {
						self->_dwm_reset();
						if (self->_callback) self->_callback->ThemeChanged(self);
						return 0;
					} else if (msg == WM_SETFOCUS) {
						if (self->_callback) self->_callback->FocusChanged(self, true);
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_KILLFOCUS) {
						if (self->_callback) self->_callback->FocusChanged(self, false);
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
						if (self->_callback) {
							bool processed;
							if (wparam == VK_SHIFT) {
								if (MapVirtualKeyW((lparam & 0xFF0000) >> 16, MAPVK_VSC_TO_VK_EX) == VK_LSHIFT) processed = self->_callback->KeyDown(self, KeyCodes::LeftShift);
								else processed = self->_callback->KeyDown(self, KeyCodes::RightShift);
							} else if (wparam == VK_CONTROL) {
								if (MapVirtualKeyW((lparam & 0xFF0000) >> 16, MAPVK_VSC_TO_VK_EX) == VK_LSHIFT) processed = self->_callback->KeyDown(self, KeyCodes::LeftControl);
								else processed = self->_callback->KeyDown(self, KeyCodes::RightControl);
							} else if (wparam == VK_MENU) {
								if (MapVirtualKeyW((lparam & 0xFF0000) >> 16, MAPVK_VSC_TO_VK_EX) == VK_LSHIFT) processed = self->_callback->KeyDown(self, KeyCodes::LeftAlternative);
								else processed = self->_callback->KeyDown(self, KeyCodes::RightAlternative);
							} else processed = self->_callback->KeyDown(self, int32(wparam));
							DefWindowProcW(wnd, msg, wparam, lparam);
							if (processed) return 0; else return 1;
						} else {
							DefWindowProcW(wnd, msg, wparam, lparam);
							return 1;
						}
					} else if (msg == WM_KEYUP || msg == WM_SYSKEYUP) {
						if (self->_callback) {
							if (wparam == VK_SHIFT) {
								if (MapVirtualKeyW((lparam & 0xFF0000) >> 16, MAPVK_VSC_TO_VK_EX) == VK_LSHIFT) self->_callback->KeyUp(self, KeyCodes::LeftShift);
								else self->_callback->KeyUp(self, KeyCodes::RightShift);
							} else if (wparam == VK_CONTROL) {
								if (MapVirtualKeyW((lparam & 0xFF0000) >> 16, MAPVK_VSC_TO_VK_EX) == VK_LSHIFT) self->_callback->KeyUp(self, KeyCodes::LeftControl);
								else self->_callback->KeyUp(self, KeyCodes::RightControl);
							} else if (wparam == VK_MENU) {
								if (MapVirtualKeyW((lparam & 0xFF0000) >> 16, MAPVK_VSC_TO_VK_EX) == VK_LSHIFT) self->_callback->KeyUp(self, KeyCodes::LeftAlternative);
								else self->_callback->KeyUp(self, KeyCodes::RightAlternative);
							} else self->_callback->KeyUp(self, int32(wparam));
						}
						DefWindowProcW(wnd, msg, wparam, lparam);
						return 0;
					} else if (msg == WM_CHAR) {
						if ((wparam & 0xFC00) == 0xD800) {
							self->_surrogate = ((wparam & 0x3FF) << 10) + 0x10000;
						} else if ((wparam & 0xFC00) == 0xDC00) {
							self->_surrogate |= (wparam & 0x3FF) + 0x10000;
							if (self->_callback) self->_callback->CharDown(self, self->_surrogate);
							self->_surrogate = 0;
						} else {
							self->_surrogate = 0;
							if (self->_callback) self->_callback->CharDown(self, uint32(wparam));
						}
						return FALSE;
					} else if (msg == WM_CAPTURECHANGED) {
						if (self->_callback) {
							if (reinterpret_cast<HWND>(lparam) != wnd) self->_callback->CaptureChanged(self, false);
							else self->_callback->CaptureChanged(self, true);
						}
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_MOUSEMOVE) {
						POINTS point = MAKEPOINTS(lparam);
						if (self->_callback) {
							self->_callback->SetCursor(self, Point(point.x, point.y));
							self->_callback->MouseMove(self, Point(point.x, point.y));
						}
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_LBUTTONDOWN) {
						POINTS point = MAKEPOINTS(lparam);
						if (self->_callback) self->_callback->LeftButtonDown(self, Point(point.x, point.y));
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_LBUTTONUP) {
						POINTS point = MAKEPOINTS(lparam);
						if (self->_callback) self->_callback->LeftButtonUp(self, Point(point.x, point.y));
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_LBUTTONDBLCLK) {
						POINTS point = MAKEPOINTS(lparam);
						if (self->_callback) self->_callback->LeftButtonDoubleClick(self, Point(point.x, point.y));
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_RBUTTONDOWN) {
						POINTS point = MAKEPOINTS(lparam);
						if (self->_callback) self->_callback->RightButtonDown(self, Point(point.x, point.y));
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_RBUTTONUP) {
						POINTS point = MAKEPOINTS(lparam);
						if (self->_callback) self->_callback->RightButtonUp(self, Point(point.x, point.y));
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_RBUTTONDBLCLK) {
						POINTS point = MAKEPOINTS(lparam);
						if (self->_callback) self->_callback->RightButtonDoubleClick(self, Point(point.x, point.y));
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_MOUSEWHEEL) {
						if (self->_callback) self->_callback->ScrollVertically(self, -double(GET_WHEEL_DELTA_WPARAM(wparam)) * 3.0 / double(WHEEL_DELTA));
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_MOUSEHWHEEL) {
						if (self->_callback) self->_callback->ScrollHorizontally(self, double(GET_WHEEL_DELTA_WPARAM(wparam)) * 3.0 / double(WHEEL_DELTA));
						return DefWindowProcW(wnd, msg, wparam, lparam);
					} else if (msg == WM_DESTROY) {
						if (self->_callback) self->_callback->Destroyed(self);
						self->Release();
						SetWindowLongPtrW(wnd, 0, 0);
						return 0;
					} else return DefWindowProcW(wnd, msg, wparam, lparam);
				} else {
					if (msg == WM_PAINT) InvalidateRect(wnd, 0, FALSE);
					else return DefWindowProcW(wnd, msg, wparam, lparam);
				}
			}
			SystemWindow(const CreateWindowDesc & desc, HINSTANCE instance, bool modal = false) : _children(0x20), _taskbar(0), _effect_flags(0), _effective_flags(0),
				_last_x(0x80000000), _last_y(0x80000000), _surrogate(0), _layer_flags(0), _first_time_render(true), _render_callback(0), _clear_color(0), _is_modal(modal), _layered(0)
			{
				_callback = desc.Callback;
				_parent = desc.ParentWindow ? static_cast<SystemWindow *>(desc.ParentWindow) : 0;
				_margins.cxLeftWidth = desc.FrameMargins.Left;
				_margins.cyTopHeight = desc.FrameMargins.Top;
				_margins.cxRightWidth = desc.FrameMargins.Right;
				_margins.cyBottomHeight = desc.FrameMargins.Bottom;
				DWORD ex_style, style;
				LPCWSTR wnd_class;
				_make_window_style_for_flags(desc.Flags, desc.ParentWindow, style, ex_style, wnd_class);
				bool need_null_bk = false;
				if (desc.Flags & WindowFlagTransparent) {
					_effect_flags |= 0x8;
					need_null_bk = true;
				}
				if (desc.Flags & WindowFlagBlurBehind) {
					_effect_flags |= 0xA;
					need_null_bk = true;
				}
				if (desc.Flags & WindowFlagWindowsExtendedFrame) _effect_flags |= 1;
				if (desc.Flags & WindowFlagBlurFactor) _blur_factor = desc.BlurFactor; else _blur_factor = UI::CurrentScaleFactor * 25.0;
				if (need_null_bk && Effect::Init()) ex_style |= WS_EX_NOREDIRECTIONBITMAP; else _effect_flags &= 0x7;
				RECT rect = { desc.Position.Left, desc.Position.Top, desc.Position.Right, desc.Position.Bottom };
				_min_constraints = desc.MinimalConstraints;
				_max_constraints = desc.MaximalConstraints;
				auto screen_box = desc.Screen ? desc.Screen->GetScreenRectangle() : Box(0, 0, 0, 0);
				rect.right -= rect.left;
				rect.bottom -= rect.top;
				rect.left += screen_box.Left;
				rect.top += screen_box.Top;
				_window = CreateWindowExW(ex_style , wnd_class, desc.Title, style, rect.left, rect.top, rect.right, rect.bottom, _parent ? _parent->_window : 0, 0, instance, this);
				if (!_window) throw Exception();
				if (!(desc.Flags & WindowFlagCloseButton)) EnableMenuItem(GetSystemMenu(_window, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				if (!(desc.Flags & WindowFlagMinimizeButton)) EnableMenuItem(GetSystemMenu(_window, FALSE), SC_MINIMIZE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				if (!(desc.Flags & WindowFlagMaximizeButton)) EnableMenuItem(GetSystemMenu(_window, FALSE), SC_MAXIMIZE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				if (!(desc.Flags & WindowFlagSizeble)) EnableMenuItem(GetSystemMenu(_window, FALSE), SC_SIZE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				if (desc.Flags & WindowFlagNonOpaque) SetOpacity(desc.Opacity);
				UINT w11_window_brush = need_null_bk ? 1 : 2;
				if ((desc.Flags & WindowFlagWindowsTitleMask) == WindowFlagWindowsTabbedTitle) {
					w11_window_brush = 4;
				} else if ((desc.Flags & WindowFlagWindowsTitleMask) == WindowFlagWindowsTransientTitle) {
					w11_window_brush = 3;
				} else if ((desc.Flags & WindowFlagWindowsTitleMask) == WindowFlagWindowsColoredTitle) {
					DWORD color = desc.BackgroundColor.Value & 0xFFFFFF;
					w11_window_brush = 1;
					DwmSetWindowAttribute(_window, 35, &color, sizeof(color));
				}
				if ((desc.Flags & WindowFlagOverrideTheme) && desc.Theme == ThemeClass::Dark) {
					UINT value = 1;
					DwmSetWindowAttribute(_window, 20, &value, sizeof(value));
				}
				DwmSetWindowAttribute(_window, 38, &w11_window_brush, sizeof(w11_window_brush));
				if (_effect_flags) { _effect_flags |= 4; _dwm_reset(); }
				if (_parent) _parent->_children.Append(this);
			}
			virtual ~SystemWindow(void) override { if (_taskbar) _taskbar->Release(); if (_layered) Effect::ReleaseLayers(_layered); }
			virtual void Show(bool show) override { ShowWindow(_window, show ? ((GetWindowLongPtr(_window, GWL_EXSTYLE) & WS_EX_NOACTIVATE) ? SW_SHOWNOACTIVATE : SW_SHOW) : SW_HIDE); }
			virtual bool IsVisible(void) override { return IsWindowVisible(_window) != 0; }
			virtual void SetText(const string & text) override { SetWindowTextW(_window, text); }
			virtual string GetText(void) override
			{
				DynamicString result;
				result.ReserveLength(GetWindowTextLengthW(_window) + 1);
				GetWindowTextW(_window, result, result.ReservedLength());
				return result.ToString();
			}
			virtual void SetPosition(const Box & box) override
			{
				int width = box.Right - box.Left;
				int height = box.Bottom - box.Top;
				SetWindowPos(_window, 0, box.Left, box.Top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
			}
			virtual Box GetPosition(void) override
			{
				RECT rect;
				GetWindowRect(_window, &rect);
				return Box(rect.left, rect.top, rect.right, rect.bottom);
			}
			virtual Point GetClientSize(void) override
			{
				RECT rect;
				GetClientRect(_window, &rect);
				return Point(rect.right, rect.bottom);
			}
			virtual void SetMinimalConstraints(Point size) override { _min_constraints = size; }
			virtual Point GetMinimalConstraints(void) override { return _min_constraints; }
			virtual void SetMaximalConstraints(Point size) override { _max_constraints = size; }
			virtual Point GetMaximalConstraints(void) override { return _max_constraints; }
			virtual void Activate(void) override { SetActiveWindow(_window); }
			virtual bool IsActive(void) override { return GetActiveWindow() == _window; }
			virtual void Maximize(void) override { ShowWindow(_window, SW_MAXIMIZE); }
			virtual bool IsMaximized(void) override { return IsZoomed(_window); }
			virtual void Minimize(void) override { ShowWindow(_window, SW_MINIMIZE); }
			virtual bool IsMinimized(void) override { return IsIconic(_window); }
			virtual void Restore(void) override { ShowWindow(_window, SW_SHOWNORMAL); }
			virtual void RequireAttention(void) override
			{
				if (!IsWindowVisible(_window)) return;
				FLASHWINFO info;
				info.cbSize = sizeof(info);
				info.hwnd = _window;
				info.dwFlags = FLASHW_ALL;
				info.uCount = 1;
				info.dwTimeout = 0;
				FlashWindowEx(&info);
			}
			virtual void SetOpacity(double opacity) override
			{
				SetWindowLongW(_window, GWL_EXSTYLE, GetWindowLongW(_window, GWL_EXSTYLE) | WS_EX_LAYERED);
				uint8 level = uint8(max(min(int(opacity * 255.0), 255), 0));
				SetLayeredWindowAttributes(_window, 0, level, LWA_ALPHA);
			}
			virtual void SetCloseButtonState(CloseButtonState state) override
			{
				if (state != CloseButtonState::Disabled) EnableMenuItem(GetSystemMenu(_window, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_ENABLED);
				else EnableMenuItem(GetSystemMenu(_window, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
			}
			virtual IWindow * GetParentWindow(void) override { return _parent; }
			virtual IWindow * GetChildWindow(int index) override { return _children[index]; }
			virtual int GetChildrenCount(void) override { return _children.Length(); }
			virtual void SetProgressMode(ProgressDisplayMode mode) override
			{
				if (!_taskbar) if (CoCreateInstance(CLSID_TaskbarList, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_taskbar)) != S_OK) _taskbar = 0;
				if (_taskbar) {
					if (mode == ProgressDisplayMode::Hide) _taskbar->SetProgressState(_window, TBPF_NOPROGRESS);
					else if (mode == ProgressDisplayMode::Normal) _taskbar->SetProgressState(_window, TBPF_NORMAL);
					else if (mode == ProgressDisplayMode::Paused) _taskbar->SetProgressState(_window, TBPF_PAUSED);
					else if (mode == ProgressDisplayMode::Error) _taskbar->SetProgressState(_window, TBPF_ERROR);
					else if (mode == ProgressDisplayMode::Indeterminated) _taskbar->SetProgressState(_window, TBPF_INDETERMINATE);
				}
			}
			virtual void SetProgressValue(double value) override
			{
				if (!_taskbar) if (CoCreateInstance(CLSID_TaskbarList, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_taskbar)) != S_OK) _taskbar = 0;
				if (_taskbar) _taskbar->SetProgressValue(_window, uint64(value * 10000.0), 10000);
			}
			virtual void SetCocoaEffectMaterial(CocoaEffectMaterial material) override {}
			virtual void SetCallback(IWindowCallback * callback) override { _callback = callback; }
			virtual IWindowCallback * GetCallback(void) override { return _callback; }
			virtual bool PointHitTest(Point at) override
			{
				POINT point; point.x = at.x; point.y = at.y;
				return (WindowFromPoint(point) == _window);
			}
			virtual Point PointClientToGlobal(Point at) override
			{
				POINT point; point.x = at.x; point.y = at.y;
				ClientToScreen(_window, &point);
				return Point(point.x, point.y);
			}
			virtual Point PointGlobalToClient(Point at) override
			{
				POINT point; point.x = at.x; point.y = at.y;
				ScreenToClient(_window, &point);
				return Point(point.x, point.y);
			}
			virtual void SetFocus(void) override { ::SetFocus(_window); }
			virtual bool IsFocused(void) override { return GetFocus() == _window; }
			virtual void SetCapture(void) override { ::SetCapture(_window); }
			virtual void ReleaseCapture(void) override { ::ReleaseCapture(); }
			virtual bool IsCaptured(void) override { return GetCapture() == _window; }
			virtual void SetTimer(uint32 id, uint32 period) override { if (period) ::SetTimer(_window, id, period, 0); else KillTimer(_window, id); }
			virtual void SetPresentationEngine(IPresentationEngine * engine) override
			{
				if (_layer) {
					_layer_flags = 0;
					_render_callback = 0;
					_layer->Detach();
					_layer.SetReference(0);
				}
				if (_layered) {
					Effect::ReleaseLayers(_layered);
					_layered = 0;
				}
				if (engine) {
					_layer.SetRetain(engine);
					_layer->Attach(this);
					_layer->Invalidate();
				}
				_first_time_render = true;
			}
			virtual IPresentationEngine * GetPresentationEngine(void) override { return _layer; }
			virtual void InvalidateContents(void) override { if (_layer) _layer->Invalidate(); }
			virtual void SetBackbufferedRenderingDevice(Codec::Frame * image, ImageRenderMode mode, Color filling) noexcept override
			{
				try {
					SafePointer<IPresentationEngine> engine;
					if (_effective_flags & WindowFlagTransparent) engine = new SystemLayeredPresentationEngine(filling, image, mode);
					else engine = new SystemBackbufferedPresentationEngine(filling, image, mode);
					SetPresentationEngine(engine);
				} catch (...) {}
			}
			virtual I2DPresentationEngine * Set2DRenderingDevice(DeviceClass device_class) noexcept override
			{
				if (device_class == DeviceClass::Null) return 0;
				try {
					SafePointer<I2DPresentationEngine> engine = new System2DRenderingDevice(device_class);
					SetPresentationEngine(engine);
					return engine;
				} catch (...) { return 0; }
			}
			virtual double GetDpiScale(void) override
			{
				HDC dc = GetDC(_window);
				if (!dc) return 0.0;
				int dpi = GetDeviceCaps(dc, LOGPIXELSX);
				ReleaseDC(_window, dc);
				return double(dpi) / 96.0;
			}
			virtual IScreen * GetCurrentScreen(void) override { return new SystemScreen(MonitorFromWindow(_window, MONITOR_DEFAULTTONEAREST)); }
			virtual ITheme * GetCurrentTheme(void) override { return Windows::GetCurrentTheme(); }
			virtual uint GetBackgroundFlags(void) override { return _effective_flags; }
			virtual handle GetOSHandle(void) override { return _window; }
			virtual void Destroy(void) override
			{
				while (_children.Length()) _children.LastElement()->Destroy();
				if (_parent) {
					for (int i = 0; i < _parent->_children.Length(); i++) if (_parent->_children[i] == this) {
						_parent->_children.Remove(i);
						break;
					}
				}
				SetPresentationEngine(0);
				DestroyWindow(_window);
			}
			virtual void SubmitTask(IDispatchTask * task) override { GetWindowSystem()->SubmitTask(task); }
			virtual void BeginSubmit(void) override {}
			virtual void AppendTask(IDispatchTask * task) override { GetWindowSystem()->SubmitTask(task); }
			virtual void EndSubmit(void) override {}
		};
		class SystemStatusBarIcon : public IStatusBarIcon
		{
			friend class WindowSystem;

			IStatusCallback * _callback;
			SafePointer<Codec::Image> _icon;
			StatusBarIconColorUsage _icon_usage;
			string _tooltip;
			int _id;
			SafePointer<IMenu> _menu;
			bool _present;
			HWND _tray_window;
			HICON _tray_icon;

			void _update_tray(void)
			{
				NOTIFYICONDATAW data;
				ZeroMemory(&data, sizeof(data));
				data.cbSize = sizeof(data);
				data.hWnd = _tray_window;
				data.uID = 1;
				data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
				data.uCallbackMessage = ERTM_TRAYEVENT;
				data.hIcon = _tray_icon;
				string tip = _tooltip.Fragment(0, 127);
				MemoryCopy(data.szTip, static_cast<const widechar *>(tip), tip.Length() * 2);
				Shell_NotifyIconW(NIM_MODIFY, &data);
			}
			bool _is_light_theme(void)
			{
				try {
					SafePointer<WindowsSpecific::RegistryKey> root = WindowsSpecific::OpenRootRegistryKey(WindowsSpecific::RegistryRootKey::CurrentUser);
					if (!root) return false;
					SafePointer<WindowsSpecific::RegistryKey> personalize =
						root->OpenKey(L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", WindowsSpecific::RegistryKeyAccess::ReadOnly);
					if (!personalize) return false;
					uint32 value = personalize->GetValueUInt32(L"SystemUsesLightTheme");
					return value != 0;
				} catch (...) { return false; }
			}
			void _update_icon(void)
			{
				if (_tray_icon) DestroyIcon(_tray_icon);
				try {
					Codec::Frame * frame = _icon ? _icon->GetFrameBestSizeFit(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON)) : 0;
					if (frame) {
						SafePointer<Codec::Frame> use_frame;
						if (_icon_usage == StatusBarIconColorUsage::Monochromic) {
							use_frame = frame->ConvertFormat(Codec::PixelFormat::B8G8R8A8, Codec::AlphaMode::Normal, Codec::ScanOrigin::BottomUp);
							uint32 color = _is_light_theme() ? 0x00000000 : 0x00FFFFFF;
							for (int y = 0; y < use_frame->GetHeight(); y++) for (int x = 0; x < use_frame->GetWidth(); x++) {
								uint32 pixel = use_frame->GetPixel(x, y);
								use_frame->SetPixel(x, y, (pixel & 0xFF000000) | color);
							}
						} else use_frame.SetRetain(frame);
						_tray_icon = _create_icon(use_frame);
					} else _tray_icon = 0;
				} catch (...) { _tray_icon = 0; }
				if (_present) _update_tray();
			}
		public:
			SystemStatusBarIcon(void) : _callback(0), _icon_usage(StatusBarIconColorUsage::Colourfull), _id(0), _present(false), _tray_icon(0)
			{
				_tray_window = CreateWindowExW(0, ENGINE_TRAY_WINDOW_CLASS, L"", WS_OVERLAPPED, 0, 0, 0, 0, 0, 0, 0, 0);
				if (!_tray_window) throw Exception();
				SetWindowLongPtrW(_tray_window, 0, reinterpret_cast<LONG_PTR>(this));
			}
			virtual ~SystemStatusBarIcon(void) override
			{
				if (_present) PresentIcon(false);
				DestroyWindow(_tray_window);
				if (_tray_icon) DestroyIcon(_tray_icon);
			}
			virtual void SetCallback(IStatusCallback * callback) noexcept override { _callback = callback; }
			virtual IStatusCallback * GetCallback(void) noexcept override { return _callback; }
			virtual Point GetIconSize(void) noexcept override { return Point(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON)); }
			virtual void SetIcon(Codec::Image * image) noexcept override { _icon.SetRetain(image); _update_icon(); }
			virtual Codec::Image * GetIcon(void) noexcept override { return _icon; }
			virtual void SetIconColorUsage(StatusBarIconColorUsage color_usage) noexcept override { _icon_usage = color_usage; _update_icon(); }
			virtual StatusBarIconColorUsage GetIconColorUsage(void) noexcept override { return _icon_usage; }
			virtual void SetTooltip(const string & text) noexcept override { _tooltip = text; if (_present) _update_tray(); }
			virtual string GetTooltip(void) noexcept override { return _tooltip; }
			virtual void SetEventID(int ID) noexcept override { _id = ID; }
			virtual int GetEventID(void) noexcept override { return _id; }
			virtual void SetMenu(IMenu * menu) noexcept override { _menu.SetRetain(menu); }
			virtual IMenu * GetMenu(void) noexcept override { return _menu; }
			virtual bool PresentIcon(bool present) noexcept override
			{
				if (present == _present) return true;
				if (!_tray_icon) return false;
				if (present) {
					NOTIFYICONDATAW data;
					ZeroMemory(&data, sizeof(data));
					data.cbSize = sizeof(data);
					data.hWnd = _tray_window;
					data.uID = 1;
					data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
					data.uCallbackMessage = ERTM_TRAYEVENT;
					data.hIcon = _tray_icon;
					string tip = _tooltip.Fragment(0, 127);
					MemoryCopy(data.szTip, static_cast<const widechar *>(tip), tip.Length() * 2);
					if (!Shell_NotifyIconW(NIM_ADD, &data)) return false;
					_present = true;
				} else {
					NOTIFYICONDATAW data;
					ZeroMemory(&data, sizeof(data));
					data.cbSize = sizeof(data);
					data.hWnd = _tray_window;
					data.uID = 1;
					if (!Shell_NotifyIconW(NIM_DELETE, &data)) return false;
					_present = false;
				}
				return true;
			}
			virtual bool IsVisible(void) noexcept override { return _present; }
		};
		class SystemIPCClient : public IIPCClient
		{
			struct PendingResponce
			{
				HGLOBAL data;
				PendingResponce * next;
				IDispatchTask * task;
				IPCStatus * status;
				DataBlock ** result;
			};
			HWND _client_window, _server_window;
			IPCStatus _status;
			bool _status_initialization;
			UINT _exchange_format;
			PendingResponce * _first, * _last;

			void _enqueue_responce(PendingResponce * responce) noexcept
			{
				responce->next = 0;
				if (_last) { _last->next = responce; _last = responce; } else { _last = _first = responce; }
			}
			PendingResponce * _dequeue_responce(void) noexcept
			{
				PendingResponce * result = _first;
				if (_first) {
					_first = _first->next;
					if (!_first) _last = 0;
				}
				return result;
			}
		public:
			static LRESULT WINAPI _dde_client_window_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
			{
				if (msg == WM_DDE_ACK) {
					auto self = reinterpret_cast<SystemIPCClient *>(GetWindowLongPtrW(wnd, 0));
					if (self->_status_initialization) {
						HWND server = reinterpret_cast<HWND>(wparam);
						GlobalDeleteAtom(LOWORD(lparam));
						GlobalDeleteAtom(HIWORD(lparam));
						if (self->_server_window) PostMessageW(server, WM_DDE_TERMINATE, reinterpret_cast<WPARAM>(wnd), 0);
						else self->_server_window = server;
					} else {
						UINT_PTR result;
						UINT_PTR atom;
						UnpackDDElParam(WM_DDE_ACK, lparam, &result, &atom);
						auto responce = self->_dequeue_responce();
						auto status = (result & 0x8000) ? true : false;
						self->_status = status ? IPCStatus::Accepted : IPCStatus::Discarded;
						if (responce) {
							if (responce->data && !status) GlobalFree(responce->data);
							if (responce->result) *responce->result = 0;
							if (responce->status) *responce->status = self->_status;
							if (responce->task) {
								GetWindowSystem()->SubmitTask(responce->task);
								responce->task->Release();
							}
						}
						delete responce;
						if (atom) GlobalDeleteAtom(atom);
						FreeDDElParam(WM_DDE_ACK, lparam);
					}
					return 0;
				} else if (msg == WM_DDE_DATA) {
					auto self = reinterpret_cast<SystemIPCClient *>(GetWindowLongPtrW(wnd, 0));
					HGLOBAL data_handle = 0;
					UINT_PTR verb_atom = 0;
					UnpackDDElParam(WM_DDE_DATA, lparam, reinterpret_cast<PUINT_PTR>(&data_handle), reinterpret_cast<PUINT_PTR>(&verb_atom));
					auto data = reinterpret_cast<DDEDATA *>(GlobalLock(data_handle));
					if (!data) {
						GlobalFree(data_handle);
						GlobalDeleteAtom(verb_atom);
					} else {
						if (uint16(data->cfFormat) == self->_exchange_format) {
							IPCStatus status;
							SafePointer<DataBlock> block;
							try {
								int length = *reinterpret_cast<uint32 *>(&data->Value);
								block = new DataBlock(length);
								block->SetLength(length);
								MemoryCopy(block->GetBuffer(), data->Value + 4, length);
								status = IPCStatus::Accepted;
							} catch (...) { status = IPCStatus::InternalError; block.SetReference(0); }
							self->_status = status;
							auto responce = self->_dequeue_responce();
							if (responce) {
								if (responce->data) GlobalFree(responce->data);
								if (responce->status) *responce->status = status;
								if (responce->result) {
									*responce->result = block.Inner();
									if (block) block->Retain();
								}
								if (responce->task) {
									GetWindowSystem()->SubmitTask(responce->task);
									responce->task->Release();
								}
							}
							delete responce;
						}
						bool release = data->fRelease ? true : false;
						bool ack = data->fAckReq ? true : false;
						GlobalUnlock(data_handle);
						if (release) GlobalFree(data_handle);
						FreeDDElParam(WM_DDE_DATA, lparam);
						if (ack) PostMessageW(self->_server_window, WM_DDE_ACK, reinterpret_cast<WPARAM>(wnd), PackDDElParam(WM_DDE_ACK, 0x8000, verb_atom));
						else GlobalDeleteAtom(verb_atom);
					}
					return 0;
				} else if (msg == WM_DDE_TERMINATE) {
					auto self = reinterpret_cast<SystemIPCClient *>(GetWindowLongPtrW(wnd, 0));
					if (self) {
						HWND sender = reinterpret_cast<HWND>(wparam);
						if (sender == self->_server_window) {
							PostMessageW(self->_server_window, WM_DDE_TERMINATE, reinterpret_cast<WPARAM>(wnd), 0);
							self->_server_window = 0;
							self->_status = IPCStatus::ServerClosed;
							PendingResponce * responce;
							while (responce = self->_dequeue_responce()) {
								if (responce->data) GlobalFree(responce->data);
								if (responce->status) *responce->status = IPCStatus::ServerClosed;
								if (responce->result) *responce->result = 0;
								if (responce->task) {
									GetWindowSystem()->SubmitTask(responce->task);
									responce->task->Release();
								}
								delete responce;
							}
						}
					} else DestroyWindow(wnd);
					return 0;
				} else return DefWindowProcW(wnd, msg, wparam, lparam);
			}
			SystemIPCClient(const string & server_name) : _server_window(0), _status(IPCStatus::Unknown), _first(0), _last(0)
			{
				_exchange_format = RegisterClipboardFormatW(ENGINE_DDE_FORMAT);
				if (!_exchange_format) throw Exception();
				_status_initialization = true;
				_client_window = CreateWindowExW(0, ENGINE_DDE_CLIENT_WINDOW_CLASS, L"", WS_OVERLAPPED, 0, 0, 0, 0, 0, 0, 0, 0);
				if (!_client_window) throw Exception();
				SetWindowLongPtrW(_client_window, 0, reinterpret_cast<LONG_PTR>(this));
				ATOM app = GlobalAddAtomW(server_name);
				ATOM topic = GlobalAddAtomW(ENGINE_DDE_TOPIC);
				try {
					if (!app || !topic) throw Exception();
					SendMessageTimeoutW(HWND_BROADCAST, WM_DDE_INITIATE, reinterpret_cast<WPARAM>(_client_window), MAKELONG(app, topic), SMTO_ABORTIFHUNG, 1000, 0);
				} catch (...) {
					DestroyWindow(_client_window);
					if (app) GlobalDeleteAtom(app);
					if (topic) GlobalDeleteAtom(topic);
					throw;
				}
				GlobalDeleteAtom(app);
				GlobalDeleteAtom(topic);
				_status_initialization = false;
				if (!_server_window) { DestroyWindow(_client_window); throw Exception(); }
			}
			virtual ~SystemIPCClient(void) override
			{
				if (_server_window) {
					SetWindowLongPtrW(_client_window, 0, 0);
					PostMessageW(_server_window, WM_DDE_TERMINATE, reinterpret_cast<WPARAM>(_client_window), 0);
				} else DestroyWindow(_client_window);
				PendingResponce * responce;
				while (responce = _dequeue_responce()) {
					if (responce->data) GlobalFree(responce->data);
					if (responce->status) *responce->status = IPCStatus::ServerClosed;
					if (responce->result) *responce->result = 0;
					if (responce->task) {
						GetWindowSystem()->SubmitTask(responce->task);
						responce->task->Release();
					}
					delete responce;
				}
			}
			virtual bool SendData(const string & verb, const DataBlock * data, IDispatchTask * on_responce, IPCStatus * result) noexcept override
			{
				if (_status == IPCStatus::ServerClosed) return false;
				if (verb.Length() > 0xFF) return false;
				PendingResponce * responce = new (std::nothrow) PendingResponce;
				if (!responce) return false;
				ATOM verb_atom = GlobalAddAtomW(verb);
				if (!verb_atom) { delete responce; return false; }
				responce->data = GlobalAlloc(GMEM_MOVEABLE, sizeof(DDEPOKE) + 4 + (data ? data->Length() : 0));
				if (!responce->data) { GlobalDeleteAtom(verb_atom); delete responce; return false; }
				auto poke = reinterpret_cast<DDEPOKE *>(GlobalLock(responce->data));
				if (!poke) { GlobalFree(responce->data); GlobalDeleteAtom(verb_atom); delete responce; return false; }
				poke->fRelease = TRUE;
				poke->unused = 0;
				poke->fReserved = 0;
				poke->cfFormat = _exchange_format;
				*reinterpret_cast<uint32 *>(&poke->Value) = (data ? data->Length() : 0);
				if (data) MemoryCopy(poke->Value + 4, data->GetBuffer(), data->Length());
				GlobalUnlock(responce->data);
				if (!PostMessageW(_server_window, WM_DDE_POKE, reinterpret_cast<WPARAM>(_client_window),
					PackDDElParam(WM_DDE_POKE, reinterpret_cast<UINT_PTR>(responce->data), verb_atom))) {
					GlobalFree(responce->data); GlobalDeleteAtom(verb_atom); delete responce; return false;
				}
				responce->status = result;
				responce->task = on_responce;
				if (responce->task) responce->task->Retain();
				responce->result = 0;
				_enqueue_responce(responce);
				return true;
			}
			virtual bool RequestData(const string & verb, IDispatchTask * on_responce, IPCStatus * result, DataBlock ** data) noexcept override
			{
				if (_status == IPCStatus::ServerClosed) return false;
				if (verb.Length() > 0xFF) return false;
				PendingResponce * responce = new (std::nothrow) PendingResponce;
				if (!responce) return false;
				ATOM verb_atom = GlobalAddAtomW(verb);
				if (!verb_atom) { delete responce; return false; }
				if (!PostMessageW(_server_window, WM_DDE_REQUEST, reinterpret_cast<WPARAM>(_client_window),
					PackDDElParam(WM_DDE_REQUEST, _exchange_format, verb_atom))) {
					GlobalDeleteAtom(verb_atom); delete responce; return false;
				}
				responce->data = 0;
				responce->status = result;
				responce->task = on_responce;
				if (responce->task) responce->task->Retain();
				responce->result = data;
				_enqueue_responce(responce);
				return true;
			}
			virtual IPCStatus GetStatus(void) noexcept override { return _status; }
		};
		class WindowSystem : public IWindowSystem
		{
			friend class DDEClient;
			class GlobalModalLock
			{
				Array<HWND> windows;
			public:
				GlobalModalLock(HWND window_for) : windows(0x10)
				{
					HWND hwindow = 0;
					DWORD pid = GetCurrentProcessId();
					do {
						hwindow = FindWindowExW(0, hwindow, 0, 0);
						if (hwindow) {
							DWORD wpid;
							GetWindowThreadProcessId(hwindow, &wpid);
							if (wpid == pid && hwindow != window_for && IsWindowEnabled(hwindow)) windows << hwindow;
						}
					} while (hwindow);
					for (auto & window : windows) EnableWindow(window, FALSE);
				}
				void Release(void) { for (auto & window : windows) EnableWindow(window, TRUE); }
			};
			class DDEClient : public Object
			{
				HWND _server, _client;
				UINT _exchange_format;
				bool _initialized;
			public:
				static LRESULT WINAPI _dde_server_window_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
				{
					if (msg == WM_DDE_POKE) {
						auto self = reinterpret_cast<DDEClient *>(GetWindowLongPtrW(wnd, 0));
						HGLOBAL data_handle = 0;
						UINT_PTR verb_atom = 0;
						UnpackDDElParam(WM_DDE_POKE, lparam, reinterpret_cast<PUINT_PTR>(&data_handle), reinterpret_cast<PUINT_PTR>(&verb_atom));
						widechar verb[0x100];
						GlobalGetAtomNameW(verb_atom, verb, 0x100);
						auto poke = reinterpret_cast<DDEPOKE *>(GlobalLock(data_handle));
						if (!poke || uint16(poke->cfFormat) != self->_exchange_format) {
							if (poke) GlobalUnlock(data_handle);
							FreeDDElParam(WM_DDE_POKE, lparam);
							PostMessageW(self->_client, WM_DDE_ACK, reinterpret_cast<WPARAM>(wnd), PackDDElParam(WM_DDE_ACK, 0, verb_atom));
							return 0;
						}
						bool result;
						try {
							uint32 length = *reinterpret_cast<uint32 *>(&poke->Value);
							SafePointer<DataBlock> data = new DataBlock(length);
							data->SetLength(length);
							MemoryCopy(data->GetBuffer(), poke->Value + 4, length);
							auto callback = GetWindowSystem()->GetCallback();
							try {
								if (callback) result = callback->DataExchangeReceive(self->_client, verb, data);
								else result = false;
							} catch (...) { result = false; }
						} catch (...) { result = false; }
						auto release = poke->fRelease;
						GlobalUnlock(data_handle);
						FreeDDElParam(WM_DDE_POKE, lparam);
						if (release && result) GlobalFree(data_handle);
						if (result) PostMessageW(self->_client, WM_DDE_ACK, reinterpret_cast<WPARAM>(wnd), PackDDElParam(WM_DDE_ACK, 0x8000, verb_atom));
						else PostMessageW(self->_client, WM_DDE_ACK, reinterpret_cast<WPARAM>(wnd), PackDDElParam(WM_DDE_ACK, 0, verb_atom));
						return 0;
					} else if (msg == WM_DDE_REQUEST) {
						auto self = reinterpret_cast<DDEClient *>(GetWindowLongPtrW(wnd, 0));
						UINT_PTR data_type;
						UINT_PTR verb_atom;
						UnpackDDElParam(WM_DDE_REQUEST, lparam, &data_type, &verb_atom);
						FreeDDElParam(WM_DDE_REQUEST, lparam);
						SafePointer<DataBlock> responce_data;
						if (data_type == self->_exchange_format) {
							widechar verb[0x100];
							GlobalGetAtomNameW(verb_atom, verb, 0x100);
							auto callback = GetWindowSystem()->GetCallback();
							try { if (callback) responce_data = callback->DataExchangeRespond(self->_client, verb); } catch (...) {}
						}
						if (responce_data) {
							HGLOBAL data_handle = GlobalAlloc(GMEM_MOVEABLE, sizeof(DDEDATA) + 4 + responce_data->Length());
							if (data_handle) {
								auto data = reinterpret_cast<DDEDATA *>(GlobalLock(data_handle));
								if (data) {
									data->unused = 0;
									data->fResponse = TRUE;
									data->fRelease = TRUE;
									data->reserved = 0;
									data->fAckReq = FALSE;
									data->cfFormat = self->_exchange_format;
									*reinterpret_cast<uint32 *>(&data->Value) = responce_data->Length();
									MemoryCopy(data->Value + 4, responce_data->GetBuffer(), responce_data->Length());
									GlobalUnlock(data_handle);
									if (!PostMessageW(self->_client, WM_DDE_DATA, reinterpret_cast<WPARAM>(wnd),
										PackDDElParam(WM_DDE_DATA, reinterpret_cast<UINT_PTR>(data_handle), verb_atom))) {
										GlobalFree(data_handle); responce_data.SetReference(0);
									}
								} else { GlobalFree(data_handle); responce_data.SetReference(0); }
							} else responce_data.SetReference(0);
						}
						if (!responce_data) PostMessageW(self->_client, WM_DDE_ACK, reinterpret_cast<WPARAM>(wnd), PackDDElParam(WM_DDE_ACK, 0, verb_atom));
						return 0;
					} else if (msg == WM_DDE_TERMINATE) {
						auto app = static_cast<WindowSystem *>(GetWindowSystem());
						auto self = reinterpret_cast<DDEClient *>(GetWindowLongPtrW(wnd, 0));
						if (self && self->_initialized) {
							self->_initialized = false;
							auto callback = GetWindowSystem()->GetCallback();
							try { if (callback) callback->DataExchangeDisconnect(self->_client); } catch (...) {}
							SetWindowLongPtrW(wnd, 0, 0);
							PostMessageW(self->_client, WM_DDE_TERMINATE, reinterpret_cast<WPARAM>(wnd), 0);
							for (int i = 0; i < app->_dde_clients.Length(); i++) if (app->_dde_clients.ElementAt(i) == self) {
								app->_dde_clients.Remove(i);
								break;
							}
						}
						return 0;
					} else return DefWindowProcW(wnd, msg, wparam, lparam);
				}
				DDEClient(HWND client) : _client(client), _initialized(false)
				{
					_exchange_format = RegisterClipboardFormatW(ENGINE_DDE_FORMAT);
					if (!_exchange_format) throw Exception();
					_server = CreateWindowExW(0, ENGINE_DDE_SERVER_WINDOW_CLASS, L"", WS_OVERLAPPED, 0, 0, 0, 0, 0, 0, 0, 0);
					if (!_server) throw Exception();
					SetWindowLongPtrW(_server, 0, reinterpret_cast<LONG_PTR>(this));
				}
				virtual ~DDEClient(void) override { DestroyWindow(_server); }
				void Terminate(void) { _initialized = false; PostMessageW(_client, WM_DDE_TERMINATE, reinterpret_cast<WPARAM>(_server), 0); }
				void FinilizeInitialization(void) { _initialized = true; }
				HWND GetWindowHandle(void) { return _server; }
			};
			struct MessageBoxData
			{
				MessageBoxResult * result;
				MessageBoxButtonSet buttons;
				MessageBoxStyle style;
				string text, title;
				IWindow * parent;
				IDispatchTask * task;
			};
			struct OpenFileData
			{
				OpenFileInfo * info;
				IWindow * parent;
				IDispatchTask * task;
			};
			struct SaveFileData
			{
				SaveFileInfo * info;
				IWindow * parent;
				IDispatchTask * task;
			};
			struct ChooseDirectoryData
			{
				ChooseDirectoryInfo * info;
				IWindow * parent;
				IDispatchTask * task;
			};

			Array<IWindow *> _main_window_list;
			Array<string> _file_list_open;
			SafePointer<ICursor> _null, _arrow, _beam, _link, _size_ew, _size_ns, _size_nwse, _size_nesw, _size_all;
			IApplicationCallback * _callback;
			HINSTANCE _instance;
			HWND _host_window;
			string _dde_server_name;
			ObjectArray<DDEClient> _dde_clients;
			bool _first_time_loop;

			static void _file_dialog_add_formats(Array<widechar> & filter, const widechar * name, const FileFormat * formats, int format_count)
			{
				Array<string> extensions(0x100);
				if (format_count) {
					for (int i = 0; i < format_count; i++) for (auto & ext : formats[i].Extensions) {
						bool present = false;
						for (auto & e : extensions) if (string::CompareIgnoreCase(ext, e) == 0) { present = true; break; }
						if (!present) extensions << ext;
					}
				} else extensions << L"*";
				DynamicString extlist;
				for (int i = 0; i < extensions.Length(); i++) { if (i) extlist << L";"; extlist << L"*." << extensions[i]; }
				filter.Append(name, StringLength(name));
				filter.Append(L" (", 2);
				filter.Append(extlist, extlist.Length());
				filter.Append(L")\0", 2);
				filter.Append(extlist, extlist.Length());
				filter.Append(0);
			}
			static int _message_box_proc(void * arg)
			{
				auto data = reinterpret_cast<MessageBoxData *>(arg);
				if (data->result) *data->result = MessageBoxResult::Cancel;
				UINT style = 0;
				if (data->buttons == MessageBoxButtonSet::Ok) style = MB_OK;
				else if (data->buttons == MessageBoxButtonSet::OkCancel) style = MB_OKCANCEL;
				else if (data->buttons == MessageBoxButtonSet::YesNo) style = MB_YESNO;
				else if (data->buttons == MessageBoxButtonSet::YesNoCancel) style = MB_YESNOCANCEL;
				if (data->style == MessageBoxStyle::Error) style |= MB_ICONSTOP;
				else if (data->style == MessageBoxStyle::Warning) style |= MB_ICONEXCLAMATION;
				else if (data->style == MessageBoxStyle::Information) style |= MB_ICONINFORMATION;
				HWND wnd_parent = data->parent ? reinterpret_cast<HWND>(data->parent->GetOSHandle()) : 0;
				int result = MessageBoxW(wnd_parent, data->text, data->title, style);
				if (data->result) {
					if (result == IDYES) *data->result = MessageBoxResult::Yes;
					else if (result == IDNO) *data->result = MessageBoxResult::No;
					else if (result == IDCANCEL) *data->result = MessageBoxResult::Cancel;
					else if (result == IDOK) *data->result = MessageBoxResult::Ok;
				}
				if (data->task) {
					try {
						if (data->parent) GetWindowSystem()->SubmitTask(data->task);
						else data->task->DoTask(GetWindowSystem());
					} catch (...) {}
					data->task->Release();
				}
				delete data;
				return 0;
			}
			static int _open_file_proc(void * arg)
			{
				auto data = reinterpret_cast<OpenFileData *>(arg);
				data->info->Files.Clear();
				try {
					OPENFILENAMEW ofn;
					ZeroMemory(&ofn, sizeof(ofn));
					ofn.lStructSize = sizeof(ofn);
					ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
					if (data->info->MultiChoose) ofn.Flags |= OFN_ALLOWMULTISELECT;
					Array<widechar> filter(0x100);
					DynamicString result;
					result.ReserveLength(0x10000);
					if (data->info->Formats.Length()) {
						if (data->info->Formats.Length() > 1) {
							_file_dialog_add_formats(filter, Assembly::GetLocalizedCommonString(201, L"All supported"),
								data->info->Formats.GetBuffer(), data->info->Formats.Length());
							ofn.nFilterIndex = 1 + data->info->DefaultFormat;
						} else ofn.nFilterIndex = 1;
						for (auto & format : data->info->Formats) {
							_file_dialog_add_formats(filter, format.Description, &format, 1);
						}
					} else ofn.nFilterIndex = 1;
					_file_dialog_add_formats(filter, Assembly::GetLocalizedCommonString(202, L"All files"), 0, 0);
					filter.Append(0);
					ofn.hwndOwner = data->parent ? reinterpret_cast<HWND>(data->parent->GetOSHandle()) : 0;
					ofn.lpstrFile = result;
					ofn.nMaxFile = result.ReservedLength();
					ofn.lpstrFilter = filter;
					if (data->info->Title.Length()) ofn.lpstrTitle = data->info->Title;
					if (GetOpenFileNameW(&ofn)) {
						if (data->info->MultiChoose) {
							Array<string> file_list(0x10);
							int sp = 0, ep = 0;
							while (true) {
								while (result[ep]) ep++;
								if (ep == sp) break;
								file_list << string(result.GetBuffer() + sp);
								ep++; sp = ep;
							}
							if (file_list.Length() > 1) {
								for (int i = 1; i < file_list.Length(); i++) data->info->Files << file_list[0] + string(IO::PathDirectorySeparator) + file_list[i];
							} else if (file_list.Length() == 1) data->info->Files << file_list.FirstElement();
						} else data->info->Files << result;
					}
				} catch (...) {}
				if (data->task) {
					try {
						if (data->parent) GetWindowSystem()->SubmitTask(data->task);
						else data->task->DoTask(GetWindowSystem());
					} catch (...) {}
					data->task->Release();
				}
				delete data;
				return 0;
			}
			static int _save_file_proc(void * arg)
			{
				auto data = reinterpret_cast<SaveFileData *>(arg);
				data->info->File = L"";
				try {
					OPENFILENAMEW ofn;
					ZeroMemory(&ofn, sizeof(ofn));
					ofn.lStructSize = sizeof(ofn);
					ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER;
					Array<widechar> filter(0x100);
					DynamicString result;
					result.ReserveLength(0x10000);
					if (data->info->Formats.Length()) {
						for (auto & format : data->info->Formats) _file_dialog_add_formats(filter, format.Description, &format, 1);
						ofn.nFilterIndex = 1 + data->info->Format;
					} else {
						_file_dialog_add_formats(filter, Assembly::GetLocalizedCommonString(202, L"All files"), 0, 0);
						ofn.nFilterIndex = 1;
					}
					filter.Append(0);
					ofn.hwndOwner = data->parent ? reinterpret_cast<HWND>(data->parent->GetOSHandle()) : 0;
					ofn.lpstrFile = result;
					ofn.nMaxFile = result.ReservedLength();
					ofn.lpstrFilter = filter;
					if (data->info->Title.Length()) ofn.lpstrTitle = data->info->Title;
					if (GetSaveFileNameW(&ofn)) {
						data->info->File = result.ToString();
						data->info->Format = ofn.nFilterIndex - 1;
						if (data->info->AppendExtension && data->info->Formats.Length()) {
							auto & format = data->info->Formats[data->info->Format];
							auto ext = IO::Path::GetExtension(data->info->File);
							bool append_ext = true;
							for (auto & e : format.Extensions) if (string::CompareIgnoreCase(ext, e) == 0) { append_ext = false; break; }
							if (append_ext) data->info->File += L"." + format.Extensions.FirstElement();
						}
					}
				} catch (...) {}
				if (data->task) {
					try {
						if (data->parent) GetWindowSystem()->SubmitTask(data->task);
						else data->task->DoTask(GetWindowSystem());
					} catch (...) {}
					data->task->Release();
				}
				delete data;
				return 0;
			}
			static int _choose_directory_proc(void * arg)
			{
				auto data = reinterpret_cast<ChooseDirectoryData *>(arg);
				data->info->Directory = L"";
				IFileOpenDialog * dialog = 0;
				HWND wnd_parent = data->parent ? reinterpret_cast<HWND>(data->parent->GetOSHandle()) : 0;
				if (CoCreateInstance(CLSID_FileOpenDialog, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)) == S_OK) {
					dialog->SetOptions(FOS_PICKFOLDERS | FOS_DONTADDTORECENT);
					if (data->info->Title.Length()) dialog->SetTitle(data->info->Title);
					if (dialog->Show(wnd_parent) == S_OK) {
						IShellItem * item = 0;
						if (dialog->GetResult(&item) == S_OK) {
							LPWSTR path = 0;
							if (item->GetDisplayName(SIGDN_FILESYSPATH, &path) == S_OK) {
								try { data->info->Directory = path; } catch (...) {}
								CoTaskMemFree(path);
							}
							item->Release();
						}
					}
					dialog->Release();
				}
				if (data->task) {
					try {
						if (data->parent) GetWindowSystem()->SubmitTask(data->task);
						else data->task->DoTask(GetWindowSystem());
					} catch (...) {}
					data->task->Release();
				}
				delete data;
				return 0;
			}
			static LRESULT WINAPI _host_window_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
			{
				if (msg == ERTM_EXECUTE) {
					auto task = reinterpret_cast<IDispatchTask *>(lparam);
					task->DoTask(GetWindowSystem());
					task->Release();
					return 0;
				} else if (msg == WM_QUERYENDSESSION) {
					auto callback = GetWindowSystem()->GetCallback();
					if (callback && callback->IsHandlerEnabled(ApplicationHandler::Terminate)) return callback->Terminate();
					else return TRUE;
				} else if (msg == WM_HOTKEY) {
					auto callback = GetWindowSystem()->GetCallback();
					if (callback) callback->HotKeyEvent(wparam);
					return 0;
				} else if (msg == WM_DDE_INITIATE) {
					auto self = static_cast<WindowSystem *>(GetWindowSystem());
					HWND sender = reinterpret_cast<HWND>(wparam);
					ATOM app = GlobalAddAtomW(self->_dde_server_name);
					ATOM topic = GlobalAddAtomW(ENGINE_DDE_TOPIC);
					ATOM required_app = LOWORD(lparam);
					ATOM required_topic = HIWORD(lparam);
					if (app && topic) {
						if (!required_app || required_app == app) if (!required_topic || required_topic == topic) {
							try {
								SafePointer<DDEClient> client = new DDEClient(sender);
								self->_dde_clients.Append(client);
								client->FinilizeInitialization();
								ATOM app_confirm = GlobalAddAtomW(self->_dde_server_name);
								ATOM topic_confirm = GlobalAddAtomW(ENGINE_DDE_TOPIC);
								SendMessageW(sender, WM_DDE_ACK, reinterpret_cast<WPARAM>(client->GetWindowHandle()), MAKELONG(app_confirm, topic_confirm));
							} catch (...) {}
						}
					}
					if (app) GlobalDeleteAtom(app);
					if (topic) GlobalDeleteAtom(topic);
					return 0;
				} else return DefWindowProcW(wnd, msg, wparam, lparam);
			}
			static LRESULT WINAPI _tray_window_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
			{
				if (msg == WM_MEASUREITEM) {
					return SystemMenu::_menu_measure_item(wnd, wparam, lparam);
				} else if (msg == WM_DRAWITEM) {
					return SystemMenu::_menu_draw_item(wnd, wparam, lparam);
				} else if (msg == ERTM_TRAYEVENT) {
					if (lparam == WM_LBUTTONUP) {
						SystemStatusBarIcon * icon = reinterpret_cast<SystemStatusBarIcon *>(GetWindowLongPtrW(wnd, 0));
						if (icon) {
							auto callback = icon->GetCallback();
							if (callback) {
								if (icon->GetEventID()) {
									callback->StatusIconCommand(icon, icon->GetEventID());
								} else if (icon->GetMenu()) {
									int result = static_cast<SystemMenu *>(icon->GetMenu())->_run(wnd, GetWindowSystem()->GetCursorPosition());
									if (result) callback->StatusIconCommand(icon, result);
								}
							}
						}
					}
					return 0;
				} else return DefWindowProcW(wnd, msg, wparam, lparam);
			}
		public:
			WindowSystem(void) : _callback(0), _main_window_list(0x20), _file_list_open(0x20), _dde_clients(0x10), _first_time_loop(true)
			{
				_instance = GetModuleHandleW(0);
				SafePointer<Codec::Frame> image = new Codec::Frame(1, 1, -1, Codec::PixelFormat::B8G8R8A8, Codec::AlphaMode::Normal, Codec::ScanOrigin::BottomUp);
				image->SetPixel(0, 0, 0);
				_null = new SystemCursor(_create_cursor(image), true);
				_arrow = new SystemCursor(LoadCursorW(0, IDC_ARROW), false);
				_beam = new SystemCursor(LoadCursorW(0, IDC_IBEAM), false);
				_link = new SystemCursor(LoadCursorW(0, IDC_HAND), false);
				_size_ew = new SystemCursor(LoadCursorW(0, IDC_SIZEWE), false);
				_size_ns = new SystemCursor(LoadCursorW(0, IDC_SIZENS), false);
				_size_nwse = new SystemCursor(LoadCursorW(0, IDC_SIZENWSE), false);
				_size_nesw = new SystemCursor(LoadCursorW(0, IDC_SIZENESW), false);
				_size_all = new SystemCursor(LoadCursorW(0, IDC_SIZEALL), false);
				WNDCLASSEXW cls;
				ZeroMemory(&cls, sizeof(cls));
				cls.cbSize = sizeof(cls);
				cls.style = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS | CS_GLOBALCLASS;
				cls.lpfnWndProc = SystemWindow::_core_window_proc;
				cls.cbWndExtra = sizeof(void *);
				cls.hInstance = _instance;
				cls.hIcon = reinterpret_cast<HICON>(LoadImageW(_instance, MAKEINTRESOURCEW(1), IMAGE_ICON,
					GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0));
				cls.hIconSm = reinterpret_cast<HICON>(LoadImageW(_instance, MAKEINTRESOURCEW(1), IMAGE_ICON,
					GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
				if (!cls.hIcon) cls.hIcon = reinterpret_cast<HICON>(LoadImageW(0, IDI_APPLICATION, IMAGE_ICON,
					GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0));
				if (!cls.hIconSm) cls.hIconSm = reinterpret_cast<HICON>(LoadImageW(0, IDI_APPLICATION, IMAGE_ICON,
					GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
				cls.lpszClassName = ENGINE_MAIN_WINDOW_CLASS;
				RegisterClassExW(&cls);
				cls.hIcon = cls.hIconSm = 0;
				cls.lpszClassName = ENGINE_DIALOG_WINDOW_CLASS;
				RegisterClassExW(&cls);
				cls.style = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS | CS_GLOBALCLASS | CS_DROPSHADOW;
				cls.lpszClassName = ENGINE_POPUP_WINDOW_CLASS;
				RegisterClassExW(&cls);
				cls.style = CS_GLOBALCLASS;
				cls.lpfnWndProc = _tray_window_proc;
				cls.lpszClassName = ENGINE_TRAY_WINDOW_CLASS;
				RegisterClassExW(&cls);
				cls.lpfnWndProc = SystemIPCClient::_dde_client_window_proc;
				cls.lpszClassName = ENGINE_DDE_CLIENT_WINDOW_CLASS;
				RegisterClassExW(&cls);
				cls.lpfnWndProc = DDEClient::_dde_server_window_proc;
				cls.lpszClassName = ENGINE_DDE_SERVER_WINDOW_CLASS;
				RegisterClassExW(&cls);
				cls.lpfnWndProc = _host_window_proc;
				cls.cbWndExtra = 0;
				cls.lpszClassName = ENGINE_HOST_WINDOW_CLASS;
				RegisterClassExW(&cls);
				_host_window = CreateWindowExW(0, ENGINE_HOST_WINDOW_CLASS, L"", 0, 0, 0, 0, 0, 0, 0, _instance, 0);
			}
			virtual ~WindowSystem(void) override { DestroyWindow(_host_window); for (auto & client : _dde_clients) client.Terminate(); _dde_clients.Clear(); }
			virtual IWindow * CreateWindow(const CreateWindowDesc & desc) noexcept override
			{
				try {
					auto result = new SystemWindow(desc, _instance);
					if (desc.Callback) desc.Callback->Created(result);
					return result;
				} catch (...) { return 0; }
			}
			virtual IWindow * CreateModalWindow(const CreateWindowDesc & desc) noexcept override
			{
				if (desc.ParentWindow && desc.ParentWindow->GetParentWindow() && !static_cast<SystemWindow *>(desc.ParentWindow)->_is_modal) return 0;
				try {
					auto dialog = new SystemWindow(desc, _instance);
					auto parent = desc.ParentWindow;
					static_cast<SystemWindow *>(dialog)->_is_modal = true;
					if (desc.Callback) desc.Callback->Created(dialog);
					dialog->Show(true);
					SetForegroundWindow(reinterpret_cast<HWND>(dialog->GetOSHandle()));
					SetActiveWindow(reinterpret_cast<HWND>(dialog->GetOSHandle()));
					if (parent) {
						EnableWindow(reinterpret_cast<HWND>(parent->GetOSHandle()), FALSE);
						return dialog;
					} else {
						GlobalModalLock lock(reinterpret_cast<HWND>(dialog->GetOSHandle()));
						RunMainLoop();
						lock.Release();
						dialog->Destroy();
						return 0;
					}
				} catch (...) { return 0; }
			}
			virtual Box ConvertClientToWindow(const Box & box, uint flags) noexcept override
			{
				DWORD style, ex_style;
				LPCWSTR wnd_class;
				_make_window_style_for_flags(flags, false, style, ex_style, wnd_class);
				RECT rect = { box.Left, box.Top, box.Right, box.Bottom };
				AdjustWindowRectEx(&rect, style, FALSE, ex_style);
				return Box(rect.left, rect.top, rect.right, rect.bottom);
			}
			virtual Point ConvertClientToWindow(const Point & size, uint flags) noexcept override
			{
				DWORD style, ex_style;
				LPCWSTR wnd_class;
				_make_window_style_for_flags(flags, false, style, ex_style, wnd_class);
				RECT rect = { 0, 0, size.x, size.y };
				AdjustWindowRectEx(&rect, style, FALSE, ex_style);
				return Point(rect.right - rect.left, rect.bottom - rect.top);
			}
			virtual void SetFilesToOpen(const string * files, int num_files) noexcept override
			{ 
				try {
					for (int i = 0; i < num_files; i++) _file_list_open.Append(files[i]);
				} catch (...) {}
			}
			virtual IApplicationCallback * GetCallback(void) noexcept override { return _callback; }
			virtual void SetCallback(IApplicationCallback * callback) noexcept override { _callback = callback; }
			virtual void RunMainLoop(void) noexcept override
			{
				if (_first_time_loop || _file_list_open.Length()) {
					bool opened = false;
					if (_callback && _callback->IsHandlerEnabled(ApplicationHandler::OpenExactFile)) {
						for (auto & file : _file_list_open) try { if (_callback->OpenExactFile(file)) opened = true; } catch (...) {}
					}
					_file_list_open.Clear();
					_first_time_loop = false;
					if (!opened && _callback && _callback->IsHandlerEnabled(ApplicationHandler::CreateFile)) _callback->CreateNewFile();
				}
				MSG msg;
				BOOL error;
				while (error = GetMessageW(&msg, 0, 0, 0)) {
					if (error == -1) break;
					auto result = DispatchMessageW(&msg);
					if (msg.message == WM_KEYDOWN || msg.message == WM_KEYUP || msg.message == WM_SYSKEYDOWN || msg.message == WM_SYSKEYUP) if (result) TranslateMessage(&msg);
				}
			}
			virtual void ExitMainLoop(void) noexcept override { PostQuitMessage(0); }
			virtual void ExitModalSession(IWindow * window) noexcept override
			{
				if (!window || !static_cast<SystemWindow *>(window)->_is_modal) return;
				if (window->GetParentWindow()) {
					auto parent = reinterpret_cast<HWND>(window->GetParentWindow()->GetOSHandle());
					EnableWindow(parent, TRUE);
					SetForegroundWindow(parent);
					window->Destroy();
				} else ExitMainLoop();
			}
			virtual void RegisterMainWindow(IWindow * window) noexcept override { try { _main_window_list.Append(window); } catch (...) {} }
			virtual void UnregisterMainWindow(IWindow * window) noexcept override
			{
				for (int i = 0; i < _main_window_list.Length(); i++) if (_main_window_list[i] == window) { _main_window_list.Remove(i); break; }
				if (!_main_window_list.Length()) ExitMainLoop();
			}
			virtual Point GetCursorPosition(void) noexcept override
			{
				POINT point; GetCursorPos(&point);
				return Point(point.x, point.y);
			}
			virtual void SetCursorPosition(Point position) noexcept override
			{
				POINT point; point.x = position.x; point.y = position.y;
				SetCursorPos(point.x, point.y);
			}
			virtual ICursor * LoadCursor(Codec::Frame * source) noexcept override
			{
				try {
					SafePointer<ICursor> result = new SystemCursor(_create_cursor(source), true);
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			virtual ICursor * GetSystemCursor(SystemCursorClass cursor) noexcept override
			{
				if (cursor == SystemCursorClass::Null) return _null;
				else if (cursor == SystemCursorClass::Arrow) return _arrow;
				else if (cursor == SystemCursorClass::Beam) return _beam;
				else if (cursor == SystemCursorClass::Link) return _link;
				else if (cursor == SystemCursorClass::SizeLeftRight) return _size_ew;
				else if (cursor == SystemCursorClass::SizeUpDown) return _size_ns;
				else if (cursor == SystemCursorClass::SizeLeftUpRightDown) return _size_nwse;
				else if (cursor == SystemCursorClass::SizeLeftDownRightUp) return _size_nesw;
				else if (cursor == SystemCursorClass::SizeAll) return _size_all;
				else return 0;
			}
			virtual void SetCursor(ICursor * cursor) noexcept override { if (cursor) ::SetCursor(reinterpret_cast<HCURSOR>(cursor->GetOSHandle())); }
			virtual Array<Point> * GetApplicationIconSizes(void) noexcept override
			{
				try {
					SafePointer< Array<Point> > result = new Array<Point>(2);
					result->Append(Point(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON)));
					result->Append(Point(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON)));
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			virtual void SetApplicationIcon(Codec::Image * icon) noexcept override
			{
				try {
					HWND wnd = 0;
					auto icon_sm_frame = icon->GetFrameBestSizeFit(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
					auto icon_nm_frame = icon->GetFrameBestSizeFit(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
					auto icon_sm_handle = _create_icon(icon_sm_frame);
					auto icon_nm_handle = _create_icon(icon_nm_frame);
					do {
						wnd = FindWindowExW(0, wnd, ENGINE_MAIN_WINDOW_CLASS, 0);
						if (wnd && reinterpret_cast<HINSTANCE>(GetClassLongPtrW(wnd, GCLP_HMODULE)) == _instance) break;
					} while (wnd);
					HICON icon_sm_old = icon_sm_handle;
					HICON icon_nm_old = icon_nm_handle;
					if (wnd) {
						icon_sm_old = reinterpret_cast<HICON>(GetClassLongPtrW(wnd, GCLP_HICONSM));
						icon_nm_old = reinterpret_cast<HICON>(GetClassLongPtrW(wnd, GCLP_HICON));
						SetClassLongPtrW(wnd, GCLP_HICONSM, reinterpret_cast<LONG_PTR>(icon_sm_handle));
						SetClassLongPtrW(wnd, GCLP_HICON, reinterpret_cast<LONG_PTR>(icon_nm_handle));
					} else {
						WNDCLASSEXW cls;
						cls.cbSize = sizeof(cls);
						if (GetClassInfoExW(_instance, ENGINE_MAIN_WINDOW_CLASS, &cls)) {
							if (UnregisterClassW(ENGINE_MAIN_WINDOW_CLASS, _instance)) {
								icon_sm_old = cls.hIconSm;
								icon_nm_old = cls.hIcon;
								cls.hIconSm = icon_sm_handle;
								cls.hIcon = icon_nm_handle;
								RegisterClassExW(&cls);
							}
						}
					}
					if (icon_sm_old) DestroyIcon(icon_sm_old);
					if (icon_nm_old) DestroyIcon(icon_nm_old);
				} catch (...) {}
			}
			virtual void SetApplicationBadge(const string & text) noexcept override {}
			virtual void SetApplicationIconVisibility(bool visible) noexcept override {}
			virtual bool OpenFileDialog(OpenFileInfo * info, IWindow * parent, IDispatchTask * on_exit) noexcept override
			{
				if (parent && parent->GetParentWindow() && !static_cast<SystemWindow *>(parent)->_is_modal) return false;
				auto data = new (std::nothrow) OpenFileData;
				if (!data) return false;
				data->info = info;
				data->parent = parent;
				data->task = on_exit;
				if (data->task) data->task->Retain();
				if (parent) {
					SafePointer<Thread> thread = CreateThread(_open_file_proc, data);
					if (!thread) { if (data->task) data->task->Release(); delete data; return false; }
				} else {
					GlobalModalLock lock(0);
					_open_file_proc(data);
					lock.Release();
				}
				return true;
			}
			virtual bool SaveFileDialog(SaveFileInfo * info, IWindow * parent, IDispatchTask * on_exit) noexcept override
			{
				if (parent && parent->GetParentWindow() && !static_cast<SystemWindow *>(parent)->_is_modal) return false;
				auto data = new (std::nothrow) SaveFileData;
				if (!data) return false;
				data->info = info;
				data->parent = parent;
				data->task = on_exit;
				if (data->task) data->task->Retain();
				if (parent) {
					SafePointer<Thread> thread = CreateThread(_save_file_proc, data);
					if (!thread) { if (data->task) data->task->Release(); delete data; return false; }
				} else {
					GlobalModalLock lock(0);
					_save_file_proc(data);
					lock.Release();
				}
				return true;
			}
			virtual bool ChooseDirectoryDialog(ChooseDirectoryInfo * info, IWindow * parent, IDispatchTask * on_exit) noexcept override
			{
				if (parent && parent->GetParentWindow() && !static_cast<SystemWindow *>(parent)->_is_modal) return false;
				auto data = new (std::nothrow) ChooseDirectoryData;
				if (!data) return false;
				data->info = info;
				data->parent = parent;
				data->task = on_exit;
				if (data->task) data->task->Retain();
				if (parent) {
					SafePointer<Thread> thread = CreateThread(_choose_directory_proc, data);
					if (!thread) { if (data->task) data->task->Release(); delete data; return false; }
				} else {
					GlobalModalLock lock(0);
					_choose_directory_proc(data);
					lock.Release();
				}
				return true;
			}
			virtual bool MessageBox(MessageBoxResult * result, const string & text, const string & title, IWindow * parent, MessageBoxButtonSet buttons, MessageBoxStyle style, IDispatchTask * on_exit) noexcept override
			{
				if (parent && parent->GetParentWindow() && !static_cast<SystemWindow *>(parent)->_is_modal) return false;
				auto data = new (std::nothrow) MessageBoxData;
				if (!data) return false;
				try {
					data->result = result;
					data->text = text;
					data->title = title;
					data->buttons = buttons;
					data->style = style;
					data->parent = parent;
					data->task = on_exit;
				} catch (...) { delete data; return false; }
				if (data->task) data->task->Retain();
				if (parent) {
					SafePointer<Thread> thread = CreateThread(_message_box_proc, data);
					if (!thread) { if (data->task) data->task->Release(); delete data; return false; }
				} else {
					GlobalModalLock lock(0);
					_message_box_proc(data);
					lock.Release();
				}
				return true;
			}
			virtual IMenu * CreateMenu(void) noexcept override { try { return new SystemMenu; } catch (...) { return 0; } }
			virtual IMenuItem * CreateMenuItem(void) noexcept override { try { return new SystemMenuItem; } catch (...) { return 0; } }
			virtual Point GetUserNotificationIconSize(void) noexcept override { return Point(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON)); }
			virtual void PushUserNotification(const string & title, const string & text, Codec::Image * icon) noexcept override
			{
				try {
					string text_cut = text.Fragment(0, 255);
					string title_cut = title.Fragment(0, 63);
					HWND notify_wnd = CreateWindowExW(0, ENGINE_TRAY_WINDOW_CLASS, L"", WS_OVERLAPPED, 0, 0, 0, 0, 0, 0, _instance, 0);
					if (!notify_wnd) return;
					NOTIFYICONDATAW data;
					ZeroMemory(&data, sizeof(data));
					data.cbSize = sizeof(data);
					data.hWnd = notify_wnd;
					data.uID = 1;
					data.uFlags = NIF_INFO;
					MemoryCopy(data.szInfo, static_cast<const widechar *>(text_cut), text_cut.Length() * 2);
					MemoryCopy(data.szInfoTitle, static_cast<const widechar *>(title_cut), title_cut.Length() * 2);
					HICON icon_handle = 0;
					if (icon) {
						Codec::Frame * frame = icon ? icon->GetFramePreciseSize(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON)) : 0;
						try { if (frame) icon_handle = _create_icon(frame); } catch (...) {}
					}
					if (!icon_handle) icon_handle = reinterpret_cast<HICON>(LoadImageW(GetModuleHandleW(0), MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0));
					if (icon_handle) {
						data.hBalloonIcon = icon_handle;
						data.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
					} else data.dwInfoFlags = NIIF_NONE;
					Shell_NotifyIconW(NIM_ADD, &data);
					Shell_NotifyIconW(NIM_DELETE, &data);
					DestroyWindow(notify_wnd);
					if (icon_handle) DestroyIcon(icon_handle);
				} catch (...) {}
			}
			virtual IStatusBarIcon * CreateStatusBarIcon(void) noexcept override { try { return new SystemStatusBarIcon; } catch (...) { return 0; } }
			virtual bool CreateHotKey(int event_id, int key_code, uint key_flags) noexcept override
			{
				if (key_code == KeyCodes::Shift || key_code == KeyCodes::RightShift || key_code == KeyCodes::LeftShift) return false;
				if (key_code == KeyCodes::Control || key_code == KeyCodes::RightControl || key_code == KeyCodes::LeftControl) return false;
				if (key_code == KeyCodes::Alternative || key_code == KeyCodes::RightAlternative || key_code == KeyCodes::LeftAlternative) return false;
				if (key_code == KeyCodes::System || key_code == KeyCodes::RightSystem || key_code == KeyCodes::LeftSystem) return false;
				UINT flags = MOD_NOREPEAT;
				if (key_flags & HotKeyShift) flags |= MOD_SHIFT;
				if (key_flags & HotKeyControl) flags |= MOD_CONTROL;
				if (key_flags & HotKeyAlternative) flags |= MOD_ALT;
				if (key_flags & HotKeySystem) flags |= MOD_WIN;
				if (RegisterHotKey(_host_window, event_id, flags, key_code)) return true;
				return false;
			}
			virtual void RemoveHotKey(int event_id) noexcept override { UnregisterHotKey(_host_window, event_id); }
			virtual bool LaunchIPCServer(const string & app_id, const string & auth_id) noexcept override
			{
				if (_dde_server_name.Length()) return false;
				try {
					_dde_server_name = auth_id + L"." + app_id;
					return true;
				} catch (...) { return false; }
			}
			virtual IIPCClient * CreateIPCClient(const string & server_app_id, const string & server_auth_id) noexcept override
			{
				try {
					return new SystemIPCClient(server_auth_id + L"." + server_app_id);
				} catch (...) { return 0; }
			}
			virtual void SubmitTask(IDispatchTask * task) override
			{
				if (task) {
					task->Retain();
					if (!PostMessageW(_host_window, ERTM_EXECUTE, 0, reinterpret_cast<LPARAM>(task))) {
						task->Release();
						throw InvalidStateException();
					}
				}
			}
			virtual void BeginSubmit(void) override {}
			virtual void AppendTask(IDispatchTask * task) override { SubmitTask(task); }
			virtual void EndSubmit(void) override {}
		};
		SafePointer<WindowSystem> _common_window_system;
		IWindowSystem * GetWindowSystem(void)
		{
			try {
				if (!_common_window_system) _common_window_system = new WindowSystem;
				return _common_window_system;
			} catch (...) { return 0; }
		}
	
		void _get_window_render_flags(IWindow * window, int * fx_flags, Color * clear_color, MARGINS ** margins)
		{
			auto wnd = static_cast<SystemWindow *>(window);
			if (fx_flags) *fx_flags = wnd->_effect_flags;
			if (clear_color) *clear_color = wnd->_clear_color;
			if (margins) *margins = &wnd->_margins;
		}
		void _set_window_render_callback(IWindow * window, func_RenderLayerCallback callback)
		{
			auto wnd = static_cast<SystemWindow *>(window);
			if (callback) {
				wnd->_layer_flags = 2;
				wnd->_render_callback = callback;
			} else {
				wnd->_layer_flags = 1;
				wnd->_render_callback = 0;
			}
		}
		void _get_window_layers(ICoreWindow * window, HLAYERS * layers, double * factor)
		{
			*layers = static_cast<SystemWindow *>(window)->_layered;
			*factor = static_cast<SystemWindow *>(window)->_blur_factor;
		}
		void _set_window_layers(ICoreWindow * window, HLAYERS layers)
		{
			auto wnd = static_cast<SystemWindow *>(window);
			if (wnd->_layered) Effect::ReleaseLayers(wnd->_layered);
			wnd->_layered = layers;
			if (layers) Effect::RetainLayers(wnd->_layered);
		}
	}
}