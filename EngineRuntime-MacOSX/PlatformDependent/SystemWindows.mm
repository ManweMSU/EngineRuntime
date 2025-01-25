#include "../Interfaces/SystemWindows.h"

#include "../Interfaces/Assembly.h"
#include "../Interfaces/KeyCodes.h"
#include "../Miscellaneous/DynamicString.h"
#include "../Miscellaneous/Volumes.h"

#include "CocoaInterop.h"
#include "CocoaKeyCodes.h"
#include "QuartzDevice.h"
#include "MetalDevice.h"
#include "SystemWindowsAPI.h"

typedef void * CGSConnection;
extern "C" OSStatus CGSSetWindowBackgroundBlurRadius(CGSConnection connection, NSInteger windowNumber, int radius);
extern "C" CGSConnection CGSDefaultConnectionForThread();

@interface ERTTask : NSObject
	{
	@public
		Engine::IDispatchTask * task;
	}
	- (instancetype) init;
	- (void) dealloc;
	- (void) executeDispatchTask;
@end
@interface ERTApplicationDelegate : NSObject<NSApplicationDelegate>
	- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication *) sender;
	- (void) application: (NSApplication *) application openURLs: (NSArray<NSURL *> *) urls;
	- (BOOL) applicationOpenUntitledFile: (NSApplication *) sender;
	- (void) applicationClose: (id) sender;
	- (void) applicationCreateNewFile: (id) sender;
	- (void) applicationOpenFile: (id) sender;
	- (void) applicationShowProperties: (id) sender;
	- (void) applicationShowHelp: (id) sender;
	- (void) applicationShowAbout: (id) sender;
@end
@interface ERTWindowDelegate : NSObject<NSWindowDelegate, NSMenuItemValidation>
	{
	@public
		Engine::Windows::IWindow * owner;
	}
	// Core delegate selectors
	- (BOOL) windowShouldClose: (NSWindow *) sender;
	- (void) windowDidBecomeKey: (NSNotification *) notification;
	- (void) windowDidResignKey: (NSNotification *) notification;
	- (void) windowWillBeginSheet: (NSNotification *) notification;
	- (void) windowDidEndSheet: (NSNotification *) notification;
	- (void) windowDidMiniaturize: (NSNotification *) notification;
	- (void) windowDidDeminiaturize: (NSNotification *) notification;
	- (void) windowDidEnterFullScreen: (NSNotification *) notification;
	- (void) windowDidExitFullScreen: (NSNotification *) notification;
	- (void) windowDidBecomeMain: (NSNotification *) notification;
	- (void) windowDidResignMain: (NSNotification *) notification;
	- (void) windowDidMove: (NSNotification *) notification;
	- (void) windowDidResize: (NSNotification *) notification;
	// Menu maintanance selectors
	- (BOOL) validateMenuItem: (NSMenuItem *) menuItem;
	- (void) engineWindowSave: (id) sender;
	- (void) engineWindowSaveAs: (id) sender;
	- (void) engineWindowExport: (id) sender;
	- (void) engineWindowPrint: (id) sender;
	- (void) engineWindowUndo: (id) sender;
	- (void) engineWindowRedo: (id) sender;
	- (void) engineWindowCut: (id) sender;
	- (void) engineWindowCopy: (id) sender;
	- (void) engineWindowPaste: (id) sender;
	- (void) engineWindowDuplicate: (id) sender;
	- (void) engineWindowDelete: (id) sender;
	- (void) engineWindowFind: (id) sender;
	- (void) engineWindowReplace: (id) sender;
	- (void) engineWindowSelectAll: (id) sender;
@end
@interface ERTPopupWindow : NSPanel
	@property(readonly) BOOL canBecomeKeyWindow;
	- (void) resignKeyWindow;
@end
@interface ERTView : NSView<NSTextInputClient>
	{
	@public
		Engine::Windows::IWindow * owner;
		uint time_left_down;
		uint time_right_down;
		uint time_double_click;
		Engine::Point last_position;
		Engine::Point last_click;
		uint flag_mouse;
		uint flag_states;
		NSTextInputContext * input_context;
	}
	- (instancetype) init;
	- (void) dealloc;
	// NSResponder and NSView selectors
	- (void) drawRect: (NSRect) dirtyRect;
	- (BOOL) acceptsFirstMouse: (NSEvent *) event;
	- (NSView *) hitTest: (NSPoint) point;
	- (void) setFrame: (NSRect) frame;
	- (void) setFrameSize: (NSSize) newSize;
	- (void) mouseMoved: (NSEvent *) event;
	- (void) mouseDragged: (NSEvent *) event;
	- (void) mouseDown: (NSEvent *) event;
	- (void) mouseUp: (NSEvent *) event;
	- (void) rightMouseDragged: (NSEvent *) event;
	- (void) rightMouseDown: (NSEvent *) event;
	- (void) rightMouseUp: (NSEvent *) event;
	- (void) keyDown: (NSEvent *) event;
	- (void) keyUp: (NSEvent *) event;
	- (void) flagsChanged: (NSEvent *) event;
	- (void) scrollWheel: (NSEvent *) event;
	- (void) viewDidChangeEffectiveAppearance;
	// Advanced text input selectors
	- (BOOL) hasMarkedText;
	- (NSRange) markedRange;
	- (NSRange) selectedRange;
	- (void) setMarkedText: (id) string selectedRange: (NSRange) selectedRange replacementRange: (NSRange) replacementRange;
	- (void) unmarkText;
	- (NSArray<NSAttributedStringKey> *) validAttributesForMarkedText;
	- (NSAttributedString *) attributedSubstringForProposedRange: (NSRange) range actualRange: (NSRangePointer) actualRange;
	- (void) insertText: (id) string replacementRange: (NSRange) replacementRange;
	- (NSUInteger) characterIndexForPoint: (NSPoint) point;
	- (NSRect) firstRectForCharacterRange: (NSRange) range actualRange: (NSRangePointer) actualRange;
	- (void) doCommandBySelector: (SEL) selector;
	// Custom view selectors
	- (void) engineTimerEvent: (NSTimer *) timer;
	// Properties
	@property(readonly) BOOL acceptsFirstResponder;
	@property(readonly, strong) NSTextInputContext * inputContext;
@end
@interface ERTTimer : NSObject
	{
	@public
		uint timer_id;
		NSTimer * timer;
	}
@end
@interface ERTOpenSaveDelegate : NSObject<NSOpenSavePanelDelegate>
	{
	@public
		NSSavePanel * panel;
		Engine::Array<Engine::Windows::FileFormat> * formats;
		int * selected;
		bool save;
	}
	- (void) panelFormatChanged: (id) sender;
@end
@interface ERTMenuDelegate : NSObject
	{
	@public
		Engine::Windows::IMenu * owner;
		int collected_responce;
		Engine::Windows::IStatusBarIcon * icon;
	}
	- (void) menuItemSelected: (id) sender;
	- (void) menuWillOpen: (NSMenu *) menu;
	- (void) menuDidClose: (NSMenu *) menu;
	- (void) menu: (NSMenu *) menu willHighlightItem: (NSMenuItem *) item;
	- (BOOL) worksWhenModal;
@end
@interface ERTMenuItemView : NSView
	{
	@public
		CGSize ref_size;
		Engine::Cocoa::QuartzDeviceContext * device;
		Engine::Windows::IMenuItem * item;
	}
	- (instancetype) init;
	- (void) dealloc;
	- (void) drawRect : (NSRect) dirtyRect;
	- (void) mouseUp: (NSEvent *) event;
	- (void) viewDidMoveToWindow;
@end
@interface ERTStatusBarDelegate : NSObject
	{
	@public
		Engine::Windows::IStatusBarIcon * icon;
	}
	- (void) statusIconSelected: (id) sender;
@end

namespace Engine
{
	namespace Cocoa
	{
		Volumes::Dictionary<KeyCodes::Key, bool> KeyboardStatus;
	}
	namespace Windows
	{
		// Screens and Themes
		class SystemScreen : public IScreen
		{
			friend class SystemWindow;
			CGDirectDisplayID _display;
			NSScreen * _screen;
		public:
			SystemScreen(NSScreen * screen) : _screen(screen)
			{
				[_screen retain];
				_display = [[[_screen deviceDescription] valueForKey: @"NSScreenNumber"] unsignedIntegerValue];
			}
			virtual ~SystemScreen(void) override { [_screen release]; }
			virtual string GetName(void) override { return Cocoa::EngineString([_screen localizedName]); }
			virtual Box GetScreenRectangle(void) noexcept override
			{
				auto frame = CGDisplayBounds(_display);
				auto scale = [_screen backingScaleFactor];
				return Box(frame.origin.x * scale, frame.origin.y * scale, (frame.size.width + frame.origin.x) * scale, (frame.size.height + frame.origin.y) * scale);
			}
			virtual Box GetUserRectangle(void) noexcept override
			{
				auto bounds = CGDisplayBounds(_display);
				auto frame = [_screen frame];
				auto visible = [_screen visibleFrame];
				auto scale = [_screen backingScaleFactor];
				auto margin_left = visible.origin.x - frame.origin.x;
				auto margin_bottom = visible.origin.y - frame.origin.y;
				auto margin_right = frame.origin.x + frame.size.width - visible.origin.x - visible.size.width;
				auto margin_top = frame.origin.y + frame.size.height - visible.origin.y - visible.size.height;
				return Box((bounds.origin.x + margin_left) * scale, (bounds.origin.y + margin_top) * scale,
					(bounds.size.width + bounds.origin.x - margin_right) * scale,
					(bounds.size.height + bounds.origin.y - margin_bottom) * scale);
			}
			virtual Point GetResolution(void) noexcept override
			{
				auto frame = CGDisplayBounds(_display);
				auto scale = [_screen backingScaleFactor];
				return Point(frame.size.width * scale, frame.size.height * scale);
			}
			virtual double GetDpiScale(void) noexcept override { return [_screen backingScaleFactor]; }
			virtual Codec::Frame * Capture(void) noexcept override
			{
				auto image = CGDisplayCreateImage(_display);
				if (!image) return 0;
				int width = CGImageGetWidth(image);
				int height = CGImageGetHeight(image);
				try {
					SafePointer<Codec::Frame> state = new Codec::Frame(width, height, -1, Codec::PixelFormat::R8G8B8A8, Codec::AlphaMode::Premultiplied, Codec::ScanOrigin::TopDown);
					CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
					CGContextRef context = CGBitmapContextCreate(state->GetData(), width, height, 8, state->GetScanLineLength(), rgb, kCGImageAlphaPremultipliedLast);
					CGRect rect = CGRectMake(0.0f, 0.0f, float(width), float(height));
					CGContextSetBlendMode(context, kCGBlendModeCopy);
					CGContextDrawImage(context, rect, image);
					CGImageRelease(image);
					CGContextRelease(context);
					CGColorSpaceRelease(rgb);
					state->Retain();
					return state;
				} catch (...) {
					CGImageRelease(image);
					return 0;
				}
			}
			CGDirectDisplayID GetDisplayID(void) const noexcept { return _display; }
		};
		class SystemTheme : public ITheme
		{
			NSAppearance * _appearance;
			Volumes::Dictionary<ThemeColor, Color> _colors;
			
			void _append(ThemeColor color_id, NSColor * color) noexcept
			{
				try {
					auto rgb = [color colorUsingColorSpace: [NSColorSpace deviceRGBColorSpace]];
					double r, g, b, a;
					[rgb getRed: &r green: &g blue: &b alpha: &a];
					_colors.Append(color_id, Color(uint8(r * 255.0), uint8(g * 255.0), uint8(b * 255.0), uint8(a * 255.0)));
				} catch (...) {}
			}
		public:
			SystemTheme(NSAppearance * appearance) : _appearance(appearance)
			{
				[_appearance retain];
				@autoreleasepool {
					[_appearance performAsCurrentDrawingAppearance: ^(void) {
						_append(ThemeColor::Accent, [NSColor controlAccentColor]);
						_append(ThemeColor::WindowBackgroup, [NSColor windowBackgroundColor]);
						_append(ThemeColor::WindowText, [NSColor textColor]);
						_append(ThemeColor::SelectedBackground, [NSColor selectedTextBackgroundColor]);
						_append(ThemeColor::SelectedText, [NSColor selectedTextColor]);
						_append(ThemeColor::MenuBackground, [NSColor clearColor]);
						_append(ThemeColor::MenuText, [NSColor textColor]);
						_append(ThemeColor::MenuHotBackground, [NSColor selectedContentBackgroundColor]);
						_append(ThemeColor::MenuHotText, [NSColor selectedMenuItemTextColor]);
						_append(ThemeColor::GrayedText, [NSColor disabledControlTextColor]);
						_append(ThemeColor::Hyperlink, [NSColor linkColor]);
					}];
				}
			}
			virtual ~SystemTheme(void) override { [_appearance release]; }
			virtual ThemeClass GetClass(void) noexcept override
			{
				NSAppearanceName name = [_appearance name];
				if ([name compare: NSAppearanceNameDarkAqua] == NSOrderedSame) return ThemeClass::Dark;
				else if ([name compare: NSAppearanceNameVibrantDark] == NSOrderedSame) return ThemeClass::Dark;
				else if ([name compare: NSAppearanceNameAccessibilityHighContrastDarkAqua] == NSOrderedSame) return ThemeClass::Dark;
				else if ([name compare: NSAppearanceNameAccessibilityHighContrastVibrantDark] == NSOrderedSame) return ThemeClass::Dark;
				else return ThemeClass::Light;
			}
			virtual Color GetColor(ThemeColor color) noexcept override
			{
				auto color_ptr = _colors[color];
				if (color_ptr) return *color_ptr; else return Color(0, 0, 0, 0);
			}
		};

		// Window System
		class SystemCursor : public ICursor
		{
			NSCursor * _cursor;
		public:
			SystemCursor(NSCursor * cursor) : _cursor(cursor) { if (!_cursor) throw Exception(); [_cursor retain]; }
			virtual ~SystemCursor(void) override { [_cursor release]; }
			virtual handle GetOSHandle(void) noexcept override { return _cursor; }
		};
		class SystemBackbufferedEngine : public IPresentationEngine
		{
			SafePointer<Codec::Frame> _buffer;
			ImageRenderMode _mode;
			Color _background;
			CGImageRef _image;
			NSColor * _default_background;
			NSView * _view;
			NSWindow * _window;

			void _invalidate(void) { [_view setNeedsDisplay: YES]; }
			static void _render_callback(IPresentationEngine * _self, IWindow * window)
			{
				auto self = static_cast<SystemBackbufferedEngine *>(_self);
				auto size = [self->_view frame].size;
				auto scale = [self->_window backingScaleFactor];
				CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
				if (!self->_image) try { self->_image = Cocoa::CocoaCoreImage(self->_buffer); } catch (...) {}
				if (self->_image && size.width > 0.0 && size.height > 0.0) {
					if (self->_mode == ImageRenderMode::Stretch) {
						CGContextDrawImage(context, NSMakeRect(0.0, 0.0, size.width, size.height), self->_image);
					} else if (self->_mode == ImageRenderMode::Blit) {
						double sw = self->_buffer->GetWidth() / scale;
						double sh = self->_buffer->GetHeight() / scale;
						CGContextDrawImage(context, NSMakeRect((size.width - sw) / 2.0, (size.height - sh) / 2.0, sw, sh), self->_image);
					} else if (self->_mode == ImageRenderMode::FitKeepAspectRatio) {
						double w = self->_buffer->GetWidth();
						double h = self->_buffer->GetHeight();
						double xc = size.width / 2.0;
						double yc = size.height / 2.0;
						auto image_aspect = w / h;
						auto rect_aspect = size.width / size.height;
						if (image_aspect < rect_aspect) {
							auto aw = size.height * image_aspect;
							auto hawl = aw / 2;
							CGContextDrawImage(context, NSMakeRect(xc - hawl, 0.0, aw, size.height), self->_image);
						} else {
							auto ah = size.width / image_aspect;
							auto hahl = ah / 2;
							CGContextDrawImage(context, NSMakeRect(0.0, yc - hahl, size.width, ah), self->_image);
						}
					} else if (self->_mode == ImageRenderMode::CoverKeepAspectRatio) {
						double w = self->_buffer->GetWidth();
						double h = self->_buffer->GetHeight();
						double xc = size.width / 2.0;
						double yc = size.height / 2.0;
						auto image_aspect = w / h;
						auto rect_aspect = size.width / size.height;
						if (image_aspect > rect_aspect) {
							auto aw = size.height * image_aspect;
							auto hawl = aw / 2;
							CGContextDrawImage(context, NSMakeRect(xc - hawl, 0.0, aw, size.height), self->_image);
						} else {
							auto ah = size.width / image_aspect;
							auto hahl = ah / 2;
							CGContextDrawImage(context, NSMakeRect(0.0, yc - hahl, size.width, ah), self->_image);
						}
					}
				}
			}
		public:
			SystemBackbufferedEngine(Codec::Frame * buffer, ImageRenderMode mode, Color background) : _mode(mode), _background(background), _default_background(0), _image(0), _view(0), _window(0) { _buffer.SetRetain(buffer); }
			virtual ~SystemBackbufferedEngine(void) override { [_default_background release]; if (_image) CGImageRelease(_image); }
			virtual void Attach(ICoreWindow * window) override
			{
				_window = Cocoa::GetWindowObject(window);
				_view = Cocoa::GetWindowCoreView(window);
				Cocoa::SetWindowRenderCallback(window, _render_callback);
				_default_background = [_window backgroundColor];
				[_default_background retain];
				double alpha;
				if (static_cast<IWindow *>(window)->GetBackgroundFlags() & WindowFlagTransparent) alpha = max(_background.a / 255.0, 0.001); else alpha = 1.0;
				@autoreleasepool { [_window setBackgroundColor: [NSColor colorWithDeviceRed: _background.r / 255.0 green: _background.g / 255.0 blue: _background.b / 255.0 alpha: alpha]]; }
			}
			virtual void Detach(void) override
			{
				[_window setBackgroundColor: _default_background];
				[_default_background release]; _default_background = 0;
				if (_image) CGImageRelease(_image); _image = 0; _view = 0; _window = 0;
			}
			virtual void Invalidate(void) override { if (_image) CGImageRelease(_image); _image = 0; _invalidate(); }
			virtual void Resize(int width, int height) override { _invalidate(); }
		};
		class QuartzPresentationEngine : public I2DPresentationEngine
		{
			NSView * _view;
			NSWindow * _window;
			SafePointer<Cocoa::QuartzDeviceContext> _device;
		public:
			QuartzPresentationEngine(void) : _view(0), _window(0) {}
			virtual ~QuartzPresentationEngine(void) override {}
			virtual void Attach(ICoreWindow * window) override
			{
				try {
					Cocoa::SetWindowRenderCallback(window);
					_window = Cocoa::GetWindowObject(window);
					_view = Cocoa::GetWindowCoreView(window);
					_device = new Cocoa::QuartzDeviceContext;
				} catch (...) { _window = 0; _view = 0; }
			}
			virtual void Detach(void) override { _window = 0; _view = 0; _device.SetReference(0); }
			virtual void Invalidate(void) override { [_view setNeedsDisplay: YES]; }
			virtual void Resize(int width, int height) override {}
			virtual Graphics::I2DDeviceContext * GetContext(void) noexcept override { return _device; }
			virtual bool BeginRenderingPass(void) noexcept override
			{
				if (_device) {
					auto size = [_view frame].size;
					auto scale = [_window backingScaleFactor];
					auto context = [[NSGraphicsContext currentContext] CGContext];
					_device->SetContext(context, int(size.width * scale), int(size.height * scale), (scale > 1.5) ? 2 : 1);
					return true;
				} else return false;
			}
			virtual bool EndRenderingPass(void) noexcept override { if (_device) return true; else return false; }
		};
		class SystemWindow : public IWindow
		{
		public:
			NSWindow * _window;
			ERTWindowDelegate * _delegate;
			IWindowCallback * _callback;
			ERTView * _view;
			NSVisualEffectView * _effect_view;
			Cocoa::RenderLayerCallback _layer_callback;
			SafePointer<IPresentationEngine> _engine;
			Array<SystemWindow *> _children;
			SystemWindow * _parent;
			bool _modal, _visible, _captured, _on_screen, _user_layer_callback, _fullscreen, _alpha_backbuffer;
			double _last_left, _last_top, _last_width, _last_height;
			Point _min_size, _max_size;
			NSSize _clear_max_size;
			uint32 _effect_flags;
			Array<ERTTimer *> _timers;
			SafePointer<Object> _touch_bar;

			static NSWindowStyleMask _style_mask_for_flags(uint flags)
			{
				if (flags & WindowFlagPopup) {
					return NSWindowStyleMaskUtilityWindow;
				} else {
					NSWindowStyleMask result = 0;
					if (flags & WindowFlagHasTitle) {
						result |= NSWindowStyleMaskTitled;
						if (flags & WindowFlagCocoaTransparentTitle) {
							if ((flags & WindowFlagCocoaContentUnderTitle) || (flags & WindowFlagCocoaEffectBackground)) result |= NSWindowStyleMaskFullSizeContentView;
						}
					}
					if (flags & WindowFlagSizeble) result |= NSWindowStyleMaskResizable;
					if (flags & WindowFlagCloseButton) result |= NSWindowStyleMaskClosable;
					if (flags & WindowFlagMinimizeButton) result |= NSWindowStyleMaskMiniaturizable;
					if (flags & WindowFlagToolWindow) result |= NSWindowStyleMaskUtilityWindow;
					return result;
				}
			}
			void _present(bool present, bool activate)
			{
				if (present) {
					_on_screen = true;
					if (_callback) _callback->Shown(this, true);
					[_window orderFront:nil];
					if (activate) [_window makeKeyWindow];
					if (_parent) [_parent->_window addChildWindow: _window ordered: NSWindowAbove];
					for (auto & child : _children) if (child->_visible) child->_present(true, false);
				} else {
					for (auto & child : _children) child->_present(false, false);
					if (_parent) [_parent->_window removeChildWindow: _window];
					[_window orderOut: nil];
					_on_screen = false;
					if (_callback) _callback->Shown(this, false);
				}
			}
			void _show_children(bool show) { if (_on_screen) for (auto & child : _children) if (child->_visible) child->_present(show, false); }
			static CGPoint _screen_engine_to_cocoa(Point point)
			{
				@autoreleasepool {
					auto main_screen = CGMainDisplayID();
					auto scale = [[NSScreen mainScreen] backingScaleFactor];
					auto size = CGDisplayBounds(main_screen).size;
					return CGPointMake(point.x / scale, size.height - point.y / scale);
				}
			}
			static Point _screen_cocoa_to_engine(CGPoint point)
			{
				@autoreleasepool {
					auto main_screen = CGMainDisplayID();
					auto scale = [[NSScreen mainScreen] backingScaleFactor];
					auto size = CGDisplayBounds(main_screen).size;
					return Point(point.x * scale, (size.height - point.y) * scale);
				}
			}
			CGPoint _view_engine_to_cocoa(Point point)
			{
				auto scale = GetDpiScale();
				auto size = [_view frame].size;
				return CGPointMake(point.x / scale, size.height - point.y / scale);
			}
			Point _view_cocoa_to_engine(CGPoint point)
			{
				auto scale = GetDpiScale();
				auto size = [_view frame].size;
				return Point(point.x * scale, (size.height - point.y) * scale);
			}
			CGPoint _view_to_window(CGPoint point) { return [_view convertPoint: point toView: nil]; }
			CGPoint _window_to_view(CGPoint point) { return [_view convertPoint: point fromView: nil]; }
			CGPoint _window_to_screen(CGPoint point) { return [_window convertPointToScreen: point]; }
			CGPoint _screen_to_window(CGPoint point) { return [_window convertPointFromScreen: point]; }
		public:
			SystemWindow(const CreateWindowDesc & desc, bool modal = false) : _layer_callback(0), _user_layer_callback(false), _modal(modal), _children(0x10), _timers(0x20), _visible(false), _on_screen(false), _captured(false), _fullscreen(false)
			{
				_alpha_backbuffer = false;
				_effect_flags = 0;
				_parent = desc.ParentWindow ? static_cast<SystemWindow *>(desc.ParentWindow) : 0;
				if (_parent) _parent->_children.Append(this);
				try {
					_delegate = [[ERTWindowDelegate alloc] init];
					_delegate->owner = this;
					_callback = desc.Callback;
					auto style = _style_mask_for_flags(desc.Flags);
					if (desc.Flags & WindowFlagPopup) {
						_window = [[ERTPopupWindow alloc] initWithContentRect: NSMakeRect(0, 0, 0, 0) styleMask: style backing: NSBackingStoreBuffered defer: NO screen: nil];
						[_window setFloatingPanel: YES];
						[_window setHidesOnDeactivate: NO];
						[_window setAnimationBehavior: NSWindowAnimationBehaviorNone];
					} else if (desc.Flags & WindowFlagToolWindow) {
						_window = [[NSPanel alloc] initWithContentRect: NSMakeRect(0, 0, 0, 0) styleMask: style backing: NSBackingStoreBuffered defer: NO screen: nil];
						[_window setAnimationBehavior: NSWindowAnimationBehaviorUtilityWindow];
					} else {
						_window = [[NSWindow alloc] initWithContentRect: NSMakeRect(0, 0, 0, 0) styleMask: style backing: NSBackingStoreBuffered defer: NO screen: nil];
						[_window setAnimationBehavior: NSWindowAnimationBehaviorDocumentWindow];
					}
					if (!_window) { [_delegate release]; throw Exception(); }
					[_window setReleasedWhenClosed: NO];
					if (desc.Flags & WindowFlagHasTitle) {
						auto title = Cocoa::CocoaString(desc.Title);
						[_window setTitle: title];
						[title release];
						if (desc.Flags & WindowFlagCocoaTransparentTitle) [_window setTitlebarAppearsTransparent: YES];
					}
					if (desc.Flags & WindowFlagOverrideTheme) {
						@autoreleasepool {
							if (desc.Theme == ThemeClass::Light) [_window setAppearance: [NSAppearance appearanceNamed: NSAppearanceNameAqua]];
							else if (desc.Theme == ThemeClass::Dark) [_window setAppearance: [NSAppearance appearanceNamed: NSAppearanceNameDarkAqua]];
						}
					}
					[_window setAcceptsMouseMovedEvents: YES];
					[_window setTabbingMode: NSWindowTabbingModeDisallowed];
					if (desc.Flags & WindowFlagCocoaShadowless) [_window setHasShadow: NO];
					if (desc.Flags & WindowFlagNonOpaque) [_window setAlphaValue: desc.Opacity];
					if (desc.Flags & WindowFlagTransparent) { _effect_flags |= WindowFlagTransparent; _alpha_backbuffer = true; }
					if (desc.Flags & WindowFlagBlurBehind) { _effect_flags |= WindowFlagTransparent | WindowFlagBlurBehind | WindowFlagBlurFactor; _alpha_backbuffer = true; }
					@autoreleasepool {
						if (_effect_flags & WindowFlagTransparent) {
							[_window setOpaque: NO];
							[_window setBackgroundColor: [NSColor colorWithDeviceRed: 0.0 green: 0.0 blue: 0.0 alpha: 0.001]];
						}
						if (_effect_flags & WindowFlagBlurBehind) {
							double factor;
							if (_effect_flags & WindowFlagBlurFactor) factor = desc.BlurFactor; else factor = 25.0;
							auto con = CGSDefaultConnectionForThread();
							CGSSetWindowBackgroundBlurRadius(con, [_window windowNumber], int(factor));
						}
						if (desc.Flags & WindowFlagCocoaCustomBackground) {
							_alpha_backbuffer = true;
							if (_effect_flags & WindowFlagTransparent) {
								[_window setBackgroundColor: [NSColor colorWithDeviceRed: desc.BackgroundColor.r / 255.0 green: desc.BackgroundColor.g / 255.0 blue: desc.BackgroundColor.b / 255.0 alpha: max(desc.BackgroundColor.a / 255.0, 0.001)]];
							} else {
								[_window setBackgroundColor: [NSColor colorWithDeviceRed: desc.BackgroundColor.r / 255.0 green: desc.BackgroundColor.g / 255.0 blue: desc.BackgroundColor.b / 255.0 alpha: 1.0]];
							}
						}
					}
					SafePointer<IScreen> screen;
					if (desc.Screen) screen.SetRetain(desc.Screen); else screen = GetDefaultScreen();
					auto main_screen = CGMainDisplayID();
					auto desktop = CGDisplayBounds(main_screen).size;
					auto scale = screen->GetDpiScale();
					auto origin = desc.Screen ? desc.Screen->GetScreenRectangle() : Box(0, 0, 0, 0);
					auto rect = NSMakeRect((origin.Left + desc.Position.Left) / scale, desktop.height - (desc.Position.Bottom - origin.Top) / scale, (desc.Position.Right - desc.Position.Left) / scale, (desc.Position.Bottom - desc.Position.Top) / scale);
					[_window setFrame: rect display: YES];
					rect = [_window frame];
					_last_left = rect.origin.x;
					_last_top = rect.origin.y + rect.size.height;
					_last_width = rect.size.width;
					_last_height = rect.size.height;
					_clear_max_size = [_window contentMaxSize];
					SetMinimalConstraints(desc.MinimalConstraints);
					SetMaximalConstraints(desc.MaximalConstraints);
					if (!(desc.Flags & WindowFlagMaximizeButton)) [_window setCollectionBehavior: [_window collectionBehavior] & ~NSWindowCollectionBehaviorFullScreenPrimary | NSWindowCollectionBehaviorFullScreenNone];
					if (desc.Flags & WindowFlagCocoaEffectBackground) _effect_view = [[NSVisualEffectView alloc] init]; else _effect_view = 0;
					_view = [[ERTView alloc] init];
					_view->owner = this;
					if (!(desc.Flags & WindowFlagPopup) && (desc.Flags & WindowFlagHasTitle) && (desc.Flags & WindowFlagCocoaTransparentTitle) && (desc.Flags & WindowFlagCocoaEffectBackground) && !(desc.Flags & WindowFlagCocoaContentUnderTitle)) {
						_alpha_backbuffer = true;
						auto client = [_window contentRectForFrameRect: [_window frame]];
						auto layout = [_window contentLayoutRect];
						[_effect_view setFrameSize: client.size];
						[_view setFrame: layout];
						[_effect_view addSubview: _view];
						[_view setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
						[_window setContentView: _effect_view];
					} else if (_effect_view) {
						_alpha_backbuffer = true;
						[_effect_view addSubview: _view];
						[_view setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
						[_window setContentView: _effect_view];
					} else [_window setContentView: _view];
					[_window setInitialFirstResponder: _view];
					[_view release];
					[_effect_view release];
					[_window setDelegate: _delegate];
				} catch (...) {
					if (_parent) _parent->_children.RemoveLast();
					throw;
				}
			}
			virtual ~SystemWindow(void) override { if (_window) abort(); }
			virtual void Show(bool show) override { if (!_parent || _parent->_on_screen) _present(show, true); _visible = show; }
			virtual bool IsVisible(void) override { return _on_screen; }
			virtual void SetText(const string & text) override { auto title = Cocoa::CocoaString(text); [_window setTitle: title]; [title release]; }
			virtual string GetText(void) override { return Cocoa::EngineString([_window title]); }
			virtual void SetPosition(const Box & box) override
			{
				auto main = CGMainDisplayID();
				auto desktop = CGDisplayBounds(main).size;
				auto scale = GetDpiScale();
				auto rect = NSMakeRect(box.Left / scale, desktop.height - box.Bottom / scale, (box.Right - box.Left) / scale, (box.Bottom - box.Top) / scale);
				[_window setFrame: rect display: YES];
			}
			virtual Box GetPosition(void) override
			{
				auto main = CGMainDisplayID();
				auto desktop = CGDisplayBounds(main).size;
				auto rect = [_window frame];
				auto scale = GetDpiScale();
				return Box(rect.origin.x * scale, (desktop.height - rect.origin.y - rect.size.height) * scale, (rect.origin.x + rect.size.width) * scale, (desktop.height - rect.origin.y) * scale);
			}
			virtual Point GetClientSize(void) override
			{
				auto scale = GetDpiScale();
				auto size = [_view frame].size;
				return Point(size.width * scale, size.height * scale);
			}
			virtual void SetMinimalConstraints(Point size) override
			{
				_min_size = size;
				auto scale = GetDpiScale();
				auto rect = [_window contentRectForFrameRect: NSMakeRect(0, 0, size.x / scale, size.y / scale)];
				[_window setContentMinSize: rect.size];
			}
			virtual Point GetMinimalConstraints(void) override { return _min_size; }
			virtual void SetMaximalConstraints(Point size) override
			{
				_max_size = size;
				if (size.x && size.y) {
					auto scale = GetDpiScale();
					auto rect = [_window contentRectForFrameRect: NSMakeRect(0, 0, size.x / scale, size.y / scale)];
					[_window setContentMaxSize: rect.size];
				} else [_window setContentMaxSize: _clear_max_size];
			}
			virtual Point GetMaximalConstraints(void) override { return _max_size; }
			virtual void Activate(void) override { if (_visible) { [_window orderFront: nil]; [_window makeMainWindow]; [_window makeKeyWindow]; } }
			virtual bool IsActive(void) override { return [_window isMainWindow] != 0; }
			virtual void Maximize(void) override { if (!IsMaximized()) [_window toggleFullScreen: nil]; }
			virtual bool IsMaximized(void) override { return ([_window styleMask] & NSFullScreenWindowMask) == NSFullScreenWindowMask; }
			virtual void Minimize(void) override { [_window miniaturize: nil]; }
			virtual bool IsMinimized(void) override { return [_window isMiniaturized] != 0; }
			virtual void Restore(void) override { if (IsMaximized()) [_window toggleFullScreen: nil]; if (IsMinimized()) [_window deminiaturize: nil]; }
			virtual void RequireAttention(void) override { [NSApp requestUserAttention: NSInformationalRequest]; }
			virtual void SetOpacity(double opacity) override { [_window setAlphaValue: opacity]; }
			virtual void SetCloseButtonState(CloseButtonState state) override
			{
				auto button = [_window standardWindowButton: NSWindowCloseButton];
				if (button) {
					if (state == CloseButtonState::Enabled) { [button setEnabled: YES]; [_window setDocumentEdited: NO]; }
					else if (state == CloseButtonState::Alert) { [button setEnabled: YES]; [_window setDocumentEdited: YES]; }
					else if (state == CloseButtonState::Disabled) { [button setEnabled: NO]; [_window setDocumentEdited: NO]; }
				}
			}
			virtual IWindow * GetParentWindow(void) override { return _parent; }
			virtual IWindow * GetChildWindow(int index) override { return _children[index]; }
			virtual int GetChildrenCount(void) override { return _children.Length(); }
			virtual void SetProgressMode(ProgressDisplayMode mode) override {}
			virtual void SetProgressValue(double value) override {}
			virtual void SetCocoaEffectMaterial(CocoaEffectMaterial material) override
			{
				if (material == CocoaEffectMaterial::Titlebar) [_effect_view setMaterial: (NSVisualEffectMaterial) 3];
				else if (material == CocoaEffectMaterial::Selection) [_effect_view setMaterial: (NSVisualEffectMaterial) 4];
				else if (material == CocoaEffectMaterial::Menu) [_effect_view setMaterial: (NSVisualEffectMaterial) 5];
				else if (material == CocoaEffectMaterial::Popover) [_effect_view setMaterial: (NSVisualEffectMaterial) 6];
				else if (material == CocoaEffectMaterial::Sidebar) [_effect_view setMaterial: (NSVisualEffectMaterial) 7];
				else if (material == CocoaEffectMaterial::HeaderView) [_effect_view setMaterial: (NSVisualEffectMaterial) 10];
				else if (material == CocoaEffectMaterial::Sheet) [_effect_view setMaterial: (NSVisualEffectMaterial) 11];
				else if (material == CocoaEffectMaterial::WindowBackground) [_effect_view setMaterial: (NSVisualEffectMaterial) 12];
				else if (material == CocoaEffectMaterial::HUD) [_effect_view setMaterial: (NSVisualEffectMaterial) 13];
				else if (material == CocoaEffectMaterial::FullScreenUI) [_effect_view setMaterial: (NSVisualEffectMaterial) 15];
				else if (material == CocoaEffectMaterial::ToolTip) [_effect_view setMaterial: (NSVisualEffectMaterial) 17];
			}
			virtual void SetCallback(IWindowCallback * callback) override { _callback = callback; }
			virtual IWindowCallback * GetCallback(void) override { return _callback; }
			virtual bool PointHitTest(Point at) override
			{
				auto screen_local = _screen_engine_to_cocoa(at);
				auto wnd_local = _screen_to_window(screen_local);
				auto size = [_window frame].size;
				return [_window isKeyWindow] && wnd_local.x >= 0.0 && wnd_local.y >= 0.0 && wnd_local.x < size.width && wnd_local.y < size.height && [NSWindow windowNumberAtPoint: screen_local belowWindowWithWindowNumber: 0] == [_window windowNumber];
			}
			virtual Point PointClientToGlobal(Point at) override { return _screen_cocoa_to_engine(_window_to_screen(_view_to_window(_view_engine_to_cocoa(at)))); }
			virtual Point PointGlobalToClient(Point at) override { return _view_cocoa_to_engine(_window_to_view(_screen_to_window(_screen_engine_to_cocoa(at)))); }
			virtual void SetFocus(void) override { [_window makeKeyWindow]; }
			virtual bool IsFocused(void) override { return [_window isKeyWindow]; }
			virtual void SetCapture(void) override { if ([_window isKeyWindow] && !_captured) { _captured = true; if (_callback) _callback->CaptureChanged(this, true); } }
			virtual void ReleaseCapture(void) override { if (_captured) { _captured = false; if (_callback) _callback->CaptureChanged(this, false); } }
			virtual bool IsCaptured(void) override { return [_window isKeyWindow] && _captured; }
			virtual void SetTimer(uint32 timer_id, uint32 period) override
			{
				if (period) {
					@autoreleasepool {
						ERTTimer * timer = 0;
						for (auto & t : _timers) if (t->timer_id == timer_id) { timer = t; break; }
						if (!timer) {
							timer = [[ERTTimer alloc] init];
							timer->timer_id = timer_id;
							_timers << timer;
						} else if (timer->timer) [timer->timer invalidate];
						timer->timer = [NSTimer timerWithTimeInterval: double(period) / 1000.0 target: _view selector: @selector(engineTimerEvent:) userInfo: timer repeats: YES];
						[[NSRunLoop currentRunLoop] addTimer: timer->timer forMode: NSRunLoopCommonModes];
					}
				} else for (int i = _timers.Length() - 1; i >= 0; i--) {
					if (_timers[i]->timer_id == timer_id) {
						[_timers[i]->timer invalidate];
						[_timers[i] release];
						_timers.Remove(i);
					}
				}
			}
			virtual void SetBackbufferedRenderingDevice(Codec::Frame * image, ImageRenderMode mode, Color filling) noexcept override
			{
				try {
					SafePointer<IPresentationEngine> engine = new SystemBackbufferedEngine(image, mode, filling);
					SetPresentationEngine(engine);
				} catch (...) {}
			}
			virtual I2DPresentationEngine * Set2DRenderingDevice(DeviceClass device_class) noexcept override
			{
				try {
					SafePointer<I2DPresentationEngine> engine;
					if (device_class == DeviceClass::DontCare) {
						engine = Cocoa::CreateMetalPresentationEngine();
						if (!engine) engine = new QuartzPresentationEngine;
					} else if (device_class == DeviceClass::Hardware) engine = Cocoa::CreateMetalPresentationEngine();
					else if (device_class == DeviceClass::Basic) engine = new QuartzPresentationEngine;
					else return 0;
					if (!engine) return 0;
					SetPresentationEngine(engine);
					return engine;
				} catch (...) { return 0; }
			}
			virtual double GetDpiScale(void) override { return [_window backingScaleFactor]; }
			virtual IScreen * GetCurrentScreen(void) override { try { return new SystemScreen([_window screen]); } catch (...) { return 0; } }
			virtual ITheme * GetCurrentTheme(void) override { try { return new SystemTheme([_window effectiveAppearance]); } catch (...) { return 0; } }
			virtual uint GetBackgroundFlags(void) override { return _effect_flags; }
			virtual void Destroy(void) override
			{
				for (auto & timer : _timers) {
					[timer->timer invalidate];
					[timer release];
				}
				_timers.Clear();
				SetPresentationEngine(0);
				if (_callback) _callback->Destroyed(this);
				_callback = 0;
				if (_parent) for (int i = 0; i < _parent->_children.Length(); i++) if (_parent->_children[i] == this) { _parent->_children.Remove(i); break; }
				while (_children.Length()) _children.LastElement()->Destroy();
				if (_visible) Show(false);
				_view->owner = 0;
				[_window setDelegate: 0];
				[_delegate release];
				[_window close];
				[_window release];
				_window = 0;
				Release();
			}
			virtual void SetPresentationEngine(IPresentationEngine * engine) override
			{
				if (_engine) {
					_engine->Detach();
					_engine.SetReference(0);
					_layer_callback = 0;
					_user_layer_callback = false;
				}
				if (engine) {
					_engine.SetRetain(engine);
					_engine->Attach(this);
					_engine->Invalidate();
				}
			}
			virtual IPresentationEngine * GetPresentationEngine(void) override { return _engine; }
			virtual void InvalidateContents(void) override { if (_engine) _engine->Invalidate(); }
			virtual handle GetOSHandle(void) override { return _window; }
			virtual void SubmitTask(IDispatchTask * task) override { GetWindowSystem()->SubmitTask(task); }
			virtual void BeginSubmit(void) override {}
			virtual void AppendTask(IDispatchTask * task) override { GetWindowSystem()->SubmitTask(task); }
			virtual void EndSubmit(void) override {}
		};
		class SystemMenuItem : public IMenuItem
		{
			NSMenu * _menu;
			NSMenuItem * _item;
			void * _user;
			int _id;
			string _title, _side;
			IMenuItemCallback * _callback;
			bool _separator, _checked, _enabled, _highlighted;
			SafePointer<IMenu> _submenu;

			void _update_title(void)
			{
				if (_separator) return;
				auto title = Cocoa::CocoaString(_title);
				[_item setTitle: title];
				[title release];
			}
			void _measure_item(void)
			{
				if (_callback) {
					auto view = (ERTMenuItemView *) [_item view];
					auto size = _callback->MeasureMenuItem(this, view->device);
					auto scale = [[NSScreen mainScreen] backingScaleFactor];
					view->ref_size = NSMakeSize(size.x / scale, size.y / scale);
					if (_menu) [_menu itemChanged: _item];
				}
			}
			void _reset_item(void)
			{
				int index;
				if (_menu) {
					index = [_menu indexOfItem: _item];
					[_menu removeItemAtIndex: index];
					[_item release];
				}
				if (_callback) {
					_item = [[NSMenuItem alloc] initWithTitle: @"" action: NULL keyEquivalent: @""];
					_update_title();
					NSMenu * hmenu = _submenu ? (NSMenu *) _submenu->GetOSHandle() : 0;
					if (hmenu) [_item setSubmenu: hmenu];
					[_item setEnabled: _enabled];
					[_item setState: _checked ? NSControlStateValueOn : NSControlStateValueOff];
					auto view = [[ERTMenuItemView alloc] init];
					view->item = this;
					[_item setView: view];
					[view release];
					_measure_item();
				} else {
					if (_separator) {
						@autoreleasepool {
							_item = [NSMenuItem separatorItem];
							[_item retain];
						}
					} else {
						_item = [[NSMenuItem alloc] initWithTitle: @"" action: NULL keyEquivalent: @""];
						_update_title();
						NSMenu * hmenu = _submenu ? (NSMenu *) _submenu->GetOSHandle() : 0;
						if (hmenu) [_item setSubmenu: hmenu];
						[_item setEnabled: _enabled];
						[_item setState: _checked ? NSControlStateValueOn : NSControlStateValueOff];
					}
				}
				[_item setTag: reinterpret_cast<NSInteger>(this)];
				if (_menu) {
					[_menu insertItem: _item atIndex: index];
					[_item setTarget: [_menu delegate]];
					[_item setAction: @selector(menuItemSelected:)];
				}
			}
		public:
			SystemMenuItem(void) : _user(0), _menu(0), _id(0), _callback(0), _separator(false), _checked(false), _enabled(true), _highlighted(false)
			{
				_item = [[NSMenuItem alloc] initWithTitle: @"" action: NULL keyEquivalent: @""];
				[_item setTag: reinterpret_cast<NSInteger>(this)];
			}
			virtual ~SystemMenuItem(void) override { if (_callback) _callback->MenuItemDisposed(this); [_item release]; }
			virtual void SetCallback(IMenuItemCallback * callback) override { _callback = callback; _reset_item(); }
			virtual IMenuItemCallback * GetCallback(void) override { return _callback; }
			virtual void SetUserData(void * data) override { _user = data; _measure_item(); }
			virtual void * GetUserData(void) override { return _user; }
			virtual void SetSubmenu(IMenu * menu) override { _submenu.SetRetain(menu); NSMenu * hmenu = _submenu ? (NSMenu *) _submenu->GetOSHandle() : 0; if (!_separator) [_item setSubmenu: hmenu]; }
			virtual IMenu * GetSubmenu(void) override { return _submenu; }
			virtual void SetID(int identifier) override { _id = identifier; }
			virtual int GetID(void) override { return _id; }
			virtual void SetText(const string & text) override { _title = text; _update_title(); _measure_item(); }
			virtual string GetText(void) override { return _title; }
			virtual void SetSideText(const string & text) override { _side = text; _measure_item(); }
			virtual string GetSideText(void) override { return _side; }
			virtual void SetIsSeparator(bool separator) override { _separator = separator; _reset_item(); _measure_item(); }
			virtual bool IsSeparator(void) override { return _separator; }
			virtual void Enable(bool enable) override { _enabled = enable; if (!_separator) [_item setEnabled: _enabled]; _measure_item(); }
			virtual bool IsEnabled(void) override { return _enabled; }
			virtual void Check(bool check) override { _checked = check; if (!_separator) [_item setState: _checked ? NSControlStateValueOn : NSControlStateValueOff]; _measure_item(); }
			virtual bool IsChecked(void) override { return _checked; }
			NSMenuItem * GetOSHandle(void) noexcept { return _item; }
			bool Attach(IMenu * menu) noexcept
			{
				if (!_menu) {
					_menu = (NSMenu *) menu->GetOSHandle();
					[_item setTarget: [_menu delegate]];
					[_item setAction: @selector(menuItemSelected:)];
					return true;
				} else return false;
			}
			void Detach(void) noexcept { [_item setTarget: nil]; [_item setAction: NULL]; _menu = 0; }
			void Highlight(bool set) { _highlighted = set; if ([_item view]) [[_item view] setNeedsDisplay: YES]; }
			bool IsHighlighted(void) { return _highlighted; }
			double GetElementWidth(void) const noexcept
			{
				if (_callback) {
					auto view = (ERTMenuItemView *) [_item view];
					return view->ref_size.width;
				} else return 0.0;
			}
			void SetActualWidth(double width) noexcept
			{
				if (_callback) {
					auto view = (ERTMenuItemView *) [_item view];
					[view setFrameSize: NSMakeSize(width, view->ref_size.height)];
					if (_menu) [_menu itemChanged: _item];
				}
			}
		};
		class SystemMenu : public IMenu
		{
			NSMenu * _menu;
			ERTMenuDelegate * _delegate;
			ObjectArray<SystemMenuItem> _items;
		public:
			SystemMenu(void) : _items(0x10)
			{
				_menu = [[NSMenu alloc] initWithTitle: @""];
				_delegate = [[ERTMenuDelegate alloc] init];
				_delegate->owner = this;
				_delegate->collected_responce = 0;
				_delegate->icon = 0;
				if (!_menu) throw Exception();
				[_menu setAutoenablesItems: NO];
				[_menu setDelegate: _delegate];
			}
			virtual ~SystemMenu(void) override
			{
				for (auto & item : _items) item.Detach();
				[_menu release];
				[_delegate release];
			}
			virtual void AppendMenuItem(IMenuItem * item) noexcept override
			{
				auto mi = static_cast<SystemMenuItem *>(item);
				if (mi->Attach(this)) try {
					_items.Append(mi);
					[_menu addItem: mi->GetOSHandle()];
				} catch (...) { mi->Detach(); }
			}
			virtual void InsertMenuItem(IMenuItem * item, int at) noexcept override
			{
				auto mi = static_cast<SystemMenuItem *>(item);
				if (mi->Attach(this)) try {
					_items.Insert(mi, at);
					[_menu insertItem: mi->GetOSHandle() atIndex: at];
				} catch (...) { mi->Detach(); }
			}
			virtual void RemoveMenuItem(int at) noexcept override { [_menu removeItemAtIndex: at]; _items[at].Detach(); _items.Remove(at); }
			virtual IMenuItem * ElementAt(int at) noexcept override { return _items.ElementAt(at); }
			virtual int Length(void) noexcept override { return _items.Length(); }
			virtual IMenuItem * FindMenuItem(int identifier) noexcept override
			{
				for (auto & item : _items) {
					if (item.GetID() == identifier) return &item;
					auto submenu = item.GetSubmenu();
					if (submenu) {
						auto inner_item = submenu->FindMenuItem(identifier);
						if (inner_item) return inner_item;
					}
				}
				return 0;
			}
			virtual int Run(IWindow * owner, Point at) noexcept override
			{
				if (!owner) return 0;
				Configure(0);
				[_menu popUpMenuPositioningItem: nil atLocation: SystemWindow::_screen_engine_to_cocoa(at) inView: nil];
				return Collect();
			}
			virtual handle GetOSHandle(void) noexcept override { return _menu; }
			void Configure(IStatusBarIcon * icon)
			{
				_delegate->icon = icon;
				_delegate->collected_responce = 0;
				for (auto & item : _items) {
					auto menu = item.GetSubmenu();
					if (menu) static_cast<SystemMenu *>(menu)->Configure(icon);
				}
			}
			int Collect(void)
			{
				if (_delegate->collected_responce) return _delegate->collected_responce;
				for (auto & item : _items) {
					auto sub = item.GetSubmenu();
					auto child = sub ? static_cast<SystemMenu *>(sub)->Collect() : 0;
					if (child) return child;
				}
				return 0;
			}
		};
		class SystemStatusBarIcon : public IStatusBarIcon
		{
			IStatusCallback * _callback;
			SafePointer<Codec::Image> _icon;
			StatusBarIconColorUsage _icon_usage;
			string _tooltip;
			int _id;
			SafePointer<SystemMenu> _menu;
			bool _present;
			NSImage * _image;
			NSStatusItem * _item;
			NSStatusBar * _bar;
			ERTStatusBarDelegate * _delegate;

			void _update_item(void)
			{
				[[_item button] setImage: _image];
				[_item setMenu: _menu ? (NSMenu *) _menu->GetOSHandle() : nil];
			}
			bool _is_light_theme(void)
			{
				auto name = [[[_item button] effectiveAppearance] name];
				if ([name compare: NSAppearanceNameDarkAqua] == NSOrderedSame) return false;
				else if ([name compare: NSAppearanceNameVibrantDark] == NSOrderedSame) return false;
				else if ([name compare: NSAppearanceNameAccessibilityHighContrastDarkAqua] == NSOrderedSame) return false;
				else if ([name compare: NSAppearanceNameAccessibilityHighContrastVibrantDark] == NSOrderedSame) return false;
				else return true;
			}
			void _recreate_icon(void)
			{
				auto scale = [[NSScreen mainScreen] backingScaleFactor];
				auto size = GetIconSize();
				Codec::Frame * frame = _icon ? _icon->GetFrameBestSizeFit(size.x, size.y) : 0;
				if (frame) {
					SafePointer<Codec::Frame> use_frame;
					if (_icon_usage == StatusBarIconColorUsage::Monochromic) {
						use_frame = frame->ConvertFormat(Codec::PixelFormat::R8G8B8A8, Codec::AlphaMode::Normal, Codec::ScanOrigin::TopDown);
						uint32 color = _is_light_theme() ? 0x00000000 : 0x00FFFFFF;
						for (int y = 0; y < use_frame->GetHeight(); y++) for (int x = 0; x < use_frame->GetWidth(); x++) {
							uint32 pixel = use_frame->GetPixel(x, y);
							use_frame->SetPixel(x, y, (pixel & 0xFF000000) | color);
						}
					} else use_frame.SetRetain(frame);
					_image = Cocoa::CocoaImage(use_frame, scale);
				} else _image = 0;
				if (_present) _update_item();
			}
			void _update_icon(void)
			{
				if (_image) [_image release];
				_image = 0;
				if (_present) _recreate_icon();
			}
		public:
			SystemStatusBarIcon(void) : _callback(0), _icon_usage(StatusBarIconColorUsage::Colourfull), _id(0), _present(0), _image(0), _item(0)
			{
				@autoreleasepool {
					_bar = [NSStatusBar systemStatusBar];
					[_bar retain];
					_delegate = [[ERTStatusBarDelegate alloc] init];
					_delegate->icon = this;
				}
			}
			virtual ~SystemStatusBarIcon(void) override
			{
				if (_present) PresentIcon(false);
				if (_menu) _menu->Configure(0);
				[_delegate release];
				[_bar release];
				[_image release];
				[_item release];
			}
			virtual void SetCallback(IStatusCallback * callback) noexcept override { _callback = callback; }
			virtual IStatusCallback * GetCallback(void) noexcept override { return _callback; }
			virtual Point GetIconSize(void) noexcept override { auto scale = [[NSScreen mainScreen] backingScaleFactor]; auto size = int([_bar thickness] * scale); return Point(size, size); }
			virtual void SetIcon(Codec::Image * image) noexcept override { _icon.SetRetain(image); _update_icon(); }
			virtual Codec::Image * GetIcon(void) noexcept override { return _icon; }
			virtual void SetIconColorUsage(StatusBarIconColorUsage color_usage) noexcept override { _icon_usage = color_usage; _update_icon(); }
			virtual StatusBarIconColorUsage GetIconColorUsage(void) noexcept override { return _icon_usage; }
			virtual void SetTooltip(const string & text) noexcept override { _tooltip = text; }
			virtual string GetTooltip(void) noexcept override { return _tooltip; }
			virtual void SetEventID(int ID) noexcept override { _id = ID; }
			virtual int GetEventID(void) noexcept override { return _id; }
			virtual void SetMenu(IMenu * menu) noexcept override
			{
				if (_menu) _menu->Configure(0);
				_menu.SetRetain(static_cast<SystemMenu *>(menu));
				if (_menu) _menu->Configure(this);
				_update_item();
			}
			virtual IMenu * GetMenu(void) noexcept override { return _menu; }
			virtual bool PresentIcon(bool present) noexcept override
			{
				@autoreleasepool {
					if (present == _present) return true;
					if (!_icon) return false;
					if (present) {
						_item = [_bar statusItemWithLength: NSSquareStatusItemLength];
						[_item retain];
						if (!_image) _recreate_icon();
						_update_item();
						[[_item button] setTarget: _delegate];
						[[_item button] setAction: @selector(statusIconSelected:)];
					} else {
						[_bar removeStatusItem: _item];
						[_item release];
						_item = 0;
					}
					return true;
				}
			}
			virtual bool IsVisible(void) noexcept override { return _present; }
		};
		class SystemIPCClient : public IIPCClient
		{
			friend class WindowSystem;
			struct packet_desc
			{
				packet_desc * next;
				uint64 pid;
				SafePointer<IDispatchTask> task;
				DataBlock ** data;
				IPCStatus * status;
			};

			CFMessagePortRef _server, _self;
			CFRunLoopSourceRef _self_event;
			string _self_name;
			IPCStatus _status;
			uint64 _current_packet;
			packet_desc * _first, * _last;

			void _push_packet_desc(packet_desc * desc) noexcept { desc->next = 0; if (_last) { _last->next = desc; _last = desc; } else _last = _first = desc; }
			packet_desc * _pop_packet_desc(void) noexcept { auto result = _first; if (_first) _first = _first->next; if (!_first) _last = 0; return result; }
			packet_desc * _pop_packet_desc(uint64 pid) noexcept
			{
				packet_desc ** ppprev = &_first;
				packet_desc * current = _first;
				packet_desc * prev = 0;
				while (current && current->pid != pid) { ppprev = &current->next; prev = current; current = current->next; }
				if (current) {
					*ppprev = current->next;
					if (current == _last) _last = prev;
					return current;
				} else return 0;
			}
			static void _packet_invalidate(packet_desc * desc) noexcept
			{
				SafePointer<IDispatchTask> task;
				DataBlock ** data;
				IPCStatus * status;
				if (desc->status) *desc->status = IPCStatus::ServerClosed;
				if (desc->data) *desc->data = 0;
				if (desc->task) desc->task->DoTask(GetWindowSystem());
				delete desc;
			}
			static CFDataRef _assembly_ipc_data(const string & verb, const string & client_name, uint64 packet_id, const DataBlock * data) noexcept
			{
				try {
					DataBlock buffer(0x1000);
					int cn_len = client_name.GetEncodedLength(Encoding::UTF8) + 1;
					int v_len = verb.GetEncodedLength(Encoding::UTF8) + 1;
					int d_len = data ? data->Length() : 0;
					int len = 12 + cn_len + v_len + d_len;
					buffer.SetLength(len);
					MemoryCopy(buffer.GetBuffer(), &packet_id, 8);
					MemoryCopy(buffer.GetBuffer() + 8, &d_len, 4);
					client_name.Encode(buffer.GetBuffer() + 12, Encoding::UTF8, true);
					verb.Encode(buffer.GetBuffer() + 12 + cn_len, Encoding::UTF8, true);
					if (d_len) MemoryCopy(buffer.GetBuffer() + 12 + cn_len + v_len, data->GetBuffer(), d_len);
					auto result = CFDataCreate(kCFAllocatorDefault, buffer.GetBuffer(), buffer.Length());
					if (!result) throw OutOfMemoryException();
					return result;
				} catch (...) { return 0; }
			}
			static void _disassembly_ipc_data(CFDataRef input, string & verb, string & client_name, uint64 & packet_id, DataBlock ** data) noexcept
			{
				verb = L""; client_name = L""; packet_id = 0;
				if (data) *data = 0;
				try {
					auto pdata = CFDataGetBytePtr(input);
					int d_len;
					MemoryCopy(&packet_id, pdata, 8);
					MemoryCopy(&d_len, pdata + 8, 4);
					client_name = string(pdata + 12, -1, Encoding::UTF8);
					verb = string(pdata + 13 + client_name.GetEncodedLength(Encoding::UTF8), -1, Encoding::UTF8);
					int d_base = 14 + client_name.GetEncodedLength(Encoding::UTF8) + verb.GetEncodedLength(Encoding::UTF8);
					if (d_len) {
						SafePointer<DataBlock> bdata = new DataBlock(d_len);
						bdata->SetLength(d_len);
						MemoryCopy(bdata->GetBuffer(), pdata + d_base, d_len);
						if (data) {
							bdata->Retain();
							*data = bdata.Inner();
						}
					} else {
						if (data) {
							SafePointer<DataBlock> bdata = new DataBlock(1);
							bdata->Retain();
							*data = bdata.Inner();
						}
					}
				} catch (...) {}
			}
			static CFDataRef _assembly_ipc_responce(const DataBlock * data, uint64 packet_id, bool status) noexcept
			{
				try {
					DataBlock buffer(0x1000);
					int d_len = data ? data->Length() : 0;
					int len = 16 + d_len;
					buffer.SetLength(len);
					uint32 s = status ? 0xFFFFFFFF : 0;
					MemoryCopy(buffer.GetBuffer(), &packet_id, 8);
					MemoryCopy(buffer.GetBuffer() + 8, &s, 4);
					MemoryCopy(buffer.GetBuffer() + 12, &d_len, 4);
					if (d_len) MemoryCopy(buffer.GetBuffer() + 16, data->GetBuffer(), d_len);
					auto result = CFDataCreate(kCFAllocatorDefault, buffer.GetBuffer(), buffer.Length());
					if (!result) throw OutOfMemoryException();
					return result;
				} catch (...) { return 0; }
			}
			static void _disassembly_ipc_responce(CFDataRef input, DataBlock ** data, uint64 & packet_id, bool & status) noexcept
			{
				packet_id = 0; status = false;
				if (data) *data = 0;
				try {
					auto pdata = CFDataGetBytePtr(input);
					int d_len, s;
					MemoryCopy(&packet_id, pdata, 8);
					MemoryCopy(&s, pdata + 8, 4);
					MemoryCopy(&d_len, pdata + 12, 4);
					status = s ? true : false;
					if (d_len) {
						SafePointer<DataBlock> bdata = new DataBlock(d_len);
						bdata->SetLength(d_len);
						MemoryCopy(bdata->GetBuffer(), pdata + 16, d_len);
						if (data) {
							bdata->Retain();
							*data = bdata.Inner();
						}
					} else {
						if (data) {
							SafePointer<DataBlock> bdata = new DataBlock(1);
							bdata->Retain();
							*data = bdata.Inner();
						}
					}
				} catch (...) {}
			}
			static CFDataRef _assembly_ipc_shutdown(const string & client_name) noexcept
			{
				try {
					DataBlock buffer(0x1000);
					int cn_len = client_name.GetEncodedLength(Encoding::UTF8) + 1;
					buffer.SetLength(cn_len);
					client_name.Encode(buffer.GetBuffer(), Encoding::UTF8, true);
					auto result = CFDataCreate(kCFAllocatorDefault, buffer.GetBuffer(), buffer.Length());
					if (!result) throw OutOfMemoryException();
					return result;
				} catch (...) { return 0; }
			}
			static void _disassembly_ipc_shutdown(CFDataRef input, string & client_name) noexcept
			{
				client_name = L"";
				try { client_name = string(CFDataGetBytePtr(input), -1, Encoding::UTF8); } catch (...) {}
			}
			static CFDataRef _ipc_client_callback(CFMessagePortRef local, SInt32 msgid, CFDataRef data, void * info)
			{
				if (msgid == 1) {
					auto client = reinterpret_cast<SystemIPCClient *>(info);
					client->_status = IPCStatus::ServerClosed;
					CFMessagePortInvalidate(client->_server);
					CFMessagePortInvalidate(client->_self);
					packet_desc * desc;
					while (desc = client->_pop_packet_desc()) _packet_invalidate(desc);
				} else if (msgid == 4) {
					SafePointer<DataBlock> block;
					uint64 packet_id;
					bool status;
					_disassembly_ipc_responce(data, block.InnerRef(), packet_id, status);
					auto client = reinterpret_cast<SystemIPCClient *>(info);
					auto desc = client->_pop_packet_desc(packet_id);
					IPCStatus ipc_status = status ? IPCStatus::Accepted : IPCStatus::Discarded;
					client->_status = ipc_status;
					if (desc) {
						if (desc->status) *desc->status = ipc_status;
						if (desc->data) {
							if (status && block) {
								*desc->data = block.Inner();
								block->Retain();
							} else *desc->data = 0;
						}
						if (desc->task) desc->task->DoTask(GetWindowSystem());
						delete desc;
					}
				}
				return 0;
			}
		public:
			SystemIPCClient(const string & server_name) : _status(IPCStatus::Unknown), _current_packet(0), _first(0), _last(0)
			{
				auto ns_name = Cocoa::CocoaString(server_name);
				if (!ns_name) throw OutOfMemoryException();
				_server = CFMessagePortCreateRemote(kCFAllocatorDefault, reinterpret_cast<CFStringRef>(ns_name));
				[ns_name release];
				if (!_server) throw Exception();
				uint i = 0;
				try {
					while (true) {
						if (i == 0x1000) throw Exception();
						string probe_name = server_name + L":" + string(i, HexadecimalBase, 4);
						::Boolean unused;
						CFMessagePortContext context;
						context.copyDescription = 0;
						context.info = this;
						context.release = 0;
						context.retain = 0;
						context.version = 0;
						auto ipc_server_name = Cocoa::CocoaString(probe_name);
						if (!ipc_server_name) throw OutOfMemoryException();
						_self = CFMessagePortCreateLocal(kCFAllocatorDefault, reinterpret_cast<CFStringRef>(ipc_server_name), _ipc_client_callback, &context, &unused);
						[ipc_server_name release];
						if (!_self) { i++; continue; }
						try {
							_self_name = probe_name;
							_self_event = CFMessagePortCreateRunLoopSource(kCFAllocatorDefault, _self, 0);
							if (!_self_event) throw Exception();
							CFRunLoopAddSource(CFRunLoopGetMain(), _self_event, kCFRunLoopCommonModes);
						} catch (...) {
							CFMessagePortInvalidate(_self);
							CFRelease(_self);
							throw;
						}
						break;
					}
				} catch (...) {
					CFMessagePortInvalidate(_server);
					CFRelease(_server);
					throw;
				}
				CFDataRef authorize_data = _assembly_ipc_shutdown(_self_name);
				if (authorize_data) {
					CFMessagePortSendRequest(_server, 4, authorize_data, 0, 0, 0, 0);
					CFRelease(authorize_data);
				}
			}
			virtual ~SystemIPCClient(void) override
			{
				CFDataRef close_req = _assembly_ipc_shutdown(_self_name);
				if (close_req) {
					CFMessagePortSendRequest(_server, 1, close_req, 0, 0, 0, 0);
					CFRelease(close_req);
				}
				CFMessagePortInvalidate(_server);
				CFRelease(_server);
				CFRunLoopRemoveSource(CFRunLoopGetMain(), _self_event, kCFRunLoopCommonModes);
				CFRelease(_self_event);
				CFMessagePortInvalidate(_self);
				CFRelease(_self);
				packet_desc * desc;
				while (desc = _pop_packet_desc()) _packet_invalidate(desc);
			}
			virtual bool SendData(const string & verb, const DataBlock * data, IDispatchTask * on_responce, IPCStatus * result) noexcept override
			{
				if (_status == IPCStatus::ServerClosed) return false;
				if (verb.Length() > 0xFF) return false;
				packet_desc * desc = new (std::nothrow) packet_desc;
				if (!desc) return false;
				CFDataRef send = _assembly_ipc_data(verb, _self_name, _current_packet, data);
				if (send) {
					CFMessagePortSendRequest(_server, 2, send, 0, 0, 0, 0);
					CFRelease(send);
					desc->pid = _current_packet;
					desc->task.SetRetain(on_responce);
					desc->data = 0;
					desc->status = result;
					_push_packet_desc(desc);
					_current_packet++;
					return true;
				} else { delete desc; return false; }
			}
			virtual bool RequestData(const string & verb, IDispatchTask * on_responce, IPCStatus * result, DataBlock ** data) noexcept override
			{
				if (_status == IPCStatus::ServerClosed) return false;
				if (verb.Length() > 0xFF) return false;
				packet_desc * desc = new (std::nothrow) packet_desc;
				if (!desc) return false;
				CFDataRef send = _assembly_ipc_data(verb, _self_name, _current_packet, 0);
				if (send) {
					CFMessagePortSendRequest(_server, 3, send, 0, 0, 0, 0);
					CFRelease(send);
					desc->pid = _current_packet;
					desc->task.SetRetain(on_responce);
					desc->data = data;
					desc->status = result;
					_push_packet_desc(desc);
					_current_packet++;
					return true;
				} else { delete desc; return false; }
			}
			virtual IPCStatus GetStatus(void) noexcept override { return _status; }
		};
		class WindowSystem : public IWindowSystem
		{
			struct KeyboardHotKeyDesc
			{
				int hotkey_id;
				uint key_code;
				uint key_flags;
			};

			SafePointer<ICursor> _null, _arrow, _beam, _link, _size_left_right, _size_up_down, _size_left_up_right_down, _size_left_down_right_up, _size_all;
			IApplicationCallback * _callback;
			ERTApplicationDelegate * _delegate;
			Array<string> _file_list_open;
			CFMessagePortRef _ipc_server;
			CFRunLoopSourceRef _ipc_event_source;
			Volumes::Dictionary<string, CFMessagePortRef> _ipc_clients;
			Array<IWindow *> _main_windows;
			Array<KeyboardHotKeyDesc> _hot_keys;
			CFMachPortRef _hot_key_tap;
			CFRunLoopSourceRef _hot_key_event_source;

			static NSString * _std_string(int string_id, const widechar * placeholder)
			{
				auto result = Cocoa::CocoaString(Assembly::GetLocalizedCommonString(string_id, placeholder));
				[result autorelease];
				return result;
			}
			template <class F> static NSMenu * _create_menu(NSString * title, F populate)
			{
				auto result = [[NSMenu alloc] initWithTitle: title];
				[result autorelease];
				populate(result);
				return result;
			}
			template <class F> static NSMenu * _create_menu(NSMenuItem * item, NSString * title, F populate)
			{
				auto menu = _create_menu(title, populate);
				[item setSubmenu: menu];
				return menu;
			}
			template <class F> static NSMenu * _create_submenu(NSMenu * at, NSString * title, F populate)
			{
				auto item = [[NSMenuItem alloc] initWithTitle: title action: NULL keyEquivalent: @""];
				[item autorelease];
				auto menu = _create_menu(item, title, populate);
				[at addItem: item];
				return menu;
			}
			static NSMenuItem * _create_menu_item(NSMenu * at, NSString * title, SEL action)
			{
				auto result = [[NSMenuItem alloc] initWithTitle: title action: action keyEquivalent: @""];
				[result autorelease];
				[at addItem: result];
				return result;
			}
			static NSMenuItem * _create_menu_item(NSMenu * at, NSString * title, SEL action, NSString * hot_key)
			{
				auto result = [[NSMenuItem alloc] initWithTitle: title action: action keyEquivalent: hot_key];
				[result autorelease];
				[at addItem: result];
				return result;
			}
			static NSMenuItem * _create_menu_item(NSMenu * at, NSString * title, SEL action, NSString * hot_key, NSEventModifierFlags flags)
			{
				auto result = _create_menu_item(at, title, action, hot_key);
				[result setKeyEquivalentModifierMask: flags];
				return result;
			}
			static NSMenuItem * _create_menu_separator(NSMenu * at)
			{
				auto result = [NSMenuItem separatorItem];
				[at addItem: result];
				return result;
			}
			static CGEventRef _event_tap_callback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void * user)
			{
				auto self = reinterpret_cast<WindowSystem *>(user);
				int id_dispatch = -1;
				bool dead;
				auto flags = CGEventGetFlags(event);
				auto value = Cocoa::EngineKeyCode(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode), dead);
				auto autorep = CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat);
				auto eflags = 0;
				if (flags & kCGEventFlagMaskShift) eflags |= HotKeyShift;
				if (flags & kCGEventFlagMaskControl) eflags |= HotKeyControl;
				if (flags & kCGEventFlagMaskAlternate) eflags |= HotKeyAlternative;
				if (flags & kCGEventFlagMaskCommand) eflags |= HotKeySystem;
				if (!autorep) for (auto & event_desc : self->_hot_keys) {
					if (value == event_desc.key_code && eflags == event_desc.key_flags) {
						id_dispatch = event_desc.hotkey_id;
						break;
					}
				}
				if (id_dispatch > 0 && self->_callback) {
					self->_callback->HotKeyEvent(id_dispatch);
					return 0;
				} else return event;
			}
			bool _setup_event_tap(void)
			{
				_hot_key_tap = CGEventTapCreate(kCGSessionEventTap, kCGTailAppendEventTap, kCGEventTapOptionDefault, CGEventMaskBit(kCGEventKeyDown), _event_tap_callback, this);
				if (!_hot_key_tap) return false;
				_hot_key_event_source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, _hot_key_tap, 0);
				CFRunLoopAddSource(CFRunLoopGetMain(), _hot_key_event_source, kCFRunLoopCommonModes);
				return true;
			}
			void _shutdown_event_tap(void)
			{
				if (_hot_key_event_source) {
					CFRunLoopRemoveSource(CFRunLoopGetMain(), _hot_key_event_source, kCFRunLoopCommonModes);
					CFRelease(_hot_key_event_source);
					_hot_key_event_source = 0;
				}
				if (_hot_key_tap) {
					CFRelease(_hot_key_tap);
					_hot_key_tap = 0;
				}
			}
			void _reset_menu(void)
			{
				@autoreleasepool {
					auto callback = _callback;
					auto menu = _create_menu(@"Main Menu", [callback] (NSMenu * menu) {
						_create_submenu(menu, @"Application Menu", [callback] (NSMenu * menu) {
							SEL about_sel = (callback && callback->IsHandlerEnabled(Engine::Windows::ApplicationHandler::ShowAbout)) ? @selector(applicationShowAbout:) : @selector(orderFrontStandardAboutPanel:);
							_create_menu_item(menu, _std_string(301, L"About"), about_sel);
							_create_menu_separator(menu);
							if (callback && callback->IsHandlerEnabled(Engine::Windows::ApplicationHandler::ShowProperties)) {
								_create_menu_item(menu, _std_string(317, L"Preferences"), @selector(applicationShowProperties:), @",");
								_create_menu_separator(menu);
							}
							_create_submenu(menu, _std_string(302, L"Services"), [] (NSMenu * menu) { [NSApp setServicesMenu: menu]; });
							_create_menu_separator(menu);
							_create_menu_item(menu, _std_string(303, L"Hide"), @selector(hide:), @"h");
							_create_menu_item(menu, _std_string(304, L"Hide others"), @selector(hideOtherApplications:), @"h", NSEventModifierFlagOption | NSEventModifierFlagCommand);
							_create_menu_item(menu, _std_string(305, L"Show all"), @selector(unhideAllApplications:));
							_create_menu_separator(menu);
							_create_menu_item(menu, _std_string(306, L"Exit"), @selector(applicationClose:), @"q");
						});
						_create_submenu(menu, _std_string(313, L"File"), [callback] (NSMenu * menu) {
							bool add_separator = false;
							if (callback && callback->IsHandlerEnabled(Engine::Windows::ApplicationHandler::CreateFile)) {
								_create_menu_item(menu, _std_string(314, L"New file"), @selector(applicationCreateNewFile:), @"n");
								add_separator = true;
							}
							if (callback && callback->IsHandlerEnabled(Engine::Windows::ApplicationHandler::OpenSomeFile)) {
								_create_menu_item(menu, _std_string(315, L"Open file"), @selector(applicationOpenFile:), @"o");
								add_separator = true;
							}
							if (add_separator) _create_menu_separator(menu);
							add_separator = false;
							if (callback && callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Save)) {
								_create_menu_item(menu, _std_string(319, L"Save file"), @selector(engineWindowSave:), @"s");
								add_separator = true;
							}
							if (callback && callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::SaveAs)) {
								_create_menu_item(menu, _std_string(320, L"Save file as"), @selector(engineWindowSaveAs:), @"s", NSEventModifierFlagShift | NSEventModifierFlagCommand);
								add_separator = true;
							}
							if (callback && callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Export)) {
								_create_menu_item(menu, _std_string(321, L"Export"), @selector(engineWindowExport:), @"s", NSEventModifierFlagOption | NSEventModifierFlagCommand);
								add_separator = true;
							}
							if (add_separator) _create_menu_separator(menu);
							if (callback && callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Print)) {
								_create_menu_item(menu, _std_string(322, L"Print"), @selector(engineWindowPrint:), @"p");
								_create_menu_separator(menu);
							}
							_create_menu_item(menu, _std_string(316, L"Close window"), @selector(performClose:), @"w");
						});
						if (callback) {
							if (callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Undo) || callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Redo) ||
								callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Cut) || callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Copy) ||
								callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Paste) || callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Duplicate) ||
								callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Delete) || callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::SelectAll) ||
								callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Find) || callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Replace)) {
								_create_submenu(menu, _std_string(323, L"Edit"), [callback] (NSMenu * menu) {
									bool add_separator = false;
									if (callback && callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Undo)) {
										_create_menu_item(menu, _std_string(324, L"Undo"), @selector(engineWindowUndo:), @"z");
										add_separator = true;
									}
									if (callback && callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Redo)) {
										_create_menu_item(menu, _std_string(325, L"Redo"), @selector(engineWindowRedo:), @"z", NSEventModifierFlagShift | NSEventModifierFlagCommand);
										add_separator = true;
									}
									if (add_separator) _create_menu_separator(menu);
									add_separator = false;
									if (callback && callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Cut)) {
										_create_menu_item(menu, _std_string(326, L"Cut"), @selector(engineWindowCut:), @"x");
										add_separator = true;
									}
									if (callback && callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Copy)) {
										_create_menu_item(menu, _std_string(327, L"Copy"), @selector(engineWindowCopy:), @"c");
										add_separator = true;
									}
									if (callback && callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Paste)) {
										_create_menu_item(menu, _std_string(328, L"Paste"), @selector(engineWindowPaste:), @"v");
										add_separator = true;
									}
									if (callback && callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Duplicate)) {
										_create_menu_item(menu, _std_string(329, L"Duplicate"), @selector(engineWindowDuplicate:), @"d");
										add_separator = true;
									}
									if (callback && callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Delete)) {
										_create_menu_item(menu, _std_string(330, L"Delete"), @selector(engineWindowDelete:));
										add_separator = true;
									}
									if (add_separator) _create_menu_separator(menu);
									add_separator = false;
									if (callback && callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::SelectAll)) {
										_create_menu_item(menu, _std_string(333, L"Select all"), @selector(engineWindowSelectAll:), @"a");
										add_separator = true;
									}
									if (add_separator) _create_menu_separator(menu);
									add_separator = false;
									if (callback && callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Find)) {
										_create_menu_item(menu, _std_string(331, L"Find"), @selector(engineWindowFind:), @"f");
										add_separator = true;
									}
									if (callback && callback->IsWindowEventAccessible(Engine::Windows::WindowHandler::Replace)) {
										_create_menu_item(menu, _std_string(332, L"Replace"), @selector(engineWindowReplace:), @"f", NSEventModifierFlagOption | NSEventModifierFlagCommand);
										add_separator = true;
									}
									if (add_separator) _create_menu_separator(menu);
								});
							}
						}
						_create_submenu(menu, _std_string(307, L"Window"), [] (NSMenu * menu) {
							_create_menu_item(menu, _std_string(308, L"Minimize"), @selector(performMiniaturize:), @"m");
							_create_menu_item(menu, _std_string(309, L"Zoom"), @selector(performZoom:));
							_create_menu_item(menu, _std_string(310, L"Toggle Full Screen"), @selector(toggleFullScreen:), @"f");
							_create_menu_separator(menu);
							_create_menu_item(menu, _std_string(311, L"Bring All to Front"), @selector(arrangeInFront:));
							[NSApp setWindowsMenu: menu];
						});
						_create_submenu(menu, _std_string(312, L"Help"), [callback] (NSMenu * menu) {
							if (callback && callback->IsHandlerEnabled(Engine::Windows::ApplicationHandler::ShowHelp)) {
								_create_menu_item(menu, _std_string(318, L"Show Help"), @selector(applicationShowHelp:));
							}
							[NSApp setHelpMenu: menu];
						});
					});
					[NSApp setMainMenu: menu];
				}
			}
			static CFDataRef _ipc_server_callback(CFMessagePortRef local, SInt32 msgid, CFDataRef data, void * info)
			{
				if (msgid == 1) {
					try {
						string client_name;
						SystemIPCClient::_disassembly_ipc_shutdown(data, client_name);
						auto winsys = static_cast<WindowSystem *>(GetWindowSystem());
						auto callback = winsys->GetCallback();
						auto port_ptr = winsys->_ipc_clients[client_name];
						if (port_ptr) {
							auto port = *port_ptr;
							CFMessagePortInvalidate(port);
							CFRelease(port);
							if (callback) try { callback->DataExchangeDisconnect(port); } catch (...) {}
						}
						winsys->_ipc_clients.Remove(client_name);
					} catch (...) {}
				} else if (msgid == 2) {
					try {
						string verb, client_name;
						uint64 packet_id;
						SafePointer<DataBlock> block;
						SystemIPCClient::_disassembly_ipc_data(data, verb, client_name, packet_id, block.InnerRef());
						auto winsys = static_cast<WindowSystem *>(GetWindowSystem());
						auto callback = winsys->GetCallback();
						auto responce_port_ptr = winsys->_ipc_clients[client_name];
						if (!responce_port_ptr) {
							auto responce_port_name = Cocoa::CocoaString(client_name);
							if (!responce_port_name) throw Exception();
							auto responce_port = CFMessagePortCreateRemote(kCFAllocatorDefault, reinterpret_cast<CFStringRef>(responce_port_name));
							[responce_port_name release];
							if (!responce_port) throw Exception();
							try {
								winsys->_ipc_clients.Append(client_name, responce_port);
								responce_port_ptr = winsys->_ipc_clients[client_name];
							} catch (...) {
								CFMessagePortInvalidate(responce_port);
								CFRelease(responce_port);
								throw;
							}
						}
						if (!responce_port_ptr) throw Exception();
						auto responce_port = *responce_port_ptr;
						bool result = false;
						if (callback && block) try { result = callback->DataExchangeReceive(responce_port, verb, block); } catch (...) {}
						auto responce = SystemIPCClient::_assembly_ipc_responce(0, packet_id, result);
						if (responce) {
							CFMessagePortSendRequest(responce_port, 4, responce, 0, 0, 0, 0);
							CFRelease(responce);
						}
					} catch (...) {}
				} else if (msgid == 3) {
					try {
						string verb, client_name;
						uint64 packet_id;
						SafePointer<DataBlock> block;
						SystemIPCClient::_disassembly_ipc_data(data, verb, client_name, packet_id, block.InnerRef());
						auto winsys = static_cast<WindowSystem *>(GetWindowSystem());
						auto callback = winsys->GetCallback();
						auto responce_port_ptr = winsys->_ipc_clients[client_name];
						if (!responce_port_ptr) {
							auto responce_port_name = Cocoa::CocoaString(client_name);
							if (!responce_port_name) throw Exception();
							auto responce_port = CFMessagePortCreateRemote(kCFAllocatorDefault, reinterpret_cast<CFStringRef>(responce_port_name));
							[responce_port_name release];
							if (!responce_port) throw Exception();
							try {
								winsys->_ipc_clients.Append(client_name, responce_port);
								responce_port_ptr = winsys->_ipc_clients[client_name];
							} catch (...) {
								CFMessagePortInvalidate(responce_port);
								CFRelease(responce_port);
								throw;
							}
						}
						if (!responce_port_ptr) throw Exception();
						auto responce_port = *responce_port_ptr;
						block.SetReference(0);
						if (callback) try { block = callback->DataExchangeRespond(responce_port, verb); } catch (...) {}
						auto responce = SystemIPCClient::_assembly_ipc_responce(block, packet_id, block ? true : false);
						if (responce) {
							CFMessagePortSendRequest(responce_port, 4, responce, 0, 0, 0, 0);
							CFRelease(responce);
						}
					} catch (...) {}
				} else if (msgid == 4) {
					try {
						string client_name;
						SystemIPCClient::_disassembly_ipc_shutdown(data, client_name);
						auto winsys = static_cast<WindowSystem *>(GetWindowSystem());
						if (!winsys->_ipc_clients[client_name]) {
							auto responce_port_name = Cocoa::CocoaString(client_name);
							if (!responce_port_name) throw Exception();
							auto responce_port = CFMessagePortCreateRemote(kCFAllocatorDefault, reinterpret_cast<CFStringRef>(responce_port_name));
							[responce_port_name release];
							if (!responce_port) throw Exception();
							try {
								winsys->_ipc_clients.Append(client_name, responce_port);
							} catch (...) {
								CFMessagePortInvalidate(responce_port);
								CFRelease(responce_port);
								throw;
							}
						}
					} catch (...) {}
				}
				return 0;
			}
			static NSCursor * _make_cursor_from_selector(SEL selector) { return [[NSCursor class] performSelector: selector]; }
			static NSString * _make_string_item_for_format_list(const FileFormat * formats, int count)
			{
				string name;
				if (count > 1) name = Assembly::GetLocalizedCommonString(201, L"All supported");
				else if (count == 1) name = formats[0].Description;
				else name = Assembly::GetLocalizedCommonString(202, L"All files");
				Array<string> ext_list(0x10);
				for (int i = 0; i < count; i++) for (auto & e : formats[i].Extensions) {
					bool present = false;
					for (auto & ee : ext_list) if (string::CompareIgnoreCase(e, ee) == 0) { present = true; break; }
					if (!present) ext_list << e;
				}
				if (!ext_list.Length()) ext_list << L"*";
				DynamicString result;
				result << name << L" (";
				for (int i = 0; i < ext_list.Length(); i++) {
					auto & e = ext_list[i];
					if (i) result << L";";
					result << L"*." << e;
				}
				result << L")";
				return Cocoa::CocoaString(result.ToString());
			}
		public:
			WindowSystem(void) : _callback(0), _hot_key_tap(0), _hot_key_event_source(0), _file_list_open(0x10), _ipc_server(0), _ipc_event_source(0), _hot_keys(0x10), _main_windows(0x20)
			{
				[NSApplication sharedApplication];
				_delegate = [[ERTApplicationDelegate alloc] init];
				[NSApp setDelegate: _delegate];
				[NSApp disableRelaunchOnLogin];
				_reset_menu();
				@autoreleasepool {
					_arrow = new SystemCursor([NSCursor arrowCursor]);
					_beam = new SystemCursor([NSCursor IBeamCursor]);
					_link = new SystemCursor([NSCursor pointingHandCursor]);
					_size_left_right = new SystemCursor(_make_cursor_from_selector(@selector(_windowResizeEastWestCursor)));
					_size_up_down = new SystemCursor(_make_cursor_from_selector(@selector(_windowResizeNorthSouthCursor)));
					_size_left_up_right_down = new SystemCursor(_make_cursor_from_selector(@selector(_windowResizeNorthWestSouthEastCursor)));
					_size_left_down_right_up = new SystemCursor(_make_cursor_from_selector(@selector(_windowResizeNorthEastSouthWestCursor)));
					_size_all = new SystemCursor([NSCursor openHandCursor]);
					SafePointer<Codec::Frame> null_frame = new Codec::Frame(1, 1, Codec::PixelFormat::R8G8B8A8, Codec::AlphaMode::Straight, Codec::ScanOrigin::TopDown);
					_null = LoadCursor(null_frame);
					if (!_null) throw Exception();
				}
			}
			virtual ~WindowSystem(void) override
			{
				_shutdown_event_tap();
				if (_ipc_event_source) {
					for (auto & client : _ipc_clients.Elements()) {
						CFMessagePortSendRequest(client.value, 1, 0, 0, 0, 0, 0);
						CFMessagePortInvalidate(client.value);
						CFRelease(client.value);
					}
					_ipc_clients.Clear();
					CFRunLoopRemoveSource(CFRunLoopGetMain(), _ipc_event_source, kCFRunLoopCommonModes);
					CFRelease(_ipc_event_source);
					_ipc_event_source = 0;
				}
				if (_ipc_server) {
					CFMessagePortInvalidate(_ipc_server);
					CFRelease(_ipc_server);
					_ipc_server = 0;
				}
				[NSApp setDelegate: 0];
				[_delegate release];
			}
			virtual IWindow * CreateWindow(const CreateWindowDesc & desc) noexcept override
			{
				try {
					auto result = new SystemWindow(desc);
					if (desc.Callback) desc.Callback->Created(result);
					return result;
				} catch (...) { return 0; }
			}
			virtual IWindow * CreateModalWindow(const CreateWindowDesc & desc) noexcept override
			{
				if (desc.ParentWindow && desc.ParentWindow->GetParentWindow() && !static_cast<SystemWindow *>(desc.ParentWindow)->_modal) return 0;
				try {
					auto pw = desc.ParentWindow;
					CreateWindowDesc local_desc = desc;
					local_desc.ParentWindow = 0;
					auto window = new SystemWindow(local_desc, true);
					if (desc.Callback) desc.Callback->Created(window);
					if (pw) {
						auto parent = Cocoa::GetWindowObject(pw);
						auto modal = Cocoa::GetWindowObject(window);
						[parent beginSheet: modal completionHandler: nil];
						return window;
					} else {
						window->Show(true);
						if (window->GetCallback()) window->GetCallback()->Shown(window, true);
						[NSApp runModalForWindow: Cocoa::GetWindowObject(window)];
						window->Destroy();
						return 0;
					}
				} catch (...) { return 0; }
			}
			virtual Box ConvertClientToWindow(const Box & box, uint flags) noexcept override
			{
				SafePointer<IScreen> screen = GetDefaultScreen();
				flags &= ~WindowFlagCocoaEffectBackground;
				auto style = SystemWindow::_style_mask_for_flags(flags);
				auto scale = screen->GetDpiScale();
				auto ssx = double(box.Right - box.Left) / scale;
				auto ssy = double(box.Bottom - box.Top) / scale;
				auto rect = NSMakeRect(0, 0, ssx, ssy);
				auto wrect = [NSWindow frameRectForContentRect: rect styleMask: style];
				auto margin_left = int(-wrect.origin.x * scale);
				auto margin_bottom = int(-wrect.origin.y * scale);
				auto size_x = int(wrect.size.width * scale);
				auto size_y = int(wrect.size.height * scale);
				return Box(box.Left - margin_left, box.Bottom + margin_bottom - size_y, box.Left - margin_left + size_x, box.Bottom + margin_bottom);
			}
			virtual Point ConvertClientToWindow(const Point & size, uint flags) noexcept override { auto box = ConvertClientToWindow(Box(0, 0, size.x, size.y), flags); return Point(box.Right - box.Left, box.Bottom - box.Top); }
			virtual void SetFilesToOpen(const string * files, int num_files) noexcept override { try { for (int i = 0; i < num_files; i++) _file_list_open.Append(files[i]); } catch (...) {} }
			virtual IApplicationCallback * GetCallback(void) noexcept override { return _callback; }
			virtual void SetCallback(IApplicationCallback * callback) noexcept override { _callback = callback; _reset_menu(); }
			virtual void RunMainLoop(void) noexcept override
			{
				if (_callback && _callback->IsHandlerEnabled(ApplicationHandler::OpenExactFile)) {
					for (auto & file : _file_list_open) if (file.Fragment(0, 5) != L"-psn_") {
						try { _callback->OpenExactFile(file); } catch (...) {}
					}
				}
				_file_list_open.Clear();
				[NSApp run];
			}
			virtual void ExitMainLoop(void) noexcept override
			{
				SubmitTask(CreateFunctionalTask([]() {
					@autoreleasepool {
						[NSApp stop: nil];
						auto event = [NSEvent otherEventWithType: NSEventTypeApplicationDefined location: NSMakePoint(0, 0) modifierFlags: 0 timestamp: 0.0 windowNumber: 0 context: 0 subtype: 0 data1: 0 data2: 0];
						[NSApp postEvent: event atStart: NO];
					}
				}));
			}
			virtual void ExitModalSession(IWindow * window) noexcept override
			{
				if (!window || !static_cast<SystemWindow *>(window)->_modal) return;
				auto modal = Cocoa::GetWindowObject(window);
				auto parent = [modal sheetParent];
				if (parent) {
					[parent endSheet: modal];
					window->Destroy();
				} else [NSApp stopModal];
			}
			virtual void RegisterMainWindow(IWindow * window) noexcept override { try { _main_windows.Append(window); } catch (...) {} }
			virtual void UnregisterMainWindow(IWindow * window) noexcept override
			{
				for (int i = 0; i < _main_windows.Length(); i++) if (_main_windows[i] == window) { _main_windows.Remove(i); break; }
				if (!_main_windows.Length()) ExitMainLoop();
			}
			virtual Point GetCursorPosition(void) noexcept override
			{
				@autoreleasepool {
					auto main_screen = CGMainDisplayID();
					auto pos = [NSEvent mouseLocation];
					auto scale = [[NSScreen mainScreen] backingScaleFactor];
					auto rect = CGDisplayBounds(main_screen);
					return Point(pos.x * scale, (rect.size.height - pos.y) * scale);
				}
			}
			virtual void SetCursorPosition(Point position) noexcept override
			{
				@autoreleasepool {
					auto display = CGMainDisplayID();
					auto scale = [[NSScreen mainScreen] backingScaleFactor];
					auto rect = CGDisplayBounds(display);
					CGDisplayMoveCursorToPoint(display, CGPointMake(position.x / scale - rect.origin.x, position.y / scale - rect.origin.y));
				}
			}
			virtual ICursor * LoadCursor(Codec::Frame * source) noexcept override
			{
				try {
					@autoreleasepool {
						auto screen = [NSScreen mainScreen];
						auto scale = [screen backingScaleFactor];
						auto image = Cocoa::CocoaImage(source, scale);
						auto cursor = [[NSCursor alloc] initWithImage: image hotSpot: NSMakePoint(double(source->HotPointX) / scale, double(source->HotPointY) / scale)];
						[image release];
						SafePointer<ICursor> result = new SystemCursor(cursor);
						[cursor release];
						result->Retain();
						return result;
					}
				} catch (...) { return 0; }
			}
			virtual ICursor * GetSystemCursor(SystemCursorClass cursor) noexcept override
			{
				if (cursor == SystemCursorClass::Null) return _null;
				else if (cursor == SystemCursorClass::Arrow) return _arrow;
				else if (cursor == SystemCursorClass::Beam) return _beam;
				else if (cursor == SystemCursorClass::Link) return _link;
				else if (cursor == SystemCursorClass::SizeLeftRight) return _size_left_right;
				else if (cursor == SystemCursorClass::SizeUpDown) return _size_up_down;
				else if (cursor == SystemCursorClass::SizeLeftUpRightDown) return _size_left_up_right_down;
				else if (cursor == SystemCursorClass::SizeLeftDownRightUp) return _size_left_down_right_up;
				else if (cursor == SystemCursorClass::SizeAll) return _size_all;
				else return 0;
			}
			virtual void SetCursor(ICursor * cursor) noexcept override { auto hcursor = (NSCursor *) cursor->GetOSHandle(); [hcursor set]; }
			virtual Array<Point> * GetApplicationIconSizes(void) noexcept override
			{
				try {
					SafePointer< Array<Point> > result = new Array<Point>(0x1);
					auto scale = [[NSScreen mainScreen] backingScaleFactor];
					auto size = [[NSApp dockTile] size];
					result->Append(Point(size.width * scale, size.height * scale));
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			virtual void SetApplicationIcon(Codec::Image * icon) noexcept override
			{
				auto scale = [[NSScreen mainScreen] backingScaleFactor];
				auto size = [[NSApp dockTile] size];
				auto frame = icon->GetFrameBestSizeFit(size.width * scale, size.height * scale);
				auto image = Cocoa::CocoaImage(frame);
				[NSApp setApplicationIconImage: image];
				[image release];
			}
			virtual void SetApplicationBadge(const string & text) noexcept override { auto txt = Cocoa::CocoaString(text); [[NSApp dockTile] setBadgeLabel: txt]; [txt release]; }
			virtual void SetApplicationIconVisibility(bool visible) noexcept override { [NSApp setActivationPolicy: visible ? NSApplicationActivationPolicyRegular : NSApplicationActivationPolicyAccessory]; }
			virtual bool OpenFileDialog(OpenFileInfo * info, IWindow * parent, IDispatchTask * on_exit) noexcept override
			{
				if (parent && parent->GetParentWindow() && !static_cast<SystemWindow *>(parent)->_modal) return false;
				@autoreleasepool {
					if (on_exit) on_exit->Retain();
					auto panel = [NSOpenPanel openPanel];
					auto delegate = [[ERTOpenSaveDelegate alloc] init];
					delegate->panel = panel;
					delegate->formats = &info->Formats;
					delegate->selected = &info->DefaultFormat;
					delegate->save = false;
					[panel retain];
					[panel setCanChooseFiles: YES];
					[panel setCanChooseDirectories: NO];
					[panel setAllowsMultipleSelection: info->MultiChoose ? YES : NO];
					[panel setCanCreateDirectories: YES];
					[panel setAllowsOtherFileTypes: YES];
					[panel setDelegate: delegate];
					if (info->Formats.Length()) {
						auto view = [[NSPopUpButton alloc] initWithFrame: NSMakeRect(10.0, 0.0, 400.0, 26.0) pullsDown: NO];
						auto & formats = info->Formats;
						if (formats.Length() > 1) {
							auto descr = _make_string_item_for_format_list(formats.GetBuffer(), formats.Length());
							[view addItemWithTitle: descr];
							[descr release];
						}
						for (auto & f : formats) {
							auto descr = _make_string_item_for_format_list(&f, 1);
							[view addItemWithTitle: descr];
							[descr release];
						}
						auto all = _make_string_item_for_format_list(0, 0);
						[view addItemWithTitle: all];
						[all release];
						[view selectItemAtIndex: (formats.Length() > 1) ? 1 + info->DefaultFormat : max(info->DefaultFormat, 0)];
						[view setAction: @selector(panelFormatChanged:)];
						[view setTarget: delegate];
						auto exts = [[NSMutableArray<NSString *> alloc] init];
						if (info->DefaultFormat == -1) {
							for (auto & f : formats) for (auto & e : f.Extensions) { auto nse = Cocoa::CocoaString(e); [exts addObject: nse]; [nse release]; }
						} else {
							for (auto & e : formats[info->DefaultFormat].Extensions) { auto nse = Cocoa::CocoaString(e); [exts addObject: nse]; [nse release]; }
						}
						[panel setAllowedFileTypes: exts];
						[exts release];
						[panel setAccessoryView: view];
						[view release];
					}
					if (info->Title.Length()) {
						auto title = Cocoa::CocoaString(info->Title);
						[panel setTitle: title];
						[title release];
					}
					void (^handler) (NSModalResponse) = ^(NSModalResponse responce)
					{
						info->Files.Clear();
						if (responce == NSModalResponseOK) {
							auto * urls = [panel URLs];
							for (int i = 0; i < [urls count]; i++) {
								auto url = [urls objectAtIndex: i];
								info->Files << Cocoa::EngineString([url path]);
							}
						}
						[panel orderOut: nil];
						[panel release];
						[delegate release];
						if (on_exit) {
							if (parent) SubmitTask(on_exit); else on_exit->DoTask(this);
							on_exit->Release();
						}
					};
					if (parent) [panel beginSheetModalForWindow: Cocoa::GetWindowObject(parent) completionHandler: handler];
					else handler([panel runModal]);
				}
				return true;
			}
			virtual bool SaveFileDialog(SaveFileInfo * info, IWindow * parent, IDispatchTask * on_exit) noexcept override
			{
				if (parent && parent->GetParentWindow() && !static_cast<SystemWindow *>(parent)->_modal) return false;
				@autoreleasepool {
					if (on_exit) on_exit->Retain();
					auto panel = [NSSavePanel savePanel];
					auto delegate = [[ERTOpenSaveDelegate alloc] init];
					delegate->panel = panel;
					delegate->formats = &info->Formats;
					delegate->selected = &info->Format;
					delegate->save = true;
					[panel retain];
					[panel setCanCreateDirectories: YES];
					[panel setAllowsOtherFileTypes: info->AppendExtension ? NO : YES];
					[panel setDelegate: delegate];
					NSPopUpButton * view = 0;
					if (info->Formats.Length()) {
						view = [[NSPopUpButton alloc] initWithFrame: NSMakeRect(10.0, 0.0, 300.0, 26.0) pullsDown: NO];
						auto & formats = info->Formats;
						for (auto & f : formats) {
							auto descr = _make_string_item_for_format_list(&f, 1);
							[view addItemWithTitle: descr];
							[descr release];
						}
						int index = min(max(info->Format, 0), formats.Length() - 1);
						[view selectItemAtIndex: index];
						[view setAction: @selector(panelFormatChanged:)];
						[view setTarget: delegate];
						auto exts = [[NSMutableArray<NSString *> alloc] init];
						for (auto & e : formats[index].Extensions) { auto nse = Cocoa::CocoaString(e); [exts addObject: nse]; [nse release]; }
						[panel setAllowedFileTypes: exts];
						[exts release];
						[panel setAccessoryView: view];
						[view release];
					}
					if (info->Title.Length()) {
						auto title = Cocoa::CocoaString(info->Title);
						[panel setTitle: title];
						[title release];
					}
					void (^handler) (NSModalResponse) = ^(NSModalResponse responce)
					{
						if (responce == NSModalResponseOK) {
							info->File = Cocoa::EngineString([[panel URL] path]);
							info->Format = view ? [view indexOfSelectedItem] : 0;
						} else info->File = L"";
						[panel orderOut: nil];
						[panel release];
						[delegate release];
						if (on_exit) {
							if (parent) SubmitTask(on_exit); else on_exit->DoTask(this);
							on_exit->Release();
						}
					};
					if (parent) [panel beginSheetModalForWindow: Cocoa::GetWindowObject(parent) completionHandler: handler];
					else handler([panel runModal]);
				}
				return true;
			}
			virtual bool ChooseDirectoryDialog(ChooseDirectoryInfo * info, IWindow * parent, IDispatchTask * on_exit) noexcept override
			{
				@autoreleasepool {
					if (parent && parent->GetParentWindow() && !static_cast<SystemWindow *>(parent)->_modal) return false;
					if (on_exit) on_exit->Retain();
					auto panel = [NSOpenPanel openPanel];
					[panel retain];
					[panel setCanChooseFiles: NO];
					[panel setCanChooseDirectories: YES];
					[panel setAllowsMultipleSelection: NO];
					[panel setCanCreateDirectories: YES];
					if (info->Title.Length()) {
						auto title = Engine::Cocoa::CocoaString(info->Title);
						[panel setTitle: title];
						[title release];
					}
					void (^handler) (NSModalResponse) = ^(NSModalResponse responce)
					{
						if (responce == NSModalResponseOK) info->Directory = Engine::Cocoa::EngineString([[panel URL] path]);
						else info->Directory = L"";
						[panel orderOut: nil];
						[panel release];
						if (on_exit) {
							if (parent) SubmitTask(on_exit); else on_exit->DoTask(this);
							on_exit->Release();
						}
					};
					if (parent) [panel beginSheetModalForWindow: Cocoa::GetWindowObject(parent) completionHandler: handler];
					else handler([panel runModal]);
				}
				return true;
			}
			virtual bool MessageBox(MessageBoxResult * result, const string & text, const string & title, IWindow * parent, MessageBoxButtonSet buttons, MessageBoxStyle style, IDispatchTask * on_exit) noexcept override
			{
				if (parent && parent->GetParentWindow() && !static_cast<SystemWindow *>(parent)->_modal) return false;
				if (on_exit) on_exit->Retain();
				auto alert = [[NSAlert alloc] init];
				{
					auto ns_text = Engine::Cocoa::CocoaString(text);
					auto ns_title = Engine::Cocoa::CocoaString(title);
					[alert setMessageText: ns_title];
					[alert setInformativeText: ns_text];
					[ns_text release];
					[ns_title release];
				}
				NSString * text_0 = 0, * text_1 = 0, * text_2 = 0;
				if (buttons == MessageBoxButtonSet::Ok) {
					text_1 = Cocoa::CocoaString(Assembly::GetLocalizedCommonString(101, L"OK"));
				} else if (buttons == MessageBoxButtonSet::OkCancel) {
					text_1 = Cocoa::CocoaString(Assembly::GetLocalizedCommonString(101, L"OK"));
					text_2 = Cocoa::CocoaString(Assembly::GetLocalizedCommonString(102, L"Cancel"));
				} else if (buttons == MessageBoxButtonSet::YesNo) {
					text_1 = Cocoa::CocoaString(Assembly::GetLocalizedCommonString(103, L"Yes"));
					text_2 = Cocoa::CocoaString(Assembly::GetLocalizedCommonString(104, L"No"));
				} else if (buttons == MessageBoxButtonSet::YesNoCancel) {
					text_0 = Cocoa::CocoaString(Assembly::GetLocalizedCommonString(102, L"Cancel"));
					text_1 = Cocoa::CocoaString(Assembly::GetLocalizedCommonString(103, L"Yes"));
					text_2 = Cocoa::CocoaString(Assembly::GetLocalizedCommonString(104, L"No"));
				}
				if (text_0) [alert addButtonWithTitle: text_0];
				if (text_2) [alert addButtonWithTitle: text_2];
				if (text_1) [alert addButtonWithTitle: text_1];
				[text_0 release]; [text_1 release]; [text_2 release];
				if (style == MessageBoxStyle::Error) [alert setAlertStyle: NSAlertStyleCritical];
				else if (style == MessageBoxStyle::Warning) [alert setAlertStyle: NSAlertStyleWarning];
				else if (style == MessageBoxStyle::Information) [alert setAlertStyle: NSAlertStyleInformational];
				void (^handler) (NSModalResponse) = ^(NSModalResponse responce)
				{
					if (result) {
						if (buttons == MessageBoxButtonSet::Ok) {
							*result = MessageBoxResult::Ok;
						} else if (buttons == MessageBoxButtonSet::OkCancel) {
							*result = MessageBoxResult::Ok;
							if (responce == NSAlertFirstButtonReturn) *result = MessageBoxResult::Cancel;
						} else if (buttons == MessageBoxButtonSet::YesNo) {
							*result = MessageBoxResult::Yes;
							if (responce == NSAlertFirstButtonReturn) *result = MessageBoxResult::No;
						} else if (buttons == MessageBoxButtonSet::YesNoCancel) {
							*result = MessageBoxResult::Yes;
							if (responce == NSAlertSecondButtonReturn) *result = MessageBoxResult::No;
							else if (responce == NSAlertFirstButtonReturn) *result = MessageBoxResult::Cancel;
						}
					}
					[alert release];
					if (on_exit) {
						if (parent) SubmitTask(on_exit); else on_exit->DoTask(this);
						on_exit->Release();
					}
				};
				if (parent) [alert beginSheetModalForWindow: Cocoa::GetWindowObject(parent) completionHandler: handler];
				else handler([alert runModal]);
				return true;
			}
			virtual IMenu * CreateMenu(void) noexcept override { try { return new SystemMenu(); } catch (...) { return 0; } }
			virtual IMenuItem * CreateMenuItem(void) noexcept override { try { return new SystemMenuItem(); } catch (...) { return 0; } }
			virtual Point GetUserNotificationIconSize(void) noexcept override { return Point(0, 0); }
			virtual void PushUserNotification(const string & title, const string & text, Codec::Image * icon) noexcept override
			{
				@autoreleasepool {
					auto center = [NSUserNotificationCenter defaultUserNotificationCenter];
					auto notification = [[NSUserNotification alloc] init];
					auto ns_title = Cocoa::CocoaString(title);
					auto ns_text = Cocoa::CocoaString(text);
					[ns_title autorelease];
					[ns_text autorelease];
					[notification autorelease];
					[notification setTitle: ns_title];
					[notification setInformativeText: ns_text];
					[center deliverNotification: notification];
				}
			}
			virtual IStatusBarIcon * CreateStatusBarIcon(void) noexcept override { try { return new SystemStatusBarIcon(); } catch (...) { return 0; } }
			virtual bool CreateHotKey(int event_id, int key_code, uint key_flags) noexcept override
			{
				if (key_code == KeyCodes::Shift || key_code == KeyCodes::RightShift || key_code == KeyCodes::LeftShift) return false;
				if (key_code == KeyCodes::Control || key_code == KeyCodes::RightControl || key_code == KeyCodes::LeftControl) return false;
				if (key_code == KeyCodes::Alternative || key_code == KeyCodes::RightAlternative || key_code == KeyCodes::LeftAlternative) return false;
				if (key_code == KeyCodes::System || key_code == KeyCodes::RightSystem || key_code == KeyCodes::LeftSystem) return false;
				KeyboardHotKeyDesc desc;
				desc.hotkey_id = event_id;
				desc.key_code = key_code;
				desc.key_flags = key_flags;
				try { _hot_keys.Append(desc); } catch (...) { return false; }
				if (_hot_keys.Length() == 1) {
					if (!_setup_event_tap()) {
						_hot_keys.RemoveLast();
						return false;
					}
				}
				return true;
			}
			virtual void RemoveHotKey(int event_id) noexcept override
			{
				for (int i = 0; i < _hot_keys.Length(); i++) if (_hot_keys[i].hotkey_id == event_id) {
					_hot_keys.Remove(i);
					break;
				}
				if (!_hot_keys.Length()) _shutdown_event_tap();
			}
			virtual bool LaunchIPCServer(const string & app_id, const string & auth_id) noexcept override
			{
				try {
					if (_ipc_server) return false;
					::Boolean unused;
					CFMessagePortContext context;
					context.copyDescription = 0;
					context.info = 0;
					context.release = 0;
					context.retain = 0;
					context.version = 0;
					auto ipc_server_name = Cocoa::CocoaString(auth_id + L"." + app_id);
					_ipc_server = CFMessagePortCreateLocal(kCFAllocatorDefault, reinterpret_cast<CFStringRef>(ipc_server_name), _ipc_server_callback, &context, &unused);
					[ipc_server_name release];
					if (!_ipc_server) return false;
					_ipc_event_source = CFMessagePortCreateRunLoopSource(kCFAllocatorDefault, _ipc_server, 0);
					if (!_ipc_event_source) {
						CFMessagePortInvalidate(_ipc_server);
						CFRelease(_ipc_server);
						return false;
					}
					CFRunLoopAddSource(CFRunLoopGetMain(), _ipc_event_source, kCFRunLoopCommonModes);
					return true;
				} catch (...) { return false; }
			}
			virtual IIPCClient * CreateIPCClient(const string & server_app_id, const string & server_auth_id) noexcept override { try { return new SystemIPCClient(server_auth_id + L"." + server_app_id); } catch (...) { return 0; } }
			virtual void SubmitTask(IDispatchTask * task) override
			{
				@autoreleasepool {
					auto modes = [NSArray<NSRunLoopMode> arrayWithObject: NSRunLoopCommonModes];
					if (!modes) return;
					auto object = [[ERTTask alloc] init];
					if (object) {
						object->task = task;
						task->Retain();
						[object performSelectorOnMainThread: @selector(executeDispatchTask) withObject: nil waitUntilDone: NO modes: modes];
					}
				}
			}
			virtual void BeginSubmit(void) override {}
			virtual void AppendTask(IDispatchTask * task) override { SubmitTask(task); }
			virtual void EndSubmit(void) override {}
		};

		SafePointer<WindowSystem> CommonWindowSystem;
		ObjectArray<IScreen> * GetActiveScreens(void)
		{
			GetWindowSystem();
			@autoreleasepool {
				try {
					SafePointer< ObjectArray<IScreen> > result = new ObjectArray<IScreen>(0x10);
					auto screens = [NSScreen screens];
					for (int i = 0; i < [screens count]; i++) {
						auto screen = [screens objectAtIndex: i];
						SafePointer<IScreen> obj = new SystemScreen(screen);
						result->Append(obj);
					}
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
		}
		IScreen * GetDefaultScreen(void)
		{
			@autoreleasepool {
				GetWindowSystem();
				return new (std::nothrow) SystemScreen([NSScreen mainScreen]);
			}
		}
		ITheme * GetCurrentTheme(void) { GetWindowSystem(); return new (std::nothrow) SystemTheme([NSApp effectiveAppearance]); }
		IWindowSystem * GetWindowSystem(void) { if (!CommonWindowSystem) CommonWindowSystem = new WindowSystem; return CommonWindowSystem; }
	}
	namespace Cocoa
	{
		void SetWindowTouchBar(Windows::IWindow * window, Object * object) { static_cast<Windows::SystemWindow *>(window)->_touch_bar.SetRetain(object); }
		Object * GetWindowTouchBar(Windows::IWindow * window) { return static_cast<Windows::SystemWindow *>(window)->_touch_bar; }
		NSWindow * GetWindowObject(Windows::ICoreWindow * window) { return static_cast<Windows::SystemWindow *>(window)->_window; }
		NSView * GetWindowCoreView(Windows::ICoreWindow * window) { return static_cast<Windows::SystemWindow *>(window)->_view; }
		Windows::IWindowCallback * GetWindowCallback(Windows::ICoreWindow * window) { return static_cast<Windows::SystemWindow *>(window)->_callback; }
		void SetWindowRenderCallback(Windows::ICoreWindow * window, RenderLayerCallback callback)
		{
			auto wnd = static_cast<Windows::SystemWindow *>(window);
			if (callback) {
				wnd->_layer_callback = callback;
				wnd->_user_layer_callback = false;
			} else {
				wnd->_layer_callback = 0;
				wnd->_user_layer_callback = true;
			}
		}
		void UnsetWindowRenderCallback(Windows::ICoreWindow * window)
		{
			auto wnd = static_cast<Windows::SystemWindow *>(window);
			wnd->_layer_callback = 0;
			wnd->_user_layer_callback = false;
		}
		bool SetWindowFullscreenMode(Windows::ICoreWindow * window, bool switch_on)
		{
			auto wnd = static_cast<Windows::SystemWindow *>(window);
			if (switch_on) {
				if (!wnd->_fullscreen) {
					if (!wnd->IsMaximized()) [wnd->_window toggleFullScreen: nil];
					wnd->_fullscreen = true;
					[NSApp setPresentationOptions: NSApplicationPresentationHideDock | NSApplicationPresentationHideMenuBar];
				}
			} else {
				if (wnd->_fullscreen) {
					[wnd->_window toggleFullScreen: nil]; 
					wnd->_fullscreen = false;
				}
			}
			return true;
		}
		bool IsWindowInFullscreenMode(Windows::ICoreWindow * window) { auto wnd = static_cast<Windows::SystemWindow *>(window); return wnd->_fullscreen; }
		bool WindowNeedsAlphaBackbuffer(Windows::ICoreWindow * window) { auto wnd = static_cast<Windows::SystemWindow *>(window); return wnd->_alpha_backbuffer; }
		CGDirectDisplayID GetDirectDisplayID(Windows::IScreen * screen)
		{
			auto scrn = static_cast<Windows::SystemScreen *>(screen);
			return scrn->GetDisplayID();
		}
	}
}

@implementation ERTTask
	- (instancetype) init { [super init]; task = 0; return self; }
	- (void) dealloc { if (task) task->Release(); [super dealloc]; }
	- (void) executeDispatchTask { if (task) task->DoTask(Engine::Windows::GetWindowSystem()); [self release]; }
@end
@implementation ERTApplicationDelegate
	- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication *) sender { [self applicationClose: sender]; return NSTerminateCancel; }
	- (void) application: (NSApplication *) application openURLs: (NSArray<NSURL *> *) urls
	{
		auto callback = Engine::Windows::GetWindowSystem()->GetCallback();
		if (callback && callback->IsHandlerEnabled(Engine::Windows::ApplicationHandler::OpenExactFile)) {
			for (int i = 0; i < [urls count]; i++) {
				NSURL * url = [urls objectAtIndex: i];
				if ([[url scheme] compare: @"file"] == NSOrderedSame) callback->OpenExactFile(Engine::Cocoa::EngineString([url path]));
				else callback->OpenExactFile(Engine::Cocoa::EngineString([url absoluteString]));
			}
		}
	}
	- (BOOL) applicationOpenUntitledFile: (NSApplication *) sender
	{
		auto callback = Engine::Windows::GetWindowSystem()->GetCallback();
		if (callback && callback->IsHandlerEnabled(Engine::Windows::ApplicationHandler::CreateFile)) {
			callback->CreateNewFile();
			return YES;
		} return NO;
	}
	- (void) applicationClose: (id) sender
	{
		auto callback = Engine::Windows::GetWindowSystem()->GetCallback();
		if (callback && callback->IsHandlerEnabled(Engine::Windows::ApplicationHandler::Terminate)) {
			callback->Terminate();
		} else {
			try {
				Engine::Array<NSWindow *> windows_send(0x10);
				auto windows = [NSApp windows];
				for (int i = 0; i < [windows count]; i++) windows_send.Append([windows objectAtIndex: i]);
				for (auto & window : windows_send) if (![window parentWindow]) [NSApp sendAction: @selector(performClose:) to: window from: self];
			} catch (...) {}
		}
	}
	- (void) applicationCreateNewFile: (id) sender
	{
		auto callback = Engine::Windows::GetWindowSystem()->GetCallback();
		if (callback && callback->IsHandlerEnabled(Engine::Windows::ApplicationHandler::CreateFile)) callback->CreateNewFile();
	}
	- (void) applicationOpenFile: (id) sender
	{
		auto callback = Engine::Windows::GetWindowSystem()->GetCallback();
		if (callback && callback->IsHandlerEnabled(Engine::Windows::ApplicationHandler::OpenSomeFile)) callback->OpenSomeFile();
	}
	- (void) applicationShowProperties: (id) sender
	{
		auto callback = Engine::Windows::GetWindowSystem()->GetCallback();
		if (callback && callback->IsHandlerEnabled(Engine::Windows::ApplicationHandler::ShowProperties)) callback->ShowProperties();
	}
	- (void) applicationShowHelp: (id) sender
	{
		auto callback = Engine::Windows::GetWindowSystem()->GetCallback();
		if (callback && callback->IsHandlerEnabled(Engine::Windows::ApplicationHandler::ShowHelp)) callback->ShowHelp();
	}
	- (void) applicationShowAbout: (id) sender
	{
		auto callback = Engine::Windows::GetWindowSystem()->GetCallback();
		if (callback && callback->IsHandlerEnabled(Engine::Windows::ApplicationHandler::ShowAbout)) callback->ShowAbout();
	}
@end
@implementation ERTWindowDelegate
	- (BOOL) windowShouldClose: (NSWindow *) sender
	{
		auto callback = owner->GetCallback();
		if (callback) callback->WindowClose(owner);
		return NO;
	}
	- (void) windowDidBecomeKey: (NSNotification *) notification
	{
		auto window = static_cast<Engine::Windows::SystemWindow *>(owner);
		if (window) {
			[window->_window makeFirstResponder: window->_view];
			if (window->_callback) window->_callback->FocusChanged(window, true);
		}
	}
	- (void) windowDidResignKey: (NSNotification *) notification
	{
		Engine::Cocoa::KeyboardStatus.Clear();
		auto window = static_cast<Engine::Windows::SystemWindow *>(owner);
		if (window) {
			window->_view->flag_states = 0;
			if (window->_callback) {
				if (window->_captured) window->_callback->CaptureChanged(window, false);
				window->_captured = false;
				window->_callback->FocusChanged(window, false);
			}
		}
	}
	- (void) windowWillBeginSheet: (NSNotification *) notification
	{
		auto window = static_cast<Engine::Windows::SystemWindow *>(owner);
		if (window) window->_show_children(false);
	}
	- (void) windowDidEndSheet: (NSNotification *) notification
	{
		auto window = static_cast<Engine::Windows::SystemWindow *>(owner);
		if (window) window->_show_children(true);
	}
	- (void) windowDidMiniaturize: (NSNotification *) notification
	{
		auto callback = owner->GetCallback();
		if (callback) callback->WindowMinimize(owner);
	}
	- (void) windowDidDeminiaturize: (NSNotification *) notification
	{
		auto callback = owner->GetCallback();
		if (callback) callback->WindowRestore(owner);
	}
	- (void) windowDidEnterFullScreen: (NSNotification *) notification
	{
		auto callback = owner->GetCallback();
		if (callback) callback->WindowMaximize(owner);
	}
	- (void) windowDidExitFullScreen: (NSNotification *) notification
	{
		auto window = static_cast<Engine::Windows::SystemWindow *>(owner);
		if (window) {
			window->_fullscreen = false;
			if ([window->_window isKeyWindow]) [NSApp setPresentationOptions: NSApplicationPresentationDefault];
			if (window->_callback) window->_callback->WindowRestore(owner);
		}
	}
	- (void) windowDidBecomeMain: (NSNotification *) notification
	{
		auto window = static_cast<Engine::Windows::SystemWindow *>(owner);
		if (window) {
			if (window->_fullscreen) [NSApp setPresentationOptions: NSApplicationPresentationHideDock | NSApplicationPresentationHideMenuBar];
			else [NSApp setPresentationOptions: NSApplicationPresentationDefault];
			if (window->_callback) window->_callback->WindowActivate(window);
		}
	}
	- (void) windowDidResignMain: (NSNotification *) notification
	{
		auto callback = owner->GetCallback();
		if (callback) callback->WindowDeactivate(owner);
	}
	- (void) windowDidMove: (NSNotification *) notification
	{
		auto wnd = static_cast<Engine::Windows::SystemWindow *>(owner);
		auto rect = [wnd->_window frame];
		auto left = rect.origin.x;
		auto top = rect.origin.y + rect.size.height;
		auto width = rect.size.width;
		auto height = rect.size.height;
		if (left != wnd->_last_left || top != wnd->_last_top) if (wnd->_callback) wnd->_callback->WindowMove(wnd);
		if (width != wnd->_last_width || height != wnd->_last_height) if (wnd->_callback) wnd->_callback->WindowSize(wnd);
		wnd->_last_left = left;
		wnd->_last_top = top;
		wnd->_last_width = width;
		wnd->_last_height = height;
	}
	- (void) windowDidResize: (NSNotification *) notification { [self windowDidMove: notification]; }
	- (BOOL) validateMenuItem: (NSMenuItem *) menuItem
	{
		auto action = [menuItem action];
		Engine::Windows::WindowHandler handler;
		if (action == @selector(engineWindowSave:)) handler = Engine::Windows::WindowHandler::Save;
		else if (action == @selector(engineWindowSaveAs:)) handler = Engine::Windows::WindowHandler::SaveAs;
		else if (action == @selector(engineWindowExport:)) handler = Engine::Windows::WindowHandler::Export;
		else if (action == @selector(engineWindowPrint:)) handler = Engine::Windows::WindowHandler::Print;
		else if (action == @selector(engineWindowUndo:)) handler = Engine::Windows::WindowHandler::Undo;
		else if (action == @selector(engineWindowRedo:)) handler = Engine::Windows::WindowHandler::Redo;
		else if (action == @selector(engineWindowCut:)) handler = Engine::Windows::WindowHandler::Cut;
		else if (action == @selector(engineWindowCopy:)) handler = Engine::Windows::WindowHandler::Copy;
		else if (action == @selector(engineWindowPaste:)) handler = Engine::Windows::WindowHandler::Paste;
		else if (action == @selector(engineWindowDuplicate:)) handler = Engine::Windows::WindowHandler::Duplicate;
		else if (action == @selector(engineWindowDelete:)) handler = Engine::Windows::WindowHandler::Delete;
		else if (action == @selector(engineWindowFind:)) handler = Engine::Windows::WindowHandler::Find;
		else if (action == @selector(engineWindowReplace:)) handler = Engine::Windows::WindowHandler::Replace;
		else if (action == @selector(engineWindowSelectAll:)) handler = Engine::Windows::WindowHandler::SelectAll;
		else return YES;
		auto callback = owner->GetCallback();
		return callback ? callback->IsWindowEventEnabled(owner, handler) : NO;
	}
	- (void) engineWindowSave: (id) sender
	{
		auto callback = owner->GetCallback();
		if (callback) callback->HandleWindowEvent(owner, Engine::Windows::WindowHandler::Save);
	}
	- (void) engineWindowSaveAs: (id) sender
	{
		auto callback = owner->GetCallback();
		if (callback) callback->HandleWindowEvent(owner, Engine::Windows::WindowHandler::SaveAs);
	}
	- (void) engineWindowExport: (id) sender
	{
		auto callback = owner->GetCallback();
		if (callback) callback->HandleWindowEvent(owner, Engine::Windows::WindowHandler::Export);
	}
	- (void) engineWindowPrint: (id) sender
	{
		auto callback = owner->GetCallback();
		if (callback) callback->HandleWindowEvent(owner, Engine::Windows::WindowHandler::Print);
	}
	- (void) engineWindowUndo: (id) sender
	{
		auto callback = owner->GetCallback();
		if (callback) callback->HandleWindowEvent(owner, Engine::Windows::WindowHandler::Undo);
	}
	- (void) engineWindowRedo: (id) sender
	{
		auto callback = owner->GetCallback();
		if (callback) callback->HandleWindowEvent(owner, Engine::Windows::WindowHandler::Redo);
	}
	- (void) engineWindowCut: (id) sender
	{
		auto callback = owner->GetCallback();
		if (callback) callback->HandleWindowEvent(owner, Engine::Windows::WindowHandler::Cut);
	}
	- (void) engineWindowCopy: (id) sender
	{
		auto callback = owner->GetCallback();
		if (callback) callback->HandleWindowEvent(owner, Engine::Windows::WindowHandler::Copy);
	}
	- (void) engineWindowPaste: (id) sender
	{
		auto callback = owner->GetCallback();
		if (callback) callback->HandleWindowEvent(owner, Engine::Windows::WindowHandler::Paste);
	}
	- (void) engineWindowDuplicate: (id) sender
	{
		auto callback = owner->GetCallback();
		if (callback) callback->HandleWindowEvent(owner, Engine::Windows::WindowHandler::Duplicate);
	}
	- (void) engineWindowDelete: (id) sender
	{
		auto callback = owner->GetCallback();
		if (callback) callback->HandleWindowEvent(owner, Engine::Windows::WindowHandler::Delete);
	}
	- (void) engineWindowFind: (id) sender
	{
		auto callback = owner->GetCallback();
		if (callback) callback->HandleWindowEvent(owner, Engine::Windows::WindowHandler::Find);
	}
	- (void) engineWindowReplace: (id) sender
	{
		auto callback = owner->GetCallback();
		if (callback) callback->HandleWindowEvent(owner, Engine::Windows::WindowHandler::Replace);
	}
	- (void) engineWindowSelectAll: (id) sender
	{
		auto callback = owner->GetCallback();
		if (callback) callback->HandleWindowEvent(owner, Engine::Windows::WindowHandler::SelectAll);
	}
@end
@implementation ERTPopupWindow
	- (BOOL) canBecomeKeyWindow { return YES; }
	- (void) resignKeyWindow { [[self delegate] windowDidResignKey: nil]; }
@end
@implementation ERTView
	- (instancetype) init
	{
		[super init];
		owner = 0;
		time_left_down = time_right_down = 0;
		time_double_click = uint([NSEvent doubleClickInterval] * 1000.0);
		last_position = last_click = Engine::Point(0, 0);
		flag_mouse = 0;
		flag_states = 0;
		input_context = [[NSTextInputContext alloc] initWithClient: self];
		return self;
	}
	- (void) dealloc
	{
		[input_context release];
		[super dealloc];
	}
	- (void) drawRect: (NSRect) dirtyRect
	{
		if (owner) {
			auto wnd = static_cast<Engine::Windows::SystemWindow *>(owner);
			if (wnd->_user_layer_callback) { if (wnd->_callback) wnd->_callback->RenderWindow(wnd); }
			else if (wnd->_layer_callback) wnd->_layer_callback(wnd->_engine, wnd);
		}
	}
	- (BOOL) acceptsFirstMouse: (NSEvent *) event { return YES; }
	- (NSView *) hitTest: (NSPoint) point
	{
		auto frame = [self frame];
		if (point.x >= frame.origin.x && point.y >= frame.origin.y && point.x < frame.origin.x + frame.size.width && point.y < frame.origin.y + frame.size.height) return self;
		else return nil;
	}
	- (void) setFrame: (NSRect) frame
	{
		[super setFrame: frame];
		if (owner) {
			auto wnd = static_cast<Engine::Windows::SystemWindow *>(owner);
			auto scale = wnd->GetDpiScale();
			if (wnd->_engine) wnd->_engine->Resize(frame.size.width * scale, frame.size.height * scale);
		}
	}
	- (void) setFrameSize: (NSSize) newSize
	{
		[super setFrameSize: newSize];
		if (owner) {
			auto wnd = static_cast<Engine::Windows::SystemWindow *>(owner);
			auto scale = wnd->GetDpiScale();
			if (wnd->_engine) wnd->_engine->Resize(newSize.width * scale, newSize.height * scale);
		}
	}
	- (void) mouseMoved: (NSEvent *) event
	{
		if ([[self window] isKeyWindow] && owner) {
			auto wnd = static_cast<Engine::Windows::SystemWindow *>(owner);
			auto position = wnd->_view_cocoa_to_engine(wnd->_window_to_view(wnd->_screen_to_window([NSEvent mouseLocation])));
			if (position != last_position || !(flag_mouse & 0x01)) {
				last_position = position;
				flag_mouse |= 0x01;
				if (wnd->_callback) {
					wnd->_callback->SetCursor(wnd, position);
					wnd->_callback->MouseMove(wnd, position);
				}
			}
		}
	}
	- (void) mouseDragged: (NSEvent *) event
	{
		if ([[self window] isKeyWindow] && owner) {
			auto wnd = static_cast<Engine::Windows::SystemWindow *>(owner);
			auto position = wnd->_view_cocoa_to_engine(wnd->_window_to_view(wnd->_screen_to_window([NSEvent mouseLocation])));
			if (position != last_position || !(flag_mouse & 0x01)) {
				last_position = position;
				flag_mouse |= 0x01;
				if (wnd->_callback) {
					wnd->_callback->SetCursor(wnd, position);
					wnd->_callback->MouseMove(wnd, position);
				}
			}
		}
	}
	- (void) mouseDown: (NSEvent *) event
	{
		if ([[self window] isKeyWindow] && owner) {
			auto wnd = static_cast<Engine::Windows::SystemWindow *>(owner);
			auto position = wnd->_view_cocoa_to_engine(wnd->_window_to_view(wnd->_screen_to_window([NSEvent mouseLocation])));
			if (position != last_position || !(flag_mouse & 0x01)) [self mouseMoved: event];
			auto time = Engine::GetTimerValue();
			if ((flag_mouse & 0x02) && (time - time_left_down) < time_double_click && position == last_click) {
				flag_mouse &= ~0x02;
				if (wnd->_callback) wnd->_callback->LeftButtonDoubleClick(wnd, position);
			} else {
				flag_mouse |= 0x02;
				time_left_down = time;
				if (wnd->_callback) wnd->_callback->LeftButtonDown(wnd, position);
			}
			last_click = position;
		}
	}
	- (void) mouseUp: (NSEvent *) event
	{
		if ([[self window] isKeyWindow] && owner) {
			auto wnd = static_cast<Engine::Windows::SystemWindow *>(owner);
			auto position = wnd->_view_cocoa_to_engine(wnd->_window_to_view(wnd->_screen_to_window([NSEvent mouseLocation])));
			if (position != last_position || !(flag_mouse & 0x01)) [self mouseMoved: event];
			if (wnd->_callback) wnd->_callback->LeftButtonUp(wnd, position);
		}
	}
	- (void) rightMouseDragged: (NSEvent *) event
	{
		if ([[self window] isKeyWindow] && owner) {
			auto wnd = static_cast<Engine::Windows::SystemWindow *>(owner);
			auto position = wnd->_view_cocoa_to_engine(wnd->_window_to_view(wnd->_screen_to_window([NSEvent mouseLocation])));
			if (position != last_position || !(flag_mouse & 0x01)) {
				last_position = position;
				flag_mouse |= 0x01;
				if (wnd->_callback) {
					wnd->_callback->SetCursor(wnd, position);
					wnd->_callback->MouseMove(wnd, position);
				}
			}
		}
	}
	- (void) rightMouseDown: (NSEvent *) event
	{
		if ([[self window] isKeyWindow] && owner) {
			auto wnd = static_cast<Engine::Windows::SystemWindow *>(owner);
			auto position = wnd->_view_cocoa_to_engine(wnd->_window_to_view(wnd->_screen_to_window([NSEvent mouseLocation])));
			if (position != last_position || !(flag_mouse & 0x01)) [self mouseMoved: event];
			auto time = Engine::GetTimerValue();
			if ((flag_mouse & 0x04) && (time - time_right_down) < time_double_click && position == last_click) {
				flag_mouse &= ~0x04;
				if (wnd->_callback) wnd->_callback->RightButtonDoubleClick(wnd, position);
			} else {
				flag_mouse |= 0x04;
				time_right_down = time;
				if (wnd->_callback) wnd->_callback->RightButtonDown(wnd, position);
			}
			last_click = position;
		}
	}
	- (void) rightMouseUp: (NSEvent *) event
	{
		if ([[self window] isKeyWindow] && owner) {
			auto wnd = static_cast<Engine::Windows::SystemWindow *>(owner);
			auto position = wnd->_view_cocoa_to_engine(wnd->_window_to_view(wnd->_screen_to_window([NSEvent mouseLocation])));
			if (position != last_position || !(flag_mouse & 0x01)) [self mouseMoved: event];
			if (wnd->_callback) wnd->_callback->RightButtonUp(wnd, position);
		}
	}
	- (void) keyDown: (NSEvent *) event
	{
		bool dead;
		auto key = static_cast<Engine::KeyCodes::Key>(Engine::Cocoa::EngineKeyCode([event keyCode], dead));
		if (Engine::Keyboard::IsKeyPressed(Engine::KeyCodes::Control) || Engine::Keyboard::IsKeyPressed(Engine::KeyCodes::Alternative) || Engine::Keyboard::IsKeyPressed(Engine::KeyCodes::System)) dead = true;
		if (!Engine::Cocoa::KeyboardStatus.GetElementByKey(key)) Engine::Cocoa::KeyboardStatus.Append(key, true);
		Engine::Windows::IWindowCallback * callback;
		if (key && owner && (callback = owner->GetCallback())) {
			if (!callback->KeyDown(owner, key) && !dead) {
				Engine::ImmutableString text;
				if (key == Engine::KeyCodes::Tab) text = L"\t";
				else if (key == Engine::KeyCodes::Space) text = L" ";
				else text = Engine::Cocoa::EngineString([event characters]);
				for (int i = 0; i < text.Length(); i++) callback->CharDown(owner, text[i]);
			}
		}
	}
	- (void) keyUp: (NSEvent *) event
	{
		bool dead;
		auto key = static_cast<Engine::KeyCodes::Key>(Engine::Cocoa::EngineKeyCode([event keyCode], dead));
		Engine::Cocoa::KeyboardStatus.Remove(key);
		Engine::Windows::IWindowCallback * callback;
		if (key && owner && (callback = owner->GetCallback())) callback->KeyUp(owner, key);
	}
	- (void) flagsChanged: (NSEvent *) event
	{
		bool shift = Engine::Keyboard::IsKeyPressed(Engine::KeyCodes::Shift);
		bool control = Engine::Keyboard::IsKeyPressed(Engine::KeyCodes::Control);
		bool alternative = Engine::Keyboard::IsKeyPressed(Engine::KeyCodes::Alternative);
		bool system = Engine::Keyboard::IsKeyPressed(Engine::KeyCodes::System);
		Engine::Windows::IWindowCallback * callback;
		if (owner && (callback = owner->GetCallback())) {
			if ((shift && !(flag_states & 1)) || (!shift && (flag_states & 1))) {
				if (shift) callback->KeyDown(owner, Engine::KeyCodes::Shift);
				else callback->KeyUp(owner, Engine::KeyCodes::Shift);
			}
			if ((control && !(flag_states & 2)) || (!control && (flag_states & 2))) {
				if (control) callback->KeyDown(owner, Engine::KeyCodes::Control);
				else callback->KeyUp(owner, Engine::KeyCodes::Control);
			}
			if ((alternative && !(flag_states & 4)) || (!alternative && (flag_states & 4))) {
				if (alternative) callback->KeyDown(owner, Engine::KeyCodes::Alternative);
				else callback->KeyUp(owner, Engine::KeyCodes::Alternative);
			}
			if ((system && !(flag_states & 8)) || (!system && (flag_states & 8))) {
				if (system) callback->KeyDown(owner, Engine::KeyCodes::System);
				else callback->KeyUp(owner, Engine::KeyCodes::System);
			}
		}
		flag_states = (shift ? 1 : 0) | (control ? 2 : 0) | (alternative ? 4 : 0) | (system ? 8 : 0);
	}
	- (void) scrollWheel: (NSEvent *) event
	{
		Engine::Windows::IWindowCallback * callback;
		if (owner && (callback = owner->GetCallback())) {
			auto dx = -[event deltaX];
			auto dy = -[event deltaY];
			if (dx) callback->ScrollHorizontally(owner, dx);
			if (dy) callback->ScrollVertically(owner, dy);
		}
	}
	- (void) viewDidChangeEffectiveAppearance { auto window = static_cast<Engine::Windows::SystemWindow *>(owner); if (window && window->_callback) window->_callback->ThemeChanged(window); }
	- (BOOL) hasMarkedText { return NO; }
	- (NSRange) markedRange { return { NSNotFound, 0 }; }
	- (NSRange) selectedRange { return { NSNotFound, 0 }; }
	- (void) setMarkedText: (id) string selectedRange: (NSRange) selectedRange replacementRange: (NSRange) replacementRange {}
	- (void) unmarkText {}
	- (NSArray<NSAttributedStringKey> *) validAttributesForMarkedText { return [NSArray<NSAttributedStringKey> array]; }
	- (NSAttributedString *) attributedSubstringForProposedRange: (NSRange) range actualRange: (NSRangePointer) actualRange { auto result = [[NSAttributedString alloc] initWithString: [NSString string]]; [result autorelease]; return result; }
	- (void) insertText: (id) string replacementRange: (NSRange) replacementRange
	{
		Engine::ImmutableString text;
		if ([string respondsToSelector: @selector(isEqualToAttributedString:)]) {
			NSString * str = [string string];
			text = Engine::Cocoa::EngineString(str);
		} else {
			NSString * str = string;
			text = Engine::Cocoa::EngineString(str);
		}
		Engine::Windows::IWindowCallback * callback;
		if (text.Length() && owner && (callback = owner->GetCallback())) for (int i = 0; i < text.Length(); i++) callback->CharDown(owner, text[i]);
	}
	- (NSUInteger) characterIndexForPoint: (NSPoint) point { return NSNotFound; }
	- (NSRect) firstRectForCharacterRange: (NSRange) range actualRange: (NSRangePointer) actualRange { return NSMakeRect(0.0, 0.0, 0.0, 0.0); }
	- (void) doCommandBySelector: (SEL) selector {}
	- (void) engineTimerEvent: (NSTimer *) timer
	{
		auto data = (ERTTimer *) [timer userInfo];
		Engine::Windows::IWindowCallback * callback;
		if (owner && (callback = owner->GetCallback())) callback->Timer(owner, data->timer_id);
	}
	- (BOOL) acceptsFirstResponder { return YES; }
	- (NSObject *) inputContext { return input_context; }
@end
@implementation ERTTimer
@end
@implementation ERTOpenSaveDelegate
	- (void) panelFormatChanged: (id) sender
	{
		if (save) {
			int index = [(NSPopUpButton *) [panel accessoryView] indexOfSelectedItem];
			NSMutableArray<NSString *> * exts = [[NSMutableArray<NSString *> alloc] init];
			for (int j = 0; j < formats->ElementAt(index).Extensions.Length(); j++) {
				NSString * ext = Engine::Cocoa::CocoaString(formats->ElementAt(index).Extensions[j]);
				[exts addObject: ext];
				[ext release];
			}
			[panel setAllowedFileTypes: exts];
			[exts release];
		} else {
			int index = [(NSPopUpButton *) [panel accessoryView] indexOfSelectedItem];
			if (index > formats->Length()) {
				[panel setAllowedFileTypes: nil];
			} else if (index <= 0) {
				NSMutableArray<NSString *> * exts = [[NSMutableArray<NSString *> alloc] init];
				for (int i = 0; i < formats->Length(); i++) for (int j = 0; j < formats->ElementAt(i).Extensions.Length(); j++) {
					NSString * ext = Engine::Cocoa::CocoaString(formats->ElementAt(i).Extensions[j]);
					[exts addObject: ext];
					[ext release];
				}
				[panel setAllowedFileTypes: exts];
				[exts release];
			} else {
				NSMutableArray<NSString *> * exts = [[NSMutableArray<NSString *> alloc] init];
				for (int j = 0; j < formats->ElementAt(index - 1).Extensions.Length(); j++) {
					NSString * ext = Engine::Cocoa::CocoaString(formats->ElementAt(index - 1).Extensions[j]);
					[exts addObject: ext];
					[ext release];
				}
				[panel setAllowedFileTypes: exts];
				[exts release];
			}
		}
	}
@end
@implementation ERTMenuDelegate
	- (void) menuItemSelected: (id) sender
	{
		auto item = reinterpret_cast<Engine::Windows::SystemMenuItem *>([(NSMenuItem *) sender tag]);
		if (icon) {
			auto callback = icon->GetCallback();
			if (callback && item->GetID()) callback->StatusIconCommand(icon, item->GetID());
		} else collected_responce = item->GetID();
	}
	- (void) menuWillOpen: (NSMenu *) menu
	{
		double width = 0.0;
		for (int i = 0; i < owner->Length(); i++) {
			auto hitem = static_cast<Engine::Windows::SystemMenuItem *>(owner->ElementAt(i));
			hitem->Highlight(false);
			auto lw = hitem->GetElementWidth();
			if (lw > width) width = lw;
		}
		for (int i = 0; i < owner->Length(); i++) {
			auto hitem = static_cast<Engine::Windows::SystemMenuItem *>(owner->ElementAt(i));
			hitem->SetActualWidth(width);
		}
	}
	- (void) menuDidClose: (NSMenu *) menu
	{
		for (int i = 0; i < owner->Length(); i++) {
			auto item = owner->ElementAt(i);
			auto callback = item->GetCallback();
			if (callback) callback->MenuClosed(item);
		}
	}
	- (void) menu: (NSMenu *) menu willHighlightItem: (NSMenuItem *) item
	{
		for (int i = 0; i < owner->Length(); i++) {
			auto hitem = static_cast<Engine::Windows::SystemMenuItem *>(owner->ElementAt(i));
			hitem->Highlight(hitem->GetOSHandle() == item);
		}
	}
	- (BOOL) worksWhenModal { return YES; }
@end
@implementation ERTMenuItemView
	- (instancetype) init
	{
		[super init];
		try { device = new Engine::Cocoa::QuartzDeviceContext; } catch (...) { device = 0; }
		return self;
	}
	- (void) dealloc
	{
		if (device) device->Release();
		[super dealloc];
	}
	- (void) drawRect : (NSRect) dirtyRect
	{
		auto callback = item->GetCallback();
		auto size = [self frame].size;
		auto scale = [[self window] backingScaleFactor];
		auto context = [[NSGraphicsContext currentContext] CGContext];
		device->SetContext(context, int(size.width * scale), int(size.height * scale), (scale > 1.5) ? 2 : 1);
		if (callback) callback->RenderMenuItem(item, device, Engine::Box(0, 0, int(size.width * scale), int(size.height * scale)), static_cast<Engine::Windows::SystemMenuItem *>(item)->IsHighlighted());
	}
	- (void) mouseUp: (NSEvent *) event
	{
		if (item->IsEnabled() && !item->IsSeparator() && !item->GetSubmenu()) {
			auto hitem = reinterpret_cast<Engine::Windows::SystemMenuItem *>(item)->GetOSHandle();
			auto menu = [hitem menu];
			while ([menu supermenu]) menu = [menu supermenu];
			[menu cancelTracking];
			[[menu delegate] performSelector: @selector(menuItemSelected:) withObject: hitem];
		}
	}
	- (void) viewDidMoveToWindow
	{
		if ([self window]) {
			auto super_size = [[self superview] frame].size;
			auto self_size = [self frame].size;
			[self setFrameSize: NSMakeSize(super_size.width, self_size.height)];
			auto delegate = (ERTMenuDelegate *) [[reinterpret_cast<Engine::Windows::SystemMenuItem *>(item)->GetOSHandle() menu] delegate];
			if (delegate->icon) [[self window] becomeKeyWindow];
		} else [self setFrameSize: ref_size];
	}
@end
@implementation ERTStatusBarDelegate
	- (void) statusIconSelected: (id) sender
	{
		auto callback = icon->GetCallback();
		if (callback) callback->StatusIconCommand(icon, icon->GetEventID());
	}
@end