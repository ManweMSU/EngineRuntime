#include "Direct3D.h"

#undef CreateWindow
#undef LoadCursor

#include "Direct2D.h"
#include "WindowLayers.h"
#include "../Storage/Archive.h"
#include "../Interfaces/SystemWindows.h"

#include <VersionHelpers.h>
#include <d3dcompiler.h>
#include <d3d11_4.h>
#include <sddl.h>

#undef ZeroMemory

using namespace Engine::Graphics;

namespace Engine
{
	namespace Direct3D
	{
		typedef HRESULT (WINAPI * func_D3DCompile) (LPCVOID src_data, SIZE_T src_size, LPCSTR name, const D3D_SHADER_MACRO * defines,
			ID3DInclude * include, LPCSTR entrypoint, LPCSTR target_model, UINT flags1, UINT flags2, ID3DBlob ** result, ID3DBlob ** errors);

		SafePointer<Graphics::IDeviceFactory> CommonFactory;
		SafePointer<Graphics::IDevice> CommonDevice;

		bool CreateD2DDeviceContextForWindow(HWND Window, ID2D1DeviceContext ** Context, IDXGISwapChain1 ** SwapChain) noexcept
		{
			ID2D1Device * device_d2d1;
			if (!CommonDevice || !(device_d2d1 = GetInnerObject2D(CommonDevice))) return false;
			SafePointer<ID2D1DeviceContext> Result;
			SafePointer<IDXGISwapChain1> SwapChainResult;
			if (device_d2d1->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, Result.InnerRef()) != S_OK) return false;

			DXGI_SWAP_CHAIN_DESC1 SwapChainDescription;
			ZeroMemory(&SwapChainDescription, sizeof(SwapChainDescription));
			SwapChainDescription.Width = 0;
			SwapChainDescription.Height = 0;
			SwapChainDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			SwapChainDescription.Stereo = false;
			SwapChainDescription.SampleDesc.Count = 1;
			SwapChainDescription.SampleDesc.Quality = 0;
			SwapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			SwapChainDescription.BufferCount = 2;
			SwapChainDescription.Scaling = DXGI_SCALING_NONE;
			SwapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
			SwapChainDescription.Flags = 0;
			SwapChainDescription.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
			SwapChainDescription.Scaling = DXGI_SCALING_STRETCH;

			SafePointer<IDXGIDevice1> DXGIDevice;
			if (GetInnerObject(CommonDevice)->QueryInterface(IID_IDXGIDevice1, reinterpret_cast<void **>(DXGIDevice.InnerRef())) != S_OK) return false;
			SafePointer<IDXGIAdapter> Adapter;
			DXGIDevice->GetAdapter(Adapter.InnerRef());
			SafePointer<IDXGIFactory2> Factory;
			Adapter->GetParent(IID_PPV_ARGS(Factory.InnerRef()));
			if (Factory->CreateSwapChainForHwnd(GetInnerObject(Direct3D::CommonDevice), Window, &SwapChainDescription, 0, 0, SwapChainResult.InnerRef()) != S_OK) return false;
			Factory->MakeWindowAssociation(Window, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_PRINT_SCREEN);
			DXGIDevice->SetMaximumFrameLatency(1);

			D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
				D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 0.0f, 0.0f);
			SafePointer<IDXGISurface> Surface;
			if (SwapChainResult->GetBuffer(0, IID_PPV_ARGS(Surface.InnerRef())) != S_OK) return false;
			SafePointer<ID2D1Bitmap1> Bitmap;
			if (Result->CreateBitmapFromDxgiSurface(Surface, props, Bitmap.InnerRef()) != S_OK) return false;
			Result->SetTarget(Bitmap);

			*Context = Result;
			*SwapChain = SwapChainResult;
			Result->AddRef();
			SwapChainResult->AddRef();
			return true;
		}
		bool CreateSwapChainForWindow(HWND Window, IDXGISwapChain ** SwapChain) noexcept
		{
			if (!CommonDevice) return false;
			DXGI_SWAP_CHAIN_DESC SwapChainDescription;
			ZeroMemory(&SwapChainDescription, sizeof(SwapChainDescription));
			SwapChainDescription.BufferDesc.Width = 0;
			SwapChainDescription.BufferDesc.Height = 0;
			SwapChainDescription.BufferDesc.RefreshRate.Numerator = 60;
			SwapChainDescription.BufferDesc.RefreshRate.Denominator = 1;
			SwapChainDescription.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			SwapChainDescription.BufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
			SwapChainDescription.SampleDesc.Count = 1;
			SwapChainDescription.SampleDesc.Quality = 0;
			SwapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			SwapChainDescription.BufferCount = 1;
			SwapChainDescription.OutputWindow = Window;
			SwapChainDescription.Windowed = TRUE;
			SafePointer<IDXGIDevice> dxgi_device;
			SafePointer<IDXGIAdapter> dxgi_adapter;
			SafePointer<IDXGIFactory> factory;
			if (GetInnerObject(CommonDevice)->QueryInterface(IID_PPV_ARGS(dxgi_device.InnerRef())) != S_OK) return false;
			if (dxgi_device->GetAdapter(dxgi_adapter.InnerRef()) != S_OK) return false;
			if (dxgi_adapter->GetParent(IID_PPV_ARGS(factory.InnerRef())) != S_OK) return false;
			if (factory->CreateSwapChain(GetInnerObject(CommonDevice), &SwapChainDescription, SwapChain) != S_OK) return false;
			factory->MakeWindowAssociation(Window, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_PRINT_SCREEN);
			return true;
		}
		bool CreateSwapChainDevice(IDXGISwapChain * SwapChain, ID2D1RenderTarget ** Target) noexcept
		{
			if (!Direct2D::D2DFactory) return false;
			IDXGISurface * Surface;
			if (SwapChain->GetBuffer(0, IID_PPV_ARGS(&Surface)) != S_OK) return false;
			D2D1_RENDER_TARGET_PROPERTIES RenderTargetProps;
			RenderTargetProps.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
			RenderTargetProps.pixelFormat.format = DXGI_FORMAT_UNKNOWN;
			RenderTargetProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
			RenderTargetProps.dpiX = RenderTargetProps.dpiY = 0.0f;
			RenderTargetProps.usage = D2D1_RENDER_TARGET_USAGE_NONE;
			RenderTargetProps.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
			if (Direct2D::D2DFactory->CreateDxgiSurfaceRenderTarget(Surface, &RenderTargetProps, Target) != S_OK) {
				Surface->Release();
				return false;
			}
			Surface->Release();
			return true;
		}
		bool ResizeRenderBufferForD2DDevice(ID2D1DeviceContext * Context, IDXGISwapChain1 * SwapChain) noexcept
		{
			if (Context && SwapChain) {
				Context->SetTarget(0);
				if (SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0) != S_OK) return false;
				D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
					D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 0.0f, 0.0f);
				SafePointer<IDXGISurface> Surface;
				if (SwapChain->GetBuffer(0, IID_PPV_ARGS(Surface.InnerRef())) != S_OK) return false;
				SafePointer<ID2D1Bitmap1> Bitmap;
				if (Context->CreateBitmapFromDxgiSurface(Surface, props, Bitmap.InnerRef()) != S_OK) return false;
				Context->SetTarget(Bitmap);
				return true;
			} else return false;
		}
		bool ResizeRenderBufferForSwapChainDevice(IDXGISwapChain * SwapChain) noexcept { if (SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0) != S_OK) return false; return true; }

		class D3D11_FakeEngine : public Windows::IPresentationEngine
		{
			HWND _handle;
			SafePointer<Windows::IWindowLayers> _layers;
			Windows::ICoreWindow * _window;
		public:
			D3D11_FakeEngine(void) : _handle(0), _window(0) {}
			virtual ~D3D11_FakeEngine(void) override {}
			virtual void Attach(Windows::ICoreWindow * window) override
			{
				_window = window;
				_handle = reinterpret_cast<HWND>(window->GetOSHandle());
				Windows::SetWindowUserRenderingCallback(window);
			}
			virtual void Detach(void) override
			{
				_window = 0; _handle = 0;
				if (_layers) _layers.SetReference(0);
			}
			virtual void Invalidate(void) override { InvalidateRect(_handle, 0, FALSE); }
			virtual void Resize(int width, int height) override {}
			bool IsAttached(void) noexcept { return _handle; }
			void SetLayers(Windows::IWindowLayers * layers) noexcept { _layers.SetRetain(layers); Windows::SetWindowLayers(_window, _layers); }
			Windows::IWindowLayers * GetLayers(void) noexcept { return _layers; }
		};
		class D3D11_DeviceResourceHandle : public Graphics::IDeviceResourceHandle
		{
		public:
			string _nt_path;
			HANDLE _nt_handle;
			uint64 _device_id;
			uint32 _format, _usage, _width, _height, _depth, _size;
			TextureType _type;
		public:
			D3D11_DeviceResourceHandle(IUnknown * resource, IDevice * parent, ITexture * wrapper)
			{
				_device_id = parent->GetDeviceIdentifier();
				_type = wrapper->GetTextureType();
				_format = uint(wrapper->GetPixelFormat());
				_usage = wrapper->GetResourceUsage() & (ResourceUsageShaderRead | ResourceUsageShaderWrite | ResourceUsageRenderTarget | ResourceUsageDepthStencil);
				_width = wrapper->GetWidth();
				_height = wrapper->GetHeight();
				_depth = wrapper->GetDepth();
				_size = wrapper->GetArraySize();
				_nt_path = L"shared-texture-" + string(uint32(GetCurrentProcessId()), HexadecimalBaseLowerCase, 8) + L"-" + string(reinterpret_cast<uint64>(this), HexadecimalBaseLowerCase, 16);
				IDXGIResource1 * dxgi;
				if (resource->QueryInterface(IID_IDXGIResource1, reinterpret_cast<void **>(&dxgi)) != S_OK) throw Exception();
				if (dxgi->CreateSharedHandle(0, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, _nt_path, &_nt_handle) != S_OK) { dxgi->Release(); throw Exception(); }
				dxgi->Release();
			}
			D3D11_DeviceResourceHandle(const DataBlock * serialized) : _nt_handle(INVALID_HANDLE_VALUE)
			{
				if (!serialized || serialized->Length() < 36) throw InvalidFormatException();
				auto view_uint64 = reinterpret_cast<const uint64 *>(serialized->GetBuffer());
				auto view_uint32 = reinterpret_cast<const uint32 *>(serialized->GetBuffer());
				_device_id = view_uint64[0];
				if (view_uint32[2] == 0) _type = TextureType::Type1D;
				else if (view_uint32[2] == 1) _type = TextureType::TypeArray1D;
				else if (view_uint32[2] == 2) _type = TextureType::Type2D;
				else if (view_uint32[2] == 3) _type = TextureType::TypeArray2D;
				else if (view_uint32[2] == 4) _type = TextureType::TypeCube;
				else if (view_uint32[2] == 5) _type = TextureType::TypeArrayCube;
				else if (view_uint32[2] == 6) _type = TextureType::Type3D;
				else throw InvalidFormatException();
				_format = view_uint32[3];
				_usage = view_uint32[4];
				_width = view_uint32[5];
				_height = view_uint32[6];
				_depth = view_uint32[7];
				_size = view_uint32[8];
				_nt_path = string(serialized->GetBuffer() + 36, serialized->Length() - 36, Encoding::ANSI);
			}
			virtual ~D3D11_DeviceResourceHandle(void) override { if (_nt_handle != INVALID_HANDLE_VALUE) CloseHandle(_nt_handle); }
			virtual uint64 GetDeviceIdentifier(void) noexcept override { return _device_id; }
			virtual DataBlock * Serialize(void) noexcept override
			{
				try {
					SafePointer<DataBlock> result = new DataBlock(1);
					result->SetLength(36 + _nt_path.GetEncodedLength(Encoding::ANSI));
					auto view_uint64 = reinterpret_cast<uint64 *>(result->GetBuffer());
					auto view_uint32 = reinterpret_cast<uint32 *>(result->GetBuffer());
					view_uint64[0] = _device_id;
					if (_type == TextureType::Type1D) view_uint32[2] = 0;
					else if (_type == TextureType::TypeArray1D) view_uint32[2] = 1;
					else if (_type == TextureType::Type2D) view_uint32[2] = 2;
					else if (_type == TextureType::TypeArray2D) view_uint32[2] = 3;
					else if (_type == TextureType::TypeCube) view_uint32[2] = 4;
					else if (_type == TextureType::TypeArrayCube) view_uint32[2] = 5;
					else if (_type == TextureType::Type3D) view_uint32[2] = 6;
					else throw InvalidStateException();
					view_uint32[3] = _format;
					view_uint32[4] = _usage;
					view_uint32[5] = _width;
					view_uint32[6] = _height;
					view_uint32[7] = _depth;
					view_uint32[8] = _size;
					_nt_path.Encode(result->GetBuffer() + 36, Encoding::ANSI, false);
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			virtual string ToString(void) const override { return L"D3D11_DeviceResourceHandle"; }
		};
		class D3D11_Buffer : public Graphics::IBuffer
		{
			IDevice * wrapper;
		public:
			ID3D11Buffer * buffer;
			ID3D11Buffer * buffer_staging;
			ID3D11ShaderResourceView * view;
			ResourceMemoryPool pool;
			uint32 usage_flags, length, stride;

			D3D11_Buffer(IDevice * _wrapper) : wrapper(_wrapper), buffer(0), buffer_staging(0), view(0),
				pool(ResourceMemoryPool::Default), usage_flags(0), length(0), stride(0) {}
			virtual ~D3D11_Buffer(void) override
			{
				if (buffer) buffer->Release();
				if (buffer_staging) buffer_staging->Release();
				if (view) view->Release();
			}
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual ResourceType GetResourceType(void) noexcept override { return ResourceType::Buffer; }
			virtual ResourceMemoryPool GetMemoryPool(void) noexcept override { return pool; }
			virtual uint32 GetResourceUsage(void) noexcept override { return usage_flags; }
			virtual uint32 GetLength(void) noexcept override { return length; }
			virtual string ToString(void) const override { return L"D3D11_Buffer"; }
		};
		class D3D11_Texture : public Graphics::ITexture
		{
			TextureType type;
			IDevice * wrapper;
		public:
			ID3D11Texture1D * tex_1d;
			ID3D11Texture2D * tex_2d;
			ID3D11Texture3D * tex_3d;
			ID3D11Texture1D * tex_staging_1d;
			ID3D11Texture2D * tex_staging_2d;
			ID3D11Texture3D * tex_staging_3d;
			ID3D11ShaderResourceView * view;
			ID3D11RenderTargetView * rt_view;
			ID3D11DepthStencilView * ds_view;
			ResourceMemoryPool pool;
			PixelFormat format;
			uint32 usage_flags;
			uint32 width, height, depth, size;
			SafePointer<D3D11_DeviceResourceHandle> shared_handle;
		public:
			D3D11_Texture(TextureType _type, IDevice * _wrapper) : type(_type), wrapper(_wrapper), tex_1d(0), tex_2d(0), tex_3d(0),
				tex_staging_1d(0), tex_staging_2d(0), tex_staging_3d(0), view(0), rt_view(0), ds_view(0), pool(ResourceMemoryPool::Default),
				format(PixelFormat::Invalid), usage_flags(0), width(0), height(0), depth(0), size(0) {}
			virtual ~D3D11_Texture(void) override
			{
				if (tex_1d) tex_1d->Release();
				if (tex_2d) tex_2d->Release();
				if (tex_3d) tex_3d->Release();
				if (tex_staging_1d) tex_staging_1d->Release();
				if (tex_staging_2d) tex_staging_2d->Release();
				if (tex_staging_3d) tex_staging_3d->Release();
				if (view) view->Release();
				if (rt_view) rt_view->Release();
				if (ds_view) ds_view->Release();
			}
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual ResourceType GetResourceType(void) noexcept override { return ResourceType::Texture; }
			virtual ResourceMemoryPool GetMemoryPool(void) noexcept override { return pool; }
			virtual uint32 GetResourceUsage(void) noexcept override { return usage_flags; }
			virtual TextureType GetTextureType(void) noexcept override { return type; }
			virtual PixelFormat GetPixelFormat(void) noexcept override { return format; }
			virtual uint32 GetWidth(void) noexcept override { return width; }
			virtual uint32 GetHeight(void) noexcept override { return height; }
			virtual uint32 GetDepth(void) noexcept override { return depth; }
			virtual uint32 GetMipmapCount(void) noexcept override
			{
				if (tex_1d) {
					D3D11_TEXTURE1D_DESC desc;
					tex_1d->GetDesc(&desc);
					return desc.MipLevels;
				} else if (tex_2d) {
					D3D11_TEXTURE2D_DESC desc;
					tex_2d->GetDesc(&desc);
					return desc.MipLevels;
				} else if (tex_3d) {
					D3D11_TEXTURE3D_DESC desc;
					tex_3d->GetDesc(&desc);
					return desc.MipLevels;
				} else return 0;
			}
			virtual uint32 GetArraySize(void) noexcept override { return size; }
			virtual string ToString(void) const override { return L"D3D11_Texture"; }
		};
		class D3D11_Shader : public Graphics::IShader
		{
			IDevice * wrapper;
			string name;
		public:
			ID3D11VertexShader * vs;
			ID3D11PixelShader * ps;

			D3D11_Shader(IDevice * _wrapper, const string & _name) : wrapper(_wrapper), name(_name), vs(0), ps(0) {}
			virtual ~D3D11_Shader(void) { if (vs) vs->Release(); if (ps) ps->Release(); }
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual string GetName(void) noexcept override { return name; }
			virtual ShaderType GetType(void) noexcept override
			{
				if (vs) return ShaderType::Vertex;
				else if (ps) return ShaderType::Pixel;
				else return ShaderType::Unknown;
			}
			virtual string ToString(void) const override { return L"D3D11_Shader"; }
		};
		class D3D11_ShaderLibrary : public Graphics::IShaderLibrary
		{
			IDevice * wrapper;
			ID3D11Device * device;
			SafePointer<Storage::Archive> shader_arc;
		public:
			D3D11_ShaderLibrary(IDevice * _wrapper, ID3D11Device * _device, Streaming::Stream * source) : wrapper(_wrapper), device(_device)
			{
				device->AddRef();
				shader_arc = Storage::OpenArchive(source, Storage::ArchiveMetadataUsage::IgnoreMetadata);
				if (!shader_arc) throw Exception();
			}
			virtual ~D3D11_ShaderLibrary(void) override { if (device) device->Release(); }
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual Array<string> * GetShaderNames(void) noexcept override
			{
				try {
					SafePointer< Array<string> > result = new Array<string>(0x10);
					for (Storage::ArchiveFile file = 1; file <= shader_arc->GetFileCount(); file++) {
						result->Append(shader_arc->GetFileName(file));
					}
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			virtual IShader * CreateShader(const string & name) noexcept override
			{
				try {
					auto file = shader_arc->FindArchiveFile(name);
					if (!file) return 0;
					auto attr = shader_arc->GetFileCustomData(file);
					auto real_name = shader_arc->GetFileName(file);
					SafePointer<Streaming::Stream> stream = shader_arc->QueryFileStream(file, Storage::ArchiveStream::Native);
					SafePointer<DataBlock> data = stream->ReadAll();
					SafePointer<D3D11_Shader> shader = new D3D11_Shader(wrapper, real_name);
					if (attr == 1) {
						if (device->CreateVertexShader(data->GetBuffer(), data->Length(), 0, &shader->vs) != S_OK) return 0;
					} else if (attr == 2) {
						if (device->CreatePixelShader(data->GetBuffer(), data->Length(), 0, &shader->ps) != S_OK) return 0;
					} else return 0;
					if (shader) {
						shader->Retain();
						return shader;
					} else return 0;
				} catch (...) { return 0; }
			}
			virtual string ToString(void) const override { return L"D3D11_ShaderLibrary"; }
		};
		class D3D11_DynamicShaderLibrary : public Graphics::IShaderLibrary
		{
			struct _shader_data { string name; ShaderType type; SafePointer<DataBlock> data; };
			IDevice * wrapper;
			ID3D11Device * device;
			Array<_shader_data> _shaders;
		public:
			D3D11_DynamicShaderLibrary(IDevice * _wrapper, ID3D11Device * _device) : wrapper(_wrapper), device(_device), _shaders(0x10) { device->AddRef(); }
			virtual ~D3D11_DynamicShaderLibrary(void) override { if (device) device->Release(); }
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual Array<string> * GetShaderNames(void) noexcept override
			{
				try {
					SafePointer< Array<string> > result = new Array<string>(0x10);
					for (auto & s : _shaders) result->Append(s.name);
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			virtual IShader * CreateShader(const string & name) noexcept override
			{
				try {
					int index = -1;
					for (int i = 0; i < _shaders.Length(); i++) if (string::CompareIgnoreCase(_shaders[i].name, name) == 0) { index = i; break; }
					if (index < 0) return 0;
					SafePointer<D3D11_Shader> shader = new D3D11_Shader(wrapper, _shaders[index].name);
					if (_shaders[index].type == ShaderType::Vertex) {
						if (device->CreateVertexShader(_shaders[index].data->GetBuffer(), _shaders[index].data->Length(), 0, &shader->vs) != S_OK) return 0;
					} else if (_shaders[index].type == ShaderType::Pixel) {
						if (device->CreatePixelShader(_shaders[index].data->GetBuffer(), _shaders[index].data->Length(), 0, &shader->ps) != S_OK) return 0;
					} else return 0;
					if (shader) {
						shader->Retain();
						return shader;
					} else return 0;
				} catch (...) { return 0; }
			}
			virtual string ToString(void) const override { return L"D3D11_DynamicShaderLibrary"; }
			bool Append(const string & name, ShaderType type, DataBlock * data) noexcept
			{
				try {
					_shader_data dta;
					dta.name = name;
					dta.type = type;
					dta.data.SetRetain(data);
					_shaders.Append(dta);
					return true;
				} catch (...) { return false; }
			}
			bool IsEmpty(void) const noexcept { return _shaders.Length() == 0; }
		};
		class D3D11_PipelineState : public Graphics::IPipelineState
		{
			IDevice * wrapper;
		public:
			ID3D11VertexShader * vs;
			ID3D11PixelShader * ps;
			ID3D11BlendState * bs;
			ID3D11DepthStencilState * dss;
			ID3D11RasterizerState * rs;
			D3D11_PRIMITIVE_TOPOLOGY pt;

			D3D11_PipelineState(IDevice * _wrapper) : wrapper(_wrapper), vs(0), ps(0), bs(0), dss(0), rs(0), pt(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED) {}
			virtual ~D3D11_PipelineState(void) override
			{
				if (vs) vs->Release();
				if (ps) ps->Release();
				if (bs) bs->Release();
				if (dss) dss->Release();
				if (rs) rs->Release();
			}
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual string ToString(void) const override { return L"D3D11_PipelineState"; }
		};
		class D3D11_SamplerState : public Graphics::ISamplerState
		{
			IDevice * wrapper;
		public:
			ID3D11SamplerState * state;

			D3D11_SamplerState(IDevice * _wrapper) : wrapper(_wrapper), state(0) {}
			virtual ~D3D11_SamplerState(void) override { if (state) state->Release(); }
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual string ToString(void) const override { return L"D3D11_SamplerState"; }
		};
		class D3D11_DeviceContext : public Graphics::IDeviceContext
		{
			IDevice * wrapper;
			ID3D11Device * device;
			ID3D11DeviceContext * context;
			ID3D11DepthStencilState * depth_stencil_state;
			ID2D1Device * d2d1_device;
			ID2D1RenderTarget * device_2d_render_target;
			ID2D1DeviceContext * device_2d_device_context;
			Direct2D::D2D_DeviceContext * device_2d;
			int pass_mode;
			bool pass_state;
			uint32 stencil_ref;
		public:
			D3D11_DeviceContext(ID3D11Device * _device, IDevice * _wrapper) : pass_mode(0), pass_state(false), context(0),
				device_2d(0), d2d1_device(0), device_2d_render_target(0), device_2d_device_context(0), depth_stencil_state(0), stencil_ref(0)
			{
				device = _device;
				wrapper = _wrapper;
				device->AddRef();
				device->GetImmediateContext(&context);
			}
			virtual ~D3D11_DeviceContext(void) override
			{
				device->Release();
				context->Release();
				if (device_2d) device_2d->Release();
				if (device_2d_render_target) device_2d_render_target->Release();
				if (device_2d_device_context) device_2d_device_context->Release();
				if (d2d1_device) d2d1_device->Release();
				if (depth_stencil_state) depth_stencil_state->Release();
			}
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual bool BeginRenderingPass(uint32 rtc, const RenderTargetViewDesc * rtv, const DepthStencilViewDesc * dsv) noexcept override
			{
				if (pass_mode || !rtc || rtc > 8) return false;
				ID3D11RenderTargetView * rtvv[8];
				ID3D11DepthStencilView * dsvv;
				for (uint i = 0; i < rtc; i++) {
					auto object = static_cast<D3D11_Texture *>(rtv[i].Texture);
					if (!object->rt_view) return false;
					rtvv[i] = object->rt_view;
				}
				if (dsv) {
					auto object = static_cast<D3D11_Texture *>(dsv->Texture);
					if (!object->ds_view) return false;
					dsvv = object->ds_view;
				} else dsvv = 0;
				context->OMSetRenderTargets(rtc, rtvv, dsvv);
				for (uint i = 0; i < rtc; i++) if (rtv[i].LoadAction == TextureLoadAction::Clear) {
					context->ClearRenderTargetView(rtvv[i], rtv[i].ClearValue);
				}
				if (dsv) {
					UINT clr = 0;
					if (dsv->DepthLoadAction == TextureLoadAction::Clear) clr |= D3D11_CLEAR_DEPTH;
					if (dsv->StencilLoadAction == TextureLoadAction::Clear) clr |= D3D11_CLEAR_STENCIL;
					if (clr) context->ClearDepthStencilView(dsvv, clr, dsv->DepthClearValue, dsv->StencilClearValue);
				}
				pass_mode = 1;
				pass_state = true;
				return true;
			}
			virtual bool Begin2DRenderingPass(const RenderTargetViewDesc & rtv) noexcept override
			{
				if (pass_mode || !rtv.Texture || rtv.Texture->GetTextureType() != TextureType::Type2D || rtv.Texture->GetMipmapCount() != 1) return false;
				if (rtv.Texture->GetPixelFormat() != PixelFormat::B8G8R8A8_unorm) return false;
				if (!(rtv.Texture->GetResourceUsage() & ResourceUsageRenderTarget)) return false;
				auto rsrc = GetInnerObject(rtv.Texture);
				ID2D1RenderTarget * target = 0;
				if (!device_2d) {
					if (!Direct2D::D2DFactory) Direct2D::InitializeFactory();
					if (!d2d1_device) TryAllocateDeviceD2D1();
					if (d2d1_device) {
						if (d2d1_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &device_2d_device_context) != S_OK) return false;
						try { device_2d = new Direct2D::D2D_DeviceContext; } catch (...) { device_2d_device_context->Release(); device_2d_device_context = 0; return false; }
						device_2d->SetRenderTargetEx(device_2d_device_context);
						device_2d->SetWrappedDevice(wrapper);
						IDXGISurface * surface;
						if (rsrc->QueryInterface(IID_PPV_ARGS(&surface)) != S_OK) {
							device_2d_device_context->Release(); device_2d_device_context = 0;
							device_2d->Release(); device_2d = 0; return false;
						}
						D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
							D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 0.0f, 0.0f);
						ID2D1Bitmap1 * bitmap;
						if (device_2d_device_context->CreateBitmapFromDxgiSurface(surface, props, &bitmap) != S_OK) {
							device_2d_device_context->Release(); device_2d_device_context = 0;
							device_2d->Release(); device_2d = 0; surface->Release(); return false;
						}
						device_2d_device_context->SetTarget(bitmap);
						surface->Release();
						bitmap->Release();
						target = device_2d_device_context;
					} else {
						if (!Direct2D::D2DFactory) return false;
						IDXGISurface * surface;
						if (rsrc->QueryInterface(IID_PPV_ARGS(&surface)) != S_OK) return false;
						D2D1_RENDER_TARGET_PROPERTIES props;
						props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
						props.pixelFormat.format = DXGI_FORMAT_UNKNOWN;
						props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
						props.dpiX = props.dpiY = 0.0f;
						props.usage = D2D1_RENDER_TARGET_USAGE_NONE;
						props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
						if (Direct2D::D2DFactory->CreateDxgiSurfaceRenderTarget(surface, &props, &device_2d_render_target) != S_OK) { surface->Release(); return false; }
						surface->Release();
						try { device_2d = new Direct2D::D2D_DeviceContext; }
						catch (...) { device_2d_render_target->Release(); device_2d_render_target = 0; return false; }
						device_2d->SetRenderTarget(device_2d_render_target);
						device_2d->SetWrappedDevice(wrapper);
						target = device_2d_render_target;
					}
				} else {
					IDXGISurface * surface;
					if (rsrc->QueryInterface(IID_PPV_ARGS(&surface)) != S_OK) return false;
					if (device_2d_device_context) {
						D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
							D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 0.0f, 0.0f);
						ID2D1Bitmap1 * bitmap;
						if (device_2d_device_context->CreateBitmapFromDxgiSurface(surface, props, &bitmap) != S_OK) { surface->Release(); return false; }
						device_2d_device_context->SetTarget(bitmap);
						bitmap->Release();
						target = device_2d_device_context;
					} else {
						D2D1_RENDER_TARGET_PROPERTIES props;
						props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
						props.pixelFormat.format = DXGI_FORMAT_UNKNOWN;
						props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
						props.dpiX = props.dpiY = 0.0f;
						props.usage = D2D1_RENDER_TARGET_USAGE_NONE;
						props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
						if (Direct2D::D2DFactory->CreateDxgiSurfaceRenderTarget(surface, &props, &device_2d_render_target) != S_OK) { surface->Release(); return false; }
						device_2d->SetRenderTarget(device_2d_render_target);
						target = device_2d_render_target;
					}
					surface->Release();
				}
				target->SetDpi(96.0f, 96.0f);
				target->BeginDraw();
				pass_mode = 2;
				pass_state = true;
				if (rtv.LoadAction == TextureLoadAction::Clear) {
					D2D1_COLOR_F clear;
					clear.r = rtv.ClearValue[0];
					clear.g = rtv.ClearValue[1];
					clear.b = rtv.ClearValue[2];
					clear.a = rtv.ClearValue[3];
					target->Clear(clear);
				}
				return true;
			}
			virtual bool BeginMemoryManagementPass(void) noexcept override
			{
				if (pass_mode) return false;
				pass_mode = 3;
				pass_state = true;
				return true;
			}
			virtual bool EndCurrentPass(void) noexcept override
			{
				if (pass_mode) {
					if (pass_mode == 1) {
						context->ClearState();
						if (depth_stencil_state) depth_stencil_state->Release();
						depth_stencil_state = 0;
						stencil_ref = 0;
					} else if (pass_mode == 2) {
						if (device_2d_device_context) {
							if (device_2d_device_context->EndDraw() != S_OK) pass_state = false;
							device_2d_device_context->SetTarget(0);
						} else if (device_2d_render_target) {
							if (device_2d_render_target->EndDraw() != S_OK) pass_state = false;
							device_2d->SetRenderTarget(0);
							device_2d_render_target->Release();
							device_2d_render_target = 0;
						}
					}
					pass_mode = 0;
					return pass_state;
				} else return false;
			}
			virtual void Flush(void) noexcept override { context->Flush(); }
			virtual void SetRenderingPipelineState(IPipelineState * state) noexcept override
			{
				if (pass_mode != 1 || !state) { pass_state = false; return; }
				auto object = static_cast<D3D11_PipelineState *>(state);
				if (depth_stencil_state) { depth_stencil_state->Release(); depth_stencil_state = 0; }
				depth_stencil_state = object->dss;
				if (depth_stencil_state) depth_stencil_state->AddRef();
				context->VSSetShader(object->vs, 0, 0);
				context->PSSetShader(object->ps, 0, 0);
				context->OMSetBlendState(object->bs, 0, 0xFFFFFFFF);
				context->OMSetDepthStencilState(depth_stencil_state, stencil_ref);
				context->RSSetState(object->rs);
				context->IASetPrimitiveTopology(object->pt);
			}
			virtual void SetViewport(float top_left_x, float top_left_y, float width, float height, float min_depth, float max_depth) noexcept override
			{
				if (pass_mode != 1) { pass_state = false; return; }
				D3D11_VIEWPORT vp;
				vp.TopLeftX = top_left_x;
				vp.TopLeftY = top_left_y;
				vp.Width = width;
				vp.Height = height;
				vp.MinDepth = min_depth;
				vp.MaxDepth = max_depth;
				context->RSSetViewports(1, &vp);
			}
			virtual void SetVertexShaderResource(uint32 at, IDeviceResource * resource) noexcept override
			{
				if (pass_mode != 1) { pass_state = false; return; }
				if (resource) {
					if (resource->GetResourceType() == ResourceType::Texture) {
						auto object = static_cast<D3D11_Texture *>(resource);
						if (!object->view) { pass_state = false; return; }
						context->VSSetShaderResources(at, 1, &object->view);
					} else if (resource->GetResourceType() == ResourceType::Buffer) {
						auto object = static_cast<D3D11_Buffer *>(resource);
						if (!object->view) { pass_state = false; return; }
						context->VSSetShaderResources(at, 1, &object->view);
					} else { pass_state = false; return; }
				} else {
					ID3D11ShaderResourceView * view = 0;
					context->VSSetShaderResources(at, 1, &view);
				}
			}
			virtual void SetVertexShaderConstant(uint32 at, IBuffer * buffer) noexcept override
			{
				if (pass_mode != 1) { pass_state = false; return; }
				if (buffer) {
					auto object = static_cast<D3D11_Buffer *>(buffer);
					context->VSSetConstantBuffers(at, 1, &object->buffer);
				} else {
					ID3D11Buffer * buffer = 0;
					context->VSSetConstantBuffers(at, 1, &buffer);
				}
			}
			virtual void SetVertexShaderConstant(uint32 at, const void * data, int length) noexcept override
			{
				if (length & 0x0000000F) {
					auto aligned_length = (length + 15) & 0xFFFFFFF0;
					auto aligned_data = malloc(aligned_length);
					if (aligned_data) {
						MemoryCopy(aligned_data, data, length);
						SetVertexShaderConstant(at, aligned_data, aligned_length);
						free(aligned_data);
					} else pass_state = false;
				} else {
					if (pass_mode != 1 || length > 4096) { pass_state = false; return; }
					if (length) {
						D3D11_BUFFER_DESC desc;
						desc.ByteWidth = length;
						desc.Usage = D3D11_USAGE_IMMUTABLE;
						desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
						desc.CPUAccessFlags = 0;
						desc.MiscFlags = 0;
						desc.StructureByteStride = 0;
						D3D11_SUBRESOURCE_DATA init;
						init.pSysMem = data;
						init.SysMemPitch = 0;
						init.SysMemSlicePitch = 0;
						ID3D11Buffer * buffer;
						if (device->CreateBuffer(&desc, &init, &buffer) != S_OK) { pass_state = false; return; }
						context->VSSetConstantBuffers(at, 1, &buffer);
						buffer->Release();
					} else {
						ID3D11Buffer * buffer = 0;
						context->VSSetConstantBuffers(at, 1, &buffer);
					}
				}
			}
			virtual void SetVertexShaderSamplerState(uint32 at, ISamplerState * sampler) noexcept override
			{
				if (pass_mode != 1) { pass_state = false; return; }
				if (sampler) {
					context->VSSetSamplers(at, 1, &static_cast<D3D11_SamplerState *>(sampler)->state);
				} else {
					ID3D11SamplerState * state = 0;
					context->VSSetSamplers(at, 1, &state);
				}
			}
			virtual void SetPixelShaderResource(uint32 at, IDeviceResource * resource) noexcept override
			{
				if (pass_mode != 1) { pass_state = false; return; }
				if (resource) {
					if (resource->GetResourceType() == ResourceType::Texture) {
						auto object = static_cast<D3D11_Texture *>(resource);
						if (!object->view) { pass_state = false; return; }
						context->PSSetShaderResources(at, 1, &object->view);
					} else if (resource->GetResourceType() == ResourceType::Buffer) {
						auto object = static_cast<D3D11_Buffer *>(resource);
						if (!object->view) { pass_state = false; return; }
						context->PSSetShaderResources(at, 1, &object->view);
					} else { pass_state = false; return; }
				} else {
					ID3D11ShaderResourceView * view = 0;
					context->PSSetShaderResources(at, 1, &view);
				}
			}
			virtual void SetPixelShaderConstant(uint32 at, IBuffer * buffer) noexcept override
			{
				if (pass_mode != 1) { pass_state = false; return; }
				if (buffer) {
					auto object = static_cast<D3D11_Buffer *>(buffer);
					context->PSSetConstantBuffers(at, 1, &object->buffer);
				} else {
					ID3D11Buffer * buffer = 0;
					context->PSSetConstantBuffers(at, 1, &buffer);
				}
			}
			virtual void SetPixelShaderConstant(uint32 at, const void * data, int length) noexcept override
			{
				if (length & 0x0000000F) {
					auto aligned_length = (length + 15) & 0xFFFFFFF0;
					auto aligned_data = malloc(aligned_length);
					if (aligned_data) {
						MemoryCopy(aligned_data, data, length);
						SetPixelShaderConstant(at, aligned_data, aligned_length);
						free(aligned_data);
					} else pass_state = false;
				} else {
					if (pass_mode != 1 || length > 4096) { pass_state = false; return; }
					if (length) {
						D3D11_BUFFER_DESC desc;
						desc.ByteWidth = length;
						desc.Usage = D3D11_USAGE_IMMUTABLE;
						desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
						desc.CPUAccessFlags = 0;
						desc.MiscFlags = 0;
						desc.StructureByteStride = 0;
						D3D11_SUBRESOURCE_DATA init;
						init.pSysMem = data;
						init.SysMemPitch = 0;
						init.SysMemSlicePitch = 0;
						ID3D11Buffer * buffer;
						if (device->CreateBuffer(&desc, &init, &buffer) != S_OK) { pass_state = false; return; }
						context->PSSetConstantBuffers(at, 1, &buffer);
						buffer->Release();
					} else {
						ID3D11Buffer * buffer = 0;
						context->PSSetConstantBuffers(at, 1, &buffer);
					}
				}
			}
			virtual void SetPixelShaderSamplerState(uint32 at, ISamplerState * sampler) noexcept override
			{
				if (pass_mode != 1) { pass_state = false; return; }
				if (sampler) {
					context->PSSetSamplers(at, 1, &static_cast<D3D11_SamplerState *>(sampler)->state);
				} else {
					ID3D11SamplerState * state = 0;
					context->PSSetSamplers(at, 1, &state);
				}
			}
			virtual void SetIndexBuffer(IBuffer * index, IndexBufferFormat format) noexcept override
			{
				if (pass_mode != 1) { pass_state = false; return; }
				if (index) {
					DXGI_FORMAT fmt;
					if (format == IndexBufferFormat::UInt16) fmt = DXGI_FORMAT_R16_UINT;
					else if (format == IndexBufferFormat::UInt32) fmt = DXGI_FORMAT_R32_UINT;
					else { pass_state = false; return; }
					context->IASetIndexBuffer(static_cast<D3D11_Buffer *>(index)->buffer, fmt, 0);
				} else context->IASetIndexBuffer(0, DXGI_FORMAT_R16_UINT, 0);
			}
			virtual void SetStencilReferenceValue(uint8 ref) noexcept override
			{
				if (pass_mode != 1) { pass_state = false; return; }
				stencil_ref = ref;
				context->OMSetDepthStencilState(depth_stencil_state, stencil_ref);
			}
			virtual void DrawPrimitives(uint32 vertex_count, uint32 first_vertex) noexcept override
			{
				if (pass_mode != 1) { pass_state = false; return; }
				context->Draw(vertex_count, first_vertex);
			}
			virtual void DrawInstancedPrimitives(uint32 vertex_count, uint32 first_vertex, uint32 instance_count, uint32 first_instance) noexcept override
			{
				if (pass_mode != 1) { pass_state = false; return; }
				context->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
			}
			virtual void DrawIndexedPrimitives(uint32 index_count, uint32 first_index, uint32 base_vertex) noexcept override
			{
				if (pass_mode != 1) { pass_state = false; return; }
				context->DrawIndexed(index_count, first_index, base_vertex);
			}
			virtual void DrawIndexedInstancedPrimitives(uint32 index_count, uint32 first_index, uint32 base_vertex, uint32 instance_count, uint32 first_instance) noexcept override
			{
				if (pass_mode != 1) { pass_state = false; return; }
				context->DrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance);
			}
			virtual I2DDeviceContext * Get2DContext(void) noexcept override { return device_2d; }
			virtual void GenerateMipmaps(ITexture * texture) noexcept override
			{
				if (!texture) { pass_state = false; return; }
				auto object = static_cast<D3D11_Texture *>(texture)->view;
				if (!object || pass_mode != 3) { pass_state = false; return; }
				context->GenerateMips(object);
			}
			virtual void CopyResourceData(IDeviceResource * dest, IDeviceResource * src) noexcept override
			{
				if (pass_mode != 3 || !dest || !src) { pass_state = false; return; }
				context->CopyResource(GetInnerObject(dest), GetInnerObject(src));
			}
			virtual void CopySubresourceData(IDeviceResource * dest, SubresourceIndex dest_subres, VolumeIndex dest_origin, IDeviceResource * src, SubresourceIndex src_subres, VolumeIndex src_origin, VolumeIndex size) noexcept override
			{
				if (pass_mode != 3 || !dest || !src) { pass_state = false; return; }
				if (!size.x || !size.y || !size.z) return;
				UINT dest_sr, src_sr;
				if (dest->GetResourceType() == ResourceType::Texture) {
					dest_sr = D3D11CalcSubresource(dest_subres.mip_level, dest_subres.array_index, static_cast<ITexture *>(dest)->GetMipmapCount());
				} else dest_sr = 0;
				if (src->GetResourceType() == ResourceType::Texture) {
					src_sr = D3D11CalcSubresource(src_subres.mip_level, src_subres.array_index, static_cast<ITexture *>(src)->GetMipmapCount());
				} else src_sr = 0;
				D3D11_BOX box;
				box.left = src_origin.x;
				box.top = src_origin.y;
				box.front = src_origin.z;
				box.right = src_origin.x + size.x;
				box.bottom = src_origin.y + size.y;
				box.back = src_origin.z + size.z;
				context->CopySubresourceRegion(GetInnerObject(dest), dest_sr, dest_origin.x, dest_origin.y, dest_origin.z, GetInnerObject(src), src_sr, &box);
			}
			virtual void UpdateResourceData(IDeviceResource * dest, SubresourceIndex subres, VolumeIndex origin, VolumeIndex size, const ResourceInitDesc & src) noexcept override
			{
				if (pass_mode != 3 || !dest) { pass_state = false; return; }
				if (!size.x || !size.y || !size.z) return;
				UINT dest_sr;
				bool use_mapping = false;
				if (dest->GetResourceType() == ResourceType::Texture) {
					auto object = static_cast<ITexture *>(dest);
					dest_sr = D3D11CalcSubresource(subres.mip_level, subres.array_index, object->GetMipmapCount());
					if (object->GetResourceUsage() & ResourceUsageDepthStencil) use_mapping = true;
				} else dest_sr = 0;
				D3D11_BOX box;
				box.left = origin.x;
				box.top = origin.y;
				box.front = origin.z;
				box.right = origin.x + size.x;
				box.bottom = origin.y + size.y;
				box.back = origin.z + size.z;
				if (use_mapping) {
					ID3D11Resource * op = 0, * st = 0;
					uint32 atom_size;
					if (dest->GetResourceType() == ResourceType::Texture) {
						auto object = static_cast<D3D11_Texture *>(dest);
						if (object->tex_1d) {
							op = object->tex_1d;
							st = object->tex_staging_1d;
						} else if (object->tex_2d) {
							op = object->tex_2d;
							st = object->tex_staging_2d;
						} else if (object->tex_3d) {
							op = object->tex_3d;
							st = object->tex_staging_3d;
						}
						atom_size = GetFormatBitsPerPixel(object->GetPixelFormat()) / 8;
					} else if (dest->GetResourceType() == ResourceType::Buffer) {
						auto object = static_cast<D3D11_Buffer *>(dest);
						op = object->buffer;
						st = object->buffer_staging;
						atom_size = 1;
					}
					if (!st) { pass_state = false; return; }
					D3D11_MAPPED_SUBRESOURCE map;
					if (context->Map(st, dest_sr, D3D11_MAP_WRITE, 0, &map) != S_OK) { pass_state = false; return; }
					uint8 * dest_ptr = reinterpret_cast<uint8 *>(map.pData);
					const uint8 * src_ptr = reinterpret_cast<const uint8 *>(src.Data);
					auto copy = atom_size * size.x;
					for (uint z = 0; z < size.z; z++) for (uint y = 0; y < size.y; y++) {
						auto dest_offs = map.DepthPitch * (z + origin.z) + map.RowPitch * (y + origin.y) + atom_size * origin.x;
						auto src_offs = src.DataSlicePitch * z + src.DataPitch * y;
						MemoryCopy(dest_ptr + dest_offs, src_ptr + src_offs, copy);
					}
					context->Unmap(st, dest_sr);
					context->CopySubresourceRegion(op, dest_sr, origin.x, origin.y, origin.z, st, dest_sr, &box);
				} else context->UpdateSubresource(GetInnerObject(dest), dest_sr, &box, src.Data, src.DataPitch, src.DataSlicePitch);
			}
			virtual void QueryResourceData(const ResourceDataDesc & dest, IDeviceResource * src, SubresourceIndex subres, VolumeIndex origin, VolumeIndex size) noexcept override
			{
				if (pass_mode != 3 || !src) { pass_state = false; return; }
				if (!size.x || !size.y || !size.z) return;
				ID3D11Resource * op = 0, * st = 0;
				UINT src_sr;
				uint32 atom_size;
				if (src->GetResourceType() == ResourceType::Texture) {
					auto object = static_cast<D3D11_Texture *>(src);
					if (object->tex_1d) {
						op = object->tex_1d;
						st = object->tex_staging_1d;
					} else if (object->tex_2d) {
						op = object->tex_2d;
						st = object->tex_staging_2d;
					} else if (object->tex_3d) {
						op = object->tex_3d;
						st = object->tex_staging_3d;
					}
					atom_size = GetFormatBitsPerPixel(object->GetPixelFormat()) / 8;
					src_sr = D3D11CalcSubresource(subres.mip_level, subres.array_index, object->GetMipmapCount());
				} else if (src->GetResourceType() == ResourceType::Buffer) {
					auto object = static_cast<D3D11_Buffer *>(src);
					op = object->buffer;
					st = object->buffer_staging;
					src_sr = 0;
					atom_size = 1;
				}
				if (!st) { pass_state = false; return; }
				D3D11_BOX box;
				box.left = origin.x;
				box.top = origin.y;
				box.front = origin.z;
				box.right = origin.x + size.x;
				box.bottom = origin.y + size.y;
				box.back = origin.z + size.z;
				context->CopySubresourceRegion(st, src_sr, origin.x, origin.y, origin.z, op, src_sr, &box);
				D3D11_MAPPED_SUBRESOURCE map;
				if (context->Map(st, src_sr, D3D11_MAP_READ, 0, &map) != S_OK) { pass_state = false; return; }
				uint8 * dest_ptr = reinterpret_cast<uint8 *>(dest.Data);
				const uint8 * src_ptr = reinterpret_cast<const uint8 *>(map.pData);
				auto copy = atom_size * size.x;
				for (uint z = 0; z < size.z; z++) for (uint y = 0; y < size.y; y++) {
					auto dest_offs = dest.DataSlicePitch * z + dest.DataPitch * y;
					auto src_offs = map.DepthPitch * (z + origin.z) + map.RowPitch * (y + origin.y) + atom_size * origin.x;
					MemoryCopy(dest_ptr + dest_offs, src_ptr + src_offs, copy);
				}
				context->Unmap(st, src_sr);
			}
			virtual bool AcquireSharedResource(IDeviceResource * rsrc) noexcept override
			{
				if (pass_mode || !rsrc || rsrc->GetMemoryPool() != ResourceMemoryPool::Shared) return false;
				IDXGIKeyedMutex * mutex;
				if (GetInnerObject(rsrc)->QueryInterface(IID_IDXGIKeyedMutex, reinterpret_cast<void **>(&mutex)) != S_OK) return false;
				auto status = mutex->AcquireSync(0, INFINITE);
				mutex->Release();
				return status == S_OK;
			}
			virtual bool AcquireSharedResource(IDeviceResource * rsrc, uint32 timeout) noexcept override
			{
				if (pass_mode || !rsrc || rsrc->GetMemoryPool() != ResourceMemoryPool::Shared) return false;
				IDXGIKeyedMutex * mutex;
				if (GetInnerObject(rsrc)->QueryInterface(IID_IDXGIKeyedMutex, reinterpret_cast<void **>(&mutex)) != S_OK) return false;
				auto status = mutex->AcquireSync(0, timeout);
				mutex->Release();
				return status == S_OK;
			}
			virtual bool ReleaseSharedResource(IDeviceResource * rsrc) noexcept override
			{
				if (pass_mode || !rsrc || rsrc->GetMemoryPool() != ResourceMemoryPool::Shared) return false;
				IDXGIKeyedMutex * mutex;
				if (GetInnerObject(rsrc)->QueryInterface(IID_IDXGIKeyedMutex, reinterpret_cast<void **>(&mutex)) != S_OK) return false;
				context->Flush();
				auto status = mutex->ReleaseSync(0);
				mutex->Release();
				return status == S_OK;
			}
			virtual string ToString(void) const override { return L"D3D11_DeviceContext"; }
			void TryAllocateDeviceD2D1(void) noexcept
			{
				if (d2d1_device) { d2d1_device->Release(); d2d1_device = 0; }
				IDXGIDevice1 * dxgi_device;
				if (Direct2D::D2DFactory1 && device->QueryInterface(IID_PPV_ARGS(&dxgi_device)) == S_OK) {
					if (Direct2D::D2DFactory1->CreateDevice(dxgi_device, &d2d1_device) != S_OK) d2d1_device = 0;
					dxgi_device->Release();
				}
			}
			ID2D1Device * GetDeviceD2D1(void) noexcept { return d2d1_device; }
		};
		class D3D11_WindowLayer : public Graphics::IWindowLayer
		{
			IDevice * wrapper;
			ID3D11Device * device;
			ID3D11DeviceContext * context;
			SafePointer<ITexture> backbuffer;
			PixelFormat format;
			DXGI_FORMAT dxgi_format;
			uint32 usage, width, height, attributes;
			IDXGISwapChain * swapchain;
			IDXGISwapChain1 * swapchain1;
			SafePointer<D3D11_FakeEngine> engine;

			void _set_alpha_attributes_on_window_properties(Windows::ICoreWindow * window) noexcept
			{
				uint32 fx_flags;
				Windows::GetWindowSurfaceFlags(window, &fx_flags, 0, 0);
				if (fx_flags) attributes |= WindowLayerAttributeAlphaChannelPremultiplied;
				else attributes |= WindowLayerAttributeAlphaChannelIgnore;
			}
			void _prop_init_swapchain(Windows::ICoreWindow * window)
			{
				auto hwnd = reinterpret_cast<HWND>(window->GetOSHandle());
				if (usage & WindowLayerAttributeExtendedDynamicRange) {
					DXGI_SWAP_CHAIN_DESC1 swcd;
					DXGI_SWAP_CHAIN_FULLSCREEN_DESC swfs;
					swcd.Width = width;
					swcd.Height = height;
					swcd.Format = dxgi_format;
					swcd.Stereo = FALSE;
					swcd.SampleDesc.Count = 1;
					swcd.SampleDesc.Quality = 0;
					swcd.BufferUsage = 0;
					swcd.BufferCount = 2;
					swcd.Scaling = DXGI_SCALING_STRETCH;
					swcd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
					swcd.Flags = 0;
					if (usage & ResourceUsageShaderRead) swcd.BufferUsage |= DXGI_USAGE_SHADER_INPUT;
					if (usage & ResourceUsageRenderTarget) swcd.BufferUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;
					if (usage & WindowLayerAttributeAlphaChannelIgnore) { swcd.AlphaMode = DXGI_ALPHA_MODE_IGNORE; attributes |= WindowLayerAttributeAlphaChannelIgnore; }
					else if (usage & WindowLayerAttributeAlphaChannelPremultiplied) { swcd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED; attributes |= WindowLayerAttributeAlphaChannelPremultiplied; }
					else if (usage & WindowLayerAttributeAlphaChannelStraight) { swcd.AlphaMode = DXGI_ALPHA_MODE_STRAIGHT; attributes |= WindowLayerAttributeAlphaChannelStraight; }
					else { swcd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; _set_alpha_attributes_on_window_properties(window); }
					if (format == PixelFormat::R16G16B16A16_float) attributes |= WindowLayerAttributeExtendedDynamicRange;
					swfs.RefreshRate.Numerator = 60;
					swfs.RefreshRate.Denominator = 1;
					swfs.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
					swfs.Scaling = DXGI_MODE_SCALING_STRETCHED;
					swfs.Windowed = TRUE;
					IDXGIDevice2 * dxgi_device;
					IDXGIAdapter * dxgi_adapter;
					IDXGIFactory2 * dxgi_factory;
					if (device->QueryInterface(IID_PPV_ARGS(&dxgi_device)) != S_OK) throw Exception();
					if (dxgi_device->GetAdapter(&dxgi_adapter) != S_OK) { dxgi_device->Release(); throw Exception(); }
					dxgi_device->Release();
					if (dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory)) != S_OK) { dxgi_adapter->Release(); throw Exception(); }
					dxgi_adapter->Release();
					if (dxgi_factory->CreateSwapChainForHwnd(device, hwnd, &swcd, &swfs, 0, &swapchain1) != S_OK) { dxgi_factory->Release(); throw Exception(); }
					if (dxgi_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_PRINT_SCREEN) != S_OK) { swapchain1->Release(); dxgi_factory->Release(); throw Exception(); }
					dxgi_factory->Release();
					swapchain = swapchain1;
					swapchain->AddRef();
				} else {
					DXGI_SWAP_CHAIN_DESC swcd;
					swcd.BufferDesc.Width = width;
					swcd.BufferDesc.Height = height;
					swcd.BufferDesc.RefreshRate.Numerator = 60;
					swcd.BufferDesc.RefreshRate.Denominator = 1;
					swcd.BufferDesc.Format = dxgi_format;
					swcd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
					swcd.BufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
					swcd.SampleDesc.Count = 1;
					swcd.SampleDesc.Quality = 0;
					swcd.BufferUsage = 0;
					if (usage & ResourceUsageShaderRead) swcd.BufferUsage |= DXGI_USAGE_SHADER_INPUT;
					if (usage & ResourceUsageRenderTarget) swcd.BufferUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;
					swcd.BufferCount = 1;
					swcd.OutputWindow = hwnd;
					swcd.Windowed = TRUE;
					swcd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
					swcd.Flags = 0;
					_set_alpha_attributes_on_window_properties(window);
					IDXGIDevice * dxgi_device;
					IDXGIAdapter * dxgi_adapter;
					IDXGIFactory * dxgi_factory;
					if (device->QueryInterface(IID_PPV_ARGS(&dxgi_device)) != S_OK) throw Exception();
					if (dxgi_device->GetAdapter(&dxgi_adapter) != S_OK) { dxgi_device->Release(); throw Exception(); }
					dxgi_device->Release();
					if (dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory)) != S_OK) { dxgi_adapter->Release(); throw Exception(); }
					dxgi_adapter->Release();
					if (dxgi_factory->CreateSwapChain(device, &swcd, &swapchain) != S_OK) { dxgi_factory->Release(); throw Exception(); }
					if (dxgi_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_PRINT_SCREEN) != S_OK) { swapchain->Release(); dxgi_factory->Release(); throw Exception(); }
					dxgi_factory->Release();
				}
				window->SetPresentationEngine(engine);
			}
			bool _prop_create_surface(void) noexcept
			{
				TextureDesc desc;
				desc.Type = TextureType::Type2D;
				desc.Format = format;
				desc.Width = width;
				desc.Height = height;
				desc.MipmapCount = 1;
				desc.Usage = usage & ~WindowLayerAttributeMask;
				desc.MemoryPool = ResourceMemoryPool::Default;
				backbuffer = wrapper->CreateTexture(desc);
				return backbuffer.Inner() != 0;
			}
			void _prop_init_layers(Windows::ICoreWindow * window)
			{
				auto window_full = static_cast<Windows::IWindow *>(window);
				auto window_handle = reinterpret_cast<HWND>(window_full->GetOSHandle());
				auto window_flags = window_full->GetBackgroundFlags();
				Windows::CreateWindowLayersDesc desc;
				desc.window = window_handle;
				desc.flags = 0;
				if (format == PixelFormat::B8G8R8A8_unorm) desc.flags |= Windows::CreateWindowLayersFormatB8G8R8A8;
				else if (format == PixelFormat::R8G8B8A8_unorm) desc.flags |= Windows::CreateWindowLayersFormatR8G8B8A8;
				else if (format == PixelFormat::R10G10B10A2_unorm) desc.flags |= Windows::CreateWindowLayersFormatR10G10B10A2;
				else if (format == PixelFormat::R16G16B16A16_float) desc.flags |= Windows::CreateWindowLayersFormatR16G16B16A16;
				else throw InvalidArgumentException();
				if (usage & WindowLayerAttributeAlphaChannelIgnore) { desc.flags |= Windows::CreateWindowLayersAlphaModeIgnore; attributes = WindowLayerAttributeAlphaChannelIgnore; }
				else if (usage & WindowLayerAttributeAlphaChannelPremultiplied) { desc.flags |= Windows::CreateWindowLayersAlphaModePremultiplied; attributes = WindowLayerAttributeAlphaChannelPremultiplied; }
				else if (usage & WindowLayerAttributeAlphaChannelStraight) { desc.flags |= Windows::CreateWindowLayersAlphaModeStraight; attributes = WindowLayerAttributeAlphaChannelStraight; }
				else { desc.flags |= Windows::CreateWindowLayersAlphaModePremultiplied; attributes = WindowLayerAttributeAlphaChannelPremultiplied; }
				if (format == PixelFormat::R16G16B16A16_float && (usage & WindowLayerAttributeExtendedDynamicRange)) attributes |= WindowLayerAttributeExtendedDynamicRange;
				desc.flags |= Windows::CreateWindowLayersTransparentBackground;
				if (window_flags & Windows::WindowFlagBlurBehind) desc.flags |= Windows::CreateWindowLayersBlurBehind;
				double blur_factor;
				Windows::GetWindowLayers(window, 0, &blur_factor);
				desc.device = device;
				desc.width = width;
				desc.height = height;
				desc.deviation = blur_factor;
				if (!_prop_create_surface()) throw Exception();
				window->SetPresentationEngine(engine);
				if (!engine->IsAttached()) throw Exception();
				SafePointer<Windows::IWindowLayers> layers = Windows::CreateWindowLayers(desc);
				if (!layers) throw Exception();
				engine->SetLayers(layers);
				device->GetImmediateContext(&context);
			}
		public:
			D3D11_WindowLayer(Windows::ICoreWindow * window, IDevice * parent, const WindowLayerDesc & desc) : wrapper(parent), device(0),
				context(0), format(PixelFormat::Invalid), dxgi_format(DXGI_FORMAT_UNKNOWN), usage(0), width(0), height(0), attributes(0), swapchain(0), swapchain1(0)
			{
				engine = new D3D11_FakeEngine;
				device = GetInnerObject(wrapper);
				format = desc.Format;
				dxgi_format = MakeDxgiFormat(desc.Format);
				if (dxgi_format == DXGI_FORMAT_UNKNOWN) throw InvalidArgumentException();
				width = max(desc.Width, 1);
				height = max(desc.Height, 1);
				usage = desc.Usage;
				if (!IsColorFormat(format)) throw InvalidArgumentException();
				if (!width || !height) throw InvalidArgumentException();
				if (usage & ~(ResourceUsageTextureMask | WindowLayerAttributeMask)) throw InvalidArgumentException();
				if (usage & ResourceUsageShaderWrite) throw InvalidArgumentException();
				if (usage & ResourceUsageDepthStencil) throw InvalidArgumentException();
				if (usage & ResourceUsageCPUAll) throw InvalidArgumentException();
				if (usage & ResourceUsageVideoAll) throw InvalidArgumentException();
				if (static_cast<Windows::IWindow *>(window)->GetBackgroundFlags() & Windows::WindowFlagTransparent) _prop_init_layers(window);
				else _prop_init_swapchain(window);
				device->AddRef();
			}
			virtual ~D3D11_WindowLayer(void) override
			{
				if (IsFullscreen()) SwitchToWindow();
				if (device) device->Release();
				if (swapchain) swapchain->Release();
				if (swapchain1) swapchain1->Release();
			}
			virtual IDevice * GetParentDevice(void) noexcept override { return wrapper; }
			virtual bool Present(void) noexcept override
			{
				if (swapchain) {
					return swapchain->Present(0, 0) == S_OK;
				} else {
					auto layers = engine->GetLayers();
					if (!layers) return false;
					uint org_x, org_y;
					ID3D11Texture2D * surface;
					if (!layers->BeginDraw(&surface, &org_x, &org_y)) return false;
					D3D11_BOX box;
					box.left = box.top = box.front = 0;
					box.right = width;
					box.bottom = height;
					box.back = 1;
					context->CopySubresourceRegion(surface, 0, org_x, org_y, 0, GetInnerObject2D(backbuffer), 0, &box);
					surface->Release();
					return layers->EndDraw();
				}
			}
			virtual ITexture * QuerySurface(void) noexcept override
			{
				if (backbuffer) {
					backbuffer->Retain();
					return backbuffer;
				}
				if (swapchain) {
					SafePointer<D3D11_Texture> result = new (std::nothrow) D3D11_Texture(TextureType::Type2D, wrapper);
					if (!result) return 0;
					if (swapchain->GetBuffer(0, IID_PPV_ARGS(&result->tex_2d)) != S_OK) return 0;
					if (usage & ResourceUsageShaderRead) {
						if (device->CreateShaderResourceView(result->tex_2d, 0, &result->view) != S_OK) return 0;
					}
					if (usage & ResourceUsageRenderTarget) {
						if (device->CreateRenderTargetView(result->tex_2d, 0, &result->rt_view) != S_OK) return 0;
					}
					result->pool = ResourceMemoryPool::Default;
					result->format = format;
					result->usage_flags = usage & ~WindowLayerAttributeMask;
					result->width = width;
					result->height = height;
					result->depth = result->size = 1;
					result->Retain();
					backbuffer.SetRetain(result);
					return result;
				} else return 0;
			}
			virtual bool ResizeSurface(uint32 _width, uint32 _height) noexcept override
			{
				if (!_width || !_height) return false;
				backbuffer.SetReference(0);
				if (swapchain) {
					if (swapchain->ResizeBuffers(1, _width, _height, dxgi_format, 0) != S_OK) return false;
					width = _width;
					height = _height;
					return true;
				} else {
					auto layers = engine->GetLayers();
					if (!layers) return false;
					if (!layers->Resize(_width, _height)) return false;
					width = _width;
					height = _height;
					return _prop_create_surface();
				}
			}
			virtual bool SwitchToFullscreen(void) noexcept override
			{
				if (!swapchain) return false;
				if (swapchain->SetFullscreenState(TRUE, 0) != S_OK) return false;
				return true;
			}
			virtual bool SwitchToWindow(void) noexcept override
			{
				if (!swapchain) return false;
				if (swapchain->SetFullscreenState(FALSE, 0) != S_OK) return false;
				return true;
			}
			virtual bool IsFullscreen(void) noexcept override
			{
				if (!swapchain) return false;
				BOOL result;
				IDXGIOutput * output;
				if (swapchain->GetFullscreenState(&result, &output) != S_OK) return false;
				if (output) output->Release();
				return result;
			}
			virtual uint GetLayerAttributes(void) noexcept override { return attributes; }
			virtual string ToString(void) const override { return L"D3D11_WindowLayer"; }
		};
		class D3D11_Device : public Graphics::IDevice
		{
			SafePointer<IDeviceFactory> parent_factory;
			ID3D11Device * device;
			IDeviceContext * context;
			IUnknown * video_acceleration;
			D3D11_COMPARISON_FUNC _make_comp_function(CompareFunction func)
			{
				if (func == CompareFunction::Always) return D3D11_COMPARISON_ALWAYS;
				else if (func == CompareFunction::Lesser) return D3D11_COMPARISON_LESS;
				else if (func == CompareFunction::Greater) return D3D11_COMPARISON_GREATER;
				else if (func == CompareFunction::Equal) return D3D11_COMPARISON_EQUAL;
				else if (func == CompareFunction::LesserEqual) return D3D11_COMPARISON_LESS_EQUAL;
				else if (func == CompareFunction::GreaterEqual) return D3D11_COMPARISON_GREATER_EQUAL;
				else if (func == CompareFunction::NotEqual) return D3D11_COMPARISON_NOT_EQUAL;
				else if (func == CompareFunction::Never) return D3D11_COMPARISON_NEVER;
				else return D3D11_COMPARISON_NEVER;
			}
			D3D11_STENCIL_OP _make_stencil_function(StencilFunction func)
			{
				if (func == StencilFunction::Keep) return D3D11_STENCIL_OP_KEEP;
				else if (func == StencilFunction::SetZero) return D3D11_STENCIL_OP_ZERO;
				else if (func == StencilFunction::Replace) return D3D11_STENCIL_OP_REPLACE;
				else if (func == StencilFunction::IncrementWrap) return D3D11_STENCIL_OP_INCR;
				else if (func == StencilFunction::DecrementWrap) return D3D11_STENCIL_OP_DECR;
				else if (func == StencilFunction::IncrementClamp) return D3D11_STENCIL_OP_INCR_SAT;
				else if (func == StencilFunction::DecrementClamp) return D3D11_STENCIL_OP_DECR_SAT;
				else if (func == StencilFunction::Invert) return D3D11_STENCIL_OP_INVERT;
				else return D3D11_STENCIL_OP_KEEP;
			}
			D3D11_BLEND_OP _make_blend_function(BlendingFunction func)
			{
				if (func == BlendingFunction::Add) return D3D11_BLEND_OP_ADD;
				else if (func == BlendingFunction::SubtractBaseFromOver) return D3D11_BLEND_OP_SUBTRACT;
				else if (func == BlendingFunction::SubtractOverFromBase) return D3D11_BLEND_OP_REV_SUBTRACT;
				else if (func == BlendingFunction::Min) return D3D11_BLEND_OP_MIN;
				else if (func == BlendingFunction::Max) return D3D11_BLEND_OP_MAX;
				else return D3D11_BLEND_OP_ADD;
			}
			D3D11_BLEND _make_blend_factor(BlendingFactor fact)
			{
				if (fact == BlendingFactor::Zero) return D3D11_BLEND_ZERO;
				else if (fact == BlendingFactor::One) return D3D11_BLEND_ONE;
				else if (fact == BlendingFactor::OverColor) return D3D11_BLEND_SRC_COLOR;
				else if (fact == BlendingFactor::InvertedOverColor) return D3D11_BLEND_INV_SRC_COLOR;
				else if (fact == BlendingFactor::OverAlpha) return D3D11_BLEND_SRC_ALPHA;
				else if (fact == BlendingFactor::InvertedOverAlpha) return D3D11_BLEND_INV_SRC_ALPHA;
				else if (fact == BlendingFactor::BaseColor) return D3D11_BLEND_DEST_COLOR;
				else if (fact == BlendingFactor::InvertedBaseColor) return D3D11_BLEND_INV_DEST_COLOR;
				else if (fact == BlendingFactor::BaseAlpha) return D3D11_BLEND_DEST_ALPHA;
				else if (fact == BlendingFactor::InvertedBaseAlpha) return D3D11_BLEND_INV_DEST_ALPHA;
				else if (fact == BlendingFactor::SecondaryColor) return D3D11_BLEND_SRC1_COLOR;
				else if (fact == BlendingFactor::InvertedSecondaryColor) return D3D11_BLEND_INV_SRC1_COLOR;
				else if (fact == BlendingFactor::SecondaryAlpha) return D3D11_BLEND_SRC1_ALPHA;
				else if (fact == BlendingFactor::InvertedSecondaryAlpha) return D3D11_BLEND_INV_SRC1_ALPHA;
				else if (fact == BlendingFactor::OverAlphaSaturated) return D3D11_BLEND_SRC_ALPHA_SAT;
				else return D3D11_BLEND_ZERO;
			}
			D3D11_TEXTURE_ADDRESS_MODE _make_address_mode(SamplerAddressMode mode)
			{
				if (mode == SamplerAddressMode::Border) return D3D11_TEXTURE_ADDRESS_BORDER;
				else if (mode == SamplerAddressMode::Clamp) return D3D11_TEXTURE_ADDRESS_CLAMP;
				else if (mode == SamplerAddressMode::Mirror) return D3D11_TEXTURE_ADDRESS_MIRROR;
				else return D3D11_TEXTURE_ADDRESS_WRAP;
			}
			D3D11_SUBRESOURCE_DATA * _make_subres_data(const ResourceInitDesc * init, int length)
			{
				D3D11_SUBRESOURCE_DATA * result = reinterpret_cast<D3D11_SUBRESOURCE_DATA *>(malloc(length * sizeof(D3D11_SUBRESOURCE_DATA)));
				if (!result) return 0;
				for (int i = 0; i < length; i++) {
					result[i].pSysMem = init[i].Data;
					result[i].SysMemPitch = init[i].DataPitch;
					result[i].SysMemSlicePitch = init[i].DataSlicePitch;
				}
				return result;
			}
			uint32 _calculate_mip_levels(uint32 w, uint32 h, uint32 d)
			{
				uint32 r = 1;
				auto s = max(max(w, h), d);
				while (s > 1) { s /= 2; r++; }
				return r;
			}
			bool _is_valid_generate_mipmaps_format(DXGI_FORMAT format) noexcept { UINT status; if (device->CheckFormatSupport(format, &status) == S_OK) return (status & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN) != 0; else return false; }
		public:
			D3D11_Device(ID3D11Device * _device, IDeviceFactory * _parent) { context = new D3D11_DeviceContext(_device, this); device = _device; device->AddRef(); video_acceleration = 0; parent_factory.SetRetain(_parent); }
			virtual ~D3D11_Device(void) override { if (video_acceleration) video_acceleration->Release(); context->Release(); device->Release(); }
			virtual string GetDeviceName(void) noexcept override
			{
				IDXGIDevice * dxgi_device;
				IDXGIAdapter * adapter;
				if (device->QueryInterface(IID_PPV_ARGS(&dxgi_device)) != S_OK) return L"";
				if (dxgi_device->GetAdapter(&adapter) != S_OK) { dxgi_device->Release(); return L""; }
				dxgi_device->Release();
				DXGI_ADAPTER_DESC desc;
				adapter->GetDesc(&desc);
				adapter->Release();
				return string(desc.Description);
			}
			virtual uint64 GetDeviceIdentifier(void) noexcept override
			{
				IDXGIDevice * dxgi_device;
				IDXGIAdapter * adapter;
				if (device->QueryInterface(IID_PPV_ARGS(&dxgi_device)) != S_OK) return 0;
				if (dxgi_device->GetAdapter(&adapter) != S_OK) { dxgi_device->Release(); return 0; }
				dxgi_device->Release();
				DXGI_ADAPTER_DESC desc;
				adapter->GetDesc(&desc);
				adapter->Release();
				return reinterpret_cast<uint64 &>(desc.AdapterLuid);
			}
			virtual bool DeviceIsValid(void) noexcept override { if (device->GetDeviceRemovedReason() == S_OK) return true; return false; }
			virtual void GetImplementationInfo(string & tech, uint32 & version_major, uint32 & version_minor) noexcept override
			{
				try { tech = L"Direct3D"; } catch (...) {} version_major = 11;
				IUnknown * version_interface;
				if (device->QueryInterface(__uuidof(ID3D11Device5), reinterpret_cast<void **>(&version_interface)) == S_OK) {
					version_minor = 5;
					version_interface->Release();
				} else if (device->QueryInterface(__uuidof(ID3D11Device4), reinterpret_cast<void **>(&version_interface)) == S_OK) {
					version_minor = 4;
					version_interface->Release();
				} else if (device->QueryInterface(__uuidof(ID3D11Device3), reinterpret_cast<void **>(&version_interface)) == S_OK) {
					version_minor = 3;
					version_interface->Release();
				} else if (device->QueryInterface(__uuidof(ID3D11Device2), reinterpret_cast<void **>(&version_interface)) == S_OK) {
					version_minor = 2;
					version_interface->Release();
				} else if (device->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void **>(&version_interface)) == S_OK) {
					version_minor = 1;
					version_interface->Release();
				} else version_minor = 0;
			}
			virtual DeviceClass GetDeviceClass(void) noexcept override
			{
				IDXGIAdapter * adapter;
				IDXGIDevice * dxgi_device;
				if (device->QueryInterface(IID_IDXGIDevice, reinterpret_cast<void **>(&dxgi_device)) == S_OK) {
					DeviceClass result = DeviceClass::Unknown;
					if (dxgi_device->GetAdapter(&adapter) == S_OK) {
						IDXGIAdapter1 * adapter1;
						if (adapter->QueryInterface(IID_IDXGIAdapter1, reinterpret_cast<void **>(&adapter1)) == S_OK) {
							DXGI_ADAPTER_DESC1 desc;
							if (adapter1->GetDesc1(&desc) == S_OK) {
								if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) result = DeviceClass::Software; else {
									if (desc.DedicatedVideoMemory) result = DeviceClass::Discrete;
									else result = DeviceClass::Integrated;
								}
							}
							adapter1->Release();
						} else {
							DXGI_ADAPTER_DESC desc;
							if (adapter->GetDesc(&desc) == S_OK) {
								if (desc.DedicatedVideoMemory) result = DeviceClass::Discrete;
								else result = DeviceClass::Integrated;
							}
						}
						adapter->Release();
					}
					dxgi_device->Release();
					return result;
				} else return DeviceClass::Unknown;
			}
			virtual uint64 GetDeviceMemory(void) noexcept override
			{
				IDXGIAdapter * adapter;
				IDXGIDevice * dxgi_device;
				if (device->QueryInterface(IID_IDXGIDevice, reinterpret_cast<void **>(&dxgi_device)) == S_OK) {
					uint64 result = 0;
					if (dxgi_device->GetAdapter(&adapter) == S_OK) {
						DXGI_ADAPTER_DESC desc;
						if (adapter->GetDesc(&desc) == S_OK) {
							if (desc.DedicatedVideoMemory) result = desc.DedicatedVideoMemory;
							else result = desc.SharedSystemMemory;
						}
						adapter->Release();
					}
					dxgi_device->Release();
					return result;
				} else return 0;
			}
			virtual bool GetDevicePixelFormatSupport(PixelFormat format, PixelFormatUsage usage) noexcept override
			{
				if (usage == PixelFormatUsage::RenderTarget2D || usage == PixelFormatUsage::BitmapSource || usage == PixelFormatUsage::VideoIO) {
					return format == PixelFormat::B8G8R8A8_unorm;
				} else {
					UINT status;
					if (device->CheckFormatSupport(MakeDxgiFormat(format), &status) == S_OK) {
						if (usage == PixelFormatUsage::ShaderRead) return (status & D3D11_FORMAT_SUPPORT_SHADER_LOAD) != 0;
						else if (usage == PixelFormatUsage::ShaderSample) return (status & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) != 0;
						else if (usage == PixelFormatUsage::RenderTarget) return (status & D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0;
						else if (usage == PixelFormatUsage::BlendRenderTarget) return (status & D3D11_FORMAT_SUPPORT_BLENDABLE) != 0;
						else if (usage == PixelFormatUsage::DepthStencil) return (status & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL) != 0;
						else if (usage == PixelFormatUsage::WindowSurface) return (status & D3D11_FORMAT_SUPPORT_DISPLAY) != 0;
						else return false;
					} else return false;
				}
			}
			virtual IShaderLibrary * LoadShaderLibrary(const void * data, int length) noexcept override
			{
				try {
					SafePointer<Streaming::Stream> storage = new Streaming::MemoryStream(length);
					storage->Write(data, length);
					return new D3D11_ShaderLibrary(this, device, storage);
				} catch (...) { return 0; }
			}
			virtual IShaderLibrary * LoadShaderLibrary(const DataBlock * data) noexcept override { return LoadShaderLibrary(data->GetBuffer(), data->Length()); }
			virtual IShaderLibrary * LoadShaderLibrary(Streaming::Stream * stream) noexcept override
			{
				try {
					SafePointer<DataBlock> data = stream->ReadAll();
					if (!data) return 0;
					return LoadShaderLibrary(data);
				} catch (...) { return 0; }
			}
			virtual IShaderLibrary * CompileShaderLibrary(const void * data, int length, ShaderError * error) noexcept override
			{
				HMODULE lib_compiler = LoadLibraryW(L"d3dcompiler_47.dll");
				if (!lib_compiler) {
					if (error) *error = ShaderError::NoCompiler;
					return 0;
				}
				auto Compile = reinterpret_cast<func_D3DCompile>(GetProcAddress(lib_compiler, "D3DCompile"));
				if (!Compile) {
					FreeLibrary(lib_compiler);
					if (error) *error = ShaderError::NoCompiler;
					return 0;
				}
				try {
					auto level = device->GetFeatureLevel();
					SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(data, length);
					SafePointer<Storage::Archive> archive = Storage::OpenArchive(stream);
					SafePointer<D3D11_DynamicShaderLibrary> library = new D3D11_DynamicShaderLibrary(this, device);
					if (!archive) throw InvalidFormatException();
					for (Storage::ArchiveFile file = 1; file <= archive->GetFileCount(); file++) {
						auto flags = archive->GetFileCustomData(file);
						if ((flags & 0xFFFF0000) == 0x10000) {
							ShaderType type;
							if ((flags & 0xFFFF) == 0x01) type = ShaderType::Vertex;
							else if ((flags & 0xFFFF) == 0x02) type = ShaderType::Pixel;
							else throw InvalidFormatException();
							auto names = archive->GetFileName(file).Split(L'!');
							if (names.Length() != 2) throw InvalidFormatException();
							SafePointer<Streaming::Stream> data_stream = archive->QueryFileStream(file, Storage::ArchiveStream::Native);
							if (!data_stream) throw InvalidFormatException();
							SafePointer<DataBlock> source = data_stream->ReadAll();
							SafePointer<DataBlock> entry = names[1].EncodeSequence(Encoding::ANSI, true);
							const char * target;
							ID3DBlob * shader;
							if (level >= D3D_FEATURE_LEVEL_11_0) {
								if (type == ShaderType::Vertex) target = "vs_5_0";
								else if (type == ShaderType::Pixel) target = "ps_5_0";
							} else if (level == D3D_FEATURE_LEVEL_10_1) {
								if (type == ShaderType::Vertex) target = "vs_4_1";
								else if (type == ShaderType::Pixel) target = "ps_4_1";
							} else {
								if (type == ShaderType::Vertex) target = "vs_4_0";
								else if (type == ShaderType::Pixel) target = "ps_4_0";
							}
							if (Compile(source->GetBuffer(), source->Length(), "", 0, 0, reinterpret_cast<char *>(entry->GetBuffer()),
								target, D3DCOMPILE_PACK_MATRIX_ROW_MAJOR | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &shader, 0) != S_OK) {
								if (error) *error = ShaderError::Compilation;
								FreeLibrary(lib_compiler);
								return 0;
							}
							SafePointer<DataBlock> shader_blob;
							try {
								shader_blob = new DataBlock(1);
								shader_blob->Append(reinterpret_cast<const uint8 *>(shader->GetBufferPointer()), shader->GetBufferSize());
							} catch (...) { shader->Release(); throw; }
							shader->Release();
							library->Append(names[0], type, shader_blob);
						}
					}
					if (library->IsEmpty()) {
						if (error) *error = ShaderError::NoPlatformVersion;
						FreeLibrary(lib_compiler);
						return 0;
					}
					if (error) *error = ShaderError::Success;
					FreeLibrary(lib_compiler);
					library->Retain();
					return library;
				} catch (InvalidFormatException &) {
					if (error) *error = ShaderError::InvalidContainerData;
				} catch (...) {
					if (error) *error = ShaderError::IO;
				}
				FreeLibrary(lib_compiler);
				return 0;
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
				if (!desc.RenderTargetCount) return 0;
				SafePointer<D3D11_PipelineState> result = new (std::nothrow) D3D11_PipelineState(this);
				if (!result) return 0;
				if (desc.Topology == PrimitiveTopology::PointList) result->pt = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
				else if (desc.Topology == PrimitiveTopology::LineList) result->pt = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
				else if (desc.Topology == PrimitiveTopology::LineStrip) result->pt = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
				else if (desc.Topology == PrimitiveTopology::TriangleList) result->pt = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
				else if (desc.Topology == PrimitiveTopology::TriangleStrip) result->pt = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
				else return 0;
				if (!desc.VertexShader || desc.VertexShader->GetType() != ShaderType::Vertex) return 0;
				if (!desc.PixelShader || desc.PixelShader->GetType() != ShaderType::Pixel) return 0;
				result->vs = static_cast<D3D11_Shader *>(desc.VertexShader)->vs;
				result->ps = static_cast<D3D11_Shader *>(desc.PixelShader)->ps;
				result->vs->AddRef();
				result->ps->AddRef();
				D3D11_BLEND_DESC bd;
				ZeroMemory(&bd, sizeof(bd));
				bool uniform_rtbd = true;
				uint max_rt = 1;
				for (uint rt = 1; rt < desc.RenderTargetCount; rt++) { if (MemoryCompare(&desc.RenderTarget[0], &desc.RenderTarget[rt], sizeof(RenderTargetDesc))) { uniform_rtbd = false; break; } }
				if (!uniform_rtbd) { bd.IndependentBlendEnable = TRUE; max_rt = desc.RenderTargetCount; }
				for (uint rt = 0; rt < max_rt; rt++) {
					auto & rtbd = bd.RenderTarget[rt];
					auto & src = desc.RenderTarget[rt];
					if (!IsColorFormat(src.Format)) return 0;
					rtbd.RenderTargetWriteMask = 0;
					if (src.Flags & RenderTargetFlagBlendingEnabled) rtbd.BlendEnable = TRUE; else rtbd.BlendEnable = FALSE;
					if (!(src.Flags & RenderTargetFlagRestrictWriteRed)) rtbd.RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_RED;
					if (!(src.Flags & RenderTargetFlagRestrictWriteGreen)) rtbd.RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_GREEN;
					if (!(src.Flags & RenderTargetFlagRestrictWriteBlue)) rtbd.RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_BLUE;
					if (!(src.Flags & RenderTargetFlagRestrictWriteAlpha)) rtbd.RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;
					rtbd.BlendOp = _make_blend_function(src.BlendRGB);
					rtbd.BlendOpAlpha = _make_blend_function(src.BlendAlpha);
					rtbd.DestBlend = _make_blend_factor(src.BaseFactorRGB);
					rtbd.DestBlendAlpha = _make_blend_factor(src.BaseFactorAlpha);
					rtbd.SrcBlend = _make_blend_factor(src.OverFactorRGB);
					rtbd.SrcBlendAlpha = _make_blend_factor(src.OverFactorAlpha);
				}
				if (device->CreateBlendState(&bd, &result->bs) != S_OK) return 0;
				D3D11_DEPTH_STENCIL_DESC dsd;
				if (desc.DepthStencil.Flags & DepthStencilFlagDepthTestEnabled) dsd.DepthEnable = TRUE; else dsd.DepthEnable = FALSE;
				if (desc.DepthStencil.Flags & DepthStencilFlagDepthWriteEnabled) dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; else dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
				if (desc.DepthStencil.Flags & DepthStencilFlagStencilTestEnabled) dsd.StencilEnable = TRUE; else dsd.StencilEnable = FALSE;
				dsd.DepthFunc = _make_comp_function(desc.DepthStencil.DepthTestFunction);
				dsd.StencilReadMask = desc.DepthStencil.StencilReadMask;
				dsd.StencilWriteMask = desc.DepthStencil.StencilWriteMask;
				dsd.FrontFace.StencilFunc = _make_comp_function(desc.DepthStencil.FrontStencil.TestFunction);
				dsd.FrontFace.StencilFailOp = _make_stencil_function(desc.DepthStencil.FrontStencil.OnStencilTestFailed);
				dsd.FrontFace.StencilDepthFailOp = _make_stencil_function(desc.DepthStencil.FrontStencil.OnDepthTestFailed);
				dsd.FrontFace.StencilPassOp = _make_stencil_function(desc.DepthStencil.FrontStencil.OnTestsPassed);
				dsd.BackFace.StencilFunc = _make_comp_function(desc.DepthStencil.BackStencil.TestFunction);
				dsd.BackFace.StencilFailOp = _make_stencil_function(desc.DepthStencil.BackStencil.OnStencilTestFailed);
				dsd.BackFace.StencilDepthFailOp = _make_stencil_function(desc.DepthStencil.BackStencil.OnDepthTestFailed);
				dsd.BackFace.StencilPassOp = _make_stencil_function(desc.DepthStencil.BackStencil.OnTestsPassed);
				if (device->CreateDepthStencilState(&dsd, &result->dss) != S_OK) return 0;
				D3D11_RASTERIZER_DESC rd;
				if (desc.Rasterization.Fill == FillMode::Solid) rd.FillMode = D3D11_FILL_SOLID;
				else if (desc.Rasterization.Fill == FillMode::Wireframe) rd.FillMode = D3D11_FILL_WIREFRAME;
				else return 0;
				if (desc.Rasterization.Cull == CullMode::None) rd.CullMode = D3D11_CULL_NONE;
				else if (desc.Rasterization.Cull == CullMode::Front) rd.CullMode = D3D11_CULL_FRONT;
				else if (desc.Rasterization.Cull == CullMode::Back) rd.CullMode = D3D11_CULL_BACK;
				else return 0;
				if (desc.Rasterization.FrontIsCounterClockwise) rd.FrontCounterClockwise = TRUE; else rd.FrontCounterClockwise = FALSE;
				rd.DepthBias = desc.Rasterization.DepthBias;
				rd.DepthBiasClamp = desc.Rasterization.DepthBiasClamp;
				rd.SlopeScaledDepthBias = desc.Rasterization.SlopeScaledDepthBias;
				if (desc.Rasterization.DepthClipEnable) rd.DepthClipEnable = TRUE; else rd.DepthClipEnable = FALSE;
				rd.ScissorEnable = FALSE;
				rd.MultisampleEnable = FALSE;
				rd.AntialiasedLineEnable = FALSE;
				if (device->CreateRasterizerState(&rd, &result->rs) != S_OK) return 0;
				result->Retain();
				return result;
			}
			virtual ISamplerState * CreateSamplerState(const SamplerDesc & desc) noexcept override
			{
				SafePointer<D3D11_SamplerState> result = new (std::nothrow) D3D11_SamplerState(this);
				if (!result) return 0;
				D3D11_SAMPLER_DESC sd;
				if (desc.MinificationFilter == SamplerFilter::Point && desc.MagnificationFilter == SamplerFilter::Point && desc.MipFilter == SamplerFilter::Point) {
					sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
				} else if (desc.MinificationFilter == SamplerFilter::Point && desc.MagnificationFilter == SamplerFilter::Point && desc.MipFilter == SamplerFilter::Linear) {
					sd.Filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
				} else if (desc.MinificationFilter == SamplerFilter::Point && desc.MagnificationFilter == SamplerFilter::Linear && desc.MipFilter == SamplerFilter::Point) {
					sd.Filter = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
				} else if (desc.MinificationFilter == SamplerFilter::Point && desc.MagnificationFilter == SamplerFilter::Linear && desc.MipFilter == SamplerFilter::Linear) {
					sd.Filter = D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
				} else if (desc.MinificationFilter == SamplerFilter::Linear && desc.MagnificationFilter == SamplerFilter::Point && desc.MipFilter == SamplerFilter::Point) {
					sd.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
				} else if (desc.MinificationFilter == SamplerFilter::Linear && desc.MagnificationFilter == SamplerFilter::Point && desc.MipFilter == SamplerFilter::Linear) {
					sd.Filter = D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
				} else if (desc.MinificationFilter == SamplerFilter::Linear && desc.MagnificationFilter == SamplerFilter::Linear && desc.MipFilter == SamplerFilter::Point) {
					sd.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
				} else if (desc.MinificationFilter == SamplerFilter::Linear && desc.MagnificationFilter == SamplerFilter::Linear && desc.MipFilter == SamplerFilter::Linear) {
					sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
				} else if (desc.MinificationFilter == SamplerFilter::Anisotropic && desc.MagnificationFilter == SamplerFilter::Anisotropic && desc.MipFilter == SamplerFilter::Anisotropic) {
					sd.Filter = D3D11_FILTER_ANISOTROPIC;
				} else return 0;
				sd.AddressU = _make_address_mode(desc.AddressU);
				sd.AddressV = _make_address_mode(desc.AddressV);
				sd.AddressW = _make_address_mode(desc.AddressW);
				sd.MipLODBias = 0.0f;
				sd.MaxAnisotropy = desc.MaximalAnisotropy;
				sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
				sd.BorderColor[0] = desc.BorderColor[0]; sd.BorderColor[1] = desc.BorderColor[1]; sd.BorderColor[2] = desc.BorderColor[2]; sd.BorderColor[3] = desc.BorderColor[3];
				sd.MinLOD = desc.MinimalLOD;
				sd.MaxLOD = desc.MaximalLOD;
				if (device->CreateSamplerState(&sd, &result->state) != S_OK) return 0;
				result->Retain();
				return result;
			}
			virtual IBuffer * CreateBuffer(const BufferDesc & desc) noexcept override
			{
				if (desc.MemoryPool == ResourceMemoryPool::Immutable || desc.MemoryPool == ResourceMemoryPool::Shared) return 0;
				if (desc.Usage & ~ResourceUsageBufferMask) return 0;
				SafePointer<D3D11_Buffer> result = new (std::nothrow) D3D11_Buffer(this);
				if (!result) return 0;
				result->pool = desc.MemoryPool;
				D3D11_BUFFER_DESC bd;
				bd.ByteWidth = desc.Length;
				bd.Usage = D3D11_USAGE_DEFAULT;
				bd.BindFlags = 0;
				bd.CPUAccessFlags = 0;
				bd.MiscFlags = 0;
				bd.StructureByteStride = desc.Stride ? desc.Stride : desc.Length;
				if ((bd.ByteWidth & 0x0000000F) && (desc.Usage & ResourceUsageConstantBuffer)) bd.ByteWidth = (bd.ByteWidth + 15) & 0xFFFFFFF0;
				if ((desc.Usage & ResourceUsageShaderRead) || (desc.Usage & ResourceUsageShaderWrite)) {
					bd.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
					result->usage_flags |= ResourceUsageShaderRead;
					result->usage_flags |= ResourceUsageShaderWrite;
				}
				if (desc.Usage & ResourceUsageIndexBuffer) {
					bd.BindFlags |= D3D11_BIND_INDEX_BUFFER;
					result->usage_flags |= ResourceUsageIndexBuffer;
				}
				if (desc.Usage & ResourceUsageConstantBuffer) {
					bd.BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
					result->usage_flags |= ResourceUsageConstantBuffer;
				}
				if (bd.BindFlags & D3D11_BIND_SHADER_RESOURCE) bd.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
				if (device->CreateBuffer(&bd, 0, &result->buffer) != S_OK) return 0;
				bool create_staging = false;
				if (desc.Usage & ResourceUsageCPURead) {
					create_staging = true;
					result->usage_flags |= ResourceUsageCPURead;
				}
				if (desc.Usage & ResourceUsageCPUWrite) result->usage_flags |= ResourceUsageCPUWrite;
				if (bd.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
					if (device->CreateShaderResourceView(result->buffer, 0, &result->view) != S_OK) return 0;
				}
				result->length = desc.Length;
				result->stride = desc.Stride;
				if (create_staging) {
					bd.Usage = D3D11_USAGE_STAGING;
					bd.BindFlags = 0;
					bd.MiscFlags = 0;
					if (desc.Usage & ResourceUsageCPURead) bd.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
					if (device->CreateBuffer(&bd, 0, &result->buffer_staging) != S_OK) return 0;
				}
				result->Retain();
				return result;
			}
			virtual IBuffer * CreateBuffer(const BufferDesc & desc, const ResourceInitDesc & init) noexcept override
			{
				if (desc.MemoryPool == ResourceMemoryPool::Shared) return 0;
				if (desc.Usage & ~ResourceUsageBufferMask) return 0;
				SafePointer<D3D11_Buffer> result = new (std::nothrow) D3D11_Buffer(this);
				if (!result) return 0;
				result->pool = desc.MemoryPool;
				if (desc.MemoryPool == ResourceMemoryPool::Immutable) {
					if (desc.Usage & ResourceUsageShaderWrite) return 0;
					if (desc.Usage & ResourceUsageCPURead) return 0;
					if (desc.Usage & ResourceUsageCPUWrite) return 0;
				}
				D3D11_BUFFER_DESC bd;
				bd.ByteWidth = desc.Length;
				if (desc.MemoryPool == ResourceMemoryPool::Default) bd.Usage = D3D11_USAGE_DEFAULT;
				else if (desc.MemoryPool == ResourceMemoryPool::Immutable) bd.Usage = D3D11_USAGE_IMMUTABLE;
				bd.BindFlags = 0;
				bd.CPUAccessFlags = 0;
				bd.MiscFlags = 0;
				bd.StructureByteStride = desc.Stride ? desc.Stride : desc.Length;
				if ((bd.ByteWidth & 0x0000000F) && (desc.Usage & ResourceUsageConstantBuffer)) bd.ByteWidth = (bd.ByteWidth + 15) & 0xFFFFFFF0;
				if ((desc.Usage & ResourceUsageShaderRead) || (desc.Usage & ResourceUsageShaderWrite)) {
					bd.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
					result->usage_flags |= ResourceUsageShaderRead;
					if (desc.MemoryPool == ResourceMemoryPool::Default) result->usage_flags |= ResourceUsageShaderWrite;
				}
				if (desc.Usage & ResourceUsageIndexBuffer) {
					bd.BindFlags |= D3D11_BIND_INDEX_BUFFER;
					result->usage_flags |= ResourceUsageIndexBuffer;
				}
				if (desc.Usage & ResourceUsageConstantBuffer) {
					bd.BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
					result->usage_flags |= ResourceUsageConstantBuffer;
				}
				void * aligned_data = 0;
				D3D11_SUBRESOURCE_DATA sr;
				if (bd.ByteWidth > desc.Length) {
					aligned_data = malloc(bd.ByteWidth);
					if (!aligned_data) return 0;
					MemoryCopy(aligned_data, init.Data, desc.Length);
					sr.pSysMem = aligned_data;
				} else sr.pSysMem = init.Data;
				sr.SysMemPitch = sr.SysMemSlicePitch = 0;
				if (bd.BindFlags & D3D11_BIND_SHADER_RESOURCE) bd.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
				if (device->CreateBuffer(&bd, &sr, &result->buffer) != S_OK) { free(aligned_data); return 0; }
				free(aligned_data);
				bool create_staging = false;
				if (desc.Usage & ResourceUsageCPURead) {
					create_staging = true;
					result->usage_flags |= ResourceUsageCPURead;
				}
				if (desc.Usage & ResourceUsageCPUWrite) result->usage_flags |= ResourceUsageCPUWrite;
				if (bd.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
					if (device->CreateShaderResourceView(result->buffer, 0, &result->view) != S_OK) return 0;
				}
				result->length = desc.Length;
				result->stride = desc.Stride;
				if (create_staging) {
					bd.Usage = D3D11_USAGE_STAGING;
					bd.BindFlags = 0;
					bd.MiscFlags = 0;
					if (desc.Usage & ResourceUsageCPURead) bd.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
					if (device->CreateBuffer(&bd, 0, &result->buffer_staging) != S_OK) return 0;
				}
				result->Retain();
				return result;
			}
			virtual ITexture * CreateTexture(const TextureDesc & desc) noexcept override
			{
				if (desc.MemoryPool == ResourceMemoryPool::Immutable) return 0;
				if (desc.Usage & ~ResourceUsageTextureMask) return 0;
				SafePointer<D3D11_Texture> result = new (std::nothrow) D3D11_Texture(desc.Type, this);
				if (!result) return 0;
				DXGI_FORMAT dxgi_format = MakeDxgiFormat(desc.Format);
				if (dxgi_format == DXGI_FORMAT_UNKNOWN) return 0;
				result->format = desc.Format;
				result->pool = desc.MemoryPool;
				if ((desc.Usage & ResourceUsageRenderTarget) && !IsColorFormat(desc.Format)) return 0;
				if ((desc.Usage & ResourceUsageDepthStencil) && !IsDepthStencilFormat(desc.Format)) return 0;
				if (desc.MemoryPool == ResourceMemoryPool::Shared && (desc.Usage & (ResourceUsageCPUAll | ResourceUsageVideoAll))) return 0;
				result->usage_flags |= ResourceUsageVideoAll;
				if (desc.Type == TextureType::Type1D || desc.Type == TextureType::TypeArray1D) {
					D3D11_TEXTURE1D_DESC td;
					td.Width = desc.Width;
					td.MipLevels = desc.MipmapCount;
					td.ArraySize = (desc.Type == TextureType::TypeArray1D) ? desc.ArraySize : 1;
					td.Format = dxgi_format;
					td.Usage = D3D11_USAGE_DEFAULT;
					td.BindFlags = 0;
					if ((desc.Usage & ResourceUsageShaderRead) || (desc.Usage & ResourceUsageShaderWrite)) {
						td.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
						result->usage_flags |= ResourceUsageShaderRead;
						result->usage_flags |= ResourceUsageShaderWrite;
					}
					if (desc.Usage & ResourceUsageRenderTarget) {
						td.BindFlags |= D3D11_BIND_RENDER_TARGET;
						result->usage_flags |= ResourceUsageRenderTarget;
					}
					if (desc.Usage & ResourceUsageDepthStencil) {
						td.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
						result->usage_flags |= ResourceUsageDepthStencil;
					}
					td.CPUAccessFlags = 0;
					td.MiscFlags = 0;
					if ((desc.Usage & ResourceUsageShaderRead) && (desc.Usage & ResourceUsageRenderTarget) && _is_valid_generate_mipmaps_format(dxgi_format)) {
						td.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
					}
					if (desc.MemoryPool == ResourceMemoryPool::Shared) td.MiscFlags |= D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
					if (device->CreateTexture1D(&td, 0, &result->tex_1d) != S_OK) return 0;
					bool create_staging = false;
					if (desc.Usage & ResourceUsageCPURead) {
						create_staging = true;
						result->usage_flags |= ResourceUsageCPURead;
					}
					if (desc.Usage & ResourceUsageCPUWrite) {
						if (desc.Usage & ResourceUsageDepthStencil) create_staging = true;
						result->usage_flags |= ResourceUsageCPUWrite;
					}
					if (td.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
						if (device->CreateShaderResourceView(result->tex_1d, 0, &result->view) != S_OK) return 0;
					}
					if (td.BindFlags & D3D11_BIND_RENDER_TARGET) {
						if (device->CreateRenderTargetView(result->tex_1d, 0, &result->rt_view) != S_OK) return 0;
					}
					if (td.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
						if (device->CreateDepthStencilView(result->tex_1d, 0, &result->ds_view) != S_OK) return 0;
					}
					result->width = desc.Width;
					result->height = result->depth = 1;
					result->size = td.ArraySize;
					if (create_staging) {
						td.Usage = D3D11_USAGE_STAGING;
						td.BindFlags = 0;
						td.MiscFlags = 0;
						if (desc.Usage & ResourceUsageCPURead) td.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
						if (desc.Usage & ResourceUsageCPUWrite) td.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
						if (device->CreateTexture1D(&td, 0, &result->tex_staging_1d) != S_OK) return 0;
					}
					if (desc.MemoryPool == ResourceMemoryPool::Shared) try { result->shared_handle = new D3D11_DeviceResourceHandle(result->tex_1d, this, result); } catch (...) { return 0; }
				} else if (desc.Type == TextureType::Type2D || desc.Type == TextureType::TypeArray2D) {
					D3D11_TEXTURE2D_DESC td;
					td.Width = desc.Width;
					td.Height = desc.Height;
					td.MipLevels = desc.MipmapCount;
					td.ArraySize = (desc.Type == TextureType::TypeArray2D) ? desc.ArraySize : 1;
					td.Format = dxgi_format;
					td.SampleDesc.Count = 1;
					td.SampleDesc.Quality = 0;
					td.Usage = D3D11_USAGE_DEFAULT;
					td.BindFlags = 0;
					if ((desc.Usage & ResourceUsageShaderRead) || (desc.Usage & ResourceUsageShaderWrite)) {
						td.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
						result->usage_flags |= ResourceUsageShaderRead;
						result->usage_flags |= ResourceUsageShaderWrite;
					}
					if (desc.Usage & ResourceUsageRenderTarget) {
						td.BindFlags |= D3D11_BIND_RENDER_TARGET;
						result->usage_flags |= ResourceUsageRenderTarget;
					}
					if (desc.Usage & ResourceUsageDepthStencil) {
						td.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
						result->usage_flags |= ResourceUsageDepthStencil;
					}
					td.CPUAccessFlags = 0;
					td.MiscFlags = 0;
					if ((desc.Usage & ResourceUsageShaderRead) && (desc.Usage & ResourceUsageRenderTarget) && _is_valid_generate_mipmaps_format(dxgi_format)) {
						td.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
					}
					if (desc.MemoryPool == ResourceMemoryPool::Shared) td.MiscFlags |= D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
					if (device->CreateTexture2D(&td, 0, &result->tex_2d) != S_OK) return 0;
					bool create_staging = false;
					if (desc.Usage & ResourceUsageCPURead) {
						create_staging = true;
						result->usage_flags |= ResourceUsageCPURead;
					}
					if (desc.Usage & ResourceUsageCPUWrite) {
						if (desc.Usage & ResourceUsageDepthStencil) create_staging = true;
						result->usage_flags |= ResourceUsageCPUWrite;
					}
					if (td.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
						if (device->CreateShaderResourceView(result->tex_2d, 0, &result->view) != S_OK) return 0;
					}
					if (td.BindFlags & D3D11_BIND_RENDER_TARGET) {
						if (device->CreateRenderTargetView(result->tex_2d, 0, &result->rt_view) != S_OK) return 0;
					}
					if (td.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
						if (device->CreateDepthStencilView(result->tex_2d, 0, &result->ds_view) != S_OK) return 0;
					}
					result->width = desc.Width;
					result->height = desc.Height;
					result->depth = 1;
					result->size = td.ArraySize;
					if (create_staging) {
						td.Usage = D3D11_USAGE_STAGING;
						td.BindFlags = 0;
						td.MiscFlags = 0;
						if (desc.Usage & ResourceUsageCPURead) td.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
						if (desc.Usage & ResourceUsageCPUWrite) td.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
						if (device->CreateTexture2D(&td, 0, &result->tex_staging_2d) != S_OK) return 0;
					}
					if (desc.MemoryPool == ResourceMemoryPool::Shared) try { result->shared_handle = new D3D11_DeviceResourceHandle(result->tex_2d, this, result); } catch (...) { return 0; }
				} else if (desc.Type == TextureType::TypeCube || desc.Type == TextureType::TypeArrayCube) {
					if (desc.Width != desc.Height) return 0;
					D3D11_TEXTURE2D_DESC td;
					td.Width = desc.Width;
					td.Height = desc.Height;
					td.MipLevels = desc.MipmapCount;
					td.ArraySize = (desc.Type == TextureType::TypeArrayCube) ? (desc.ArraySize * 6) : 6;
					td.Format = dxgi_format;
					td.SampleDesc.Count = 1;
					td.SampleDesc.Quality = 0;
					td.Usage = D3D11_USAGE_DEFAULT;
					td.BindFlags = 0;
					if ((desc.Usage & ResourceUsageShaderRead) || (desc.Usage & ResourceUsageShaderWrite)) {
						td.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
						result->usage_flags |= ResourceUsageShaderRead;
						result->usage_flags |= ResourceUsageShaderWrite;
					}
					if (desc.Usage & ResourceUsageRenderTarget) {
						td.BindFlags |= D3D11_BIND_RENDER_TARGET;
						result->usage_flags |= ResourceUsageRenderTarget;
					}
					if (desc.Usage & ResourceUsageDepthStencil) {
						td.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
						result->usage_flags |= ResourceUsageDepthStencil;
					}
					td.CPUAccessFlags = 0;
					td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
					if ((desc.Usage & ResourceUsageShaderRead) && (desc.Usage & ResourceUsageRenderTarget) && _is_valid_generate_mipmaps_format(dxgi_format)) {
						td.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
					}
					if (desc.MemoryPool == ResourceMemoryPool::Shared) td.MiscFlags |= D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
					if (device->CreateTexture2D(&td, 0, &result->tex_2d) != S_OK) return 0;
					bool create_staging = false;
					if (desc.Usage & ResourceUsageCPURead) {
						create_staging = true;
						result->usage_flags |= ResourceUsageCPURead;
					}
					if (desc.Usage & ResourceUsageCPUWrite) {
						if (desc.Usage & ResourceUsageDepthStencil) create_staging = true;
						result->usage_flags |= ResourceUsageCPUWrite;
					}
					if (td.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
						if (device->CreateShaderResourceView(result->tex_2d, 0, &result->view) != S_OK) return 0;
					}
					if (td.BindFlags & D3D11_BIND_RENDER_TARGET) {
						if (device->CreateRenderTargetView(result->tex_2d, 0, &result->rt_view) != S_OK) return 0;
					}
					if (td.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
						if (device->CreateDepthStencilView(result->tex_2d, 0, &result->ds_view) != S_OK) return 0;
					}
					result->width = desc.Width;
					result->height = desc.Height;
					result->depth = 1;
					result->size = td.ArraySize / 6;
					if (create_staging) {
						td.Usage = D3D11_USAGE_STAGING;
						td.BindFlags = 0;
						td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
						if (desc.Usage & ResourceUsageCPURead) td.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
						if (desc.Usage & ResourceUsageCPUWrite) td.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
						if (device->CreateTexture2D(&td, 0, &result->tex_staging_2d) != S_OK) return 0;
					}
					if (desc.MemoryPool == ResourceMemoryPool::Shared) try { result->shared_handle = new D3D11_DeviceResourceHandle(result->tex_2d, this, result); } catch (...) { return 0; }
				} else if (desc.Type == TextureType::Type3D) {
					D3D11_TEXTURE3D_DESC td;
					td.Width = desc.Width;
					td.Height = desc.Height;
					td.Depth = desc.Depth;
					td.MipLevels = desc.MipmapCount;
					td.Format = dxgi_format;
					td.Usage = D3D11_USAGE_DEFAULT;
					td.BindFlags = 0;
					if ((desc.Usage & ResourceUsageShaderRead) || (desc.Usage & ResourceUsageShaderWrite)) {
						td.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
						result->usage_flags |= ResourceUsageShaderRead;
						result->usage_flags |= ResourceUsageShaderWrite;
					}
					if (desc.Usage & ResourceUsageRenderTarget) {
						td.BindFlags |= D3D11_BIND_RENDER_TARGET;
						result->usage_flags |= ResourceUsageRenderTarget;
					}
					if (desc.Usage & ResourceUsageDepthStencil) {
						td.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
						result->usage_flags |= ResourceUsageDepthStencil;
					}
					td.CPUAccessFlags = 0;
					td.MiscFlags = 0;
					if ((desc.Usage & ResourceUsageShaderRead) && (desc.Usage & ResourceUsageRenderTarget) && _is_valid_generate_mipmaps_format(dxgi_format)) {
						td.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
					}
					if (desc.MemoryPool == ResourceMemoryPool::Shared) td.MiscFlags |= D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
					if (device->CreateTexture3D(&td, 0, &result->tex_3d) != S_OK) return 0;
					bool create_staging = false;
					if (desc.Usage & ResourceUsageCPURead) {
						create_staging = true;
						result->usage_flags |= ResourceUsageCPURead;
					}
					if (desc.Usage & ResourceUsageCPUWrite) {
						if (desc.Usage & ResourceUsageDepthStencil) create_staging = true;
						result->usage_flags |= ResourceUsageCPUWrite;
					}
					if (td.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
						if (device->CreateShaderResourceView(result->tex_3d, 0, &result->view) != S_OK) return 0;
					}
					if (td.BindFlags & D3D11_BIND_RENDER_TARGET) {
						if (device->CreateRenderTargetView(result->tex_3d, 0, &result->rt_view) != S_OK) return 0;
					}
					if (td.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
						if (device->CreateDepthStencilView(result->tex_3d, 0, &result->ds_view) != S_OK) return 0;
					}
					result->width = desc.Width;
					result->height = desc.Height;
					result->depth = desc.Depth;
					result->size = 1;
					if (create_staging) {
						td.Usage = D3D11_USAGE_STAGING;
						td.BindFlags = 0;
						td.MiscFlags = 0;
						if (desc.Usage & ResourceUsageCPURead) td.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
						if (desc.Usage & ResourceUsageCPUWrite) td.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
						if (device->CreateTexture3D(&td, 0, &result->tex_staging_3d) != S_OK) return 0;
					}
					if (desc.MemoryPool == ResourceMemoryPool::Shared) try { result->shared_handle = new D3D11_DeviceResourceHandle(result->tex_3d, this, result); } catch (...) { return 0; }
				} else return 0;
				result->Retain();
				return result;
			}
			virtual ITexture * CreateTexture(const TextureDesc & desc, const ResourceInitDesc * init) noexcept override
			{
				if (desc.MemoryPool == ResourceMemoryPool::Shared) return 0;
				if (!init) return 0;
				if (desc.Usage & ~ResourceUsageTextureMask) return 0;
				SafePointer<D3D11_Texture> result = new (std::nothrow) D3D11_Texture(desc.Type, this);
				if (!result) return 0;
				DXGI_FORMAT dxgi_format = MakeDxgiFormat(desc.Format);
				if (dxgi_format == DXGI_FORMAT_UNKNOWN) return 0;
				result->format = desc.Format;
				result->pool = desc.MemoryPool;
				if ((desc.Usage & ResourceUsageRenderTarget) && !IsColorFormat(desc.Format)) return 0;
				if ((desc.Usage & ResourceUsageDepthStencil) && !IsDepthStencilFormat(desc.Format)) return 0;
				if (desc.MemoryPool == ResourceMemoryPool::Immutable) {
					result->usage_flags |= ResourceUsageVideoRead;
					if (desc.Usage & ResourceUsageShaderWrite) return 0;
					if (desc.Usage & ResourceUsageRenderTarget) return 0;
					if (desc.Usage & ResourceUsageDepthStencil) return 0;
					if (desc.Usage & ResourceUsageCPURead) return 0;
					if (desc.Usage & ResourceUsageCPUWrite) return 0;
					if (desc.Usage & ResourceUsageVideoWrite) return 0;
				} else result->usage_flags |= ResourceUsageVideoAll;
				if (desc.Type == TextureType::Type1D || desc.Type == TextureType::TypeArray1D) {
					D3D11_TEXTURE1D_DESC td;
					td.Width = desc.Width;
					td.MipLevels = desc.MipmapCount;
					if (!td.MipLevels) td.MipLevels = _calculate_mip_levels(desc.Width, 1, 1);
					td.ArraySize = (desc.Type == TextureType::TypeArray1D) ? desc.ArraySize : 1;
					td.Format = dxgi_format;
					if (desc.MemoryPool == ResourceMemoryPool::Default) td.Usage = D3D11_USAGE_DEFAULT;
					else if (desc.MemoryPool == ResourceMemoryPool::Immutable) td.Usage = D3D11_USAGE_IMMUTABLE;
					td.BindFlags = 0;
					if ((desc.Usage & ResourceUsageShaderRead) || (desc.Usage & ResourceUsageShaderWrite)) {
						td.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
						result->usage_flags |= ResourceUsageShaderRead;
						if (desc.MemoryPool == ResourceMemoryPool::Default) result->usage_flags |= ResourceUsageShaderWrite;
					}
					if (desc.Usage & ResourceUsageRenderTarget) {
						td.BindFlags |= D3D11_BIND_RENDER_TARGET;
						result->usage_flags |= ResourceUsageRenderTarget;
					}
					if (desc.Usage & ResourceUsageDepthStencil) {
						td.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
						result->usage_flags |= ResourceUsageDepthStencil;
					}
					td.CPUAccessFlags = 0;
					td.MiscFlags = 0;
					if ((desc.Usage & ResourceUsageShaderRead) && (desc.Usage & ResourceUsageRenderTarget) && _is_valid_generate_mipmaps_format(dxgi_format)) {
						td.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
					}
					auto sr = _make_subres_data(init, td.MipLevels * td.ArraySize);
					if (!sr) return 0;
					if (device->CreateTexture1D(&td, sr, &result->tex_1d) != S_OK) { free(sr); return 0; }
					free(sr);
					bool create_staging = false;
					if (desc.Usage & ResourceUsageCPURead) {
						create_staging = true;
						result->usage_flags |= ResourceUsageCPURead;
					}
					if (desc.Usage & ResourceUsageCPUWrite) {
						if (desc.Usage & ResourceUsageDepthStencil) create_staging = true;
						result->usage_flags |= ResourceUsageCPUWrite;
					}
					if (td.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
						if (device->CreateShaderResourceView(result->tex_1d, 0, &result->view) != S_OK) return 0;
					}
					if (td.BindFlags & D3D11_BIND_RENDER_TARGET) {
						if (device->CreateRenderTargetView(result->tex_1d, 0, &result->rt_view) != S_OK) return 0;
					}
					if (td.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
						if (device->CreateDepthStencilView(result->tex_1d, 0, &result->ds_view) != S_OK) return 0;
					}
					result->width = desc.Width;
					result->height = result->depth = 1;
					result->size = td.ArraySize;
					if (create_staging) {
						td.Usage = D3D11_USAGE_STAGING;
						td.BindFlags = 0;
						td.MiscFlags = 0;
						if (desc.Usage & ResourceUsageCPURead) td.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
						if (desc.Usage & ResourceUsageCPUWrite) td.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
						if (device->CreateTexture1D(&td, 0, &result->tex_staging_1d) != S_OK) return 0;
					}
				} else if (desc.Type == TextureType::Type2D || desc.Type == TextureType::TypeArray2D) {
					D3D11_TEXTURE2D_DESC td;
					td.Width = desc.Width;
					td.Height = desc.Height;
					td.MipLevels = desc.MipmapCount;
					if (!td.MipLevels) td.MipLevels = _calculate_mip_levels(desc.Width, desc.Height, 1);
					td.ArraySize = (desc.Type == TextureType::TypeArray2D) ? desc.ArraySize : 1;
					td.Format = dxgi_format;
					td.SampleDesc.Count = 1;
					td.SampleDesc.Quality = 0;
					if (desc.MemoryPool == ResourceMemoryPool::Default) td.Usage = D3D11_USAGE_DEFAULT;
					else if (desc.MemoryPool == ResourceMemoryPool::Immutable) td.Usage = D3D11_USAGE_IMMUTABLE;
					td.BindFlags = 0;
					if ((desc.Usage & ResourceUsageShaderRead) || (desc.Usage & ResourceUsageShaderWrite)) {
						td.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
						result->usage_flags |= ResourceUsageShaderRead;
						if (desc.MemoryPool == ResourceMemoryPool::Default) result->usage_flags |= ResourceUsageShaderWrite;
					}
					if (desc.Usage & ResourceUsageRenderTarget) {
						td.BindFlags |= D3D11_BIND_RENDER_TARGET;
						result->usage_flags |= ResourceUsageRenderTarget;
					}
					if (desc.Usage & ResourceUsageDepthStencil) {
						td.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
						result->usage_flags |= ResourceUsageDepthStencil;
					}
					td.CPUAccessFlags = 0;
					td.MiscFlags = 0;
					if ((desc.Usage & ResourceUsageShaderRead) && (desc.Usage & ResourceUsageRenderTarget) && _is_valid_generate_mipmaps_format(dxgi_format)) {
						td.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
					}
					auto sr = _make_subres_data(init, td.MipLevels * td.ArraySize);
					if (!sr) return 0;
					if (device->CreateTexture2D(&td, sr, &result->tex_2d) != S_OK) { free(sr); return 0; }
					free(sr);
					bool create_staging = false;
					if (desc.Usage & ResourceUsageCPURead) {
						create_staging = true;
						result->usage_flags |= ResourceUsageCPURead;
					}
					if (desc.Usage & ResourceUsageCPUWrite) {
						if (desc.Usage & ResourceUsageDepthStencil) create_staging = true;
						result->usage_flags |= ResourceUsageCPUWrite;
					}
					if (td.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
						if (device->CreateShaderResourceView(result->tex_2d, 0, &result->view) != S_OK) return 0;
					}
					if (td.BindFlags & D3D11_BIND_RENDER_TARGET) {
						if (device->CreateRenderTargetView(result->tex_2d, 0, &result->rt_view) != S_OK) return 0;
					}
					if (td.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
						if (device->CreateDepthStencilView(result->tex_2d, 0, &result->ds_view) != S_OK) return 0;
					}
					result->width = desc.Width;
					result->height = desc.Height;
					result->depth = 1;
					result->size = td.ArraySize;
					if (create_staging) {
						td.Usage = D3D11_USAGE_STAGING;
						td.BindFlags = 0;
						td.MiscFlags = 0;
						if (desc.Usage & ResourceUsageCPURead) td.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
						if (desc.Usage & ResourceUsageCPUWrite) td.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
						if (device->CreateTexture2D(&td, 0, &result->tex_staging_2d) != S_OK) return 0;
					}
				} else if (desc.Type == TextureType::TypeCube || desc.Type == TextureType::TypeArrayCube) {
					if (desc.Width != desc.Height) return 0;
					D3D11_TEXTURE2D_DESC td;
					td.Width = desc.Width;
					td.Height = desc.Height;
					td.MipLevels = desc.MipmapCount;
					if (!td.MipLevels) td.MipLevels = _calculate_mip_levels(desc.Width, desc.Height, 1);
					td.ArraySize = (desc.Type == TextureType::TypeArrayCube) ? (desc.ArraySize * 6) : 6;
					td.Format = dxgi_format;
					td.SampleDesc.Count = 1;
					td.SampleDesc.Quality = 0;
					if (desc.MemoryPool == ResourceMemoryPool::Default) td.Usage = D3D11_USAGE_DEFAULT;
					else if (desc.MemoryPool == ResourceMemoryPool::Immutable) td.Usage = D3D11_USAGE_IMMUTABLE;
					td.BindFlags = 0;
					if ((desc.Usage & ResourceUsageShaderRead) || (desc.Usage & ResourceUsageShaderWrite)) {
						td.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
						result->usage_flags |= ResourceUsageShaderRead;
						if (desc.MemoryPool == ResourceMemoryPool::Default) result->usage_flags |= ResourceUsageShaderWrite;
					}
					if (desc.Usage & ResourceUsageRenderTarget) {
						td.BindFlags |= D3D11_BIND_RENDER_TARGET;
						result->usage_flags |= ResourceUsageRenderTarget;
					}
					if (desc.Usage & ResourceUsageDepthStencil) {
						td.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
						result->usage_flags |= ResourceUsageDepthStencil;
					}
					td.CPUAccessFlags = 0;
					td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
					if ((desc.Usage & ResourceUsageShaderRead) && (desc.Usage & ResourceUsageRenderTarget) && _is_valid_generate_mipmaps_format(dxgi_format)) {
						td.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
					}
					auto sr = _make_subres_data(init, td.MipLevels * td.ArraySize);
					if (!sr) return 0;
					if (device->CreateTexture2D(&td, sr, &result->tex_2d) != S_OK) { free(sr); return 0; }
					free(sr);
					bool create_staging = false;
					if (desc.Usage & ResourceUsageCPURead) {
						create_staging = true;
						result->usage_flags |= ResourceUsageCPURead;
					}
					if (desc.Usage & ResourceUsageCPUWrite) {
						if (desc.Usage & ResourceUsageDepthStencil) create_staging = true;
						result->usage_flags |= ResourceUsageCPUWrite;
					}
					if (td.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
						if (device->CreateShaderResourceView(result->tex_2d, 0, &result->view) != S_OK) return 0;
					}
					if (td.BindFlags & D3D11_BIND_RENDER_TARGET) {
						if (device->CreateRenderTargetView(result->tex_2d, 0, &result->rt_view) != S_OK) return 0;
					}
					if (td.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
						if (device->CreateDepthStencilView(result->tex_2d, 0, &result->ds_view) != S_OK) return 0;
					}
					result->width = desc.Width;
					result->height = desc.Height;
					result->depth = 1;
					result->size = td.ArraySize / 6;
					if (create_staging) {
						td.Usage = D3D11_USAGE_STAGING;
						td.BindFlags = 0;
						td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
						if (desc.Usage & ResourceUsageCPURead) td.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
						if (desc.Usage & ResourceUsageCPUWrite) td.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
						if (device->CreateTexture2D(&td, 0, &result->tex_staging_2d) != S_OK) return 0;
					}
				} else if (desc.Type == TextureType::Type3D) {
					D3D11_TEXTURE3D_DESC td;
					td.Width = desc.Width;
					td.Height = desc.Height;
					td.Depth = desc.Depth;
					td.MipLevels = desc.MipmapCount;
					if (!td.MipLevels) td.MipLevels = _calculate_mip_levels(desc.Width, desc.Height, desc.Depth);
					td.Format = dxgi_format;
					if (desc.MemoryPool == ResourceMemoryPool::Default) td.Usage = D3D11_USAGE_DEFAULT;
					else if (desc.MemoryPool == ResourceMemoryPool::Immutable) td.Usage = D3D11_USAGE_IMMUTABLE;
					td.BindFlags = 0;
					if ((desc.Usage & ResourceUsageShaderRead) || (desc.Usage & ResourceUsageShaderWrite)) {
						td.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
						result->usage_flags |= ResourceUsageShaderRead;
						if (desc.MemoryPool == ResourceMemoryPool::Default) result->usage_flags |= ResourceUsageShaderWrite;
					}
					if (desc.Usage & ResourceUsageRenderTarget) {
						td.BindFlags |= D3D11_BIND_RENDER_TARGET;
						result->usage_flags |= ResourceUsageRenderTarget;
					}
					if (desc.Usage & ResourceUsageDepthStencil) {
						td.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
						result->usage_flags |= ResourceUsageDepthStencil;
					}
					td.CPUAccessFlags = 0;
					td.MiscFlags = 0;
					if ((desc.Usage & ResourceUsageShaderRead) && (desc.Usage & ResourceUsageRenderTarget) && _is_valid_generate_mipmaps_format(dxgi_format)) {
						td.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
					}
					auto sr = _make_subres_data(init, td.MipLevels);
					if (!sr) return 0;
					if (device->CreateTexture3D(&td, sr, &result->tex_3d) != S_OK) { free(sr); return 0; }
					free(sr);
					bool create_staging = false;
					if (desc.Usage & ResourceUsageCPURead) {
						create_staging = true;
						result->usage_flags |= ResourceUsageCPURead;
					}
					if (desc.Usage & ResourceUsageCPUWrite) {
						if (desc.Usage & ResourceUsageDepthStencil) create_staging = true;
						result->usage_flags |= ResourceUsageCPUWrite;
					}
					if (td.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
						if (device->CreateShaderResourceView(result->tex_3d, 0, &result->view) != S_OK) return 0;
					}
					if (td.BindFlags & D3D11_BIND_RENDER_TARGET) {
						if (device->CreateRenderTargetView(result->tex_3d, 0, &result->rt_view) != S_OK) return 0;
					}
					if (td.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
						if (device->CreateDepthStencilView(result->tex_3d, 0, &result->ds_view) != S_OK) return 0;
					}
					result->width = desc.Width;
					result->height = desc.Height;
					result->depth = desc.Depth;
					result->size = 1;
					if (create_staging) {
						td.Usage = D3D11_USAGE_STAGING;
						td.BindFlags = 0;
						td.MiscFlags = 0;
						if (desc.Usage & ResourceUsageCPURead) td.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
						if (desc.Usage & ResourceUsageCPUWrite) td.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
						if (device->CreateTexture3D(&td, 0, &result->tex_staging_3d) != S_OK) return 0;
					}
				} else return 0;
				result->Retain();
				return result;
			}
			virtual ITexture * CreateRenderTargetView(ITexture * texture, uint32 mip_level, uint32 array_offset_or_depth) noexcept override
			{
				if (!texture || texture->GetParentDevice() != this || !(texture->GetResourceUsage() & ResourceUsageRenderTarget)) return 0;
				SafePointer<D3D11_Texture> result = new (std::nothrow) D3D11_Texture(texture->GetTextureType(), this);
				if (!result) return 0;
				auto source = static_cast<D3D11_Texture *>(texture);
				result->tex_1d = source->tex_1d;
				result->tex_2d = source->tex_2d;
				result->tex_3d = source->tex_3d;
				if (result->tex_1d) result->tex_1d->AddRef();
				if (result->tex_2d) result->tex_2d->AddRef();
				if (result->tex_3d) result->tex_3d->AddRef();
				result->pool = source->pool;
				result->format = source->format;
				result->usage_flags = ResourceUsageRenderTarget;
				result->width = source->width; result->height = source->height;
				result->depth = source->depth; result->size = source->size;
				D3D11_RENDER_TARGET_VIEW_DESC rtvd;
				rtvd.Format = MakeDxgiFormat(texture->GetPixelFormat());
				ID3D11Resource * source_resource = 0;
				if (texture->GetTextureType() == TextureType::Type1D) {
					rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
					rtvd.Texture1D.MipSlice = mip_level;
					source_resource = result->tex_1d;
				} else if (texture->GetTextureType() == TextureType::TypeArray1D) {
					rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
					rtvd.Texture1DArray.MipSlice = mip_level;
					rtvd.Texture1DArray.FirstArraySlice = array_offset_or_depth;
					rtvd.Texture1DArray.ArraySize = 1;
					source_resource = result->tex_1d;
				} else if (texture->GetTextureType() == TextureType::Type2D) {
					rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
					rtvd.Texture2D.MipSlice = mip_level;
					source_resource = result->tex_2d;
				} else if (texture->GetTextureType() == TextureType::TypeArray2D || texture->GetTextureType() == TextureType::TypeCube || texture->GetTextureType() == TextureType::TypeArrayCube) {
					rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
					rtvd.Texture2DArray.MipSlice = mip_level;
					rtvd.Texture2DArray.FirstArraySlice = array_offset_or_depth;
					rtvd.Texture2DArray.ArraySize = 1;
					source_resource = result->tex_2d;
				} else if (texture->GetTextureType() == TextureType::Type3D) {
					rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
					rtvd.Texture3D.MipSlice = mip_level;
					rtvd.Texture3D.FirstWSlice = array_offset_or_depth;
					rtvd.Texture3D.WSize = 1;
					source_resource = result->tex_3d;
				} else return 0;
				if (device->CreateRenderTargetView(source_resource, &rtvd, &result->rt_view) != S_OK) return 0;
				result->Retain();
				return result;
			}
			virtual IDeviceResource * OpenResource(IDeviceResourceHandle * handle) noexcept override
			{
				if (!handle || handle->GetDeviceIdentifier() != GetDeviceIdentifier()) return 0;
				auto src = static_cast<D3D11_DeviceResourceHandle *>(handle);
				SafePointer<D3D11_Texture> result = new (std::nothrow) D3D11_Texture(src->_type, this);
				if (!result) return 0;
				ID3D11Resource * resource;
				if (src->_type == TextureType::Type1D || src->_type == TextureType::TypeArray1D) {
					ID3D11Device1 * device1;
					if (device->QueryInterface(IID_ID3D11Device1, reinterpret_cast<void **>(&device1)) != S_OK) return 0;
					if (device1->OpenSharedResourceByName(src->_nt_path, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, IID_ID3D11Texture1D, reinterpret_cast<void **>(&result->tex_1d)) != S_OK) { device1->Release(); return 0; }
					device1->Release();
					resource = result->tex_1d;
				} else if (src->_type == TextureType::Type2D || src->_type == TextureType::TypeArray2D || src->_type == TextureType::TypeCube || src->_type == TextureType::TypeArrayCube) {
					ID3D11Device1 * device1;
					if (device->QueryInterface(IID_ID3D11Device1, reinterpret_cast<void **>(&device1)) != S_OK) return 0;
					if (device1->OpenSharedResourceByName(src->_nt_path, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, IID_ID3D11Texture2D, reinterpret_cast<void **>(&result->tex_2d)) != S_OK) { device1->Release(); return 0; }
					device1->Release();
					resource = result->tex_2d;
				} else if (src->_type == TextureType::Type3D) {
					ID3D11Device1 * device1;
					if (device->QueryInterface(IID_ID3D11Device1, reinterpret_cast<void **>(&device1)) != S_OK) return 0;
					if (device1->OpenSharedResourceByName(src->_nt_path, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, IID_ID3D11Texture3D, reinterpret_cast<void **>(&result->tex_3d)) != S_OK) { device1->Release(); return 0; }
					device1->Release();
					resource = result->tex_3d;
				} else throw InvalidArgumentException();
				if (src->_usage & ResourceUsageShaderRead) {
					if (device->CreateShaderResourceView(resource, 0, &result->view) != S_OK) return 0;
				}
				if (src->_usage & ResourceUsageRenderTarget) {
					if (device->CreateRenderTargetView(resource, 0, &result->rt_view) != S_OK) return 0;
				}
				if (src->_usage & ResourceUsageDepthStencil) {
					if (device->CreateDepthStencilView(resource, 0, &result->ds_view) != S_OK) return 0;
				}
				result->pool = ResourceMemoryPool::Shared;
				result->format = static_cast<PixelFormat>(src->_format);
				result->usage_flags = src->_usage;
				result->width = src->_width;
				result->height = src->_height;
				result->depth = src->_depth;
				result->size = src->_size;
				result->shared_handle.SetRetain(src);
				result->Retain();
				return result;
			}
			virtual IWindowLayer * CreateWindowLayer(Windows::ICoreWindow * window, const WindowLayerDesc & desc) noexcept override { try { return new D3D11_WindowLayer(window, this, desc); } catch (...) { return 0; } }
			virtual string ToString(void) const override { return L"D3D11_Device"; }
			ID3D11Device * GetWrappedDevice(void) const { return device; }
			IUnknown * GetVideoAcceleration(void) const { return video_acceleration; }
			void SetVideoAcceleration(IUnknown * va)
			{
				if (video_acceleration) video_acceleration->Release();
				video_acceleration = va;
				if (video_acceleration) video_acceleration->AddRef();
			}
		};
		class D3D11_DeviceFactory : public Graphics::IDeviceFactory
		{
			typedef HRESULT (WINAPI * func_CreateDXGIFactory) (REFIID iid, void ** factory);
		public:
			HMODULE _lib_dxgi, _lib_d3d11;
			IDXGIFactory * _dxgi_factory;
			IDXGIFactory1 * _dxgi_factory1;
			func_CreateDXGIFactory CreateDXGIFactory;
			func_CreateDXGIFactory CreateDXGIFactory1;
			PFN_D3D11_CREATE_DEVICE D3D11CreateDevice;
		private:
			D3D11_Device * _internal_create_device(IDXGIAdapter * adapter, D3D_DRIVER_TYPE driver) noexcept
			{
				if (!D3D11CreateDevice) return 0;
				ID3D11Device * result = 0;
				SafePointer<D3D11_Device> device;
				D3D_FEATURE_LEVEL feature_level[] = {
					D3D_FEATURE_LEVEL_11_1,
					D3D_FEATURE_LEVEL_11_0,
					D3D_FEATURE_LEVEL_10_1,
					D3D_FEATURE_LEVEL_10_0
				};
				D3D_FEATURE_LEVEL level_selected;
				if (D3D11CreateDevice(adapter, driver, 0, D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT, feature_level, 4, D3D11_SDK_VERSION, &result, &level_selected, 0) != S_OK) {
					if (D3D11CreateDevice(adapter, driver, 0, D3D11_CREATE_DEVICE_BGRA_SUPPORT, feature_level, 4, D3D11_SDK_VERSION, &result, &level_selected, 0) != S_OK) return 0;
				}
				try { device = new D3D11_Device(result, this); } catch (...) {}
				result->Release();
				if (device) device->Retain();
				return device;
			}
		public:
			D3D11_DeviceFactory(void)
			{
				_lib_dxgi = LoadLibraryW(L"dxgi.dll");
				if (!_lib_dxgi) throw Exception();
				CreateDXGIFactory = reinterpret_cast<func_CreateDXGIFactory>(GetProcAddress(_lib_dxgi, "CreateDXGIFactory"));
				CreateDXGIFactory1 = reinterpret_cast<func_CreateDXGIFactory>(GetProcAddress(_lib_dxgi, "CreateDXGIFactory1"));
				if (CreateDXGIFactory1) {
					if (CreateDXGIFactory1(IID_PPV_ARGS(&_dxgi_factory1)) != S_OK) { FreeLibrary(_lib_dxgi); throw Exception(); }
					_dxgi_factory = _dxgi_factory1;
					_dxgi_factory->AddRef();
				} else if (CreateDXGIFactory) {
					if (CreateDXGIFactory(IID_PPV_ARGS(&_dxgi_factory)) != S_OK) { FreeLibrary(_lib_dxgi); throw Exception(); }
					_dxgi_factory1 = 0;
				} else { FreeLibrary(_lib_dxgi); throw Exception(); }
				_lib_d3d11 = LoadLibraryW(L"d3d11.dll");
				if (_lib_d3d11) D3D11CreateDevice = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(GetProcAddress(_lib_d3d11, "D3D11CreateDevice")); else D3D11CreateDevice = 0;
			}
			virtual ~D3D11_DeviceFactory(void) override
			{
				if (_dxgi_factory1) _dxgi_factory1->Release();
				if (_dxgi_factory) _dxgi_factory->Release();
				if (_lib_d3d11) FreeLibrary(_lib_d3d11);
				FreeLibrary(_lib_dxgi);
			}
			virtual Volumes::Dictionary<uint64, string> * GetAvailableDevices(void) noexcept override
			{
				try {
					SafePointer< Volumes::Dictionary<uint64, string> > result = new Volumes::Dictionary<uint64, string>;
					if (_dxgi_factory1) {
						uint32 index = 0;
						DXGI_ADAPTER_DESC1 desc;
						while (true) {
							IDXGIAdapter1 * adapter;
							if (_dxgi_factory1->EnumAdapters1(index, &adapter) != S_OK) break;
							adapter->GetDesc1(&desc);
							adapter->Release();
							result->Append(reinterpret_cast<uint64 &>(desc.AdapterLuid), string(desc.Description));
							index++;
						}
					} else {
						uint32 index = 0;
						DXGI_ADAPTER_DESC desc;
						while (true) {
							IDXGIAdapter * adapter;
							if (_dxgi_factory->EnumAdapters(index, &adapter) != S_OK) break;
							adapter->GetDesc(&desc);
							adapter->Release();
							result->Append(reinterpret_cast<uint64 &>(desc.AdapterLuid), string(desc.Description));
							index++;
						}
					}
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			virtual IDevice * CreateDevice(uint64 identifier) noexcept override
			{
				uint32 index = 0;
				DXGI_ADAPTER_DESC desc;
				D3D11_Device * device = 0;
				while (true) {
					if (_dxgi_factory1) {
						IDXGIAdapter1 * adapter;
						if (_dxgi_factory1->EnumAdapters1(index, &adapter) != S_OK) return 0;
						adapter->GetDesc(&desc);
						if (reinterpret_cast<uint64 &>(desc.AdapterLuid) == identifier) {
							device = _internal_create_device(adapter, D3D_DRIVER_TYPE_UNKNOWN);
							adapter->Release();
							break;
						}
						adapter->Release();
					} else {
						IDXGIAdapter * adapter;
						if (_dxgi_factory->EnumAdapters(index, &adapter) != S_OK) return 0;
						adapter->GetDesc(&desc);
						if (reinterpret_cast<uint64 &>(desc.AdapterLuid) == identifier) {
							device = _internal_create_device(adapter, D3D_DRIVER_TYPE_UNKNOWN);
							adapter->Release();
							break;
						}
						adapter->Release();
					}
					index++;
				}
				return device;
			}
			virtual IDevice * CreateDefaultDevice(void) noexcept override
			{
				auto device = _internal_create_device(0, D3D_DRIVER_TYPE_HARDWARE);
				if (!device) device = _internal_create_device(0, D3D_DRIVER_TYPE_WARP);
				if (!device) return 0;
				return device;
			}
			virtual IDeviceResourceHandle * QueryResourceHandle(IDeviceResource * resource) noexcept override
			{
				if (!resource || resource->GetResourceType() != ResourceType::Texture) return 0;
				auto handle = static_cast<D3D11_Texture *>(resource)->shared_handle.Inner();
				if (handle) handle->Retain();
				return handle;
			}
			virtual IDeviceResourceHandle * OpenResourceHandle(const DataBlock * data) noexcept override { try { return new D3D11_DeviceResourceHandle(data); } catch (...) { return 0; } }
			virtual string ToString(void) const override { return L"D3D11_DeviceFactory"; }
		};

		void CreateCommonDeviceFactory(void) noexcept { if (CommonFactory) return; try { CommonFactory = new D3D11_DeviceFactory; } catch (...) {} }
		void CreateCommonDevice(void) noexcept { if (CommonDevice) return; if (!CommonFactory) CreateCommonDeviceFactory(); if (CommonFactory) CommonDevice = CommonFactory->CreateDefaultDevice(); }

		IDXGIFactory * GetInnerObject(Graphics::IDeviceFactory * factory) noexcept { return static_cast<D3D11_DeviceFactory *>(factory)->_dxgi_factory; }
		IDXGIFactory1 * GetInnerObject1(Graphics::IDeviceFactory * factory) noexcept { return static_cast<D3D11_DeviceFactory *>(factory)->_dxgi_factory1; }
		ID3D11Device * GetInnerObject(Graphics::IDevice * device) noexcept { return static_cast<D3D11_Device *>(device)->GetWrappedDevice(); }
		ID2D1Device * GetInnerObject2D(Graphics::IDevice * device) noexcept
		{
			auto context = static_cast<D3D11_DeviceContext *>(device->GetDeviceContext());
			if (!context->GetDeviceD2D1()) context->TryAllocateDeviceD2D1();
			auto device_d2d1 = context->GetDeviceD2D1();
			return device_d2d1;
		}
		ID3D11Resource * GetInnerObject(Graphics::IDeviceResource * resource) noexcept
		{
			if (resource->GetResourceType() == ResourceType::Texture) {
				auto object = static_cast<D3D11_Texture *>(resource);
				if (object->tex_1d) return object->tex_1d;
				else if (object->tex_2d) return object->tex_2d;
				else if (object->tex_3d) return object->tex_3d;
				else return 0;
			} else if (resource->GetResourceType() == ResourceType::Buffer) {
				auto object = static_cast<D3D11_Buffer *>(resource);
				return object->buffer;
			} else return 0;
		}
		ID3D11Texture2D * GetInnerObject2D(Graphics::ITexture * texture) noexcept { return static_cast<D3D11_Texture *>(texture)->tex_2d; }

		DXGI_FORMAT MakeDxgiFormat(Graphics::PixelFormat format) noexcept
		{
			if (IsColorFormat(format)) {
				auto bpp = GetFormatBitsPerPixel(format);
				if (bpp == 8) {
					if (format == PixelFormat::A8_unorm) return DXGI_FORMAT_A8_UNORM;
					else if (format == PixelFormat::R8_unorm) return DXGI_FORMAT_R8_UNORM;
					else if (format == PixelFormat::R8_snorm) return DXGI_FORMAT_R8_SNORM;
					else if (format == PixelFormat::R8_uint) return DXGI_FORMAT_R8_UINT;
					else if (format == PixelFormat::R8_sint) return DXGI_FORMAT_R8_SINT;
					else return DXGI_FORMAT_UNKNOWN;
				} else if (bpp == 16) {
					if (format == PixelFormat::R16_unorm) return DXGI_FORMAT_R16_UNORM;
					else if (format == PixelFormat::R16_snorm) return DXGI_FORMAT_R16_SNORM;
					else if (format == PixelFormat::R16_uint) return DXGI_FORMAT_R16_UINT;
					else if (format == PixelFormat::R16_sint) return DXGI_FORMAT_R16_SINT;
					else if (format == PixelFormat::R16_float) return DXGI_FORMAT_R16_FLOAT;
					else if (format == PixelFormat::R8G8_unorm) return DXGI_FORMAT_R8G8_UNORM;
					else if (format == PixelFormat::R8G8_snorm) return DXGI_FORMAT_R8G8_SNORM;
					else if (format == PixelFormat::R8G8_uint) return DXGI_FORMAT_R8G8_UINT;
					else if (format == PixelFormat::R8G8_sint) return DXGI_FORMAT_R8G8_SINT;
					else if (format == PixelFormat::B5G6R5_unorm) return DXGI_FORMAT_B5G6R5_UNORM;
					else if (format == PixelFormat::B5G5R5A1_unorm) return DXGI_FORMAT_B5G5R5A1_UNORM;
					else if (format == PixelFormat::B4G4R4A4_unorm) return DXGI_FORMAT_B4G4R4A4_UNORM;
					else return DXGI_FORMAT_UNKNOWN;
				} else if (bpp == 32) {
					if (format == PixelFormat::R32_uint) return DXGI_FORMAT_R32_UINT;
					else if (format == PixelFormat::R32_sint) return DXGI_FORMAT_R32_SINT;
					else if (format == PixelFormat::R32_float) return DXGI_FORMAT_R32_FLOAT;
					else if (format == PixelFormat::R16G16_unorm) return DXGI_FORMAT_R16G16_UNORM;
					else if (format == PixelFormat::R16G16_snorm) return DXGI_FORMAT_R16G16_SNORM;
					else if (format == PixelFormat::R16G16_uint) return DXGI_FORMAT_R16G16_UINT;
					else if (format == PixelFormat::R16G16_sint) return DXGI_FORMAT_R16G16_SINT;
					else if (format == PixelFormat::R16G16_float) return DXGI_FORMAT_R16G16_FLOAT;
					else if (format == PixelFormat::B8G8R8A8_unorm) return DXGI_FORMAT_B8G8R8A8_UNORM;
					else if (format == PixelFormat::R8G8B8A8_unorm) return DXGI_FORMAT_R8G8B8A8_UNORM;
					else if (format == PixelFormat::R8G8B8A8_snorm) return DXGI_FORMAT_R8G8B8A8_SNORM;
					else if (format == PixelFormat::R8G8B8A8_uint) return DXGI_FORMAT_R8G8B8A8_UINT;
					else if (format == PixelFormat::R8G8B8A8_sint) return DXGI_FORMAT_R8G8B8A8_SINT;
					else if (format == PixelFormat::R10G10B10A2_unorm) return DXGI_FORMAT_R10G10B10A2_UNORM;
					else if (format == PixelFormat::R10G10B10A2_uint) return DXGI_FORMAT_R10G10B10A2_UINT;
					else if (format == PixelFormat::R11G11B10_float) return DXGI_FORMAT_R11G11B10_FLOAT;
					else if (format == PixelFormat::R9G9B9E5_float) return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
					else return DXGI_FORMAT_UNKNOWN;
				} else if (bpp == 64) {
					if (format == PixelFormat::R32G32_uint) return DXGI_FORMAT_R32G32_UINT;
					else if (format == PixelFormat::R32G32_sint) return DXGI_FORMAT_R32G32_SINT;
					else if (format == PixelFormat::R32G32_float) return DXGI_FORMAT_R32G32_FLOAT;
					else if (format == PixelFormat::R16G16B16A16_unorm) return DXGI_FORMAT_R16G16B16A16_UNORM;
					else if (format == PixelFormat::R16G16B16A16_snorm) return DXGI_FORMAT_R16G16B16A16_SNORM;
					else if (format == PixelFormat::R16G16B16A16_uint) return DXGI_FORMAT_R16G16B16A16_UINT;
					else if (format == PixelFormat::R16G16B16A16_sint) return DXGI_FORMAT_R16G16B16A16_SINT;
					else if (format == PixelFormat::R16G16B16A16_float) return DXGI_FORMAT_R16G16B16A16_FLOAT;
					else return DXGI_FORMAT_UNKNOWN;
				} else if (bpp == 128) {
					if (format == PixelFormat::R32G32B32A32_uint) return DXGI_FORMAT_R32G32B32A32_UINT;
					else if (format == PixelFormat::R32G32B32A32_sint) return DXGI_FORMAT_R32G32B32A32_SINT;
					else if (format == PixelFormat::R32G32B32A32_float) return DXGI_FORMAT_R32G32B32A32_FLOAT;
					else return DXGI_FORMAT_UNKNOWN;
				} else return DXGI_FORMAT_UNKNOWN;
			} else if (IsDepthStencilFormat(format)) {
				if (format == PixelFormat::D16_unorm) return DXGI_FORMAT_D16_UNORM;
				else if (format == PixelFormat::D32_float) return DXGI_FORMAT_D32_FLOAT;
				else if (format == PixelFormat::D24S8_unorm) return DXGI_FORMAT_D24_UNORM_S8_UINT;
				else if (format == PixelFormat::D32S8_float) return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
				else return DXGI_FORMAT_UNKNOWN;
			} else return DXGI_FORMAT_UNKNOWN;
		}
		IUnknown * GetVideoAccelerationDevice(Graphics::IDevice * device) noexcept { return static_cast<D3D11_Device *>(device)->GetVideoAcceleration(); }
		void SetVideoAccelerationDevice(Graphics::IDevice * device_for, IUnknown * device_set) noexcept { static_cast<D3D11_Device *>(device_for)->SetVideoAcceleration(device_set); }
		IDXGIDevice * QueryDXGIDevice(Graphics::IDevice * device) noexcept
		{
			auto dev = static_cast<D3D11_Device *>(device)->GetWrappedDevice();
			IDXGIDevice * dxgi_device;
			if (dev->QueryInterface(IID_PPV_ARGS(&dxgi_device)) != S_OK) return 0;
			return dxgi_device;
		}
	}
}