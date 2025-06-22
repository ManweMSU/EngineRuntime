#include "MetalGraphics.h"

#include "SystemWindowsAPI.h"
#include "MetalDevice.h"
#include "CocoaInterop.h"
#include "../Interfaces/SystemIPC.h"
#include "../Interfaces/Process.h"
#include "../Storage/Archive.h"

#include <atomic>
#import "objc/runtime.h"
@import IOSurface;

using namespace Engine;
using namespace Engine::Graphics;

@interface ERTMetalView : NSView<CALayerDelegate>
	{
	@public
		Engine::Windows::ICoreWindow * window_ref;
		CAMetalLayer * layer;
		double w, h;
		bool autosize;
	}
	- (instancetype) init;
	- (void) dealloc;
	- (void) displayLayer: (CALayer *) layer;
	- (void) setFrame: (NSRect) frame;
	- (void) setFrameSize: (NSSize) newSize;
@end
@implementation ERTMetalView
	- (instancetype) init
	{
		@autoreleasepool {
			[super init];
			layer = [CAMetalLayer layer];
			[layer setDelegate: self];
			[self setLayer: layer];
			[self setWantsLayer: YES];
			w = h = 1.0;
			autosize = false;
			return self;
		}
	}
	- (void) dealloc { [super dealloc]; }
	- (void) displayLayer: (CALayer *) layer
	{
		@autoreleasepool {
			auto callback = Engine::Cocoa::GetWindowCallback(window_ref);
			if (callback) callback->RenderWindow(static_cast<Engine::Windows::IWindow *>(window_ref));
		}
	}
	- (void) setFrame: (NSRect) frame
	{
		[super setFrame: frame];
		if (autosize) {
			double scale = [[self window] backingScaleFactor];
			w = max(frame.size.width * scale, 1.0); h = max(frame.size.height * scale, 1.0);
			[layer setDrawableSize: NSMakeSize(w, h)];
		}
		[layer setNeedsDisplay];
	}
	- (void) setFrameSize: (NSSize) newSize
	{
		[super setFrameSize: newSize];
		if (autosize) {
			double scale = [[self window] backingScaleFactor];
			w = max(newSize.width * scale, 1.0); h = max(newSize.height * scale, 1.0);
			[layer setDrawableSize: NSMakeSize(w, h)];
		}
		[layer setNeedsDisplay];
	}
@end
@interface ERTXPCHookEx : NSCoder
	{
	@public
		mach_port_t port;
		bool port_set;
	}
	- (instancetype) init;
	- (void) encodeXPCObject: (xpc_object_t) xpcObject forKey: (NSString *) key;
	- (xpc_object_t) decodeXPCObjectOfType: (xpc_type_t) type forKey: (NSString *) key;
	- (NSXPCConnection *) getConnection;
@end
@implementation ERTXPCHookEx
	- (instancetype) init { [super init]; port = 0; port_set = false; return self; }
	- (BOOL) isKindOfClass: (Class) aClass { return (aClass == [NSXPCCoder class]); }
	- (void) encodeObject: (id) object forKey: (NSString *) key
	{
		if ([object class] == [IOSurface class] && [key compare: @"ioSurface"] == NSOrderedSame) {
			if (!port_set) {
				port = IOSurfaceCreateMachPort((IOSurfaceRef) object);
				port_set = true;
			}
		}
	}
	- (void) encodeXPCObject: (xpc_object_t) xpcObject forKey: (NSString *) key { @throw [NSException exceptionWithName: @"ERTXPCHookExException" reason: @"Unsupported selector call" userInfo: nil]; }
	- (xpc_object_t) decodeXPCObjectOfType: (xpc_type_t) type forKey: (NSString *) key { @throw [NSException exceptionWithName: @"ERTXPCHookExException" reason: @"Unsupported selector call" userInfo: nil]; }
	- (NSXPCConnection *) getConnection { @throw [NSException exceptionWithName: @"ERTXPCHookExException" reason: @"Unsupported selector call" userInfo: nil]; }
@end
@interface ERTXPCHookIn : NSCoder
	{
	@public
		mach_port_t port;
	}
	- (instancetype) init;
	- (void) encodeXPCObject: (xpc_object_t) xpcObject forKey: (NSString *) key;
	- (xpc_object_t) decodeXPCObjectOfType: (xpc_type_t) type forKey: (NSString *) key;
	- (NSXPCConnection *) getConnection;
@end
@implementation ERTXPCHookIn
	- (instancetype) init { [super init]; port = 0; return self; }
	- (BOOL) isKindOfClass: (Class) aClass { return (aClass == [NSXPCCoder class]); }
	- (id) decodeObjectOfClass: (Class) aClass forKey: (NSString *) key
	{
		if ([key compare: @"ioSurface"] == NSOrderedSame) {
			if (aClass == [IOSurface class]) {
				auto surface = (id) IOSurfaceLookupFromMachPort(port);
				if (!surface) @throw [NSException exceptionWithName: @"ERTXPCHookInException" reason: @"Invalid IOSurface port number" userInfo: nil];
				return surface;
			} else @throw [NSException exceptionWithName: @"ERTXPCHookInException" reason: @"Invalid expected field type" userInfo: nil];
		} else return nil;
	}
	- (void) encodeXPCObject: (xpc_object_t) xpcObject forKey: (NSString *) key { @throw [NSException exceptionWithName: @"ERTXPCHookInException" reason: @"Unsupported selector call" userInfo: nil]; }
	- (xpc_object_t) decodeXPCObjectOfType: (xpc_type_t) type forKey: (NSString *) key { @throw [NSException exceptionWithName: @"ERTXPCHookInException" reason: @"Unsupported selector call" userInfo: nil]; }
	- (NSXPCConnection *) getConnection { @throw [NSException exceptionWithName: @"ERTXPCHookInException" reason: @"Unsupported selector call" userInfo: nil]; }
@end

namespace Engine
{
	namespace MetalGraphics
	{
		MetalPresentationEngine::MetalPresentationEngine(void) : _root_view(0), _metal_view(0) {}
		MetalPresentationEngine::~MetalPresentationEngine(void) { [_metal_view release]; }
		void MetalPresentationEngine::Attach(Windows::ICoreWindow * window)
		{
			auto view = [[ERTMetalView alloc] init];
			if (view) view->window_ref = window;
			_metal_view = view;
			_root_view = Cocoa::GetWindowCoreView(window);
			Cocoa::UnsetWindowRenderCallback(window);
			[_root_view addSubview: _metal_view];
			[_root_view setWantsLayer: YES];
			[_metal_view setFrameSize: [_root_view frame].size];
			[_metal_view setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
		}
		void MetalPresentationEngine::Detach(void)
		{
			if (_metal_view) {
				[_metal_view removeFromSuperview];
				[_metal_view release];
				[_root_view setWantsLayer: NO];
				_metal_view = _root_view = 0;
			}
		}
		void MetalPresentationEngine::Invalidate(void) { if (_metal_view) [reinterpret_cast<ERTMetalView *>(_metal_view)->layer setNeedsDisplay]; }
		void MetalPresentationEngine::Resize(int width, int height) {}
		I2DDeviceContext * MetalPresentationEngine::GetContext(void) noexcept { return 0; }
		bool MetalPresentationEngine::BeginRenderingPass(void) noexcept { return false; }
		bool MetalPresentationEngine::EndRenderingPass(void) noexcept { return false; }
		id<MTLDevice> MetalPresentationEngine::GetDevice(void) { if (_metal_view) return [reinterpret_cast<ERTMetalView *>(_metal_view)->layer device]; else return 0; }
		void MetalPresentationEngine::SetDevice(id<MTLDevice> device) { if (_metal_view) [reinterpret_cast<ERTMetalView *>(_metal_view)->layer setDevice: device]; }
		MTLPixelFormat MetalPresentationEngine::GetPixelFormat(void) { if (_metal_view) return [reinterpret_cast<ERTMetalView *>(_metal_view)->layer pixelFormat]; else return MTLPixelFormatInvalid; }
		void MetalPresentationEngine::SetPixelFormat(MTLPixelFormat format) { if (_metal_view) [reinterpret_cast<ERTMetalView *>(_metal_view)->layer setPixelFormat: format]; }
		Point MetalPresentationEngine::GetSize(void) { if (_metal_view) return Point(reinterpret_cast<ERTMetalView *>(_metal_view)->w, reinterpret_cast<ERTMetalView *>(_metal_view)->h); else return Point(1, 1); }
		void MetalPresentationEngine::SetSize(Point size) { if (_metal_view) [reinterpret_cast<ERTMetalView *>(_metal_view)->layer setDrawableSize: NSMakeSize(size.x, size.y)]; }
		void MetalPresentationEngine::SetAutosize(bool autosize)
		{
			if (_metal_view) {
				auto view = reinterpret_cast<ERTMetalView *>(_metal_view);
				view->autosize = autosize;
				if (autosize) {
					auto size = [view frame].size;
					double scale = [[view window] backingScaleFactor];
					view->w = max(size.width * scale, 1.0);
					view->h = max(size.height * scale, 1.0);
					[view->layer setDrawableSize: NSMakeSize(view->w, view->h)];
					[view->layer setNeedsDisplay];
				}
			}
		}
		void MetalPresentationEngine::SetOpaque(bool opaque) { if (_metal_view) [reinterpret_cast<ERTMetalView *>(_metal_view)->layer setOpaque: opaque]; }
		void MetalPresentationEngine::SetFramebufferOnly(bool framebuffer_only) { if (_metal_view) [reinterpret_cast<ERTMetalView *>(_metal_view)->layer setFramebufferOnly: framebuffer_only]; }
		void MetalPresentationEngine::SetExtendedDynamicRange(bool edr) { if (_metal_view) [reinterpret_cast<ERTMetalView *>(_metal_view)->layer setWantsExtendedDynamicRangeContent: edr]; }
		id<CAMetalDrawable> MetalPresentationEngine::GetDrawable(void) { if (_metal_view) return [reinterpret_cast<ERTMetalView *>(_metal_view)->layer nextDrawable]; else return 0; }

		class MTL_DeviceResourceHandle : public IDeviceResourceHandle
		{
		public:
			uint64 _device_id;
			SafePointer<DataBlock> _metal_handle_data;
			SafePointer<DataBlock> _mutex_path;
		public:
			MTL_DeviceResourceHandle(uint64 device_id, DataBlock * metal_handle, DataBlock * mutex_path) noexcept : _device_id(device_id) { _metal_handle_data.SetRetain(metal_handle); _mutex_path.SetRetain(mutex_path); }
			MTL_DeviceResourceHandle(const DataBlock * data)
			{
				if (!data || data->Length() < 16) throw InvalidFormatException();
				_device_id = reinterpret_cast<const uint64 *>(data->GetBuffer())[0];
				auto length1 = reinterpret_cast<const uint32 *>(data->GetBuffer())[2];
				auto length2 = reinterpret_cast<const uint32 *>(data->GetBuffer())[3];
				if (length1 > 0x10000 || length2 > 0x10000) throw InvalidFormatException();
				if (length1 + length2 + 16 != data->Length()) throw InvalidFormatException();
				_metal_handle_data = new DataBlock(1);
				_mutex_path = new DataBlock(1);
				_metal_handle_data->SetLength(length1);
				_mutex_path->SetLength(length2);
				MemoryCopy(_metal_handle_data->GetBuffer(), data->GetBuffer() + 16, length1);
				MemoryCopy(_mutex_path->GetBuffer(), data->GetBuffer() + 16 + length1, length2);
			}
			virtual ~MTL_DeviceResourceHandle(void) override {}
			virtual uint64 GetDeviceIdentifier(void) noexcept override { return _device_id; }
			virtual DataBlock * Serialize(void) noexcept override
			{
				try {
					auto length1 = _metal_handle_data->Length();
					auto length2 = _mutex_path->Length();
					SafePointer<DataBlock> result = new DataBlock(1);
					result->SetLength(length1 + length2 + 16);
					reinterpret_cast<uint64 *>(result->GetBuffer())[0] = _device_id;
					reinterpret_cast<uint32 *>(result->GetBuffer())[2] = length1;
					reinterpret_cast<uint32 *>(result->GetBuffer())[3] = length2;
					MemoryCopy(result->GetBuffer() + 16, _metal_handle_data->GetBuffer(), length1);
					MemoryCopy(result->GetBuffer() + 16 + length1, _mutex_path->GetBuffer(), length2);
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			virtual string ToString(void) const override { return L"MTL_DeviceResourceHandle"; }
		};
		class MTL_SharedInfrastructure : public Object
		{
		public:
			struct SharedTextureInfo
			{
				uint type, format, usage, size;
				uint width, height, depth, __unused0;
				union { std::atomic_flag lock; uint64 __unused1; };
			};
		public:
			SafePointer<MTL_DeviceResourceHandle> _handle;
			SafePointer<IPC::ISharedMemory> _meta_memory;
			NSMachPort * _ns_shared_port;
			mach_port_t _shared_port;
			SharedTextureInfo * _meta;
		public:
			MTL_SharedInfrastructure(ITexture * texture, MTLSharedTextureHandle * handle, uint64 device_id) : _ns_shared_port(0)
			{
				auto pid_name = string(uint32(getpid()), HexadecimalBaseLowerCase, 8);
				auto addr_name = string(uint64(reinterpret_cast<uintptr>(this)), HexadecimalBaseLowerCase, 16);
				auto public_port_name = FormatString(L"com.EngineSoftware.shared-texture-%0-%1", pid_name, addr_name);
				auto public_memory_name = FormatString(L"stex_%0_%1", pid_name, addr_name);
				@autoreleasepool {
					auto hook = [[ERTXPCHookEx alloc] init];
					if (!hook) throw OutOfMemoryException();
					@try { [handle encodeWithCoder: hook]; } @catch (NSException *exception) {} @finally {}
					if (!hook->port_set) { [hook release]; throw Exception(); }
					_shared_port = hook->port;
					[hook release];
					auto ns_public_name = Cocoa::CocoaString(public_port_name);
					if (!ns_public_name) { mach_port_deallocate(mach_task_self(), _shared_port); throw OutOfMemoryException(); }
					auto register_status = [[NSMachBootstrapServer sharedInstance] registerPort: [NSMachPort portWithMachPort: _shared_port options: NSMachPortDeallocateNone] name: ns_public_name];
					[ns_public_name release];
					if (!register_status) { mach_port_deallocate(mach_task_self(), _shared_port); throw OutOfMemoryException(); }
					try {
						IPC::DestroySharedMemory(public_memory_name);
						_meta_memory = IPC::CreateSharedMemory(public_memory_name, sizeof(SharedTextureInfo), IPC::SharedMemoryCreateNew);
						if (!_meta_memory) throw OutOfMemoryException();
						if (!_meta_memory->Map(reinterpret_cast<void **>(&_meta), IPC::SharedMemoryMapReadWrite)) throw OutOfMemoryException();
					} catch (...) {
						mach_port_deallocate(mach_task_self(), _shared_port);
						throw Exception();
					}
				}
				try {
					SafePointer<DataBlock> port_name_ascii = public_port_name.EncodeSequence(Encoding::ANSI, true);
					SafePointer<DataBlock> memory_name_ascii = public_memory_name.EncodeSequence(Encoding::ANSI, true);
					_handle = new MTL_DeviceResourceHandle(device_id, port_name_ascii, memory_name_ascii);
				} catch (...) {
					_meta_memory->Unmap();
					mach_port_deallocate(mach_task_self(), _shared_port);
					throw;
				}
				_meta->type = uint(texture->GetTextureType());
				_meta->format = uint(texture->GetPixelFormat());
				_meta->usage = texture->GetResourceUsage();
				_meta->size = texture->GetArraySize();
				_meta->width = texture->GetWidth();
				_meta->height = texture->GetHeight();
				_meta->depth = texture->GetDepth();
				_meta->__unused0 = 0;
				_meta->__unused1 = 0;
				_meta->lock.clear();
			}
			MTL_SharedInfrastructure(MTL_DeviceResourceHandle * source)
			{
				_handle.SetRetain(source);
				auto public_port_name = string(source->_metal_handle_data->GetBuffer(), source->_metal_handle_data->Length(), Encoding::ANSI);
				auto public_memory_name = string(source->_mutex_path->GetBuffer(), source->_mutex_path->Length(), Encoding::ANSI);
				auto ns_public_name = Cocoa::CocoaString(public_port_name);
				if (!ns_public_name) throw OutOfMemoryException();
				_ns_shared_port = [[NSMachBootstrapServer sharedInstance] portForName: ns_public_name];
				[ns_public_name release];
				if (!_ns_shared_port) throw Exception();
				_shared_port = [_ns_shared_port machPort];
				_meta_memory = IPC::CreateSharedMemory(public_memory_name, sizeof(SharedTextureInfo), IPC::SharedMemoryOpenExisting);
				if (!_meta_memory) { [_ns_shared_port release]; throw Exception(); }
				if (!_meta_memory->Map(reinterpret_cast<void **>(&_meta), IPC::SharedMemoryMapReadWrite)) { [_ns_shared_port release]; throw Exception(); }
			}
			virtual ~MTL_SharedInfrastructure(void) override
			{
				_meta_memory->Unmap();
				if (_ns_shared_port) [_ns_shared_port release];
				else mach_port_deallocate(mach_task_self(), _shared_port);
			}
			virtual string ToString(void) const override { return L"MTL_SharedInfrastructure"; }
		};
		class MTL_Buffer : public IBuffer
		{
			IDevice * wrapper;
		public:
			id<MTLBuffer> buffer;
			uint32 size, usage;

			MTL_Buffer(IDevice * _wrapper) : wrapper(_wrapper), buffer(0), size(0), usage(0) {}
			virtual ~MTL_Buffer(void) override { [buffer release]; }
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual ResourceType GetResourceType(void) noexcept override { return ResourceType::Buffer; }
			virtual ResourceMemoryPool GetMemoryPool(void) noexcept override { return ResourceMemoryPool::Default; }
			virtual uint32 GetResourceUsage(void) noexcept override { return usage; }
			virtual uint32 GetLength(void) noexcept override { return size; }
			virtual string ToString(void) const override { return L"MTL_Buffer"; }
		};
		class MTL_Texture : public ITexture
		{
			IDevice * wrapper;
		public:
			id<MTLTexture> texture;
			SafePointer<MTL_SharedInfrastructure> shared;
			TextureType type;
			PixelFormat format;
			uint32 usage, width, height, depth, size;
			uint32 rtv_mip, rtv_slice, rtv_depth;

			MTL_Texture(IDevice * _wrapper) : wrapper(_wrapper), texture(0), format(PixelFormat::Invalid), usage(0), width(0), height(0), depth(0), size(0), rtv_mip(0), rtv_slice(0), rtv_depth(0) {}
			virtual ~MTL_Texture(void) override { [texture release]; }
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual ResourceType GetResourceType(void) noexcept override { return ResourceType::Texture; }
			virtual ResourceMemoryPool GetMemoryPool(void) noexcept override { return shared ? ResourceMemoryPool::Shared : ResourceMemoryPool::Default; }
			virtual uint32 GetResourceUsage(void) noexcept override { return usage; }
			virtual TextureType GetTextureType(void) noexcept override { return type; }
			virtual PixelFormat GetPixelFormat(void) noexcept override { return format; }
			virtual uint32 GetWidth(void) noexcept override { return width; }
			virtual uint32 GetHeight(void) noexcept override { return height; }
			virtual uint32 GetDepth(void) noexcept override { return depth; }
			virtual uint32 GetMipmapCount(void) noexcept override { return [texture mipmapLevelCount]; }
			virtual uint32 GetArraySize(void) noexcept override { return size; }
			virtual string ToString(void) const override { return L"MTL_Texture"; }
			bool SharedTextureInit(void) noexcept
			{
				auto handle = [texture newSharedTextureHandle];
				if (!handle) return false;
				try { shared = new MTL_SharedInfrastructure(this, handle, wrapper->GetDeviceIdentifier()); } catch (...) { [handle release]; return false; }
				[handle release];
				return true;
			}
		};
		class MTL_SamplerState : public ISamplerState
		{
			IDevice * wrapper;
		public:
			id<MTLSamplerState> state;

			MTL_SamplerState(IDevice * _wrapper) : wrapper(_wrapper), state(0) {}
			virtual ~MTL_SamplerState(void) override { [state release]; }
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual string ToString(void) const override { return L"MTL_SamplerState"; }
		};
		class MTL_Shader : public IShader
		{
			IDevice * wrapper;
		public:
			id<MTLFunction> func;

			MTL_Shader(IDevice * _wrapper) : wrapper(_wrapper), func(0) {}
			virtual ~MTL_Shader(void) override { [func release]; }
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual string GetName(void) noexcept override { return Cocoa::EngineString(func.name); }
			virtual ShaderType GetType(void) noexcept override
			{
				if (func.functionType == MTLFunctionTypeVertex) return ShaderType::Vertex;
				else if (func.functionType == MTLFunctionTypeFragment) return ShaderType::Pixel;
				else return ShaderType::Unknown;
			}
			virtual string ToString(void) const override { return L"MTL_Shader"; }
		};
		class MTL_ShaderLibrary : public IShaderLibrary
		{
			IDevice * wrapper;
		public:
			id<MTLLibrary> ref;

			MTL_ShaderLibrary(IDevice * _wrapper) : wrapper(_wrapper), ref(0) {}
			virtual ~MTL_ShaderLibrary(void) override { [ref release]; }
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual Array<string> * GetShaderNames(void) noexcept override
			{
				try {
					SafePointer< Array<string> > result = new Array<string>(0x10);
					for (int i = 0; i < [ref.functionNames count]; i++) result->Append(Cocoa::EngineString([ref.functionNames objectAtIndex: i]));
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			virtual IShader * CreateShader(const string & name) noexcept override
			{
				SafePointer<MTL_Shader> result = new (std::nothrow) MTL_Shader(wrapper);
				NSString * obj_name = Cocoa::CocoaString(name);
				result->func = [ref newFunctionWithName: obj_name];
				[obj_name release];
				if (!result->func) return 0;
				result->Retain();
				return result;
			}
			virtual string ToString(void) const override { return L"MTL_ShaderLibrary"; }
		};
		class MTL_PipelineState : public IPipelineState
		{
			IDevice * wrapper;
		public:
			id<MTLRenderPipelineState> state;
			id<MTLDepthStencilState> ds_state;
			MTLTriangleFillMode fill_mode;
			MTLCullMode cull_mode;
			MTLWinding front_winding;
			MTLPrimitiveType primitive_type;
			int depth_bias;
			float depth_bias_clamp;
			float slope_scaled_depth_bias;
			MTLDepthClipMode depth_clip;

			MTL_PipelineState(IDevice * _wrapper) : wrapper(_wrapper), state(0), ds_state(0) {}
			virtual ~MTL_PipelineState(void) override { [state release]; [ds_state release]; }
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual string ToString(void) const override { return L"MTL_PipelineState"; }
		};
		class MTL_DeviceContext : public IDeviceContext
		{
			SafePointer<I2DDeviceContext> device_2d;
			SafePointer<MTL_Buffer> index_buffer;
			MTLIndexType index_buffer_format;
			MTLPrimitiveType primitive_type;
			IDevice * wrapper;
		public:
			id<MTLDevice> device;
			id<MTLCommandQueue> queue;
			id<MTLCommandBuffer> current_command;
			id<MTLRenderCommandEncoder> render_command_encoder;
			id<MTLBlitCommandEncoder> blit_command_encoder;
			NSAutoreleasePool * autorelease_pool;
			uint32 state;
			bool error_state;

			MTL_DeviceContext(IDevice * _wrapper, id<MTLDevice> _device, id<MTLCommandQueue> _queue) : wrapper(_wrapper), device(_device), queue(_queue),
				current_command(0), state(0), autorelease_pool(0), render_command_encoder(0), blit_command_encoder(0)
			{
				[device retain];
				[queue retain];
			}
			virtual ~MTL_DeviceContext(void) override
			{
				[autorelease_pool release];
				[device release];
				[queue release];
				[current_command release];
			}
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual bool BeginRenderingPass(uint32 rtc, const RenderTargetViewDesc * rtv, const DepthStencilViewDesc * dsv) noexcept override
			{
				if (state) return false;
				if (!rtc || !rtv) return false;
				autorelease_pool = [[NSAutoreleasePool alloc] init];
				if (!current_command) {
					current_command = [queue commandBuffer];
					if (!current_command) {
						[autorelease_pool release];
						autorelease_pool = 0;
						return false;
					}
					[current_command retain];
				}
				MTLRenderPassDescriptor * desc = [MTLRenderPassDescriptor renderPassDescriptor];
				for (uint i = 0; i < rtc; i++) {
					if (!rtv[i].Texture || !(rtv[i].Texture->GetResourceUsage() & ResourceUsageRenderTarget)) {
						[autorelease_pool release];
						autorelease_pool = 0;
						return false;
					}
					MTLRenderPassColorAttachmentDescriptor * ca = [[MTLRenderPassColorAttachmentDescriptor alloc] init];
					[ca autorelease];
					ca.texture = GetInnerMetalTexture(rtv[i].Texture);
					auto mtl_texture = static_cast<MTL_Texture *>(rtv[i].Texture);
					ca.level = mtl_texture->rtv_mip;
					ca.slice = mtl_texture->rtv_slice;
					ca.depthPlane = mtl_texture->rtv_depth;
					if (rtv[i].LoadAction == TextureLoadAction::Clear) {
						ca.loadAction = MTLLoadActionClear;
						MTLClearColor clr;
						clr.red = rtv[i].ClearValue[0];
						clr.green = rtv[i].ClearValue[1];
						clr.blue = rtv[i].ClearValue[2];
						clr.alpha = rtv[i].ClearValue[3];
						ca.clearColor = clr;
					} else if (rtv[i].LoadAction == TextureLoadAction::Load) ca.loadAction = MTLLoadActionLoad;
					else if (rtv[i].LoadAction == TextureLoadAction::DontCare) ca.loadAction = MTLLoadActionDontCare;
					ca.storeAction = MTLStoreActionStore;
					[desc.colorAttachments setObject: ca atIndexedSubscript: i];
				}
				if (dsv) {
					if (!dsv->Texture || !(dsv->Texture->GetResourceUsage() & ResourceUsageDepthStencil)) {
						[autorelease_pool release];
						autorelease_pool = 0;
						return false;
					}
					auto format = dsv->Texture->GetPixelFormat();
					MTLRenderPassDepthAttachmentDescriptor * da = [[MTLRenderPassDepthAttachmentDescriptor alloc] init];
					[da autorelease];
					da.texture = GetInnerMetalTexture(dsv->Texture);
					if (dsv->DepthLoadAction == TextureLoadAction::Clear) {
						da.loadAction = MTLLoadActionClear;
						da.clearDepth = dsv->DepthClearValue;
					} else if (dsv->DepthLoadAction == TextureLoadAction::Load) da.loadAction = MTLLoadActionLoad;
					else if (dsv->DepthLoadAction == TextureLoadAction::DontCare) da.loadAction = MTLLoadActionDontCare;
					da.storeAction = MTLStoreActionStore;
					desc.depthAttachment = da;
					if (format == PixelFormat::D24S8_unorm || format == PixelFormat::D32S8_float) {
						MTLRenderPassStencilAttachmentDescriptor * sa = [[MTLRenderPassStencilAttachmentDescriptor alloc] init];
						[sa autorelease];
						sa.texture = GetInnerMetalTexture(dsv->Texture);
						if (dsv->StencilLoadAction == TextureLoadAction::Clear) {
							sa.loadAction = MTLLoadActionClear;
							sa.clearStencil = dsv->StencilClearValue;
						} else if (dsv->StencilLoadAction == TextureLoadAction::Load) sa.loadAction = MTLLoadActionLoad;
						else if (dsv->StencilLoadAction == TextureLoadAction::DontCare) sa.loadAction = MTLLoadActionDontCare;
						sa.storeAction = MTLStoreActionStore;
						desc.stencilAttachment = sa;
					}
				}
				render_command_encoder = [current_command renderCommandEncoderWithDescriptor: desc];
				state = 1;
				error_state = true;
				return true;
			}
			virtual bool Begin2DRenderingPass(const RenderTargetViewDesc & rtv) noexcept override
			{
				if (state) return false;
				if (!device_2d) {
					device_2d = Cocoa::CreateMetalRenderingDevice(wrapper);
					if (!device_2d) return false;
				}
				if (!rtv.Texture || !(rtv.Texture->GetResourceUsage() & ResourceUsageRenderTarget)) return false;
				if (rtv.Texture->GetTextureType() != TextureType::Type2D || rtv.Texture->GetPixelFormat() != PixelFormat::B8G8R8A8_unorm || rtv.Texture->GetMipmapCount() != 1) return false;
				autorelease_pool = [[NSAutoreleasePool alloc] init];
				if (!autorelease_pool) return false;
				if (!current_command) {
					current_command = [queue commandBuffer];
					if (!current_command) {
						[autorelease_pool release];
						autorelease_pool = 0;
						return false;
					}
					[current_command retain];
				}
				try {
					if (rtv.LoadAction == TextureLoadAction::Load) Cocoa::PureMetalRenderingDeviceBeginDraw(device_2d, current_command, GetInnerMetalTexture(rtv.Texture), rtv.Texture->GetWidth(), rtv.Texture->GetHeight(), MTLLoadActionLoad, 0);
					else if (rtv.LoadAction == TextureLoadAction::Clear) Cocoa::PureMetalRenderingDeviceBeginDraw(device_2d, current_command, GetInnerMetalTexture(rtv.Texture), rtv.Texture->GetWidth(), rtv.Texture->GetHeight(), MTLLoadActionClear, rtv.ClearValue);
					else Cocoa::PureMetalRenderingDeviceBeginDraw(device_2d, current_command, GetInnerMetalTexture(rtv.Texture), rtv.Texture->GetWidth(), rtv.Texture->GetHeight(), MTLLoadActionDontCare, 0);
				} catch (...) {
					[autorelease_pool release];
					autorelease_pool = 0;
					return false;
				}
				state = 2;
				error_state = true;
				return true;
			}
			virtual bool BeginMemoryManagementPass(void) noexcept override
			{
				if (state) return false;
				autorelease_pool = [[NSAutoreleasePool alloc] init];
				if (!autorelease_pool) return false;
				if (!current_command) {
					current_command = [queue commandBuffer];
					if (!current_command) {
						[autorelease_pool release];
						autorelease_pool = 0;
						return false;
					}
					[current_command retain];
				}
				state = 3;
				error_state = true;
				return true;
			}
			virtual bool EndCurrentPass(void) noexcept override
			{
				if (!state) return false;
				if (state == 1) {
					[render_command_encoder endEncoding];
					render_command_encoder = 0;
					index_buffer.SetReference(0);
				} else if (state == 2) {
					Cocoa::PureMetalRenderingDeviceEndDraw(device_2d);
				} else if (state == 3) {
					if (blit_command_encoder) [blit_command_encoder endEncoding];
					blit_command_encoder = 0;
				}
				[autorelease_pool release];
				autorelease_pool = 0;
				state = 0;
				return error_state;
			}
			virtual void Flush(void) noexcept override
			{
				if (!state && current_command) {
					[current_command commit];
					[current_command release];
					current_command = 0;
				}
			}
			virtual void SetRenderingPipelineState(IPipelineState * _state) noexcept override
			{
				if (state != 1 || !_state) { error_state = false; return; }
				auto wr = static_cast<MTL_PipelineState *>(_state);
				[render_command_encoder setRenderPipelineState: wr->state];
				[render_command_encoder setTriangleFillMode: wr->fill_mode];
				[render_command_encoder setFrontFacingWinding: wr->front_winding];
				[render_command_encoder setCullMode: wr->cull_mode];
				[render_command_encoder setDepthBias: wr->depth_bias slopeScale: wr->slope_scaled_depth_bias clamp: wr->depth_bias_clamp];
				[render_command_encoder setDepthClipMode: wr->depth_clip];
				if (wr->ds_state) [render_command_encoder setDepthStencilState: wr->ds_state];
				primitive_type = wr->primitive_type;
			}
			virtual void SetViewport(float top_left_x, float top_left_y, float width, float height, float min_depth, float max_depth) noexcept override
			{
				if (state != 1) { error_state = false; return; }
				MTLViewport viewport;
				viewport.originX = top_left_x;
				viewport.originY = top_left_y;
				viewport.width = width;
				viewport.height = height;
				viewport.znear = min_depth;
				viewport.zfar = max_depth;
				[render_command_encoder setViewport: viewport];
			}
			virtual void SetVertexShaderResource(uint32 at, IDeviceResource * resource) noexcept override
			{
				if (state != 1) { error_state = false; return; }
				if (resource) {
					if (resource->GetResourceType() == ResourceType::Buffer) {
						[render_command_encoder setVertexBuffer: static_cast<MTL_Buffer *>(resource)->buffer offset: 0 atIndex: at];
					} else if (resource->GetResourceType() == ResourceType::Texture) {
						[render_command_encoder setVertexTexture: static_cast<MTL_Texture *>(resource)->texture atIndex: at];
					}
				} else {
					[render_command_encoder setVertexBuffer: 0 offset: 0 atIndex: at];
					[render_command_encoder setVertexTexture: 0 atIndex: at];
				}
			}
			virtual void SetVertexShaderConstant(uint32 at, IBuffer * buffer) noexcept override
			{
				if (state != 1) { error_state = false; return; }
				if (buffer) {
					[render_command_encoder setVertexBuffer: static_cast<MTL_Buffer *>(buffer)->buffer offset: 0 atIndex: at];
				} else {
					[render_command_encoder setVertexBuffer: 0 offset: 0 atIndex: at];
				}
			}
			virtual void SetVertexShaderConstant(uint32 at, const void * data, int length) noexcept override
			{
				if (state != 1 || length > 4096) { error_state = false; return; }
				if (length) {
					[render_command_encoder setVertexBytes: data length: length atIndex: at];
				} else {
					[render_command_encoder setVertexBuffer: 0 offset: 0 atIndex: at];
				}
			}
			virtual void SetVertexShaderSamplerState(uint32 at, ISamplerState * sampler) noexcept override
			{
				if (state != 1) { error_state = false; return; }
				if (sampler) {
					[render_command_encoder setVertexSamplerState: static_cast<MTL_SamplerState *>(sampler)->state atIndex: at];
				} else {
					[render_command_encoder setVertexSamplerState: 0 atIndex: at];
				}
			}
			virtual void SetPixelShaderResource(uint32 at, IDeviceResource * resource) noexcept override
			{
				if (state != 1) { error_state = false; return; }
				if (resource) {
					if (resource->GetResourceType() == ResourceType::Buffer) {
						[render_command_encoder setFragmentBuffer: static_cast<MTL_Buffer *>(resource)->buffer offset: 0 atIndex: at];
					} else if (resource->GetResourceType() == ResourceType::Texture) {
						[render_command_encoder setFragmentTexture: static_cast<MTL_Texture *>(resource)->texture atIndex: at];
					}
				} else {
					[render_command_encoder setFragmentBuffer: 0 offset: 0 atIndex: at];
					[render_command_encoder setFragmentTexture: 0 atIndex: at];
				}
			}
			virtual void SetPixelShaderConstant(uint32 at, IBuffer * buffer) noexcept override
			{
				if (state != 1) { error_state = false; return; }
				if (buffer) {
					[render_command_encoder setFragmentBuffer: static_cast<MTL_Buffer *>(buffer)->buffer offset: 0 atIndex: at];
				} else {
					[render_command_encoder setFragmentBuffer: 0 offset: 0 atIndex: at];
				}
			}
			virtual void SetPixelShaderConstant(uint32 at, const void * data, int length) noexcept override
			{
				if (state != 1) { error_state = false; return; }
				if (length) {
					[render_command_encoder setFragmentBytes: data length: length atIndex: at];
				} else {
					[render_command_encoder setFragmentBuffer: 0 offset: 0 atIndex: at];
				}
			}
			virtual void SetPixelShaderSamplerState(uint32 at, ISamplerState * sampler) noexcept override
			{
				if (state != 1) { error_state = false; return; }
				if (sampler) {
					[render_command_encoder setFragmentSamplerState: static_cast<MTL_SamplerState *>(sampler)->state atIndex: at];
				} else {
					[render_command_encoder setFragmentSamplerState: 0 atIndex: at];
				}
			}
			virtual void SetIndexBuffer(IBuffer * index, IndexBufferFormat format) noexcept override
			{
				if (state != 1) { error_state = false; return; }
				if (index) {
					index_buffer.SetRetain(static_cast<MTL_Buffer *>(index));
					if (format == IndexBufferFormat::UInt16) index_buffer_format = MTLIndexTypeUInt16;
					else if (format == IndexBufferFormat::UInt32) index_buffer_format = MTLIndexTypeUInt32;
				} else {
					index_buffer.SetReference(0);
				}
			}
			virtual void SetStencilReferenceValue(uint8 ref) noexcept override
			{
				if (state != 1) { error_state = false; return; }
				[render_command_encoder setStencilReferenceValue: ref];
			}
			virtual void DrawPrimitives(uint32 vertex_count, uint32 first_vertex) noexcept override
			{
				if (state != 1) { error_state = false; return; }
				[render_command_encoder drawPrimitives: primitive_type vertexStart: first_vertex vertexCount: vertex_count];
			}
			virtual void DrawInstancedPrimitives(uint32 vertex_count, uint32 first_vertex, uint32 instance_count, uint32 first_instance) noexcept override
			{
				if (state != 1) { error_state = false; return; }
				[render_command_encoder drawPrimitives: primitive_type vertexStart: first_vertex vertexCount: vertex_count
					instanceCount: instance_count baseInstance: first_instance];
			}
			virtual void DrawIndexedPrimitives(uint32 index_count, uint32 first_index, uint32 base_vertex) noexcept override
			{
				if (state != 1 || !index_buffer) { error_state = false; return; }
				[render_command_encoder drawIndexedPrimitives: primitive_type indexCount: index_count indexType: index_buffer_format
					indexBuffer: index_buffer->buffer indexBufferOffset: 0 instanceCount: 1 baseVertex: base_vertex baseInstance: 0];
			}
			virtual void DrawIndexedInstancedPrimitives(uint32 index_count, uint32 first_index, uint32 base_vertex, uint32 instance_count, uint32 first_instance) noexcept override
			{
				if (state != 1 || !index_buffer) { error_state = false; return; }
				[render_command_encoder drawIndexedPrimitives: primitive_type indexCount: index_count indexType: index_buffer_format
					indexBuffer: index_buffer->buffer indexBufferOffset: 0 instanceCount: instance_count baseVertex: base_vertex baseInstance: first_instance];
			}
			virtual I2DDeviceContext * Get2DContext(void) noexcept override { return device_2d; }
			virtual void GenerateMipmaps(ITexture * texture) noexcept override
			{
				if (state != 3 || !texture) { error_state = false; return; }
				if (!blit_command_encoder) blit_command_encoder = [current_command blitCommandEncoder];
				[blit_command_encoder generateMipmapsForTexture: GetInnerMetalTexture(texture)];
			}
			virtual void CopyResourceData(IDeviceResource * dest, IDeviceResource * src) noexcept override
			{
				if (state != 3 || !dest || !src) { error_state = false; return; }
				if (!blit_command_encoder) blit_command_encoder = [current_command blitCommandEncoder];
				if (dest->GetResourceType() == ResourceType::Buffer && src->GetResourceType() == ResourceType::Buffer) {
					auto src_rsrc = static_cast<MTL_Buffer *>(src);
					auto dest_rsrc = static_cast<MTL_Buffer *>(dest);
					if (src_rsrc->size != dest_rsrc->size) { error_state = false; return; }
					[blit_command_encoder copyFromBuffer: src_rsrc->buffer sourceOffset: 0 toBuffer: dest_rsrc->buffer destinationOffset: 0 size: src_rsrc->size];
				} else if (dest->GetResourceType() == ResourceType::Texture && src->GetResourceType() == ResourceType::Texture) {
					auto src_rsrc = static_cast<MTL_Texture *>(src);
					auto dest_rsrc = static_cast<MTL_Texture *>(dest);
					[blit_command_encoder copyFromTexture: src_rsrc->texture toTexture: dest_rsrc->texture];
				} else error_state = false;
			}
			virtual void CopySubresourceData(IDeviceResource * dest, SubresourceIndex dest_subres, VolumeIndex dest_origin, IDeviceResource * src, SubresourceIndex src_subres, VolumeIndex src_origin, VolumeIndex size) noexcept override
			{
				if (state != 3 || !dest || !src) { error_state = false; return; }
				if (!blit_command_encoder) blit_command_encoder = [current_command blitCommandEncoder];
				if (dest->GetResourceType() == ResourceType::Buffer && src->GetResourceType() == ResourceType::Buffer) {
					auto src_rsrc = static_cast<MTL_Buffer *>(src);
					auto dest_rsrc = static_cast<MTL_Buffer *>(dest);
					[blit_command_encoder copyFromBuffer: src_rsrc->buffer sourceOffset: src_origin.x
						toBuffer: dest_rsrc->buffer destinationOffset: dest_origin.x size: size.x];
				} else if (dest->GetResourceType() == ResourceType::Texture && src->GetResourceType() == ResourceType::Texture) {
					auto src_rsrc = static_cast<MTL_Texture *>(src);
					auto dest_rsrc = static_cast<MTL_Texture *>(dest);
					[blit_command_encoder
						copyFromTexture: src_rsrc->texture sourceSlice: src_subres.array_index sourceLevel: src_subres.mip_level
						sourceOrigin: MTLOriginMake(src_origin.x, src_origin.y, src_origin.z) sourceSize: MTLSizeMake(size.x, size.y, size.z)
						toTexture: dest_rsrc->texture destinationSlice: dest_subres.array_index destinationLevel: dest_subres.mip_level
						destinationOrigin: MTLOriginMake(dest_origin.x, dest_origin.y, dest_origin.z)];
				} else error_state = false;
			}
			virtual void UpdateResourceData(IDeviceResource * dest, SubresourceIndex subres, VolumeIndex origin, VolumeIndex size, const ResourceInitDesc & src) noexcept override
			{
				if (state != 3 || !dest) { error_state = false; return; }
				if (!(dest->GetResourceUsage() & ResourceUsageCPUWrite)) { error_state = false; return; }
				if (!blit_command_encoder) blit_command_encoder = [current_command blitCommandEncoder];
				id<MTLResource> rsrc = 0;
				if (dest->GetResourceType() == ResourceType::Buffer) rsrc = static_cast<MTL_Buffer *>(dest)->buffer;
				else if (dest->GetResourceType() == ResourceType::Texture) rsrc = static_cast<MTL_Texture *>(dest)->texture;
				[blit_command_encoder synchronizeResource: rsrc];
				[blit_command_encoder endEncoding];
				blit_command_encoder = 0;
				[current_command commit];
				[current_command waitUntilCompleted];
				[current_command release];
				current_command = 0;
				if (dest->GetResourceType() == ResourceType::Buffer) {
					auto buffer = static_cast<MTL_Buffer *>(dest)->buffer;
					auto dest_data = reinterpret_cast<uint8 *>([buffer contents]);
					auto src_data = reinterpret_cast<const uint8 *>(src.Data);
					MemoryCopy(dest_data + origin.x, src_data, size.x);
					[buffer didModifyRange: NSMakeRange(origin.x, size.x)];
				} else if (dest->GetResourceType() == ResourceType::Texture) {
					auto tex_wrapper = static_cast<MTL_Texture *>(dest);
					auto texture = tex_wrapper->texture;
					if (tex_wrapper->GetTextureType() == TextureType::Type1D || tex_wrapper->GetTextureType() == TextureType::TypeArray1D) {
						[texture replaceRegion: MTLRegionMake1D(origin.x, size.x)
							mipmapLevel: subres.mip_level slice: subres.array_index withBytes: src.Data bytesPerRow: 0 bytesPerImage: 0];
					} else if (tex_wrapper->GetTextureType() == TextureType::Type2D || tex_wrapper->GetTextureType() == TextureType::TypeArray2D) {
						[texture replaceRegion: MTLRegionMake2D(origin.x, origin.y, size.x, size.y)
							mipmapLevel: subres.mip_level slice: subres.array_index withBytes: src.Data bytesPerRow: src.DataPitch bytesPerImage: 0];
					} else if (tex_wrapper->GetTextureType() == TextureType::TypeCube || tex_wrapper->GetTextureType() == TextureType::TypeArrayCube) {
						[texture replaceRegion: MTLRegionMake2D(origin.x, origin.y, size.x, size.y)
							mipmapLevel: subres.mip_level slice: subres.array_index withBytes: src.Data bytesPerRow: src.DataPitch bytesPerImage: 0];
					} else if (tex_wrapper->GetTextureType() == TextureType::Type3D) {
						[texture replaceRegion: MTLRegionMake3D(origin.x, origin.y, origin.z, size.x, size.y, size.z)
							mipmapLevel: subres.mip_level slice: subres.array_index
							withBytes: src.Data bytesPerRow: src.DataPitch bytesPerImage: src.DataSlicePitch];
					}
				}
				current_command = [queue commandBuffer];
				[current_command retain];
			}
			virtual void QueryResourceData(const ResourceDataDesc & dest, IDeviceResource * src, SubresourceIndex subres, VolumeIndex origin, VolumeIndex size) noexcept override
			{
				if (state != 3 || !src) { error_state = false; return; }
				if (!(src->GetResourceUsage() & ResourceUsageCPURead)) { error_state = false; return; }
				if (!blit_command_encoder) blit_command_encoder = [current_command blitCommandEncoder];
				id<MTLResource> rsrc = 0;
				if (src->GetResourceType() == ResourceType::Buffer) rsrc = static_cast<MTL_Buffer *>(src)->buffer;
				else if (src->GetResourceType() == ResourceType::Texture) rsrc = static_cast<MTL_Texture *>(src)->texture;
				[blit_command_encoder synchronizeResource: rsrc];
				[blit_command_encoder endEncoding];
				[current_command commit];
				[current_command waitUntilCompleted];
				[current_command release];
				blit_command_encoder = 0;
				current_command = 0;
				if (src->GetResourceType() == ResourceType::Buffer) {
					auto buffer = static_cast<MTL_Buffer *>(src)->buffer;
					auto dest_data = reinterpret_cast<uint8 *>(dest.Data);
					auto src_data = reinterpret_cast<const uint8 *>([buffer contents]);
					MemoryCopy(dest_data, src_data + origin.x, size.x);
				} else if (src->GetResourceType() == ResourceType::Texture) {
					auto tex_wrapper = static_cast<MTL_Texture *>(src);
					auto texture = tex_wrapper->texture;
					if (tex_wrapper->GetTextureType() == TextureType::Type1D || tex_wrapper->GetTextureType() == TextureType::TypeArray1D) {
						[texture getBytes: dest.Data bytesPerRow: 0 bytesPerImage: 0
							fromRegion: MTLRegionMake1D(origin.x, size.x) mipmapLevel: subres.mip_level slice: subres.array_index];
					} else if (tex_wrapper->GetTextureType() == TextureType::Type2D || tex_wrapper->GetTextureType() == TextureType::TypeArray2D) {
						[texture getBytes: dest.Data bytesPerRow: dest.DataPitch bytesPerImage: 0
							fromRegion: MTLRegionMake2D(origin.x, origin.y, size.x, size.y) mipmapLevel: subres.mip_level slice: subres.array_index];
					} else if (tex_wrapper->GetTextureType() == TextureType::TypeCube || tex_wrapper->GetTextureType() == TextureType::TypeArrayCube) {
						[texture getBytes: dest.Data bytesPerRow: dest.DataPitch bytesPerImage: 0
							fromRegion: MTLRegionMake2D(origin.x, origin.y, size.x, size.y) mipmapLevel: subres.mip_level slice: subres.array_index];
					} else if (tex_wrapper->GetTextureType() == TextureType::Type3D) {
						[texture getBytes: dest.Data bytesPerRow: dest.DataPitch bytesPerImage: dest.DataSlicePitch
							fromRegion: MTLRegionMake3D(origin.x, origin.y, origin.z, size.x, size.y, size.z) mipmapLevel: subres.mip_level slice: subres.array_index];
					}
				}
				current_command = [queue commandBuffer];
				[current_command retain];
			}
			virtual bool AcquireSharedResource(IDeviceResource * rsrc) noexcept override
			{
				if (state || !rsrc || rsrc->GetMemoryPool() != ResourceMemoryPool::Shared) return false;
				auto sync = static_cast<MTL_Texture *>(rsrc)->shared->_meta;
				while (sync->lock.test_and_set(std::memory_order_acquire)) Sleep(1);
				return true;
			}
			virtual bool AcquireSharedResource(IDeviceResource * rsrc, uint32 timeout) noexcept override
			{
				if (state || !rsrc || rsrc->GetMemoryPool() != ResourceMemoryPool::Shared) return false;
				auto sync = static_cast<MTL_Texture *>(rsrc)->shared->_meta;
				if (timeout) {
					auto start = GetTimerValue();
					while (sync->lock.test_and_set(std::memory_order_acquire)) {
						if (GetTimerValue() - start < timeout) Sleep(1);
						else return false;
					}
					return true;
				} else return !sync->lock.test_and_set(std::memory_order_acquire);
			}
			virtual bool ReleaseSharedResource(IDeviceResource * rsrc) noexcept override
			{
				if (state || !rsrc || rsrc->GetMemoryPool() != ResourceMemoryPool::Shared) return false;
				auto sync = static_cast<MTL_Texture *>(rsrc)->shared->_meta;
				if (current_command) {
					[current_command commit];
					[current_command waitUntilCompleted];
					[current_command release];
					current_command = 0;
				}
				sync->lock.clear(std::memory_order_release);
				return true;
			}
			virtual string ToString(void) const override { return L"MTL_DeviceContext"; }
		};
		class MTL_WindowLayer : public IWindowLayer
		{
			SafePointer<MTL_DeviceContext> context;
			SafePointer<MTL_Texture> texture;
			SafePointer<MetalPresentationEngine> engine;
			IDevice * wrapper;
			Windows::ICoreWindow * window;
			PixelFormat format;
			uint32 width, height, usage, attributes;
			id<CAMetalDrawable> last_drawable;
		public:
			MTL_WindowLayer(IDevice * _wrapper, MTL_DeviceContext * _context, const WindowLayerDesc & desc, Windows::ICoreWindow * _window) : wrapper(_wrapper), last_drawable(0), attributes(0)
			{
				if (desc.Format != PixelFormat::Invalid) {
					if (desc.Format == PixelFormat::B8G8R8A8_unorm) format = desc.Format;
					else if (desc.Format == PixelFormat::R16G16B16A16_float) format = desc.Format;
					else if (desc.Format == PixelFormat::R10G10B10A2_unorm) format = desc.Format;
					else throw InvalidArgumentException();
				} else format = PixelFormat::B8G8R8A8_unorm;
				if (desc.Usage & ~(ResourceUsageShaderRead | ResourceUsageRenderTarget | WindowLayerAttributeMask)) throw InvalidArgumentException();
				if (!(desc.Usage & ResourceUsageRenderTarget)) throw InvalidArgumentException();
				window = _window;
				context.SetRetain(_context);
				engine = new MetalPresentationEngine;
				window->SetPresentationEngine(engine);
				engine->SetDevice(GetInnerMetalDevice(wrapper));
				if (desc.Usage & ResourceUsageShaderRead) engine->SetFramebufferOnly(false);
				else engine->SetFramebufferOnly(true);
				if (desc.Usage & WindowLayerAttributeAlphaChannelIgnore) {
					engine->SetOpaque(true);
					attributes |= WindowLayerAttributeAlphaChannelIgnore;
				} else if (desc.Usage & WindowLayerAttributeAlphaChannelPremultiplied) {
					engine->SetOpaque(false);
					attributes |= WindowLayerAttributeAlphaChannelPremultiplied;
				} else if (desc.Usage & WindowLayerAttributeAlphaChannelStraight) {
					throw InvalidArgumentException();
				} else {
					if (Cocoa::WindowNeedsAlphaBackbuffer(_window)) {
						engine->SetOpaque(false);
						attributes |= WindowLayerAttributeAlphaChannelPremultiplied;
					} else {
						engine->SetOpaque(true);
						attributes |= WindowLayerAttributeAlphaChannelIgnore;
					}
				}
				if (desc.Usage & WindowLayerAttributeExtendedDynamicRange && (desc.Format == PixelFormat::R16G16B16A16_float)) {
					engine->SetExtendedDynamicRange(true);
					attributes |= WindowLayerAttributeExtendedDynamicRange;
				}
				engine->SetSize(Point(max(desc.Width, 1U), max(desc.Height, 1U)));
				engine->SetPixelFormat(MakeMetalPixelFormat(format));
				width = max(desc.Width, 1U);
				height = max(desc.Height, 1U);
				usage = ResourceUsageRenderTarget;
				if (desc.Usage & ResourceUsageShaderRead) usage |= ResourceUsageShaderRead;
			}
			virtual ~MTL_WindowLayer(void) override { [last_drawable release]; }
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual bool Present(void) noexcept override
			{
				if (texture && texture->GetReferenceCount() > 1) return false;
				if (last_drawable) {
					if (context->state) return false;
					if (!context->current_command) {
						@autoreleasepool {
							context->current_command = [context->queue commandBuffer];
							[context->current_command retain];
						}
					}
					[context->current_command presentDrawable: last_drawable];
					[context->current_command commit];
					[context->current_command release];
					context->current_command = 0;
					return true;
				} else return false;
			}
			virtual ITexture * QuerySurface(void) noexcept override
			{
				if (!texture) {
					texture = new (std::nothrow) MTL_Texture(wrapper);
					if (!texture) return 0;
					texture->type = TextureType::Type2D;
					texture->format = format;
					texture->width = width;
					texture->height = height;
					texture->depth = 1;
					texture->size = 1;
					texture->usage = usage;
				}
				if (last_drawable) {
					[last_drawable release];
					last_drawable = 0;
				}
				if (texture->texture) {
					[texture->texture release];
					texture->texture = 0;
				}
				last_drawable = engine->GetDrawable();
				if (!last_drawable) return 0;
				[last_drawable retain];
				texture->texture = [last_drawable texture];
				[texture->texture retain];
				texture->Retain();
				return texture;
			}
			virtual bool ResizeSurface(uint32 _width, uint32 _height) noexcept override
			{
				if (texture && texture->GetReferenceCount() > 1) return false;
				if (_width > 16384 || _height > 16384) return false;
				width = max(_width, 1U);
				height = max(_height, 1U);
				engine->SetSize(Point(width, height));
				texture.SetReference(0);
				return true;
			}
			virtual bool SwitchToFullscreen(void) noexcept override { return Cocoa::SetWindowFullscreenMode(window, true); }
			virtual bool SwitchToWindow(void) noexcept override { return Cocoa::SetWindowFullscreenMode(window, false); }
			virtual bool IsFullscreen(void) noexcept override { return Cocoa::IsWindowInFullscreenMode(window); }
			virtual uint GetLayerAttributes(void) noexcept override { return attributes; }
			virtual string ToString(void) const override { return L"MTL_WindowLayer"; }
		};
		class MTL_Device : public IDevice
		{
			SafePointer<MTL_DeviceContext> context;

			static MTLBlendOperation _make_blend_op(BlendingFunction func)
			{
				if (func == BlendingFunction::Add) return MTLBlendOperationAdd;
				else if (func == BlendingFunction::SubtractOverFromBase) return MTLBlendOperationReverseSubtract;
				else if (func == BlendingFunction::SubtractBaseFromOver) return MTLBlendOperationSubtract;
				else if (func == BlendingFunction::Min) return MTLBlendOperationMin;
				else if (func == BlendingFunction::Max) return MTLBlendOperationMax;
				else return MTLBlendOperationAdd;
			}
			static MTLBlendFactor _make_blend_fact(BlendingFactor fact)
			{
				if (fact == BlendingFactor::Zero) return MTLBlendFactorZero;
				else if (fact == BlendingFactor::One) return MTLBlendFactorOne;
				else if (fact == BlendingFactor::OverColor) return MTLBlendFactorSourceColor;
				else if (fact == BlendingFactor::InvertedOverColor) return MTLBlendFactorOneMinusSourceColor;
				else if (fact == BlendingFactor::OverAlpha) return MTLBlendFactorSourceAlpha;
				else if (fact == BlendingFactor::InvertedOverAlpha) return MTLBlendFactorOneMinusSourceAlpha;
				else if (fact == BlendingFactor::BaseColor) return MTLBlendFactorDestinationColor;
				else if (fact == BlendingFactor::InvertedBaseColor) return MTLBlendFactorOneMinusDestinationColor;
				else if (fact == BlendingFactor::BaseAlpha) return MTLBlendFactorDestinationAlpha;
				else if (fact == BlendingFactor::InvertedBaseAlpha) return MTLBlendFactorOneMinusDestinationAlpha;
				else if (fact == BlendingFactor::SecondaryColor) return MTLBlendFactorSource1Color;
				else if (fact == BlendingFactor::InvertedSecondaryColor) return MTLBlendFactorOneMinusSource1Color;
				else if (fact == BlendingFactor::SecondaryAlpha) return MTLBlendFactorSource1Alpha;
				else if (fact == BlendingFactor::InvertedSecondaryAlpha) return MTLBlendFactorOneMinusSource1Alpha;
				else if (fact == BlendingFactor::OverAlphaSaturated) return MTLBlendFactorSourceAlphaSaturated;
				else return MTLBlendFactorZero;
			}
			static MTLSamplerMinMagFilter _make_min_mag_filter(SamplerFilter filter)
			{
				if (filter == SamplerFilter::Point) return MTLSamplerMinMagFilterNearest;
				else return MTLSamplerMinMagFilterLinear;
			}
			static MTLSamplerMipFilter _make_mip_filter(SamplerFilter filter)
			{
				if (filter == SamplerFilter::Point) return MTLSamplerMipFilterNearest;
				else return MTLSamplerMipFilterLinear;
			}
			static MTLSamplerAddressMode _make_address_mode(SamplerAddressMode mode)
			{
				if (mode == SamplerAddressMode::Wrap) return MTLSamplerAddressModeRepeat;
				else if (mode == SamplerAddressMode::Mirror) return MTLSamplerAddressModeMirrorRepeat;
				else if (mode == SamplerAddressMode::Clamp) return MTLSamplerAddressModeClampToEdge;
				else if (mode == SamplerAddressMode::Border) return MTLSamplerAddressModeClampToBorderColor;
				else return MTLSamplerAddressModeRepeat;
			}
			static MTLCompareFunction _make_comp_func(CompareFunction func)
			{
				if (func == CompareFunction::Always) return MTLCompareFunctionAlways;
				else if (func == CompareFunction::Lesser) return MTLCompareFunctionLess;
				else if (func == CompareFunction::Greater) return MTLCompareFunctionGreater;
				else if (func == CompareFunction::Equal) return MTLCompareFunctionEqual;
				else if (func == CompareFunction::LesserEqual) return MTLCompareFunctionLessEqual;
				else if (func == CompareFunction::GreaterEqual) return MTLCompareFunctionGreaterEqual;
				else if (func == CompareFunction::NotEqual) return MTLCompareFunctionNotEqual;
				else if (func == CompareFunction::Never) return MTLCompareFunctionNever;
				else return MTLCompareFunctionNever;
			}
			static MTLStencilOperation _make_stencil_func(StencilFunction func)
			{
				if (func == StencilFunction::Keep) return MTLStencilOperationKeep;
				else if (func == StencilFunction::SetZero) return MTLStencilOperationZero;
				else if (func == StencilFunction::Replace) return MTLStencilOperationReplace;
				else if (func == StencilFunction::IncrementWrap) return MTLStencilOperationIncrementWrap;
				else if (func == StencilFunction::DecrementWrap) return MTLStencilOperationDecrementWrap;
				else if (func == StencilFunction::IncrementClamp) return MTLStencilOperationIncrementClamp;
				else if (func == StencilFunction::DecrementClamp) return MTLStencilOperationDecrementClamp;
				else if (func == StencilFunction::Invert) return MTLStencilOperationInvert;
				else return MTLStencilOperationKeep;
			}
			static void _evaluate_mipmap_size(uint level, VolumeIndex & index)
			{
				while (level) { index.x >>= 1; index.y >>= 1; index.z >>= 1; level--; }
				if (!index.x) index.x = 1;
				if (!index.y) index.y = 1;
				if (!index.z) index.z = 1;
			}
		public:
			id<MTLDevice> device;
			id<MTLCommandQueue> queue;
			bool is_valid;

			MTL_Device(id<MTLDevice> _device) : device(_device)
			{
				is_valid = true;
				[device retain];
				queue = [device newCommandQueue];
				context = new MTL_DeviceContext(this, device, queue);
			}
			virtual ~MTL_Device(void) override
			{
				[device release];
				[queue release];
			}
			virtual string GetDeviceName(void) noexcept override { return Cocoa::EngineString([device name]); }
			virtual uint64 GetDeviceIdentifier(void) noexcept override { return [device registryID]; }
			virtual bool DeviceIsValid(void) noexcept override { return is_valid; }
			virtual void GetImplementationInfo(string & tech, uint32 & version_major, uint32 & version_minor) noexcept override { try { tech = L"Metal"; } catch (...) {} QueryMetalVersion(device, version_major, version_minor); }
			virtual DeviceClass GetDeviceClass(void) noexcept override { if ([device hasUnifiedMemory]) return DeviceClass::Integrated; else return DeviceClass::Discrete; }
			virtual uint64 GetDeviceMemory(void) noexcept override { return [device recommendedMaxWorkingSetSize]; }
			virtual bool GetDevicePixelFormatSupport(PixelFormat format, PixelFormatUsage usage) noexcept override
			{
				if (usage == Graphics::PixelFormatUsage::ShaderRead) return QueryMetalFormatSupportForShaderRead(device, format);
				else if (usage == Graphics::PixelFormatUsage::ShaderSample) return QueryMetalFormatSupportForShaderSample(device, format);
				else if (usage == Graphics::PixelFormatUsage::RenderTarget) return QueryMetalFormatSupportForRenderTarget(device, format);
				else if (usage == Graphics::PixelFormatUsage::BlendRenderTarget) return QueryMetalFormatSupportForBlendRenderTarget(device, format);
				else if (usage == Graphics::PixelFormatUsage::DepthStencil) return QueryMetalFormatSupportForDepthStencil(device, format);
				else if (usage == Graphics::PixelFormatUsage::WindowSurface) return format == Graphics::PixelFormat::B8G8R8A8_unorm || format == Graphics::PixelFormat::R10G10B10A2_unorm || format == Graphics::PixelFormat::R16G16B16A16_float;
				else if (usage == Graphics::PixelFormatUsage::RenderTarget2D || usage == Graphics::PixelFormatUsage::VideoIO) return format == Graphics::PixelFormat::B8G8R8A8_unorm;
				else if (usage == Graphics::PixelFormatUsage::BitmapSource) return QueryMetalFormatSupportForShaderRead(device, format) && QueryMetalFormatSupportForShaderSample(device, format) && Graphics::IsColorFormat(format);
				else return false;
			}
			virtual IShaderLibrary * LoadShaderLibrary(const void * data, int length) noexcept override
			{
				@autoreleasepool {
					SafePointer<MTL_ShaderLibrary> lib = new (std::nothrow) MTL_ShaderLibrary(this);
					if (!lib) return 0;
					NSError * error;
					dispatch_data_t data_handle = dispatch_data_create(data, length, dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0), DISPATCH_DATA_DESTRUCTOR_DEFAULT);
					if (!data_handle) return 0;
					lib->ref = [device newLibraryWithData: data_handle error: &error];
					[data_handle release];
					if (!lib->ref) return 0;
					lib->Retain();
					return lib;
				}
			}
			virtual IShaderLibrary * LoadShaderLibrary(const DataBlock * data) noexcept override
			{
				try { return LoadShaderLibrary(data->GetBuffer(), data->Length()); }
				catch (...) { return 0; }
			}
			virtual IShaderLibrary * LoadShaderLibrary(Streaming::Stream * stream) noexcept override
			{
				try {
					SafePointer<DataBlock> data = stream->ReadAll();
					return LoadShaderLibrary(data);
				} catch (...) { return 0; }
			}
			virtual IShaderLibrary * CompileShaderLibrary(const void * data, int length, ShaderError * error) noexcept override
			{
				@autoreleasepool {
					try {
						SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(data, length);
						SafePointer<Storage::Archive> archive = Storage::OpenArchive(stream);
						if (!archive) throw InvalidFormatException();
						for (Storage::ArchiveFile file = 1; file <= archive->GetFileCount(); file++) {
							auto flags = archive->GetFileCustomData(file);
							if ((flags & 0xFFFF0000) == 0x20000) {
								SafePointer<MTL_ShaderLibrary> result = new (std::nothrow) MTL_ShaderLibrary(this);
								if (!result) throw OutOfMemoryException();
								SafePointer<Streaming::Stream> data_stream = archive->QueryFileStream(file, Storage::ArchiveStream::Native);
								if (!data_stream) throw InvalidFormatException();
								SafePointer<DataBlock> code = data_stream->ReadAll();
								auto code_ns = Cocoa::CocoaString(string(code->GetBuffer(), code->Length(), Encoding::ANSI));
								auto options = [[MTLCompileOptions alloc] init];
								if (!code_ns || !options) {
									[code_ns release];
									[options release];
									throw OutOfMemoryException();
								}
								NSError * error_ns;
								auto library = [device newLibraryWithSource: code_ns options: options error: &error_ns];
								[code_ns release];
								[options release];
								if (!library) {
									if (error) *error = ShaderError::Compilation;
									return 0;
								}
								if (error) *error = ShaderError::Success;
								result->ref = library;
								result->Retain();
								return result;
							}
						}
						if (error) *error = ShaderError::NoPlatformVersion;
					} catch (InvalidFormatException &) {
						if (error) *error = ShaderError::InvalidContainerData;
					} catch (...) {
						if (error) *error = ShaderError::IO;
					}
					return 0;
				}
			}
			virtual IShaderLibrary * CompileShaderLibrary(const DataBlock * data, ShaderError * error) noexcept override { return CompileShaderLibrary(data->GetBuffer(), data->Length(), error); }
			virtual IShaderLibrary * CompileShaderLibrary(Streaming::Stream * stream, ShaderError * error) noexcept override
			{
				try {
					SafePointer<DataBlock> data = stream->ReadAll();
					if (!data) throw Exception();
					return CompileShaderLibrary(data, error);
				} catch (...) { if (error) *error = ShaderError::IO; return 0; }
			}
			virtual IDeviceContext * GetDeviceContext(void) noexcept override { return context; }
			virtual IPipelineState * CreateRenderingPipelineState(const PipelineStateDesc & desc) noexcept override
			{
				@autoreleasepool {
					if (!desc.VertexShader || desc.VertexShader->GetType() != ShaderType::Vertex) return 0;
					if (!desc.PixelShader || desc.PixelShader->GetType() != ShaderType::Pixel) return 0;
					SafePointer<MTL_PipelineState> result = new (std::nothrow) MTL_PipelineState(this);
					if (!result) return 0;
					MTLRenderPipelineDescriptor * descriptor = [[MTLRenderPipelineDescriptor alloc] init];
					descriptor.vertexFunction = static_cast<MTL_Shader *>(desc.VertexShader)->func;
					descriptor.fragmentFunction = static_cast<MTL_Shader *>(desc.PixelShader)->func;
					try {
						if (!desc.RenderTargetCount || desc.RenderTargetCount > 8) throw Exception();
						for (uint i = 0; i < desc.RenderTargetCount; i++) {
							auto & rtd = desc.RenderTarget[i];
							if (!IsColorFormat(rtd.Format)) throw Exception();
							MTLRenderPipelineColorAttachmentDescriptor * cad = [[MTLRenderPipelineColorAttachmentDescriptor alloc] init];
							cad.pixelFormat = MakeMetalPixelFormat(rtd.Format);
							MTLColorWriteMask wrmask = 0;
							if (!(rtd.Flags & RenderTargetFlagRestrictWriteRed)) wrmask |= MTLColorWriteMaskRed;
							if (!(rtd.Flags & RenderTargetFlagRestrictWriteGreen)) wrmask |= MTLColorWriteMaskGreen;
							if (!(rtd.Flags & RenderTargetFlagRestrictWriteBlue)) wrmask |= MTLColorWriteMaskBlue;
							if (!(rtd.Flags & RenderTargetFlagRestrictWriteAlpha)) wrmask |= MTLColorWriteMaskAlpha;
							cad.writeMask = wrmask;
							if (rtd.Flags & RenderTargetFlagBlendingEnabled) cad.blendingEnabled = YES;
							else cad.blendingEnabled = NO;
							cad.alphaBlendOperation = _make_blend_op(rtd.BlendAlpha);
							cad.rgbBlendOperation = _make_blend_op(rtd.BlendRGB);
							cad.destinationAlphaBlendFactor = _make_blend_fact(rtd.BaseFactorAlpha);
							cad.destinationRGBBlendFactor = _make_blend_fact(rtd.BaseFactorRGB);
							cad.sourceAlphaBlendFactor = _make_blend_fact(rtd.OverFactorAlpha);
							cad.sourceRGBBlendFactor = _make_blend_fact(rtd.OverFactorRGB);
							[descriptor.colorAttachments setObject: cad atIndexedSubscript: i];
							[cad release];
						}
						if (desc.DepthStencil.Flags) {
							if (!IsDepthStencilFormat(desc.DepthStencil.Format)) throw Exception();
							descriptor.depthAttachmentPixelFormat = MakeMetalPixelFormat(desc.DepthStencil.Format);
							if (desc.DepthStencil.Format == PixelFormat::D24S8_unorm || desc.DepthStencil.Format == PixelFormat::D32S8_float) {
								descriptor.stencilAttachmentPixelFormat = MakeMetalPixelFormat(desc.DepthStencil.Format);
							}
						}
						descriptor.rasterizationEnabled = YES;
					} catch (...) { [descriptor release]; return 0; }
					NSError * error;
					result->state = [device newRenderPipelineStateWithDescriptor: descriptor error: &error];
					[descriptor release];
					if (!result->state) return 0;
					if ((desc.DepthStencil.Flags & DepthStencilFlagDepthTestEnabled) || (desc.DepthStencil.Flags & DepthStencilFlagStencilTestEnabled)) {
						MTLDepthStencilDescriptor * ds_descriptor = [[MTLDepthStencilDescriptor alloc] init];
						auto & ds_desc = desc.DepthStencil;
						if (desc.DepthStencil.Flags & DepthStencilFlagDepthTestEnabled) {
							ds_descriptor.depthCompareFunction = _make_comp_func(ds_desc.DepthTestFunction);
							if (desc.DepthStencil.Flags & DepthStencilFlagDepthWriteEnabled) ds_descriptor.depthWriteEnabled = YES;
							else ds_descriptor.depthWriteEnabled = NO;
						} else {
							ds_descriptor.depthCompareFunction = MTLCompareFunctionAlways;
							ds_descriptor.depthWriteEnabled = NO;
						}
						if (desc.DepthStencil.Flags & DepthStencilFlagStencilTestEnabled) {
							ds_descriptor.frontFaceStencil.readMask = ds_desc.StencilReadMask;
							ds_descriptor.backFaceStencil.readMask = ds_desc.StencilReadMask;
							ds_descriptor.frontFaceStencil.writeMask = ds_desc.StencilWriteMask;
							ds_descriptor.backFaceStencil.writeMask = ds_desc.StencilWriteMask;
							ds_descriptor.frontFaceStencil.stencilFailureOperation = _make_stencil_func(ds_desc.FrontStencil.OnStencilTestFailed);
							ds_descriptor.frontFaceStencil.depthFailureOperation = _make_stencil_func(ds_desc.FrontStencil.OnDepthTestFailed);
							ds_descriptor.frontFaceStencil.depthStencilPassOperation = _make_stencil_func(ds_desc.FrontStencil.OnTestsPassed);
							ds_descriptor.frontFaceStencil.stencilCompareFunction = _make_comp_func(ds_desc.FrontStencil.TestFunction);
							ds_descriptor.backFaceStencil.stencilFailureOperation = _make_stencil_func(ds_desc.BackStencil.OnStencilTestFailed);
							ds_descriptor.backFaceStencil.depthFailureOperation = _make_stencil_func(ds_desc.BackStencil.OnDepthTestFailed);
							ds_descriptor.backFaceStencil.depthStencilPassOperation = _make_stencil_func(ds_desc.BackStencil.OnTestsPassed);
							ds_descriptor.backFaceStencil.stencilCompareFunction = _make_comp_func(ds_desc.BackStencil.TestFunction);
						} else {
							ds_descriptor.frontFaceStencil.stencilFailureOperation = MTLStencilOperationKeep;
							ds_descriptor.frontFaceStencil.depthFailureOperation = MTLStencilOperationKeep;
							ds_descriptor.frontFaceStencil.depthStencilPassOperation = MTLStencilOperationKeep;
							ds_descriptor.frontFaceStencil.stencilCompareFunction = MTLCompareFunctionAlways;
							ds_descriptor.frontFaceStencil.readMask = 0;
							ds_descriptor.frontFaceStencil.writeMask = 0;
							ds_descriptor.backFaceStencil.stencilFailureOperation = MTLStencilOperationKeep;
							ds_descriptor.backFaceStencil.depthFailureOperation = MTLStencilOperationKeep;
							ds_descriptor.backFaceStencil.depthStencilPassOperation = MTLStencilOperationKeep;
							ds_descriptor.backFaceStencil.stencilCompareFunction = MTLCompareFunctionAlways;
							ds_descriptor.backFaceStencil.readMask = 0;
							ds_descriptor.backFaceStencil.writeMask = 0;
						}
						result->ds_state = [device newDepthStencilStateWithDescriptor: ds_descriptor];
						[ds_descriptor release];
						if (!result->ds_state) return 0;
					}
					result->depth_bias = desc.Rasterization.DepthBias;
					result->depth_bias_clamp = desc.Rasterization.DepthBiasClamp;
					result->slope_scaled_depth_bias = desc.Rasterization.SlopeScaledDepthBias;
					if (desc.Rasterization.DepthClipEnable) result->depth_clip = MTLDepthClipModeClip;
					else result->depth_clip = MTLDepthClipModeClamp;
					if (desc.Rasterization.Fill == FillMode::Solid) result->fill_mode = MTLTriangleFillModeFill;
					else if (desc.Rasterization.Fill == FillMode::Wireframe) result->fill_mode = MTLTriangleFillModeLines;
					else result->fill_mode = MTLTriangleFillModeFill;
					if (desc.Rasterization.Cull == CullMode::None) result->cull_mode = MTLCullModeNone;
					else if (desc.Rasterization.Cull == CullMode::Front) result->cull_mode = MTLCullModeFront;
					else if (desc.Rasterization.Cull == CullMode::Back) result->cull_mode = MTLCullModeBack;
					else result->cull_mode = MTLCullModeNone;
					if (desc.Rasterization.FrontIsCounterClockwise) result->front_winding = MTLWindingCounterClockwise;
					else result->front_winding = MTLWindingClockwise;
					if (desc.Topology == PrimitiveTopology::PointList) result->primitive_type = MTLPrimitiveTypePoint;
					else if (desc.Topology == PrimitiveTopology::LineList) result->primitive_type = MTLPrimitiveTypeLine;
					else if (desc.Topology == PrimitiveTopology::LineStrip) result->primitive_type = MTLPrimitiveTypeLineStrip;
					else if (desc.Topology == PrimitiveTopology::TriangleList) result->primitive_type = MTLPrimitiveTypeTriangle;
					else if (desc.Topology == PrimitiveTopology::TriangleStrip) result->primitive_type = MTLPrimitiveTypeTriangleStrip;
					else result->primitive_type = MTLPrimitiveTypeTriangle;
					result->Retain();
					return result;
				}
			}
			virtual ISamplerState * CreateSamplerState(const SamplerDesc & desc) noexcept override
			{
				SafePointer<MTL_SamplerState> sampler = new (std::nothrow) MTL_SamplerState(this);
				if (!sampler) return 0;
				MTLSamplerDescriptor * descriptor = [[MTLSamplerDescriptor alloc] init];
				descriptor.normalizedCoordinates = YES;
				descriptor.sAddressMode = _make_address_mode(desc.AddressU);
				descriptor.tAddressMode = _make_address_mode(desc.AddressV);
				descriptor.rAddressMode = _make_address_mode(desc.AddressW);
				if (desc.BorderColor[3] < 0.5f) descriptor.borderColor = MTLSamplerBorderColorTransparentBlack;
				else if (desc.BorderColor[0] < 0.5f && desc.BorderColor[1] < 0.5f && desc.BorderColor[2] < 0.5f) descriptor.borderColor = MTLSamplerBorderColorOpaqueBlack;
				else descriptor.borderColor = MTLSamplerBorderColorOpaqueWhite;
				descriptor.minFilter = _make_min_mag_filter(desc.MinificationFilter);
				descriptor.magFilter = _make_min_mag_filter(desc.MagnificationFilter);
				descriptor.mipFilter = _make_mip_filter(desc.MipFilter);
				descriptor.lodMinClamp = desc.MinimalLOD;
				descriptor.lodMaxClamp = desc.MaximalLOD;
				if (desc.MinificationFilter == SamplerFilter::Anisotropic && desc.MagnificationFilter == SamplerFilter::Anisotropic && desc.MipFilter == SamplerFilter::Anisotropic) {
					descriptor.maxAnisotropy = desc.MaximalAnisotropy;
				} else descriptor.maxAnisotropy = 1;
				sampler->state = [device newSamplerStateWithDescriptor: descriptor];
				[descriptor release];
				if (!sampler->state) return 0;
				sampler->Retain();
				return sampler;
			}
			virtual IBuffer * CreateBuffer(const BufferDesc & desc) noexcept override
			{
				if (desc.Usage & ~ResourceUsageBufferMask) return 0;
				if (!desc.Length) return 0;
				if (desc.MemoryPool == ResourceMemoryPool::Immutable || desc.MemoryPool == ResourceMemoryPool::Shared) return 0;
				SafePointer<MTL_Buffer> buffer = new (std::nothrow) MTL_Buffer(this);
				if (!buffer) return 0;
				if ((desc.Usage & ResourceUsageShaderAll) || (desc.Usage & ResourceUsageConstantBuffer) || (desc.Usage & ResourceUsageIndexBuffer)) {
					buffer->usage |= ResourceUsageShaderAll;
					buffer->usage |= ResourceUsageConstantBuffer;
					buffer->usage |= ResourceUsageIndexBuffer;
				}
				if (desc.Usage & ResourceUsageCPUAll) {
					buffer->usage |= ResourceUsageCPUAll;
				}
				MTLResourceOptions options;
				if (buffer->usage & ResourceUsageCPUAll) options = MTLResourceStorageModePrivate;
				else options = MTLResourceStorageModeManaged;
				buffer->buffer = [device newBufferWithLength: desc.Length options: options];
				if (!buffer->buffer) return 0;
				buffer->size = desc.Length;
				buffer->Retain();
				return buffer;
			}
			virtual IBuffer * CreateBuffer(const BufferDesc & desc, const ResourceInitDesc & init) noexcept override
			{
				if (desc.Usage & ~ResourceUsageBufferMask) return 0;
				if (!desc.Length || desc.MemoryPool == ResourceMemoryPool::Shared) return 0;
				if (desc.MemoryPool == ResourceMemoryPool::Immutable) {
					if (desc.Usage & ResourceUsageShaderWrite) return 0;
					if (desc.Usage & ResourceUsageCPUAll) return 0;
				}
				SafePointer<MTL_Buffer> buffer = new (std::nothrow) MTL_Buffer(this);
				if (!buffer) return 0;
				if ((desc.Usage & ResourceUsageShaderAll) || (desc.Usage & ResourceUsageConstantBuffer) || (desc.Usage & ResourceUsageIndexBuffer)) {
					buffer->usage |= ResourceUsageShaderAll;
					buffer->usage |= ResourceUsageConstantBuffer;
					buffer->usage |= ResourceUsageIndexBuffer;
				}
				buffer->usage |= ResourceUsageCPUAll;
				buffer->buffer = [device newBufferWithBytes: init.Data length: desc.Length options: MTLResourceStorageModeManaged];
				if (!buffer->buffer) return 0;
				buffer->size = desc.Length;
				buffer->Retain();
				return buffer;
			}
			virtual ITexture * CreateTexture(const TextureDesc & desc) noexcept override
			{
				if (desc.Usage & ~ResourceUsageTextureMask) return 0;
				if (desc.MemoryPool == ResourceMemoryPool::Immutable) return 0;
				if (desc.Usage & ResourceUsageRenderTarget) {
					if (!Graphics::IsColorFormat(desc.Format)) return 0;
				}
				if (desc.Usage & ResourceUsageDepthStencil) {
					if (!Graphics::IsDepthStencilFormat(desc.Format)) return 0;
				}
				if (desc.MemoryPool == ResourceMemoryPool::Shared) {
					if (desc.Usage & (ResourceUsageCPUAll | ResourceUsageVideoAll)) return 0;
				}
				SafePointer<MTL_Texture> texture = new (std::nothrow) MTL_Texture(this);
				if (!texture) return 0;
				MTLTextureDescriptor * descriptor = [[MTLTextureDescriptor alloc] init];
				descriptor.width = desc.Width;
				texture->type = desc.Type;
				texture->format = desc.Format;
				texture->width = desc.Width;
				texture->height = texture->depth = texture->size = 1;
				if (desc.Width < 1 || desc.Width > 16384) return 0;
				if (desc.Type == TextureType::Type1D) {
					descriptor.textureType = MTLTextureType1D;
				} else if (desc.Type == TextureType::TypeArray1D) {
					if (desc.ArraySize < 1 || desc.ArraySize > 2048) return 0;
					descriptor.arrayLength = desc.ArraySize;
					descriptor.textureType = MTLTextureType1DArray;
					texture->size = desc.ArraySize;
				} else if (desc.Type == TextureType::Type2D) {
					if (desc.Height < 1 || desc.Height > 16384) return 0;
					descriptor.height = desc.Height;
					descriptor.textureType = MTLTextureType2D;
					texture->height = desc.Height;
				} else if (desc.Type == TextureType::TypeArray2D) {
					if (desc.Height < 1 || desc.Height > 16384) return 0;
					if (desc.ArraySize < 1 || desc.ArraySize > 2048) return 0;
					descriptor.height = desc.Height;
					descriptor.arrayLength = desc.ArraySize;
					descriptor.textureType = MTLTextureType2DArray;
					texture->height = desc.Height;
					texture->size = desc.ArraySize;
				} else if (desc.Type == TextureType::TypeCube) {
					if (desc.Width != desc.Height) return 0;
					descriptor.height = desc.Height;
					descriptor.textureType = MTLTextureTypeCube;
					texture->height = desc.Height;
				} else if (desc.Type == TextureType::TypeArrayCube) {
					if (desc.Width != desc.Height) return 0;
					if (desc.ArraySize < 1 || desc.ArraySize > 2048) return 0;
					descriptor.height = desc.Height;
					descriptor.arrayLength = desc.ArraySize;
					descriptor.textureType = MTLTextureTypeCubeArray;
					texture->height = desc.Height;
					texture->size = desc.ArraySize;
				} else if (desc.Type == TextureType::Type3D) {
					if (desc.Height < 1 || desc.Height > 16384) return 0;
					if (desc.Depth < 1 || desc.Depth > 2048) return 0;
					descriptor.height = desc.Height;
					descriptor.depth = desc.Depth;
					descriptor.textureType = MTLTextureType3D;
					texture->height = desc.Height;
					texture->depth = desc.Depth;
				} else { [descriptor release]; return 0; }
				uint32 mips = desc.MipmapCount;
				if (!mips) {
					uint32 mx_size = max(max(texture->width, texture->height), texture->depth);
					while (mx_size) { mips++; mx_size /= 2; }
				}
				auto metal_format = MakeMetalPixelFormat(desc.Format);
				if (metal_format == MTLPixelFormatInvalid) { [descriptor release]; return 0; }
				descriptor.mipmapLevelCount = mips;
				descriptor.pixelFormat = metal_format;
				descriptor.usage = 0;
				if (desc.Usage & ResourceUsageShaderRead) {
					descriptor.usage |= MTLTextureUsageShaderRead;
					texture->usage |= ResourceUsageShaderRead;
				}
				if (desc.Usage & ResourceUsageShaderWrite) {
					descriptor.usage |= MTLTextureUsageShaderWrite;
					texture->usage |= ResourceUsageShaderWrite;
				}
				if ((desc.Usage & ResourceUsageRenderTarget) || (desc.Usage & ResourceUsageDepthStencil)) {
					descriptor.usage |= MTLTextureUsageRenderTarget;
					if (Graphics::IsColorFormat(desc.Format)) texture->usage |= ResourceUsageRenderTarget;
					if (Graphics::IsDepthStencilFormat(desc.Format)) texture->usage |= ResourceUsageDepthStencil;
				}
				if (desc.Usage & (ResourceUsageCPUAll | ResourceUsageVideoAll)) texture->usage |= ResourceUsageCPUAll | ResourceUsageVideoAll;
				if (texture->usage & (ResourceUsageCPUAll | ResourceUsageVideoAll)) descriptor.storageMode = MTLStorageModeManaged;
				else descriptor.storageMode = MTLStorageModePrivate;
				if (desc.MemoryPool == ResourceMemoryPool::Shared) texture->texture = [device newSharedTextureWithDescriptor: descriptor];
				else texture->texture = [device newTextureWithDescriptor: descriptor];
				[descriptor release];
				if (!texture->texture) return 0;
				if (desc.MemoryPool == ResourceMemoryPool::Shared && !texture->SharedTextureInit()) return 0;
				texture->Retain();
				return texture;
			}
			virtual ITexture * CreateTexture(const TextureDesc & desc, const ResourceInitDesc * init) noexcept override
			{
				if (!init) return 0;
				if (desc.Usage & ~ResourceUsageTextureMask || desc.MemoryPool == ResourceMemoryPool::Shared) return 0;
				if (desc.MemoryPool == ResourceMemoryPool::Immutable) {
					if (desc.Usage & ResourceUsageShaderWrite) return 0;
					if (desc.Usage & ResourceUsageCPUAll) return 0;
					if (desc.Usage & ResourceUsageRenderTarget) return 0;
					if (desc.Usage & ResourceUsageDepthStencil) return 0;
					if (desc.Usage & ResourceUsageVideoWrite) return 0;
				}
				if (desc.Usage & ResourceUsageRenderTarget) {
					if (!Graphics::IsColorFormat(desc.Format)) return 0;
				}
				if (desc.Usage & ResourceUsageDepthStencil) {
					if (!Graphics::IsDepthStencilFormat(desc.Format)) return 0;
				}
				SafePointer<MTL_Texture> texture = new (std::nothrow) MTL_Texture(this);
				if (!texture) return 0;
				MTLTextureDescriptor * descriptor = [[MTLTextureDescriptor alloc] init];
				descriptor.width = desc.Width;
				texture->type = desc.Type;
				texture->format = desc.Format;
				texture->width = desc.Width;
				texture->height = texture->depth = texture->size = 1;
				if (desc.Width < 1 || desc.Width > 16384) return 0;
				if (desc.Type == TextureType::Type1D) {
					descriptor.textureType = MTLTextureType1D;
				} else if (desc.Type == TextureType::TypeArray1D) {
					if (desc.ArraySize < 1 || desc.ArraySize > 2048) return 0;
					descriptor.arrayLength = desc.ArraySize;
					descriptor.textureType = MTLTextureType1DArray;
					texture->size = desc.ArraySize;
				} else if (desc.Type == TextureType::Type2D) {
					if (desc.Height < 1 || desc.Height > 16384) return 0;
					descriptor.height = desc.Height;
					descriptor.textureType = MTLTextureType2D;
					texture->height = desc.Height;
				} else if (desc.Type == TextureType::TypeArray2D) {
					if (desc.Height < 1 || desc.Height > 16384) return 0;
					if (desc.ArraySize < 1 || desc.ArraySize > 2048) return 0;
					descriptor.height = desc.Height;
					descriptor.arrayLength = desc.ArraySize;
					descriptor.textureType = MTLTextureType2DArray;
					texture->height = desc.Height;
					texture->size = desc.ArraySize;
				} else if (desc.Type == TextureType::TypeCube) {
					if (desc.Width != desc.Height) return 0;
					descriptor.height = desc.Height;
					descriptor.textureType = MTLTextureTypeCube;
					texture->height = desc.Height;
				} else if (desc.Type == TextureType::TypeArrayCube) {
					if (desc.Width != desc.Height) return 0;
					if (desc.ArraySize < 1 || desc.ArraySize > 2048) return 0;
					descriptor.height = desc.Height;
					descriptor.arrayLength = desc.ArraySize;
					descriptor.textureType = MTLTextureTypeCubeArray;
					texture->height = desc.Height;
					texture->size = desc.ArraySize;
				} else if (desc.Type == TextureType::Type3D) {
					if (desc.Height < 1 || desc.Height > 16384) return 0;
					if (desc.Depth < 1 || desc.Depth > 2048) return 0;
					descriptor.height = desc.Height;
					descriptor.depth = desc.Depth;
					descriptor.textureType = MTLTextureType3D;
					texture->height = desc.Height;
					texture->depth = desc.Depth;
				} else { [descriptor release]; return 0; }
				uint32 mips = desc.MipmapCount;
				if (!mips) {
					uint32 mx_size = max(max(texture->width, texture->height), texture->depth);
					while (mx_size) { mips++; mx_size /= 2; }
				}
				auto metal_format = MakeMetalPixelFormat(desc.Format);
				if (metal_format == MTLPixelFormatInvalid) { [descriptor release]; return 0; }
				descriptor.mipmapLevelCount = mips;
				descriptor.pixelFormat = metal_format;
				descriptor.usage = 0;
				if (desc.Usage & ResourceUsageShaderRead) {
					descriptor.usage |= MTLTextureUsageShaderRead;
					texture->usage |= ResourceUsageShaderRead;
				}
				if (desc.Usage & ResourceUsageShaderWrite) {
					descriptor.usage |= MTLTextureUsageShaderWrite;
					texture->usage |= ResourceUsageShaderWrite;
				}
				if ((desc.Usage & ResourceUsageRenderTarget) || (desc.Usage & ResourceUsageDepthStencil)) {
					descriptor.usage |= MTLTextureUsageRenderTarget;
					if (Graphics::IsColorFormat(desc.Format)) texture->usage |= ResourceUsageRenderTarget;
					if (Graphics::IsDepthStencilFormat(desc.Format)) texture->usage |= ResourceUsageDepthStencil;
				}
				texture->usage |= ResourceUsageCPUAll | ResourceUsageVideoAll;
				descriptor.storageMode = MTLStorageModeManaged;
				texture->texture = [device newTextureWithDescriptor: descriptor];
				[descriptor release];
				if (!texture->texture) return 0;
				if (desc.Type == TextureType::Type1D || desc.Type == TextureType::TypeArray1D) {
					for (uint j = 0; j < texture->size; j++) for (uint i = 0; i < mips; i++) {
						uint subres = i + j * mips;
						VolumeIndex size(texture->width);
						_evaluate_mipmap_size(i, size);
						[texture->texture replaceRegion: MTLRegionMake1D(0, size.x) mipmapLevel: i slice: j
							withBytes: init[subres].Data bytesPerRow: 0 bytesPerImage: 0];
					}
				} else if (desc.Type == TextureType::Type2D || desc.Type == TextureType::TypeArray2D) {
					for (uint j = 0; j < texture->size; j++) for (uint i = 0; i < mips; i++) {
						uint subres = i + j * mips;
						VolumeIndex size(texture->width, texture->height);
						_evaluate_mipmap_size(i, size);
						[texture->texture replaceRegion: MTLRegionMake2D(0, 0, size.x, size.y) mipmapLevel: i slice: j
							withBytes: init[subres].Data bytesPerRow: init[subres].DataPitch bytesPerImage: 0];
					}
				} else if (desc.Type == TextureType::TypeCube || desc.Type == TextureType::TypeArrayCube) {
					for (uint j = 0; j < texture->size * 6; j++) for (uint i = 0; i < mips; i++) {
						uint subres = i + j * mips;
						VolumeIndex size(texture->width, texture->height);
						_evaluate_mipmap_size(i, size);
						[texture->texture replaceRegion: MTLRegionMake2D(0, 0, size.x, size.y) mipmapLevel: i slice: j
							withBytes: init[subres].Data bytesPerRow: init[subres].DataPitch bytesPerImage: 0];
					}
				} else if (desc.Type == TextureType::Type3D) {
					for (uint i = 0; i < mips; i++) {
						VolumeIndex size(texture->width, texture->height, texture->depth);
						_evaluate_mipmap_size(i, size);
						[texture->texture replaceRegion: MTLRegionMake3D(0, 0, 0, size.x, size.y, size.z)
							mipmapLevel: i slice: 0 withBytes: init[i].Data bytesPerRow: init[i].DataPitch bytesPerImage: init[i].DataSlicePitch];
					}
				}
				texture->Retain();
				return texture;
			}
			virtual ITexture * CreateRenderTargetView(ITexture * texture, uint32 mip_level, uint32 array_offset_or_depth) noexcept override
			{
				if (!texture || texture->GetParentDevice() != this || !(texture->GetResourceUsage() & ResourceUsageRenderTarget)) return 0;
				SafePointer<MTL_Texture> result = new (std::nothrow) MTL_Texture(this);
				if (!result) return 0;
				auto source = static_cast<MTL_Texture *>(texture);
				result->texture = source->texture;
				[result->texture retain];
				result->type = source->type;
				result->format = source->format;
				result->width = source->width;
				result->height = source->height;
				result->depth = source->depth;
				result->size = source->size;
				result->usage = ResourceUsageRenderTarget;
				result->rtv_mip = mip_level;
				if (result->type == TextureType::TypeArray1D || result->type == TextureType::TypeArray2D ||
					result->type == TextureType::TypeCube || result->type == TextureType::TypeArrayCube) {
					result->rtv_slice = array_offset_or_depth;
				} else if (result->type == TextureType::Type3D) {
					result->rtv_depth = array_offset_or_depth;
				}
				result->Retain();
				return result;
			}
			virtual IDeviceResource * OpenResource(IDeviceResourceHandle * handle) noexcept override
			{
				if (!handle || handle->GetDeviceIdentifier() != GetDeviceIdentifier()) return 0;
				try {
					SafePointer<MTL_SharedInfrastructure> shared = new MTL_SharedInfrastructure(static_cast<MTL_DeviceResourceHandle *>(handle));
					SafePointer<MTL_Texture> result = new (std::nothrow) MTL_Texture(this);
					result->shared = shared;
					result->type = static_cast<TextureType>(shared->_meta->type);
					result->format = static_cast<PixelFormat>(shared->_meta->format);
					result->usage = shared->_meta->usage;
					result->width = shared->_meta->width;
					result->height = shared->_meta->height;
					result->depth = shared->_meta->depth;
					result->size = shared->_meta->size;
					auto hook = [[ERTXPCHookIn alloc] init];
					if (!hook) throw OutOfMemoryException();
					hook->port = shared->_shared_port;
					MTLSharedTextureHandle * metal_handle = 0;
					@try { metal_handle = [[MTLSharedTextureHandle alloc] initWithCoder: hook]; } @catch (NSException *exception) {} @finally {}
					[hook release];
					result->texture = [device newSharedTextureWithHandle: metal_handle];
					[metal_handle release];
					if (!result->texture) return 0;
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			virtual IWindowLayer * CreateWindowLayer(Windows::ICoreWindow * window, const WindowLayerDesc & desc) noexcept override
			{
				try {
					if (desc.Width > 16384 || desc.Height > 16384) return 0;
					SafePointer<MTL_WindowLayer> layer = new MTL_WindowLayer(this, context, desc, window);
					layer->Retain();
					return layer;
				} catch (...) { return 0; }
			}
			virtual string ToString(void) const override { return L"MTL_Device"; }
		};
		class MTL_Factory : public IDeviceFactory
		{
		public:
			virtual Volumes::Dictionary<uint64, string> * GetAvailableDevices(void) noexcept override
			{
				SafePointer< Volumes::Dictionary<uint64, string> > result = new (std::nothrow) Volumes::Dictionary<uint64, string>;
				NSArray< id<MTLDevice> > * devs = MTLCopyAllDevices();
				try {
					for (int i = 0; i < [devs count]; i++) {
						auto dev = [devs objectAtIndex: i];
						result->Append([dev registryID], Cocoa::EngineString([dev name]));
					}
				} catch (...) {
					[devs release];
					return 0;
				}
				[devs release];
				result->Retain();
				return result;
			}
			virtual IDevice * CreateDevice(uint64 identifier) noexcept override
			{
				NSArray< id<MTLDevice> > * devs = MTLCopyAllDevices();
				id<MTLDevice> selected = 0;
				for (int i = 0; i < [devs count]; i++) {
					auto dev = [devs objectAtIndex: i];
					if ([dev registryID] == identifier) { selected = dev; break; }
				}
				if (selected) {
					SafePointer<MTL_Device> device = new (std::nothrow) MTL_Device(selected);
					[devs release];
					if (!device) return 0;
					device->Retain();
					return device;
				} else {
					[devs release];
					return 0;
				}
			}
			virtual IDevice * CreateDefaultDevice(void) noexcept override
			{
				id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
				SafePointer<MTL_Device> device = new (std::nothrow) MTL_Device(dev);
				[dev release];
				if (!device) return 0;
				device->Retain();
				return device;
			}
			virtual IDeviceResourceHandle * QueryResourceHandle(IDeviceResource * resource) noexcept override
			{
				if (!resource || resource->GetResourceType() != ResourceType::Texture || resource->GetMemoryPool() != ResourceMemoryPool::Shared) return 0;
				MTL_SharedInfrastructure * shared = static_cast<MTL_Texture *>(resource)->shared;
				if (!shared) return 0;
				MTL_DeviceResourceHandle * handle = shared->_handle;
				if (!handle) return 0;
				handle->Retain();
				return handle;
			}
			virtual IDeviceResourceHandle * OpenResourceHandle(const DataBlock * data) noexcept override { try { return new MTL_DeviceResourceHandle(data); } catch (...) { return 0; } }
			virtual string ToString(void) const override { return L"MTL_Factory"; }
		};

		SafePointer<MTL_Device> common_device;

		Graphics::IDeviceFactory * CreateMetalDeviceFactory(void)
		{
			SafePointer<MTL_Factory> factory = new (std::nothrow) MTL_Factory;
			if (!factory) return 0;
			factory->Retain();
			return factory;
		}
		Graphics::IDevice * GetMetalCommonDevice(void)
		{
			if (!common_device) {
				id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
				common_device = new (std::nothrow) MTL_Device(dev);
				[dev release];
			}
			return common_device;
		}

		id<MTLDevice> GetInnerMetalDevice(Graphics::IDevice * wrapper) { return static_cast<MTL_Device *>(wrapper)->device; }
		id<MTLCommandQueue> GetInnerMetalQueue(Graphics::IDevice * wrapper) { return static_cast<MTL_Device *>(wrapper)->queue; }
		id<MTLBuffer> GetInnerMetalBuffer(Graphics::IBuffer * buffer) { return static_cast<MTL_Buffer *>(buffer)->buffer; }
		id<MTLTexture> GetInnerMetalTexture(Graphics::ITexture * texture) { return static_cast<MTL_Texture *>(texture)->texture; }
		MTLPixelFormat MakeMetalPixelFormat(Graphics::PixelFormat format)
		{
			if (IsColorFormat(format)) {
				auto bpp = GetFormatBitsPerPixel(format);
				if (bpp == 8) {
					if (format == PixelFormat::A8_unorm) return MTLPixelFormatA8Unorm;
					else if (format == PixelFormat::R8_unorm) return MTLPixelFormatR8Unorm;
					else if (format == PixelFormat::R8_snorm) return MTLPixelFormatR8Snorm;
					else if (format == PixelFormat::R8_uint) return MTLPixelFormatR8Uint;
					else if (format == PixelFormat::R8_sint) return MTLPixelFormatR8Sint;
					else return MTLPixelFormatInvalid;
				} else if (bpp == 16) {
					if (format == PixelFormat::R16_unorm) return MTLPixelFormatR16Unorm;
					else if (format == PixelFormat::R16_snorm) return MTLPixelFormatR16Snorm;
					else if (format == PixelFormat::R16_uint) return MTLPixelFormatR16Uint;
					else if (format == PixelFormat::R16_sint) return MTLPixelFormatR16Sint;
					else if (format == PixelFormat::R16_float) return MTLPixelFormatR16Float;
					else if (format == PixelFormat::R8G8_unorm) return MTLPixelFormatRG8Unorm;
					else if (format == PixelFormat::R8G8_snorm) return MTLPixelFormatRG8Snorm;
					else if (format == PixelFormat::R8G8_uint) return MTLPixelFormatRG8Uint;
					else if (format == PixelFormat::R8G8_sint) return MTLPixelFormatRG8Sint;
					else if (format == PixelFormat::B5G6R5_unorm) return MTLPixelFormatB5G6R5Unorm;
					else if (format == PixelFormat::B5G5R5A1_unorm) return MTLPixelFormatBGR5A1Unorm;
					else if (format == PixelFormat::A1B5G5R5_unorm) return MTLPixelFormatA1BGR5Unorm;
					else if (format == PixelFormat::A4B4G4R4_unorm) return MTLPixelFormatABGR4Unorm;
					else return MTLPixelFormatInvalid;
				} else if (bpp == 32) {
					if (format == PixelFormat::R32_uint) return MTLPixelFormatR32Uint;
					else if (format == PixelFormat::R32_sint) return MTLPixelFormatR32Sint;
					else if (format == PixelFormat::R32_float) return MTLPixelFormatR32Float;
					else if (format == PixelFormat::R16G16_unorm) return MTLPixelFormatRG16Unorm;
					else if (format == PixelFormat::R16G16_snorm) return MTLPixelFormatRG16Snorm;
					else if (format == PixelFormat::R16G16_uint) return MTLPixelFormatRG16Uint;
					else if (format == PixelFormat::R16G16_sint) return MTLPixelFormatRG16Sint;
					else if (format == PixelFormat::R16G16_float) return MTLPixelFormatRG16Float;
					else if (format == PixelFormat::B8G8R8A8_unorm) return MTLPixelFormatBGRA8Unorm;
					else if (format == PixelFormat::R8G8B8A8_unorm) return MTLPixelFormatRGBA8Unorm;
					else if (format == PixelFormat::R8G8B8A8_snorm) return MTLPixelFormatRGBA8Snorm;
					else if (format == PixelFormat::R8G8B8A8_uint) return MTLPixelFormatRGBA8Uint;
					else if (format == PixelFormat::R8G8B8A8_sint) return MTLPixelFormatRGBA8Sint;
					else if (format == PixelFormat::R10G10B10A2_unorm) return MTLPixelFormatRGB10A2Unorm;
					else if (format == PixelFormat::R10G10B10A2_uint) return MTLPixelFormatRGB10A2Uint;
					else if (format == PixelFormat::R11G11B10_float) return MTLPixelFormatRG11B10Float;
					else if (format == PixelFormat::R9G9B9E5_float) return MTLPixelFormatRGB9E5Float;
					else return MTLPixelFormatInvalid;
				} else if (bpp == 64) {
					if (format == PixelFormat::R32G32_uint) return MTLPixelFormatRG32Uint;
					else if (format == PixelFormat::R32G32_sint) return MTLPixelFormatRG32Sint;
					else if (format == PixelFormat::R32G32_float) return MTLPixelFormatRG32Float;
					else if (format == PixelFormat::R16G16B16A16_unorm) return MTLPixelFormatRGBA16Unorm;
					else if (format == PixelFormat::R16G16B16A16_snorm) return MTLPixelFormatRGBA16Snorm;
					else if (format == PixelFormat::R16G16B16A16_uint) return MTLPixelFormatRGBA16Uint;
					else if (format == PixelFormat::R16G16B16A16_sint) return MTLPixelFormatRGBA16Sint;
					else if (format == PixelFormat::R16G16B16A16_float) return MTLPixelFormatRGBA16Float;
					else return MTLPixelFormatInvalid;
				} else if (bpp == 128) {
					if (format == PixelFormat::R32G32B32A32_uint) return MTLPixelFormatRGBA32Uint;
					else if (format == PixelFormat::R32G32B32A32_sint) return MTLPixelFormatRGBA32Sint;
					else if (format == PixelFormat::R32G32B32A32_float) return MTLPixelFormatRGBA32Float;
					else return MTLPixelFormatInvalid;
				} else return MTLPixelFormatInvalid;
			} else if (IsDepthStencilFormat(format)) {
				if (format == PixelFormat::D16_unorm) return MTLPixelFormatDepth16Unorm;
				else if (format == PixelFormat::D32_float) return MTLPixelFormatDepth32Float;
				else if (format == PixelFormat::D24S8_unorm) return MTLPixelFormatDepth24Unorm_Stencil8;
				else if (format == PixelFormat::D32S8_float) return MTLPixelFormatDepth32Float_Stencil8;
				else return MTLPixelFormatInvalid;
			} else return MTLPixelFormatInvalid;
		}
		void UpdateMetalTexture(Graphics::ITexture * dest, const void * data, uint stride, Graphics::SubresourceIndex subres)
		{
			auto surface = GetInnerMetalTexture(dest);
			auto region = MTLRegionMake3D(0, 0, 0, dest->GetWidth(), dest->GetHeight(), 1);
			[surface replaceRegion: region mipmapLevel: subres.mip_level slice: subres.array_index withBytes: data bytesPerRow: stride bytesPerImage: 0];
		}
		void QueryMetalTexture(Graphics::ITexture * from, void * buffer, uint stride, Graphics::SubresourceIndex subres)
		{
			auto surface = GetInnerMetalTexture(from);
			auto region = MTLRegionMake3D(0, 0, 0, from->GetWidth(), from->GetHeight(), 1);
			auto device = GetInnerMetalDevice(from->GetParentDevice());
			auto queue = GetInnerMetalQueue(from->GetParentDevice());
			@autoreleasepool {
				auto command = [queue commandBuffer];
				auto blt = [command blitCommandEncoder];
				[blt synchronizeTexture: surface slice: subres.array_index level: subres.mip_level];
				[blt endEncoding];
				[command commit];
				[command waitUntilCompleted];
			}
			[surface getBytes: buffer bytesPerRow: stride bytesPerImage: 0 fromRegion: region mipmapLevel: subres.mip_level slice: subres.array_index];
		}
		void QueryMetalVersion(id<MTLDevice> device, uint & major, uint & minor) noexcept
		{
			if ([device supportsFamily: MTLGPUFamilyMetal3]) major = 3;
			else if ([device supportsFamily: MTLGPUFamilyMac2]) major = 2;
			else major = 1;
			minor = 0;
		}
		bool QueryMetalFormatSupportForShaderRead(id<MTLDevice> device, Graphics::PixelFormat format) noexcept
		{
			if (IsColorFormat(format)) {
				auto bpp = GetFormatBitsPerPixel(format);
				if (bpp == 8) {
					if (format == PixelFormat::A8_unorm) return true;
					else if (format == PixelFormat::R8_unorm) return true;
					else if (format == PixelFormat::R8_snorm) return true;
					else if (format == PixelFormat::R8_uint) return true;
					else if (format == PixelFormat::R8_sint) return true;
					else return false;
				} else if (bpp == 16) {
					if (format == PixelFormat::R16_unorm) return true;
					else if (format == PixelFormat::R16_snorm) return true;
					else if (format == PixelFormat::R16_uint) return true;
					else if (format == PixelFormat::R16_sint) return true;
					else if (format == PixelFormat::R16_float) return true;
					else if (format == PixelFormat::R8G8_unorm) return true;
					else if (format == PixelFormat::R8G8_snorm) return true;
					else if (format == PixelFormat::R8G8_uint) return true;
					else if (format == PixelFormat::R8G8_sint) return true;
					else if (format == PixelFormat::B5G6R5_unorm) return true;
					else if (format == PixelFormat::B5G5R5A1_unorm) return true;
					else if (format == PixelFormat::A1B5G5R5_unorm) return true;
					else if (format == PixelFormat::A4B4G4R4_unorm) return true;
					else return false;
				} else if (bpp == 32) {
					if (format == PixelFormat::R32_uint) return true;
					else if (format == PixelFormat::R32_sint) return true;
					else if (format == PixelFormat::R32_float) return true;
					else if (format == PixelFormat::R16G16_unorm) return true;
					else if (format == PixelFormat::R16G16_snorm) return true;
					else if (format == PixelFormat::R16G16_uint) return true;
					else if (format == PixelFormat::R16G16_sint) return true;
					else if (format == PixelFormat::R16G16_float) return true;
					else if (format == PixelFormat::B8G8R8A8_unorm) return true;
					else if (format == PixelFormat::R8G8B8A8_unorm) return true;
					else if (format == PixelFormat::R8G8B8A8_snorm) return true;
					else if (format == PixelFormat::R8G8B8A8_uint) return true;
					else if (format == PixelFormat::R8G8B8A8_sint) return true;
					else if (format == PixelFormat::R10G10B10A2_unorm) return true;
					else if (format == PixelFormat::R10G10B10A2_uint) return true;
					else if (format == PixelFormat::R11G11B10_float) return true;
					else if (format == PixelFormat::R9G9B9E5_float) return true;
					else return false;
				} else if (bpp == 64) {
					if (format == PixelFormat::R32G32_uint) return true;
					else if (format == PixelFormat::R32G32_sint) return true;
					else if (format == PixelFormat::R32G32_float) return true;
					else if (format == PixelFormat::R16G16B16A16_unorm) return true;
					else if (format == PixelFormat::R16G16B16A16_snorm) return true;
					else if (format == PixelFormat::R16G16B16A16_uint) return true;
					else if (format == PixelFormat::R16G16B16A16_sint) return true;
					else if (format == PixelFormat::R16G16B16A16_float) return true;
					else return false;
				} else if (bpp == 128) {
					if (format == PixelFormat::R32G32B32A32_uint) return true;
					else if (format == PixelFormat::R32G32B32A32_sint) return true;
					else if (format == PixelFormat::R32G32B32A32_float) return true;
					else return false;
				} else return false;
			} else if (IsDepthStencilFormat(format)) {
				if (format == PixelFormat::D16_unorm) return true;
				else if (format == PixelFormat::D32_float) return true;
				else if (format == PixelFormat::D24S8_unorm) return [device isDepth24Stencil8PixelFormatSupported];
				else if (format == PixelFormat::D32S8_float) return true;
				else return false;
			} else return false;
		}
		bool QueryMetalFormatSupportForShaderSample(id<MTLDevice> device, Graphics::PixelFormat format) noexcept
		{
			uint version, subversion;
			QueryMetalVersion(device, version, subversion);
			if (IsColorFormat(format)) {
				auto bpp = GetFormatBitsPerPixel(format);
				if (bpp == 8) {
					if (format == PixelFormat::A8_unorm) return true;
					else if (format == PixelFormat::R8_unorm) return true;
					else if (format == PixelFormat::R8_snorm) return true;
					else if (format == PixelFormat::R8_uint) return false;
					else if (format == PixelFormat::R8_sint) return false;
					else return false;
				} else if (bpp == 16) {
					if (format == PixelFormat::R16_unorm) return true;
					else if (format == PixelFormat::R16_snorm) return true;
					else if (format == PixelFormat::R16_uint) return false;
					else if (format == PixelFormat::R16_sint) return false;
					else if (format == PixelFormat::R16_float) return true;
					else if (format == PixelFormat::R8G8_unorm) return true;
					else if (format == PixelFormat::R8G8_snorm) return true;
					else if (format == PixelFormat::R8G8_uint) return false;
					else if (format == PixelFormat::R8G8_sint) return false;
					else if (format == PixelFormat::B5G6R5_unorm) return version >= 4;
					else if (format == PixelFormat::B5G5R5A1_unorm) return version >= 4;
					else if (format == PixelFormat::A1B5G5R5_unorm) return version >= 4;
					else if (format == PixelFormat::A4B4G4R4_unorm) return version >= 4;
					else return false;
				} else if (bpp == 32) {
					if (format == PixelFormat::R32_uint) return false;
					else if (format == PixelFormat::R32_sint) return false;
					else if (format == PixelFormat::R32_float) return [device supports32BitFloatFiltering];
					else if (format == PixelFormat::R16G16_unorm) return true;
					else if (format == PixelFormat::R16G16_snorm) return true;
					else if (format == PixelFormat::R16G16_uint) return false;
					else if (format == PixelFormat::R16G16_sint) return false;
					else if (format == PixelFormat::R16G16_float) return true;
					else if (format == PixelFormat::B8G8R8A8_unorm) return true;
					else if (format == PixelFormat::R8G8B8A8_unorm) return true;
					else if (format == PixelFormat::R8G8B8A8_snorm) return true;
					else if (format == PixelFormat::R8G8B8A8_uint) return false;
					else if (format == PixelFormat::R8G8B8A8_sint) return false;
					else if (format == PixelFormat::R10G10B10A2_unorm) return true;
					else if (format == PixelFormat::R10G10B10A2_uint) return false;
					else if (format == PixelFormat::R11G11B10_float) return true;
					else if (format == PixelFormat::R9G9B9E5_float) return true;
					else return false;
				} else if (bpp == 64) {
					if (format == PixelFormat::R32G32_uint) return false;
					else if (format == PixelFormat::R32G32_sint) return false;
					else if (format == PixelFormat::R32G32_float) return [device supports32BitFloatFiltering];
					else if (format == PixelFormat::R16G16B16A16_unorm) return true;
					else if (format == PixelFormat::R16G16B16A16_snorm) return true;
					else if (format == PixelFormat::R16G16B16A16_uint) return false;
					else if (format == PixelFormat::R16G16B16A16_sint) return false;
					else if (format == PixelFormat::R16G16B16A16_float) return true;
					else return false;
				} else if (bpp == 128) {
					if (format == PixelFormat::R32G32B32A32_uint) return false;
					else if (format == PixelFormat::R32G32B32A32_sint) return false;
					else if (format == PixelFormat::R32G32B32A32_float) return [device supports32BitFloatFiltering];
					else return false;
				} else return false;
			} else if (IsDepthStencilFormat(format)) {
				if (format == PixelFormat::D16_unorm) return true;
				else if (format == PixelFormat::D32_float) return [device supports32BitFloatFiltering];
				else if (format == PixelFormat::D24S8_unorm) return [device isDepth24Stencil8PixelFormatSupported];
				else if (format == PixelFormat::D32S8_float) return false;
				else return false;
			} else return false;
		}
		bool QueryMetalFormatSupportForRenderTarget(id<MTLDevice> device, Graphics::PixelFormat format) noexcept
		{
			uint version, subversion;
			QueryMetalVersion(device, version, subversion);
			if (IsColorFormat(format)) {
				auto bpp = GetFormatBitsPerPixel(format);
				if (bpp == 8) {
					if (format == PixelFormat::A8_unorm) return true;
					else if (format == PixelFormat::R8_unorm) return true;
					else if (format == PixelFormat::R8_snorm) return true;
					else if (format == PixelFormat::R8_uint) return true;
					else if (format == PixelFormat::R8_sint) return true;
					else return false;
				} else if (bpp == 16) {
					if (format == PixelFormat::R16_unorm) return true;
					else if (format == PixelFormat::R16_snorm) return true;
					else if (format == PixelFormat::R16_uint) return true;
					else if (format == PixelFormat::R16_sint) return true;
					else if (format == PixelFormat::R16_float) return true;
					else if (format == PixelFormat::R8G8_unorm) return true;
					else if (format == PixelFormat::R8G8_snorm) return true;
					else if (format == PixelFormat::R8G8_uint) return true;
					else if (format == PixelFormat::R8G8_sint) return true;
					else if (format == PixelFormat::B5G6R5_unorm) return version >= 4;
					else if (format == PixelFormat::B5G5R5A1_unorm) return version >= 4;
					else if (format == PixelFormat::A1B5G5R5_unorm) return version >= 4;
					else if (format == PixelFormat::A4B4G4R4_unorm) return version >= 4;
					else return false;
				} else if (bpp == 32) {
					if (format == PixelFormat::R32_uint) return true;
					else if (format == PixelFormat::R32_sint) return true;
					else if (format == PixelFormat::R32_float) return true;
					else if (format == PixelFormat::R16G16_unorm) return true;
					else if (format == PixelFormat::R16G16_snorm) return true;
					else if (format == PixelFormat::R16G16_uint) return true;
					else if (format == PixelFormat::R16G16_sint) return true;
					else if (format == PixelFormat::R16G16_float) return true;
					else if (format == PixelFormat::B8G8R8A8_unorm) return true;
					else if (format == PixelFormat::R8G8B8A8_unorm) return true;
					else if (format == PixelFormat::R8G8B8A8_snorm) return true;
					else if (format == PixelFormat::R8G8B8A8_uint) return true;
					else if (format == PixelFormat::R8G8B8A8_sint) return true;
					else if (format == PixelFormat::R10G10B10A2_unorm) return true;
					else if (format == PixelFormat::R10G10B10A2_uint) return true;
					else if (format == PixelFormat::R11G11B10_float) return true;
					else if (format == PixelFormat::R9G9B9E5_float) return version >= 4;
					else return false;
				} else if (bpp == 64) {
					if (format == PixelFormat::R32G32_uint) return true;
					else if (format == PixelFormat::R32G32_sint) return true;
					else if (format == PixelFormat::R32G32_float) return true;
					else if (format == PixelFormat::R16G16B16A16_unorm) return true;
					else if (format == PixelFormat::R16G16B16A16_snorm) return true;
					else if (format == PixelFormat::R16G16B16A16_uint) return true;
					else if (format == PixelFormat::R16G16B16A16_sint) return true;
					else if (format == PixelFormat::R16G16B16A16_float) return true;
					else return false;
				} else if (bpp == 128) {
					if (format == PixelFormat::R32G32B32A32_uint) return true;
					else if (format == PixelFormat::R32G32B32A32_sint) return true;
					else if (format == PixelFormat::R32G32B32A32_float) return true;
					else return false;
				} else return false;
			} else return false;
		}
		bool QueryMetalFormatSupportForBlendRenderTarget(id<MTLDevice> device, Graphics::PixelFormat format) noexcept
		{
			uint version, subversion;
			QueryMetalVersion(device, version, subversion);
			if (IsColorFormat(format)) {
				auto bpp = GetFormatBitsPerPixel(format);
				if (bpp == 8) {
					if (format == PixelFormat::A8_unorm) return true;
					else if (format == PixelFormat::R8_unorm) return true;
					else if (format == PixelFormat::R8_snorm) return true;
					else if (format == PixelFormat::R8_uint) return false;
					else if (format == PixelFormat::R8_sint) return false;
					else return false;
				} else if (bpp == 16) {
					if (format == PixelFormat::R16_unorm) return true;
					else if (format == PixelFormat::R16_snorm) return true;
					else if (format == PixelFormat::R16_uint) return false;
					else if (format == PixelFormat::R16_sint) return false;
					else if (format == PixelFormat::R16_float) return true;
					else if (format == PixelFormat::R8G8_unorm) return true;
					else if (format == PixelFormat::R8G8_snorm) return true;
					else if (format == PixelFormat::R8G8_uint) return false;
					else if (format == PixelFormat::R8G8_sint) return false;
					else if (format == PixelFormat::B5G6R5_unorm) return version >= 4;
					else if (format == PixelFormat::B5G5R5A1_unorm) return version >= 4;
					else if (format == PixelFormat::A1B5G5R5_unorm) return version >= 4;
					else if (format == PixelFormat::A4B4G4R4_unorm) return version >= 4;
					else return false;
				} else if (bpp == 32) {
					if (format == PixelFormat::R32_uint) return false;
					else if (format == PixelFormat::R32_sint) return false;
					else if (format == PixelFormat::R32_float) return true;
					else if (format == PixelFormat::R16G16_unorm) return true;
					else if (format == PixelFormat::R16G16_snorm) return true;
					else if (format == PixelFormat::R16G16_uint) return false;
					else if (format == PixelFormat::R16G16_sint) return false;
					else if (format == PixelFormat::R16G16_float) return true;
					else if (format == PixelFormat::B8G8R8A8_unorm) return true;
					else if (format == PixelFormat::R8G8B8A8_unorm) return true;
					else if (format == PixelFormat::R8G8B8A8_snorm) return true;
					else if (format == PixelFormat::R8G8B8A8_uint) return false;
					else if (format == PixelFormat::R8G8B8A8_sint) return false;
					else if (format == PixelFormat::R10G10B10A2_unorm) return true;
					else if (format == PixelFormat::R10G10B10A2_uint) return false;
					else if (format == PixelFormat::R11G11B10_float) return true;
					else if (format == PixelFormat::R9G9B9E5_float) return version >= 4;
					else return false;
				} else if (bpp == 64) {
					if (format == PixelFormat::R32G32_uint) return false;
					else if (format == PixelFormat::R32G32_sint) return false;
					else if (format == PixelFormat::R32G32_float) return true;
					else if (format == PixelFormat::R16G16B16A16_unorm) return true;
					else if (format == PixelFormat::R16G16B16A16_snorm) return true;
					else if (format == PixelFormat::R16G16B16A16_uint) return false;
					else if (format == PixelFormat::R16G16B16A16_sint) return false;
					else if (format == PixelFormat::R16G16B16A16_float) return true;
					else return false;
				} else if (bpp == 128) {
					if (format == PixelFormat::R32G32B32A32_uint) return false;
					else if (format == PixelFormat::R32G32B32A32_sint) return false;
					else if (format == PixelFormat::R32G32B32A32_float) return true;
					else return false;
				} else return false;
			} else return false;
		}
		bool QueryMetalFormatSupportForDepthStencil(id<MTLDevice> device, Graphics::PixelFormat format) noexcept
		{
			if (IsDepthStencilFormat(format)) {
				if (format == PixelFormat::D16_unorm) return true;
				else if (format == PixelFormat::D32_float) return true;
				else if (format == PixelFormat::D24S8_unorm) return [device isDepth24Stencil8PixelFormatSupported];
				else if (format == PixelFormat::D32S8_float) return true;
				else return false;
			} else return false;
		}
	}
}