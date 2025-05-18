#include "MetalDevice.h"

#include "QuartzDevice.h"
#include "MetalDeviceShaders.h"
#include "CocoaInterop.h"

using namespace Engine;
using namespace Engine::Codec;
using namespace Engine::Streaming;
using namespace Engine::Graphics;

namespace Engine
{
	namespace Cocoa
	{
		CocoaPointer< id<MTLLibrary> > common_library;

		class MTL_Bitmap : public IDeviceBitmap
		{
			IBitmap * _base;
			I2DDeviceContext * _parent;
			CocoaPointer< id<MTLTexture> > _surface;
			int _width, _height;
		public:
			MTL_Bitmap(IBitmap * base, I2DDeviceContext * mtl_context) : _base(base), _parent(mtl_context), _width(-1), _height(-1) { if (!Reload()) throw Exception(); }
			virtual ~MTL_Bitmap(void) override {}
			virtual int GetWidth(void) const noexcept override { return _width; }
			virtual int GetHeight(void) const noexcept override { return _height; }
			virtual bool Reload(void) noexcept override
			{
				auto frame = GetBitmapSurface(_base);
				int width = frame->GetWidth();
				int height = frame->GetHeight();
				if (width != _width || height != _height) {
					@autoreleasepool {
						auto device = MetalGraphics::GetInnerMetalDevice(_parent->GetParentDevice());
						CocoaPointer< id<MTLTexture> > surface = [device newTextureWithDescriptor: [MTLTextureDescriptor texture2DDescriptorWithPixelFormat: MTLPixelFormatRGBA8Unorm width: width height: height mipmapped: NO]];
						if (!surface) return false;
						_width = width;
						_height = height;
						_surface = surface;
					}
				}
				[_surface replaceRegion: MTLRegionMake2D(0, 0, _width, _height) mipmapLevel: 0 withBytes: frame->GetData() bytesPerRow: frame->GetScanLineLength()];
				return true;
			}
			virtual IBitmap * GetParentBitmap(void) const noexcept override { return _base; }
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept override { return _parent; }
			virtual string ToString(void) const override { return L"MTL_Bitmap"; }
			id<MTLTexture> GetSurface(void) const noexcept { return _surface; }
		};

		class MTL_ColorBrush : public IColorBrush
		{
		public:
			I2DDeviceContext * _parent;
			CocoaPointer< id<MTLBuffer> > _area;
			Point _from, _to;
			int _vertex_count;
			bool _gradient;
		public:
			MTL_ColorBrush(I2DDeviceContext * parent) : _parent(parent) {}
			virtual ~MTL_ColorBrush(void) override {}
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept override { return _parent; }
			virtual string ToString(void) const override { return L"MTL_ColorBrush"; }
		};
		class MTL_BitmapBrush : public IBitmapBrush
		{
		public:
			I2DDeviceContext * _parent;
			CocoaPointer< id<MTLBuffer> > _area;
			CocoaPointer< id<MTLTexture> > _texture;
			Point _size;
			int _vertex_count;
			bool _tile, _alpha, _wrapped;
		public:
			MTL_BitmapBrush(I2DDeviceContext * parent) : _parent(parent) {}
			virtual ~MTL_BitmapBrush(void) override {}
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept override { return _parent; }
			virtual string ToString(void) const override { return L"MTL_BitmapBrush"; }
		};
		class MTL_TextBrush : public ITextBrush
		{
		public:
			I2DDeviceContext * _parent;
			SafePointer<ITextBrush> core_text_info;
			SafePointer<IColorBrush> highlight_info;
			id<MTLTexture> texture;
			int width, height;
			int horz_align, vert_align;
			int highlight_left, highlight_right;
			int render_ofs_x, render_ofs_y;
			int real_width, real_height;
			bool dynamic_ofs;
			Color highlight_color;
		public:
			MTL_TextBrush(I2DDeviceContext * parent) : _parent(parent) { texture = 0; width = height = highlight_left = highlight_right = -1; highlight_color = 0; render_ofs_x = render_ofs_y = 0; dynamic_ofs = false; }
			virtual ~MTL_TextBrush(void) override { [texture release]; }
			void UpdateTexture(void) { if (texture) [texture release]; texture = 0; width = height = -1; }
			virtual void GetExtents(int & width, int & height) noexcept override { core_text_info->GetExtents(width, height); }
			virtual void SetHighlightColor(const Color & color) noexcept override { if (color != highlight_color) { highlight_color = color; highlight_info.SetReference(0); } }
			virtual void HighlightText(int start, int end) noexcept override
			{
				if (start < 0 || start == end) highlight_left = highlight_right = -1;
				else {
					if (start == 0) highlight_left = 0; else highlight_left = EndOfChar(start - 1);
					highlight_right = EndOfChar(end - 1);
				}
			}
			virtual int TestPosition(int point) noexcept override { return core_text_info->TestPosition(point); }
			virtual int EndOfChar(int Index) noexcept override { return core_text_info->EndOfChar(Index); }
			virtual int GetStringLength(void) noexcept override { return core_text_info->GetStringLength(); }
			virtual void SetCharPalette(const Color * colors, int count) override { core_text_info->SetCharPalette(colors, count); UpdateTexture(); }
			virtual void SetCharColors(const uint8 * indicies, int count) override { core_text_info->SetCharColors(indicies, count); UpdateTexture(); }
			virtual void SetCharAdvances(const double * advances) override { core_text_info->SetCharAdvances(advances); UpdateTexture(); }
			virtual void GetCharAdvances(double * advances) noexcept override { core_text_info->GetCharAdvances(advances); }
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept override { return _parent; }
			virtual string ToString(void) const override { return L"MTL_TextBrush"; }
		};
		class MTL_BlurEffectBrush : public IBlurEffectBrush
		{
		public:
			I2DDeviceContext * _parent;
			float _sigma;
		public:
			MTL_BlurEffectBrush(I2DDeviceContext * parent, float sigma) : _parent(parent), _sigma(sigma) {}
			virtual ~MTL_BlurEffectBrush(void) override {}
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept override { return _parent; }
			virtual string ToString(void) const override { return L"MTL_BlurEffectBrush"; }
		};
		class MTL_InversionEffectBrush : public IInversionEffectBrush
		{
		public:
			I2DDeviceContext * _parent;
		public:
			MTL_InversionEffectBrush(I2DDeviceContext * parent) : _parent(parent) {}
			virtual ~MTL_InversionEffectBrush(void) override {}
			virtual I2DDeviceContext * GetParentDevice(void) const noexcept override { return _parent; }
			virtual string ToString(void) const override { return L"MTL_InversionEffectBrush"; }
		};

		class MTL_2DDeviceContext : public I2DDeviceContext
		{
			struct _vertex {
				Math::Vector2f position;
				Math::Vector4f color;
				Math::Vector2f tex_coord;
			};
			struct _viewport_info {
				Point viewport_size;
				Point viewport_offset;
			};
			struct _gradient_info {
				Math::Vector2f from;
				Math::Vector2f to;
				Math::Vector2f side;
				Math::Vector2f extents;
			};
			struct _tile_info {
				Box draw_rect;
				Point periods;
			};
			struct _layer_info {
				Box render_at;
				Point size;
				float alpha;
			};
			struct _layer_data {
				id<MTLTexture> surface;
				MTLRenderPassDescriptor * descriptor;
				double alpha;
			};

			IDevice * _parent_device;
			I2DDeviceContextFactory * _parent_factory;
			SafePointer<I2DDeviceContext> _dropback_device;
			id<MTLDevice> _device;
			CocoaPointer< id<MTLLibrary> > _library;
			MTLPixelFormat _pixel_format;
			id<MTLCommandBuffer> _command_buffer;
			id<MTLRenderCommandEncoder> _encoder;
			MTLRenderPassDescriptor * _current_descriptor;
			int _width, _height;
			CocoaPointer< id<MTLRenderPipelineState> > _main_alpha_state, _main_opaque_state, _gradient_state, _tile_state, _invert_state, _layer_state, _blur_state;
			Volumes::List< SafePointer<IBitmapLink> > _links;
			Volumes::ObjectCache<Color, IColorBrush> _color_cache;
			Volumes::ObjectCache<double, IBlurEffectBrush> _blur_cache;
			SafePointer<IInversionEffectBrush> _inversion_cache;
			Volumes::Stack<Box> _clipping;
			Volumes::Stack<_viewport_info> _viewports;
			Volumes::Stack<_layer_data> _layer_states;
			CocoaPointer< id<MTLTexture> > _white;
			CocoaPointer< id<MTLBuffer> > _common_area;
			uint _link_clear_counter;

			static Math::Vector4f CreateColor(Color color) noexcept
			{
				Math::Vector4f result;
				result.w = double(color.a) / 255.0;
				result.x = double(color.r) * result.w / 255.0;
				result.y = double(color.g) * result.w / 255.0;
				result.z = double(color.b) * result.w / 255.0;
				return result;
			}
			id<MTLBuffer> CreateAreaBuffer(const Box & area) noexcept
			{
				Array<_vertex> data(6);
				data.SetLength(6);
				data[0].position = Math::Vector2f(0.0f, 0.0f);
				data[0].color = Math::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
				data[0].tex_coord = Math::Vector2f(area.Left, area.Top);
				data[1].position = Math::Vector2f(1.0f, 0.0f);
				data[1].color = data[0].color;
				data[1].tex_coord = Math::Vector2f(area.Right, area.Top);
				data[2].position = Math::Vector2f(0.0f, 1.0f);
				data[2].color = data[0].color;
				data[2].tex_coord = Math::Vector2f(area.Left, area.Bottom);
				data[3] = data[1];
				data[4] = data[2];
				data[5].position = Math::Vector2f(1.0f, 1.0f);
				data[5].color = data[0].color;
				data[5].tex_coord = Math::Vector2f(area.Right, area.Bottom);
				return [_device newBufferWithBytes: data.GetBuffer() length: sizeof(_vertex) * data.Length() options: MTLResourceStorageModeShared];
			}
			id<MTLBuffer> CreateAreaBuffer(Color color) noexcept
			{
				auto clr = CreateColor(color);
				Array<_vertex> data(6);
				data.SetLength(6);
				data[0].position = Math::Vector2f(0.0f, 0.0f);
				data[0].color = clr;
				data[0].tex_coord = Math::Vector2f(0.0f, 0.0f);
				data[1].position = Math::Vector2f(1.0f, 0.0f);
				data[1].color = data[0].color;
				data[1].tex_coord = Math::Vector2f(0.0f, 0.0f);
				data[2].position = Math::Vector2f(0.0f, 1.0f);
				data[2].color = data[0].color;
				data[2].tex_coord = Math::Vector2f(0.0f, 0.0f);
				data[3] = data[1];
				data[4] = data[2];
				data[5].position = Math::Vector2f(1.0f, 1.0f);
				data[5].color = data[0].color;
				data[5].tex_coord = Math::Vector2f(0.0f, 0.0f);
				return [_device newBufferWithBytes: data.GetBuffer() length: sizeof(_vertex) * data.Length() options: MTLResourceStorageModeShared];
			}
		public:
			MTL_2DDeviceContext(IDevice * device) : _command_buffer(0), _encoder(0), _current_descriptor(0), _width(1), _height(1), _color_cache(0x80), _blur_cache(0x10), _link_clear_counter(0)
			{
				_pixel_format = MTLPixelFormatInvalid;
				_parent_device = device;
				_parent_factory = GetCommonDeviceContextFactory();
				if (!_parent_factory) throw Exception();
				_dropback_device = _parent_factory->CreateBitmapContext();
				if (!_dropback_device) throw Exception();
				_device = MetalGraphics::GetInnerMetalDevice(_parent_device);
				if (_parent_device == MetalGraphics::GetMetalCommonDevice()) {
					if (!common_library) common_library = CreateMetalRenderingDeviceShaders(_device);
					_library = common_library;
				} else _library = CreateMetalRenderingDeviceShaders(_device);
				if (!_library) throw Exception();
			}
			virtual ~MTL_2DDeviceContext(void) override { ClearInternalCache(); }
			bool InitializeWithPixelFormat(MTLPixelFormat format) noexcept
			{
				_pixel_format = format;
				NSError * error;
				id<MTLFunction> main_vertex = [_library newFunctionWithName: @"MetalDeviceMainVertexShader"];
				id<MTLFunction> main_vertex_gradient = [_library newFunctionWithName: @"MetalDeviceMainVertexShaderGradient"];
				id<MTLFunction> main_pixel = [_library newFunctionWithName: @"MetalDeviceMainPixelShader"];
				id<MTLFunction> main_pixel_no_alpha = [_library newFunctionWithName: @"MetalDeviceMainPixelShaderNoAlpha"];
				id<MTLFunction> tile_vertex = [_library newFunctionWithName: @"MetalDeviceTileVertexShader"];
				id<MTLFunction> tile_pixel = [_library newFunctionWithName: @"MetalDeviceTilePixelShader"];
				id<MTLFunction> layer_vertex_f = [_library newFunctionWithName: @"MetalDeviceLayerVertexShader"];
				id<MTLFunction> layer_pixel_f = [_library newFunctionWithName: @"MetalDeviceLayerPixelShader"];
				id<MTLFunction> blur_vertex_f = [_library newFunctionWithName: @"MetalDeviceBlurVertexShader"];
				id<MTLFunction> blur_pixel_f = [_library newFunctionWithName: @"MetalDeviceBlurPixelShader"];
				{
					MTLRenderPipelineDescriptor * descriptor = [[MTLRenderPipelineDescriptor alloc] init];
					descriptor.colorAttachments[0].pixelFormat = _pixel_format;
					descriptor.colorAttachments[0].blendingEnabled = YES;
					descriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
					descriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
					descriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
					descriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
					descriptor.vertexFunction = main_vertex;
					descriptor.fragmentFunction = main_pixel;
					_main_alpha_state = [_device newRenderPipelineStateWithDescriptor: descriptor error: &error];
					descriptor.vertexFunction = main_vertex_gradient;
					_gradient_state = [_device newRenderPipelineStateWithDescriptor: descriptor error: &error];
					descriptor.vertexFunction = tile_vertex;
					descriptor.fragmentFunction = tile_pixel;
					_tile_state = [_device newRenderPipelineStateWithDescriptor: descriptor error: &error];
					descriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
					descriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
					descriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
					descriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorZero;
					descriptor.vertexFunction = main_vertex;
					descriptor.fragmentFunction = main_pixel_no_alpha;
					_main_opaque_state = [_device newRenderPipelineStateWithDescriptor: descriptor error: &error];
					descriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationSubtract;
					descriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
					descriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
					descriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorDestinationAlpha;
					descriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
					descriptor.vertexFunction = main_vertex;
					descriptor.fragmentFunction = main_pixel;
					_invert_state = [_device newRenderPipelineStateWithDescriptor: descriptor error: &error];
					descriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
					descriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
					descriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
					descriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
					descriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
					descriptor.vertexFunction = layer_vertex_f;
					descriptor.fragmentFunction = layer_pixel_f;
					_layer_state = [_device newRenderPipelineStateWithDescriptor: descriptor error: &error];
					descriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
					descriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
					descriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
					descriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorZero;
					descriptor.vertexFunction = blur_vertex_f;
					descriptor.fragmentFunction = blur_pixel_f;
					_blur_state = [_device newRenderPipelineStateWithDescriptor: descriptor error: &error];
					[descriptor release];
				}
				[main_vertex release];
				[main_vertex_gradient release];
				[main_pixel release];
				[main_pixel_no_alpha release];
				[tile_vertex release];
				[tile_pixel release];
				[layer_vertex_f release];
				[layer_pixel_f release];
				[blur_vertex_f release];
				[blur_pixel_f release];
				@autoreleasepool {
					Color data(255, 255, 255, 255);
					_white = [_device newTextureWithDescriptor: [MTLTextureDescriptor texture2DDescriptorWithPixelFormat: MTLPixelFormatBGRA8Unorm width: 1 height: 1 mipmapped: NO]];
					[_white replaceRegion: MTLRegionMake2D(0, 0, 1, 1) mipmapLevel: 0 withBytes: &data bytesPerRow: 4];
				}
				Array<_vertex> data(6);
				data.SetLength(6);
				data[0].position = Math::Vector2f(0.0f, 0.0f);
				data[0].color = Math::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
				data[0].tex_coord = Math::Vector2f(0.0f, 0.0f);
				data[1].position = Math::Vector2f(1.0f, 0.0f);
				data[1].color = data[0].color;
				data[1].tex_coord = Math::Vector2f(1.0f, 0.0f);
				data[2].position = Math::Vector2f(0.0f, 1.0f);
				data[2].color = data[0].color;
				data[2].tex_coord = Math::Vector2f(0.0f, 1.0f);
				data[3] = data[1];
				data[4] = data[2];
				data[5].position = Math::Vector2f(1.0f, 1.0f);
				data[5].color = data[0].color;
				data[5].tex_coord = Math::Vector2f(1.0f, 1.0f);
				_common_area = [_device newBufferWithBytes: data.GetBuffer() length: sizeof(_vertex) * data.Length() options: MTLResourceStorageModeShared];
				return true;
			}
			bool BeginRendering(id<MTLCommandBuffer> command_buffer, MTLRenderPassDescriptor * descriptor, int width, int height) noexcept
			{
				_width = width;
				_height = height;
				_current_descriptor = descriptor;
				_command_buffer = command_buffer;
				_encoder = [command_buffer renderCommandEncoderWithDescriptor: _current_descriptor];
				[_encoder setViewport: (MTLViewport) { 0.0, 0.0, double(_width), double(_height), 0.0, 1.0 }];
				_viewport_info info;
				info.viewport_size = Point(_width, _height);
				info.viewport_offset = Point(0, 0);
				[_encoder setVertexBytes: &info length: sizeof(info) atIndex: 0];
				try {
					_clipping.Clear();
					_clipping.Push(Box(0, 0, _width, _height));
					_viewports.Clear();
					_viewports.Push(info);
					_layer_states.Clear();
				} catch (...) { return false; }
				return true;
			}
			bool EndRendering(id<CAMetalDrawable> present) noexcept
			{
				[_encoder endEncoding];
				if (present) {
					[_command_buffer presentDrawable: present];
					[_command_buffer commit];
					[_command_buffer waitUntilCompleted];
				}
				_encoder = 0;
				_command_buffer = 0;
				_current_descriptor = 0;
				return true;
			}
			// Information API
			virtual void GetImplementationInfo(string & tech, uint32 & version) override { tech = L"Metal"; version = 1; }
			virtual uint32 GetFeatureList(void) noexcept override
			{
				return DeviceContextFeatureBlurCapable | DeviceContextFeatureInversionCapable |
					DeviceContextFeatureLayersCapable | DeviceContextFeatureHardware | DeviceContextFeatureGraphicsInteropEnabled;
			}
			virtual string ToString(void) const override { return L"MTL_2DDeviceContext"; }
			// Brush creation
			virtual IColorBrush * CreateSolidColorBrush(Color color) noexcept override
			{
				auto cached = _color_cache.GetObjectByKey(color);
				if (cached) { cached->Retain(); return cached; }
				SafePointer<MTL_ColorBrush> result = new (std::nothrow) MTL_ColorBrush(this);
				if (!result) return 0;
				result->_area = CreateAreaBuffer(color);
				result->_from = result->_to = Point(0, 0);
				result->_vertex_count = 6;
				result->_gradient = false;
				try { _color_cache.Push(color, result); } catch (...) {}
				result->Retain();
				return result;
			}
			virtual IColorBrush * CreateGradientBrush(Point rel_from, Point rel_to, const GradientPoint * points, int count) noexcept override
			{
				if (count < 1) return 0;
				if (count == 1) return CreateSolidColorBrush(points[0].Value);
				SafePointer<MTL_ColorBrush> result = new (std::nothrow) MTL_ColorBrush(this);
				if (!result) return 0;
				try {
					Array<_vertex> data(0x80);
					_vertex v;
					v.tex_coord = Math::Vector2f(0.0f, 0.0f);
					v.position = Math::Vector2f(-1.0f, -1.0f);
					v.color = CreateColor(points[0].Value);
					data << v;
					v.position = Math::Vector2f(-1.0f, 1.0f);
					data << v;
					v.position = Math::Vector2f(Math::saturate(points[0].Position), -1.0f);
					data << v;
					data << data[data.Length() - 2];
					data << data[data.Length() - 2];
					v.position = Math::Vector2f(Math::saturate(points[0].Position), 1.0f);
					data << v;
					for (int i = 1; i < count; i++) {
						data << data[data.Length() - 2];
						data << data[data.Length() - 2];
						v.position = Math::Vector2f(Math::saturate(points[i].Position), -1.0f);
						v.color = CreateColor(points[i].Value);
						data << v;
						data << data[data.Length() - 2];
						data << data[data.Length() - 2];
						v.position = Math::Vector2f(Math::saturate(points[i].Position), 1.0f);
						data << v;
					}
					data << data[data.Length() - 2];
					data << data[data.Length() - 2];
					v.position = Math::Vector2f(2.0f, -1.0f);
					data << v;
					data << data[data.Length() - 2];
					data << data[data.Length() - 2];
					v.position = Math::Vector2f(2.0f, 1.0f);
					data << v;
					result->_area = [_device newBufferWithBytes: data.GetBuffer() length: sizeof(_vertex) * data.Length() options: MTLResourceStorageModeShared];
					result->_vertex_count = data.Length();
				} catch (...) { return 0; }
				result->_from = rel_from;
				result->_to = rel_to;
				result->_gradient = true;
				result->Retain();
				return result;
			}
			virtual IBlurEffectBrush * CreateBlurEffectBrush(double power) noexcept override
			{
				auto cached = _blur_cache.GetObjectByKey(power);
				if (cached) { cached->Retain(); return cached; }
				SafePointer<MTL_BlurEffectBrush> result = new (std::nothrow) MTL_BlurEffectBrush(this, power);
				if (!result) return 0;
				try { _blur_cache.Push(power, result); } catch (...) {}
				result->Retain();
				return result;
			}
			virtual IInversionEffectBrush * CreateInversionEffectBrush(void) noexcept override
			{
				if (!_inversion_cache) _inversion_cache = new (std::nothrow) MTL_InversionEffectBrush(this);
				if (!_inversion_cache) return 0;
				_inversion_cache->Retain();
				return _inversion_cache;
			}
			virtual IBitmapBrush * CreateBitmapBrush(IBitmap * bitmap, const Box & area, bool tile) noexcept override
			{
				if (!bitmap || area.Left >= area.Right || area.Top >= area.Bottom) return 0;
				if (area.Left < 0 || area.Top < 0 || area.Right > bitmap->GetWidth() || area.Bottom > bitmap->GetHeight()) return 0;
				SafePointer<MTL_Bitmap> device_bitmap = static_cast<MTL_Bitmap *>(bitmap->GetDeviceBitmap(this));
				if (!device_bitmap) {
					try {
						SafePointer<IBitmapLink> link;
						device_bitmap = new MTL_Bitmap(bitmap, this);
						link.SetRetain(bitmap->GetLinkObject());
						_links.InsertLast(link);
						bitmap->AddDeviceBitmap(device_bitmap, this);
						_link_clear_counter++;
						if (_link_clear_counter >= 0x100) {
							_link_clear_counter = 0;
							auto current = _links.GetFirst();
							while (current) {
								auto next = current->GetNext();
								if (!current->GetValue()->GetBitmap()) _links.Remove(current);
								current = next;
							}
						}
					} catch (...) { return 0; }
				} else device_bitmap->Retain();
				SafePointer<MTL_BitmapBrush> result = new (std::nothrow) MTL_BitmapBrush(this);
				if (!result) return 0;
				result->_wrapped = false;
				result->_alpha = true;
				result->_tile = tile;
				result->_vertex_count = 6;
				result->_size = Point(area.Right - area.Left, area.Bottom - area.Top);
				if (tile) {
					if (area.Left == 0 && area.Top == 0 && area.Right == bitmap->GetWidth() && area.Bottom == bitmap->GetHeight()) {
						result->_texture = device_bitmap->GetSurface();
						[result->_texture retain];
					} else {
						MTLPixelFormat pixel_format = [device_bitmap->GetSurface() pixelFormat];
						void * data_buffer = malloc(4 * result->_size.x * result->_size.y);
						if (!data_buffer) return 0;
						[device_bitmap->GetSurface() getBytes: data_buffer bytesPerRow: 4 * result->_size.x fromRegion: MTLRegionMake2D(area.Left, area.Top, result->_size.x, result->_size.y) mipmapLevel: 0];
						@autoreleasepool {
							result->_texture = [_device newTextureWithDescriptor: [MTLTextureDescriptor texture2DDescriptorWithPixelFormat: pixel_format width: result->_size.x height: result->_size.y mipmapped: NO]];
							[result->_texture replaceRegion: MTLRegionMake2D(0, 0, result->_size.x, result->_size.y) mipmapLevel: 0 withBytes: data_buffer bytesPerRow: 4 * result->_size.x];
						}
						free(data_buffer);
					}
					result->_area = _common_area;
					[result->_area retain];
				} else {
					result->_texture = device_bitmap->GetSurface();
					[result->_texture retain];
					result->_area = CreateAreaBuffer(area);
				}
				result->Retain();
				return result;
			}
			virtual IBitmapBrush * CreateTextureBrush(ITexture * texture, TextureAlphaMode mode) noexcept override
			{
				if (!texture) return 0;
				SafePointer<MTL_BitmapBrush> result = new (std::nothrow) MTL_BitmapBrush(this);
				if (!result) return 0;
				result->_area = CreateAreaBuffer(Box(0, 0, texture->GetWidth(), texture->GetHeight()));
				result->_texture = MetalGraphics::GetInnerMetalTexture(texture);
				result->_vertex_count = 6;
				result->_size = Point(texture->GetWidth(), texture->GetHeight());
				result->_tile = false;
				result->_wrapped = true;
				result->_alpha = mode == TextureAlphaMode::Premultiplied;
				[result->_texture retain];
				result->Retain();
				return result;
			}
			virtual ITextBrush * CreateTextBrush(IFont * font, const string & text, uint32 horizontal_align, uint32 vertical_align, const Color & color) noexcept override
			{
				if (!font) return 0;
				SafePointer<ITextBrush> core_info = _dropback_device->CreateTextBrush(font, text, 0, 0, color);
				SafePointer<MTL_TextBrush> info = new MTL_TextBrush(this);
				info->core_text_info = core_info;
				info->horz_align = horizontal_align;
				info->vert_align = vertical_align;
				info->Retain();
				return info;
			}
			virtual ITextBrush * CreateTextBrush(IFont * font, const uint32 * ucs, int length, uint32 horizontal_align, uint32 vertical_align, const Color & color) noexcept override
			{
				if (!font) return 0;
				SafePointer<ITextBrush> core_info = _dropback_device->CreateTextBrush(font, ucs, length, 0, 0, color);
				SafePointer<MTL_TextBrush> info = new MTL_TextBrush(this);
				info->core_text_info = core_info;
				info->horz_align = horizontal_align;
				info->vert_align = vertical_align;
				info->Retain();
				return info;
			}
			virtual void ClearInternalCache(void) noexcept override
			{
				auto current = _links.GetFirst();
				while (current) {
					auto bitmap = current->GetValue()->GetBitmap();
					if (bitmap) bitmap->RemoveDeviceBitmap(this);
					current = current->GetNext();
				}
				_links.Clear();
				_color_cache.Clear();
				_blur_cache.Clear();
				_inversion_cache.SetReference(0);
			}
			// Clipping and layers
			virtual void PushClip(const Box & rect) noexcept override
			{
				if (_clipping.IsEmpty()) return;
				try {
					auto at = Box::Intersect(_clipping.GetLast()->GetValue(), rect);
					_clipping.Push(at);
					auto & viewport = _viewports.GetLast()->GetValue();
					MTLScissorRect scissor;
					scissor.x = at.Left - viewport.viewport_offset.x;
					scissor.y = at.Top - viewport.viewport_offset.y;
					scissor.width = at.Right - at.Left;
					scissor.height = at.Bottom - at.Top;
					[_encoder setScissorRect: scissor];
				} catch (...) {}
			}
			virtual void PopClip(void) noexcept override
			{
				if (!_clipping.IsEmpty()) {
					_clipping.RemoveLast();
					auto & box = _clipping.GetLast()->GetValue();
					auto & viewport = _viewports.GetLast()->GetValue();
					MTLScissorRect scissor;
					scissor.x = box.Left - viewport.viewport_offset.x;
					scissor.y = box.Top - viewport.viewport_offset.y;
					scissor.width = box.Right - box.Left;
					scissor.height = box.Bottom - box.Top;
					[_encoder setScissorRect: scissor];
				}
			}
			virtual void BeginLayer(const Box & rect, double opacity) noexcept override
			{
				auto layer_rect = Box::Intersect(_clipping.GetLast()->GetValue(), rect);
				if (layer_rect.Right <= layer_rect.Left) { opacity = 0.0; layer_rect.Right = layer_rect.Left + 1; }
				if (layer_rect.Bottom <= layer_rect.Top) { opacity = 0.0; layer_rect.Bottom = layer_rect.Top + 1; }
				_layer_data layer;
				_viewport_info viewport;
				layer.alpha = opacity;
				layer.descriptor = _current_descriptor;
				viewport.viewport_size = Point(layer_rect.Right - layer_rect.Left, layer_rect.Bottom - layer_rect.Top);
				viewport.viewport_offset = Point(layer_rect.Left, layer_rect.Top);
				try {
					@autoreleasepool {
						auto tex_desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat: _pixel_format width: viewport.viewport_size.x height: viewport.viewport_size.y mipmapped: NO];
						[tex_desc setUsage: MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget];
						layer.surface = [_device newTextureWithDescriptor: tex_desc];
					}
					_clipping.Push(layer_rect);
					_viewports.Push(viewport);
					_layer_states.Push(layer);
				} catch (...) {}
				_current_descriptor = [MTLRenderPassDescriptor renderPassDescriptor];
				auto attachment = [[MTLRenderPassColorAttachmentDescriptor alloc] init];
				[attachment setTexture: layer.surface];
				[attachment setClearColor: MTLClearColorMake(0.0, 0.0, 0.0, 0.0)];
				[attachment setLoadAction: MTLLoadActionClear];
				[[_current_descriptor colorAttachments] setObject: attachment atIndexedSubscript: 0];
				[attachment release];
				[_encoder endEncoding];
				_encoder = [_command_buffer renderCommandEncoderWithDescriptor: _current_descriptor];
				[_encoder setVertexBytes: &viewport length: sizeof(viewport) atIndex: 0];
			}
			virtual void EndLayer(void) noexcept override
			{
				if (_clipping.IsEmpty() || _viewports.IsEmpty() || _layer_states.IsEmpty()) return;
				[_encoder endEncoding];
				auto layer = _layer_states.Pop();
				auto current_viewport = _viewports.Pop();
				auto new_viewport = _viewports.GetLast()->GetValue();
				_current_descriptor = layer.descriptor;
				[[[_current_descriptor colorAttachments] objectAtIndexedSubscript: 0] setLoadAction: MTLLoadActionLoad];
				_encoder = [_command_buffer renderCommandEncoderWithDescriptor: _current_descriptor];
				[_encoder setVertexBytes: &new_viewport length: sizeof(new_viewport) atIndex: 0];
				if (layer.alpha) {
					_layer_info info;
					info.render_at = Box(current_viewport.viewport_offset.x, current_viewport.viewport_offset.y,
						current_viewport.viewport_offset.x + current_viewport.viewport_size.x, current_viewport.viewport_offset.y + current_viewport.viewport_size.y);
					info.size = Point(current_viewport.viewport_size.x, current_viewport.viewport_size.y);
					info.alpha = layer.alpha;
					[_encoder setRenderPipelineState: _layer_state];
					[_encoder setFragmentTexture: layer.surface atIndex: 0];
					[_encoder setVertexBytes: &info length: sizeof(info) atIndex: 2];
					[_encoder setVertexBuffer: _common_area offset: 0 atIndex: 1];
					[_encoder drawPrimitives: MTLPrimitiveTypeTriangle vertexStart: 0 vertexCount: 6];
				}
				[layer.surface release];
				PopClip();
			}
			// Rendering API
			virtual void Render(IColorBrush * brush, const Box & at) noexcept override
			{
				if (!brush || at.Left >= at.Right || at.Top >= at.Bottom) return;
				auto info = static_cast<MTL_ColorBrush *>(brush);
				if (info->_gradient) {
					PushClip(at);
					auto d = Math::Vector2f(info->_to.x - info->_from.x, info->_to.y - info->_from.y);
					auto s = Math::Vector2f(d.y, -d.x);
					auto o = Math::Vector2f(info->_from.x + at.Left, info->_from.y + at.Top);
					auto n = d * d;
					float xc1 = (Math::Vector2f(at.Left, at.Top) - o) * d / n;
					float xc2 = (Math::Vector2f(at.Right, at.Top) - o) * d / n;
					float xc3 = (Math::Vector2f(at.Left, at.Bottom) - o) * d / n;
					float xc4 = (Math::Vector2f(at.Right, at.Bottom) - o) * d / n;
					float xmx = abs(max(max(xc1, xc2), max(xc3, xc4)) - 1.0f);
					float xmn = abs(min(min(xc1, xc2), min(xc3, xc4)));
					float yc1 = (Math::Vector2f(at.Left, at.Top) - o) * s / n;
					float yc2 = (Math::Vector2f(at.Right, at.Top) - o) * s / n;
					float yc3 = (Math::Vector2f(at.Left, at.Bottom) - o) * s / n;
					float yc4 = (Math::Vector2f(at.Right, at.Bottom) - o) * s / n;
					float ymx = abs(max(max(yc1, yc2), max(yc3, yc4)));
					float ymn = abs(min(min(yc1, yc2), min(yc3, yc4)));
					_gradient_info grad;
					grad.from = Math::Vector2f(at.Left + info->_from.x, at.Top + info->_from.y);
					grad.to = Math::Vector2f(at.Left + info->_to.x, at.Top + info->_to.y);
					grad.side = s;
					grad.extents = Math::Vector2f(max(xmx, xmn) + 0.01f, max(ymx, ymn) + 0.01f);
					[_encoder setRenderPipelineState: _gradient_state];
					[_encoder setFragmentTexture: _white atIndex: 0];
					[_encoder setVertexBytes: &grad length: sizeof(grad) atIndex: 2];
					[_encoder setVertexBuffer: info->_area offset: 0 atIndex: 1];
					[_encoder drawPrimitives: MTLPrimitiveTypeTriangle vertexStart: 0 vertexCount: info->_vertex_count];
					PopClip();
				} else {
					[_encoder setRenderPipelineState: _main_alpha_state];
					[_encoder setFragmentTexture: _white atIndex: 0];
					[_encoder setVertexBytes: &at length: sizeof(at) atIndex: 2];
					[_encoder setVertexBuffer: info->_area offset: 0 atIndex: 1];
					[_encoder drawPrimitives: MTLPrimitiveTypeTriangle vertexStart: 0 vertexCount: info->_vertex_count];
				}
			}
			virtual void Render(IBitmapBrush * brush, const Box & at) noexcept override
			{
				if (!brush || at.Left >= at.Right || at.Top >= at.Bottom) return;
				auto info = static_cast<MTL_BitmapBrush *>(brush);
				if (info->_tile) {
					_tile_info tile_info;
					tile_info.draw_rect = at;
					tile_info.periods = info->_size;
					[_encoder setRenderPipelineState: _tile_state];
					[_encoder setFragmentTexture: info->_texture atIndex: 0];
					[_encoder setVertexBytes: &tile_info length: sizeof(tile_info) atIndex: 2];
					[_encoder setVertexBuffer: info->_area offset: 0 atIndex: 1];
					[_encoder drawPrimitives: MTLPrimitiveTypeTriangle vertexStart: 0 vertexCount: info->_vertex_count];
				} else {
					if (info->_wrapped) _parent_device->GetDeviceContext()->Flush();
					[_encoder setRenderPipelineState: info->_alpha ? _main_alpha_state : _main_opaque_state];
					[_encoder setFragmentTexture: info->_texture atIndex: 0];
					[_encoder setVertexBytes: &at length: sizeof(at) atIndex: 2];
					[_encoder setVertexBuffer: info->_area offset: 0 atIndex: 1];
					[_encoder drawPrimitives: MTLPrimitiveTypeTriangle vertexStart: 0 vertexCount: info->_vertex_count];
				}
			}
			virtual void Render(ITextBrush * brush, const Box & at, bool clip) noexcept override
			{
				if (!brush || at.Left >= at.Right || at.Top >= at.Bottom) return;
				auto info = static_cast<MTL_TextBrush *>(brush);
				int new_render_ofs_x = max(-at.Left, 0);
				int new_render_ofs_y = max(-at.Top, 0);
				if (!info->texture) {
					info->render_ofs_x = new_render_ofs_y = -1;
					info->core_text_info->GetExtents(info->real_width, info->real_height);
					info->width = info->real_width;
					info->height = info->real_height;
					if (info->width > 16384) { info->width = 16384; info->dynamic_ofs = true; }
					if (info->height > 16384) { info->height = 16384; info->dynamic_ofs = true; }
					if (info->width > 0 && info->height > 0) {
						@autoreleasepool {
							info->texture = [_device newTextureWithDescriptor: [MTLTextureDescriptor texture2DDescriptorWithPixelFormat: MTLPixelFormatRGBA8Unorm
								width: info->width height: info->height mipmapped: NO]];
						}
					}
				}
				if (!info->dynamic_ofs) { new_render_ofs_x = new_render_ofs_y = 0; }
				if (info->texture && (info->render_ofs_x != new_render_ofs_x || info->render_ofs_y != new_render_ofs_y)) {
					info->render_ofs_x = new_render_ofs_x;
					info->render_ofs_y = new_render_ofs_y;
					SafePointer<IBitmapContext> inner_device = _parent_factory->CreateBitmapContext();
					SafePointer<IBitmap> surface = _parent_factory->CreateBitmap(info->width, info->height, 0);
					inner_device->BeginRendering(surface);
					inner_device->Render(info->core_text_info, Box(-new_render_ofs_x, -new_render_ofs_y, info->width - new_render_ofs_x, info->height - new_render_ofs_y), false);
					inner_device->EndRendering();
					SafePointer<Codec::Frame> backbuffer = surface->QueryFrame();
					[info->texture replaceRegion: MTLRegionMake2D(0, 0, info->width, info->height) mipmapLevel: 0 withBytes: backbuffer->GetData() bytesPerRow: backbuffer->GetScanLineLength()];
				}
				if (info->texture) {
					if (clip) PushClip(at);
					_layer_info rinfo;
					if (info->horz_align == 0) {
						rinfo.render_at.Left = at.Left + new_render_ofs_x;
						rinfo.render_at.Right = at.Left + new_render_ofs_x + info->width;
					} else if (info->horz_align == 1) {
						rinfo.render_at.Left = (at.Left + at.Right - info->real_width) / 2 + new_render_ofs_x;
						rinfo.render_at.Right = rinfo.render_at.Left + info->width;
					} else {
						rinfo.render_at.Right = at.Right + new_render_ofs_x;
						rinfo.render_at.Left = at.Right - info->real_width;
						rinfo.render_at.Right = rinfo.render_at.Left + info->width;
					}
					if (info->vert_align == 0) {
						rinfo.render_at.Top = at.Top + new_render_ofs_y;
						rinfo.render_at.Bottom = at.Top + new_render_ofs_y + info->height;
					} else if (info->vert_align == 1) {
						rinfo.render_at.Top = (at.Top + at.Bottom - info->real_height) / 2 + new_render_ofs_y;
						rinfo.render_at.Bottom = rinfo.render_at.Top + info->height;
					} else {
						rinfo.render_at.Bottom = at.Bottom + new_render_ofs_y;
						rinfo.render_at.Top = at.Bottom - info->real_height;
						rinfo.render_at.Bottom = rinfo.render_at.Top + info->height;
					}
					rinfo.size = Point(info->width, info->height);
					rinfo.alpha = 1.0f;
					if (info->highlight_right > info->highlight_left) {
						if (!info->highlight_info) info->highlight_info.SetReference(CreateSolidColorBrush(info->highlight_color));
						Box highlight(rinfo.render_at.Left - new_render_ofs_x + info->highlight_left, at.Top, rinfo.render_at.Left - new_render_ofs_x + info->highlight_right, at.Bottom);
						Render(info->highlight_info, highlight);
					}
					[_encoder setRenderPipelineState: _layer_state];
					[_encoder setFragmentTexture: info->texture atIndex: 0];
					[_encoder setVertexBytes: &rinfo length: sizeof(rinfo) atIndex: 2];
					[_encoder setVertexBuffer: _common_area offset: 0 atIndex: 1];
					[_encoder drawPrimitives: MTLPrimitiveTypeTriangle vertexStart: 0 vertexCount: 6];
					if (clip) PopClip();
				}
			}
			virtual void Render(IBlurEffectBrush * brush, const Box & at) noexcept override
			{
				if (!_layer_states.IsEmpty() || !brush) return;
				auto blur_box = Box::Intersect(_clipping.GetLast()->GetValue(), at);
				if (blur_box.Right <= blur_box.Left || blur_box.Bottom <= blur_box.Top) return;
				auto info = static_cast<MTL_BlurEffectBrush *>(brush);
				auto size = Point(blur_box.Right - blur_box.Left, blur_box.Bottom - blur_box.Top);
				[_encoder endEncoding];
				id<MTLTexture> blur_region;
				id<MTLTexture> blur_region_mip;
				uint lod = 1;
				float sigma = info->_sigma;
				Point mip_size = size;
				while (sigma > 2.0f) { sigma /= 2.0f; lod++; mip_size.x /= 2; mip_size.y /= 2; }
				@autoreleasepool {
					auto tex_desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat: _pixel_format width: size.x height: size.y mipmapped: NO];
					tex_desc.mipmapLevelCount = lod;
					[tex_desc setUsage: MTLTextureUsageShaderRead];
					blur_region = [_device newTextureWithDescriptor: tex_desc];
					blur_region_mip = [blur_region newTextureViewWithPixelFormat: _pixel_format textureType: MTLTextureType2D levels: NSMakeRange(lod - 1, 1) slices: NSMakeRange(0, 1)];
				}
				id<MTLTexture> render_target = [[[_current_descriptor colorAttachments] objectAtIndexedSubscript: 0] texture];
				id<MTLBlitCommandEncoder> blit_encoder = [_command_buffer blitCommandEncoder];
				[blit_encoder copyFromTexture: render_target sourceSlice: 0 sourceLevel: 0 sourceOrigin: MTLOriginMake(blur_box.Left, blur_box.Top, 0)
					sourceSize: MTLSizeMake(size.x, size.y, 1) toTexture: blur_region destinationSlice: 0 destinationLevel: 0 destinationOrigin: MTLOriginMake(0, 0, 0)];
				[blit_encoder generateMipmapsForTexture: blur_region];
				[blit_encoder endEncoding];
				auto prev_desc = _current_descriptor;
				[[[_current_descriptor colorAttachments] objectAtIndexedSubscript: 0] setLoadAction: MTLLoadActionLoad];
				_encoder = [_command_buffer renderCommandEncoderWithDescriptor: _current_descriptor];
				[_encoder setVertexBytes: &_viewports.GetLast()->GetValue() length: sizeof(_viewport_info) atIndex: 0];
				auto & box = _clipping.GetLast()->GetValue();
				auto & viewport = _viewports.GetLast()->GetValue();
				MTLScissorRect scissor;
				scissor.x = box.Left - viewport.viewport_offset.x;
				scissor.y = box.Top - viewport.viewport_offset.y;
				scissor.width = box.Right - box.Left;
				scissor.height = box.Bottom - box.Top;
				[_encoder setScissorRect: scissor];
				_layer_info rinfo;
				rinfo.render_at = blur_box;
				rinfo.size = mip_size;
				rinfo.alpha = sigma;
				[_encoder setRenderPipelineState: _blur_state];
				[_encoder setFragmentTexture: blur_region_mip atIndex: 0];
				[_encoder setVertexBytes: &rinfo length: sizeof(rinfo) atIndex: 2];
				[_encoder setVertexBuffer: _common_area offset: 0 atIndex: 1];
				[_encoder drawPrimitives: MTLPrimitiveTypeTriangle vertexStart: 0 vertexCount: 6];
				[blur_region release];
				[blur_region_mip release];
			}
			virtual void Render(IInversionEffectBrush * brush, const Box & at, bool blink) noexcept override
			{
				if (!brush || at.Left >= at.Right || at.Top >= at.Bottom) return;
				if (blink && !_dropback_device->IsCaretVisible()) return;
				[_encoder setRenderPipelineState: _invert_state];
				[_encoder setFragmentTexture: _white atIndex: 0];
				[_encoder setVertexBytes: &at length: sizeof(at) atIndex: 2];
				[_encoder setVertexBuffer: _common_area offset: 0 atIndex: 1];
				[_encoder drawPrimitives: MTLPrimitiveTypeTriangle vertexStart: 0 vertexCount: 6];
			}
			// Polygon functions
			virtual void RenderPolyline(const Math::Vector2 * points, int count, Color color, double width) noexcept override {}
			virtual void RenderPolygon(const Math::Vector2 * points, int count, Color color) noexcept override {}
			// Time and animation API
			virtual void SetAnimationTime(uint32 value) noexcept override { _dropback_device->SetAnimationTime(value); }
			virtual uint32 GetAnimationTime(void) noexcept override { return _dropback_device->GetAnimationTime(); }
			virtual void SetCaretReferenceTime(uint32 value) noexcept override { _dropback_device->SetCaretReferenceTime(value); }
			virtual uint32 GetCaretReferenceTime(void) noexcept override { return _dropback_device->GetCaretReferenceTime(); }
			virtual void SetCaretBlinkPeriod(uint32 value) noexcept override { _dropback_device->SetCaretBlinkPeriod(value); }
			virtual uint32 GetCaretBlinkPeriod(void) noexcept override { return _dropback_device->GetCaretBlinkPeriod(); }
			virtual bool IsCaretVisible(void) noexcept override { return _dropback_device->IsCaretVisible(); }
			// Parent device information
			virtual IDevice * GetParentDevice(void) noexcept override { return _parent_device; }
			virtual I2DDeviceContextFactory * GetParentFactory(void) noexcept override { return _parent_factory; }
		};
		class MTL_2DPresentationEngine : public MetalGraphics::MetalPresentationEngine
		{
			SafePointer<MTL_2DDeviceContext> _device;
			id<CAMetalDrawable> _drawable;
		public:
			MTL_2DPresentationEngine(void) : _drawable(0) {}
			virtual ~MTL_2DPresentationEngine(void) override {}
			virtual void Attach(Windows::ICoreWindow * window) override
			{
				MetalGraphics::MetalPresentationEngine::Attach(window);
				try { _device = new MTL_2DDeviceContext(MetalGraphics::GetMetalCommonDevice()); } catch (...) { return; }
				SetDevice(MetalGraphics::GetInnerMetalDevice(_device->GetParentDevice()));
				SetAutosize(true);
				SetOpaque(false);
				SetFramebufferOnly(false);
				if (!_device->InitializeWithPixelFormat(GetPixelFormat())) { _device.SetReference(0); }
			}
			virtual void Detach(void) override { _device.SetReference(0); MetalGraphics::MetalPresentationEngine::Detach(); }
			virtual Graphics::I2DDeviceContext * GetContext(void) noexcept override { return _device; }
			virtual bool BeginRenderingPass(void) noexcept override
			{
				if (_device && !_drawable) {
					_drawable = GetDrawable();
					if (_drawable) {
						auto size = GetSize();
						auto descriptor = [MTLRenderPassDescriptor renderPassDescriptor];
						descriptor.colorAttachments[0].texture = _drawable.texture;
						descriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
						descriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
						auto command_buffer = [MetalGraphics::GetInnerMetalQueue(_device->GetParentDevice()) commandBuffer];
						return _device->BeginRendering(command_buffer, descriptor, size.x, size.y);
					} else return false;
				} else return false;
			}
			virtual bool EndRenderingPass(void) noexcept override
			{
				if (_device && _drawable) {
					auto status = _device->EndRendering(_drawable);
					_drawable = 0;
					return status;
				} else return false;
			}
			virtual string ToString(void) const override { return L"MTL_2DPresentationEngine"; }
		};

		id<MTLLibrary> CreateMetalRenderingDeviceShaders(id<MTLDevice> device)
		{
			id<MTLLibrary> library = 0;
			@autoreleasepool {
				const void * shaders_data;
				int shaders_size;
				NSError * error;
				GetMetalDeviceShaders(&shaders_data, &shaders_size);
				dispatch_data_t data_handle = dispatch_data_create(shaders_data, shaders_size, dispatch_get_main_queue(), DISPATCH_DATA_DESTRUCTOR_DEFAULT);
				library = [device newLibraryWithData: data_handle error: &error];
				[data_handle release];
				if (error) [error release];
			}
			return library;
		}
		Windows::I2DPresentationEngine * CreateMetalPresentationEngine(void) { return new MTL_2DPresentationEngine; }
		Graphics::I2DDeviceContext * CreateMetalRenderingDevice(Graphics::IDevice * device)
		{
			try {
				SafePointer<MTL_2DDeviceContext> context = new MTL_2DDeviceContext(device);
				if (!context->InitializeWithPixelFormat(MTLPixelFormatBGRA8Unorm)) throw Exception();
				context->Retain();
				return context;
			} catch (...) { return 0; }
		}
		void PureMetalRenderingDeviceBeginDraw(Graphics::I2DDeviceContext * device, id<MTLCommandBuffer> command, id<MTLTexture> texture, uint width, uint height)
		{
			MTLRenderPassDescriptor * descriptor = [MTLRenderPassDescriptor renderPassDescriptor];
			descriptor.colorAttachments[0].texture = texture;
			descriptor.colorAttachments[0].loadAction = MTLLoadActionLoad;
			static_cast<MTL_2DDeviceContext *>(device)->BeginRendering(command, descriptor, width, height);
		}
		void PureMetalRenderingDeviceEndDraw(Graphics::I2DDeviceContext * device) { static_cast<MTL_2DDeviceContext *>(device)->EndRendering(0); }
	}
}