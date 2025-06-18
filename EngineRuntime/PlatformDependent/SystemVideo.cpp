#include "../Interfaces/SystemVideo.h"

#include "../Graphics/GraphicsHelper.h"
#include "../Interfaces/Socket.h"
#include "../PlatformDependent/Direct3D.h"
#include "../Miscellaneous/DynamicString.h"

#include <Windows.h>
#include <dxgi1_2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <wmcodecdsp.h>
#include <Mferror.h>
#include <Codecapi.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

#undef CreateSemaphore

namespace Engine
{
	namespace Windows { HMONITOR _get_monitor_handle(IScreen * screen); }
	namespace Video
	{
		ENGINE_PACKED_STRUCTURE(h264_header)
			uint8 version;
			uint8 avc_profile;
			uint8 avc_compatibility;
			uint8 avc_level;
			uint8 length_field_size;
			uint8 data[0];
		ENGINE_END_PACKED_STRUCTURE

		typedef HRESULT (* WINAPI func_MFCreateDXGIDeviceManager) (UINT * reset_token, IMFDXGIDeviceManager ** result);
		typedef HRESULT (* WINAPI func_MFCreateDXGISurfaceBuffer) (REFIID riid, IUnknown * surface, UINT subres, BOOL bottom_up, IMFMediaBuffer ** result);

		func_MFCreateDXGIDeviceManager CreateDXGIDeviceManager = 0;
		func_MFCreateDXGISurfaceBuffer CreateDXGISurfaceBuffer = 0;

		bool EnableVideoSurfaceInterop(void)
		{
			if (!CreateDXGISurfaceBuffer) {
				HMODULE mfplat_lib = LoadLibraryW(L"mfplat.dll");
				if (!mfplat_lib) return 0;
				CreateDXGISurfaceBuffer = reinterpret_cast<func_MFCreateDXGISurfaceBuffer>(GetProcAddress(mfplat_lib, "MFCreateDXGISurfaceBuffer"));
				FreeLibrary(mfplat_lib);
			}
			return CreateDXGISurfaceBuffer != 0;
		}
		IMFDXGIDeviceManager * EnableVideoAcceleration(Graphics::IDevice * device)
		{
			IMFDXGIDeviceManager * manager;
			IUnknown * mref = Direct3D::GetVideoAccelerationDevice(device);
			if (!mref) {
				if (!CreateDXGIDeviceManager) {
					HMODULE mfplat_lib = LoadLibraryW(L"mfplat.dll");
					if (!mfplat_lib) return 0;
					CreateDXGIDeviceManager = reinterpret_cast<func_MFCreateDXGIDeviceManager>(GetProcAddress(mfplat_lib, "MFCreateDXGIDeviceManager"));
					FreeLibrary(mfplat_lib);
				}
				if (!CreateDXGIDeviceManager) return 0;
				UINT token;
				auto dxgi_device = Direct3D::QueryDXGIDevice(device);
				if (!dxgi_device) return 0;
				if (CreateDXGIDeviceManager(&token, &manager) != S_OK) { dxgi_device->Release(); return 0; }
				if (manager->ResetDevice(dxgi_device, token) != S_OK) { manager->Release(); dxgi_device->Release(); return 0; }
				dxgi_device->Release();
				auto d3d11_device = Direct3D::GetInnerObject(device);
				ID3D10Multithread * multithreaded;
				if (d3d11_device->QueryInterface(IID_PPV_ARGS(&multithreaded)) != S_OK) { manager->Release(); return 0; }
				multithreaded->SetMultithreadProtected(TRUE);
				multithreaded->Release();
				Direct3D::SetVideoAccelerationDevice(device, manager);
				manager->Release();
			} else manager = static_cast<IMFDXGIDeviceManager *>(mref);
			return manager;
		}
		class MediaFoundationFrame : public IVideoFrame
		{
			VideoObjectDesc _desc;
			IMFMediaType * _type;
			IMFSample * _sample;
		public:
			MediaFoundationFrame(IMFMediaType * format, IMFSample * sample, Graphics::IDevice * device = 0)
			{
				_type = format;
				_sample = sample;
				UINT w, h;
				if (MFGetAttributeSize(_type, MF_MT_FRAME_SIZE, &w, &h) != S_OK) throw Exception();
				_desc.Width = w;
				_desc.Height = h;
				_desc.FramePresentation = 0;
				_desc.FrameDuration = 1;
				_desc.TimeScale = 1;
				_desc.Device = device;
				if (MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET) != S_OK) throw Exception();
				_type->AddRef();
				_sample->AddRef();
				if (_desc.Device) _desc.Device->Retain();
			}
			virtual ~MediaFoundationFrame(void) override
			{
				if (_type) _type->Release();
				if (_sample) _sample->Release();
				if (_desc.Device) _desc.Device->Release();
				MFShutdown();
			}
			virtual const VideoObjectDesc & GetObjectDescriptor(void) const noexcept override { return _desc; }
			virtual VideoObjectType GetObjectType(void) const noexcept override { return VideoObjectType::Frame; }
			virtual handle GetBufferFormat(void) const noexcept override { return _type; }
			virtual void SetFramePresentation(uint presentation) noexcept override { _desc.FramePresentation = presentation; }
			virtual void SetFrameDuration(uint duration) noexcept override { _desc.FrameDuration = duration; }
			virtual void SetTimeScale(uint scale) noexcept override { _desc.TimeScale = scale; }
			virtual Codec::Frame * QueryFrame(void) const noexcept override
			{
				IMFTransform * transform;
				if (CoCreateInstance(CLSID_VideoProcessorMFT, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&transform)) != S_OK) return 0;
				IMFAttributes * attr;
				if (transform->GetAttributes(&attr) != S_OK) { transform->Release(); return 0; }
				if (attr->SetUINT32(MF_XVP_DISABLE_FRC, TRUE) != S_OK) { attr->Release(); transform->Release(); return 0; }
				attr->Release();
				if (transform->SetInputType(0, _type, 0) != S_OK) { transform->Release(); return 0; }
				IMFMediaType * out;
				if (MFCreateMediaType(&out) != S_OK) { transform->Release(); return 0; }
				if (out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video) != S_OK) { transform->Release(); out->Release(); return 0; }
				if (out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32) != S_OK) { transform->Release(); out->Release(); return 0; }
				if (out->SetUINT32(MF_MT_ALPHA_MODE, DXGI_ALPHA_MODE_STRAIGHT) != S_OK) { transform->Release(); out->Release(); return 0; }
				if (out->SetUINT32(MF_MT_DEFAULT_STRIDE, _desc.Width * 4) != S_OK) { transform->Release(); out->Release(); return 0; }
				if (MFSetAttributeSize(out, MF_MT_FRAME_SIZE, _desc.Width, _desc.Height) != S_OK) { transform->Release(); out->Release(); return 0; }
				if (transform->SetOutputType(0, out, 0) != S_OK) { transform->Release(); out->Release(); return 0; }
				out->Release();
				if (transform->ProcessInput(0, _sample, 0) != S_OK) { transform->Release(); return 0; }
				MFT_OUTPUT_STREAM_INFO info;
				if (transform->GetOutputStreamInfo(0, &info) != S_OK) { transform->Release(); return 0; }
				IMFSample * sample;
				IMFMediaBuffer * buffer;
				if (MFCreateMemoryBuffer(info.cbSize, &buffer) != S_OK) { transform->Release(); return 0; }
				if (MFCreateSample(&sample) != S_OK) { buffer->Release(); transform->Release(); return 0; }
				if (sample->AddBuffer(buffer) != S_OK) { buffer->Release(); sample->Release(); transform->Release(); return 0; }
				MFT_OUTPUT_DATA_BUFFER output;
				DWORD status;
				ZeroMemory(&output, sizeof(output));
				output.pSample = sample;
				if (transform->ProcessOutput(0, 1, &output, &status) != S_OK) { buffer->Release(); sample->Release(); transform->Release(); return 0; }
				if (output.pEvents) output.pEvents->Release();
				transform->Release();
				sample->Release();
				SafePointer<Codec::Frame> result;
				try {
					result = new Codec::Frame(_desc.Width, _desc.Height, _desc.Width * 4, Codec::PixelFormat::B8G8R8A8, Codec::AlphaMode::Straight, Codec::ScanOrigin::TopDown);
					LPBYTE data;
					if (buffer->Lock(&data, 0, 0) != S_OK) throw Exception();
					MemoryCopy(result->GetData(), data, _desc.Width * _desc.Height * 4);
					buffer->Unlock();
				} catch (...) { buffer->Release(); return 0; }
				buffer->Release();
				result->Retain();
				return result;
			}
			IMFSample * GetSample(void) const noexcept { return _sample; }
			IMFMediaType * GetFormat(void) const noexcept { return _type; }
			void SetSample(IMFSample * sample) noexcept
			{
				if (_sample) _sample->Release();
				_sample = sample;
				if (_sample) _sample->AddRef();
			}
			void SetFormat(IMFMediaType * format) noexcept
			{
				if (_type) _type->Release();
				_type = format;
				if (_type) _type->AddRef();
			}
		};
		class MediaFoundationFrameBlt : public IVideoFrameBlt
		{
			IMFMediaType * _input;
			IMFMediaType * _output;
			IMFTransform * _transform;
			IMFSample * _cached_sample;
			ID3D11Texture2D * _transition_surface;
			SafePointer<Graphics::IDevice> _input_device, _output_device;
			Graphics::PixelFormat _input_gf, _output_gf;
			bool _dxva;

			bool _init_transform(void) noexcept
			{
				_dxva = false;
				if (CoCreateInstance(CLSID_VideoProcessorMFT, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_transform)) != S_OK) return false;
				IMFAttributes * attr;
				if (_transform->GetAttributes(&attr) != S_OK) { _transform->Release(); _transform = 0; return false; }
				try {
					if (attr->SetUINT32(MF_XVP_DISABLE_FRC, TRUE) != S_OK) throw Exception();
					if (_input_device.Inner() && _input_device.Inner() == _output_device.Inner()) {
						UINT32 d3d_aware;
						if (attr->GetUINT32(MF_SA_D3D11_AWARE, &d3d_aware) != S_OK) d3d_aware = false;
						if (d3d_aware) {
							IMFDXGIDeviceManager * manager = EnableVideoAcceleration(_input_device);
							if (manager) {
								if (_transform->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(manager)) == S_OK) _dxva = true;
							}
						}
					}
				} catch (...) { attr->Release(); _transform->Release(); _transform = 0; return false; }
				attr->Release();
				if (_transform->SetInputType(0, _input, 0) != S_OK) { _transform->Release(); _transform = 0; return false; }
				if (_transform->SetOutputType(0, _output, 0) != S_OK) { _transform->Release(); _transform = 0; return false; }
				return true;
			}
			static bool _get_surface(IMFSample * sample, ID3D11Texture2D ** texture, UINT * subres) noexcept
			{
				IMFMediaBuffer * buffer;
				if (sample->GetBufferByIndex(0, &buffer) != S_OK) return false;
				IMFDXGIBuffer * dxgi;
				if (buffer->QueryInterface(IID_PPV_ARGS(&dxgi)) != S_OK) { buffer->Release(); return false; }
				buffer->Release();
				if (dxgi->GetSubresourceIndex(subres) != S_OK) { dxgi->Release(); return false; }
				if (dxgi->GetResource(IID_PPV_ARGS(texture)) != S_OK) { dxgi->Release(); return false; }
				dxgi->Release();
				return true;
			}
		public:
			MediaFoundationFrameBlt(void) : _input(0), _output(0), _transform(0), _cached_sample(0), _transition_surface(0), _dxva(false)
			{
				if (MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET) != S_OK) throw Exception();
				_input_gf = _output_gf = Graphics::PixelFormat::Invalid;
			}
			virtual ~MediaFoundationFrameBlt(void) override
			{
				if (_input) _input->Release();
				if (_output) _output->Release();
				if (_transform) _transform->Release();
				if (_cached_sample) _cached_sample->Release();
				if (_transition_surface) _transition_surface->Release();
				MFShutdown();
			}
			static IMFMediaType * ProduceFormat(Graphics::PixelFormat format, Codec::AlphaMode alpha, const VideoObjectDesc & desc) noexcept
			{
				if (format == Graphics::PixelFormat::B8G8R8A8_unorm) {
					IMFMediaType * result;
					if (MFCreateMediaType(&result) != S_OK) return 0;
					if (result->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video) != S_OK) { result->Release(); return 0; }
					if (result->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32) != S_OK) { result->Release(); return 0; }
					if (MFSetAttributeSize(result, MF_MT_FRAME_SIZE, desc.Width, desc.Height) != S_OK) { result->Release(); return 0; }
					if (result->SetUINT32(MF_MT_DEFAULT_STRIDE, desc.Width * 4) != S_OK) { result->Release(); return 0; }
					if (alpha == Codec::AlphaMode::Straight) {
						if (result->SetUINT32(MF_MT_ALPHA_MODE, DXGI_ALPHA_MODE_STRAIGHT) != S_OK) { result->Release(); return 0; }
					} else {
						if (result->SetUINT32(MF_MT_ALPHA_MODE, DXGI_ALPHA_MODE_PREMULTIPLIED) != S_OK) { result->Release(); return 0; }
					}
					return result;
				} else return 0;
			}
			static IMFSample * MakeDXGISample(Graphics::ITexture * texture, Graphics::SubresourceIndex subres) noexcept
			{
				if (!EnableVideoSurfaceInterop() || !CreateDXGISurfaceBuffer) return 0;
				IMFMediaBuffer * buffer;
				UINT sr = D3D11CalcSubresource(subres.mip_level, subres.array_index, texture->GetMipmapCount());
				if (CreateDXGISurfaceBuffer(IID_ID3D11Texture2D, Direct3D::GetInnerObject2D(texture), sr, TRUE, &buffer) != S_OK) return 0;
				IMFSample * sample;
				if (MFCreateSample(&sample) != S_OK) { buffer->Release(); return 0; }
				if (sample->AddBuffer(buffer) != S_OK) { buffer->Release(); sample->Release(); return 0; }
				buffer->Release();
				sample->SetSampleTime(0);
				sample->SetSampleDuration(1);
				return sample;
			}
			bool SetInputFormat(IMFMediaType * format, Graphics::IDevice * device) noexcept
			{
				if (_input) return false;
				_input = format;
				_input->AddRef();
				_input_device.SetRetain(device);
				if (_output && !_init_transform()) return false;
				return true;
			}
			bool SetOutputFormat(IMFMediaType * format, Graphics::IDevice * device) noexcept
			{
				if (_output) return false;
				_output = reinterpret_cast<IMFMediaType *>(format);
				_output->AddRef();
				_output_device.SetRetain(device);
				if (_input && !_init_transform()) return false;
				return true;
			}
			virtual bool SetInputFormat(const IVideoObject * format_provider) noexcept override
			{
				if (_input || !format_provider || !format_provider->GetBufferFormat()) return false;
				_input = reinterpret_cast<IMFMediaType *>(format_provider->GetBufferFormat());
				_input->AddRef();
				_input_device.SetRetain(format_provider->GetObjectDescriptor().Device);
				if (_output && !_init_transform()) return false;
				return true;
			}
			virtual bool SetInputFormat(Graphics::PixelFormat format, Codec::AlphaMode alpha, const VideoObjectDesc & desc) noexcept override
			{
				if (_input) return false;
				_input = ProduceFormat(format, alpha, desc);
				if (!_input) return false;
				_input_device.SetRetain(desc.Device);
				if (_output && !_init_transform()) return false;
				_input_gf = format;
				return true;
			}
			virtual bool SetOutputFormat(const IVideoObject * format_provider) noexcept override
			{
				if (_output || !format_provider || !format_provider->GetBufferFormat()) return false;
				_output = reinterpret_cast<IMFMediaType *>(format_provider->GetBufferFormat());
				_output->AddRef();
				_output_device.SetRetain(format_provider->GetObjectDescriptor().Device);
				if (_input && !_init_transform()) return false;
				return true;
			}
			virtual bool SetOutputFormat(Graphics::PixelFormat format, Codec::AlphaMode alpha, const VideoObjectDesc & desc) noexcept override
			{
				if (_output) return false;
				_output = ProduceFormat(format, alpha, desc);
				if (!_output) return false;
				_output_device.SetRetain(desc.Device);
				if (_input && !_init_transform()) return false;
				_output_gf = format;
				return true;
			}
			virtual bool Reset(void) noexcept override
			{
				if (_input) _input->Release();
				if (_output) _output->Release();
				if (_transform) _transform->Release();
				_input = 0;
				_output = 0;
				_transform = 0;
				_input_device.SetReference(0);
				_output_device.SetReference(0);
				_input_gf = Graphics::PixelFormat::Invalid;
				_output_gf = Graphics::PixelFormat::Invalid;
				return true;
			}
			virtual bool IsInitialized(void) const noexcept override { return _input && _output && _transform; }
			virtual bool Process(IVideoFrame * dest, Graphics::ITexture * from, Graphics::SubresourceIndex subres) noexcept override
			{
				if (!IsInitialized()) {
					VideoObjectDesc desc;
					desc.Width = from->GetWidth();
					desc.Height = from->GetHeight();
					desc.FrameDuration = 1;
					desc.FramePresentation = 0;
					desc.TimeScale = 0;
					desc.Device = from->GetParentDevice();
					if (!_output && !SetOutputFormat(dest)) return false;
					if (!_input && !SetInputFormat(from->GetPixelFormat(), Codec::AlphaMode::Straight, desc)) return false;
					if (!IsInitialized()) return false;
				}
				if (subres.mip_level) return false;
				if (from->GetTextureType() != Graphics::TextureType::Type2D && from->GetTextureType() != Graphics::TextureType::TypeArray2D) return false;
				if (from->GetPixelFormat() != _input_gf) return false;
				if (!_transition_surface) {
					auto sample = MakeDXGISample(from, subres);
					if (sample) {
						if (_transform->ProcessInput(0, sample, 0) != S_OK) { sample->Release(); return false; }
						sample->Release();
					} else {
						auto device = Direct3D::GetInnerObject(from->GetParentDevice());
						D3D11_TEXTURE2D_DESC desc2d;
						desc2d.Width = from->GetWidth();
						desc2d.Height = from->GetHeight();
						desc2d.MipLevels = 1;
						desc2d.ArraySize = 1;
						desc2d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
						desc2d.SampleDesc.Count = 1;
						desc2d.SampleDesc.Quality = 0;
						desc2d.Usage = D3D11_USAGE_STAGING;
						desc2d.BindFlags = 0;
						desc2d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
						desc2d.MiscFlags = 0;
						if (device->CreateTexture2D(&desc2d, 0, &_transition_surface) != S_OK) return false;
					}
				}
				if (_transition_surface) {
					ID3D11DeviceContext * context;
					D3D11_BOX box;
					box.left = box.top = box.front = 0;
					box.right = from->GetWidth();
					box.bottom = from->GetHeight();
					box.back = 1;
					Direct3D::GetInnerObject(from->GetParentDevice())->GetImmediateContext(&context);
					context->CopySubresourceRegion(_transition_surface, 0, 0, 0, 0, Direct3D::GetInnerObject2D(from),
						D3D11CalcSubresource(subres.mip_level, subres.array_index, from->GetMipmapCount()), &box);
					if (!_cached_sample) {
						auto length = box.right * box.bottom * 4;
						IMFMediaBuffer * buffer;
						if (MFCreateMemoryBuffer(length, &buffer) != S_OK) return false;
						if (buffer->SetCurrentLength(length) != S_OK) { buffer->Release(); return false; }
						if (MFCreateSample(&_cached_sample) != S_OK) { buffer->Release(); return false; }
						if (_cached_sample->AddBuffer(buffer) != S_OK) { buffer->Release(); _cached_sample->Release(); _cached_sample = 0; return false; }
						buffer->Release();
						_cached_sample->SetSampleTime(0);
						_cached_sample->SetSampleDuration(1);
					}
					uint8 * blt_dest;
					D3D11_MAPPED_SUBRESOURCE map;
					IMFMediaBuffer * buffer;
					if (_cached_sample->GetBufferByIndex(0, &buffer) != S_OK) return false;
					if (buffer->Lock(&blt_dest, 0, 0) != S_OK) { buffer->Release(); return false; }
					if (context->Map(_transition_surface, 0, D3D11_MAP_READ, 0, &map) != S_OK) { buffer->Unlock(); buffer->Release(); return false; }
					const uint8 * blt_src = reinterpret_cast<const uint8 *>(map.pData);
					intptr dest_stride = intptr(box.right) * 4;
					intptr src_stride = map.RowPitch;
					for (int y = 0; y < box.bottom; y++) MemoryCopy(blt_dest + y * dest_stride, blt_src + y * src_stride, dest_stride);
					context->Unmap(_transition_surface, 0);
					buffer->Unlock();
					buffer->Release();
					if (_transform->ProcessInput(0, _cached_sample, 0) != S_OK) return false;
				}
				if (_dxva) {
					MFT_OUTPUT_DATA_BUFFER output;
					DWORD status;
					ZeroMemory(&output, sizeof(output));
					if (_transform->ProcessOutput(0, 1, &output, &status) != S_OK) return false;
					if (output.pEvents) output.pEvents->Release();
					ID3D11Texture2D * tex_dest, * tex_src;
					UINT dest_subres, src_subres;
					if (!_get_surface(output.pSample, &tex_src, &src_subres)) { output.pSample->Release(); return false; }
					if (!_get_surface(static_cast<MediaFoundationFrame *>(dest)->GetSample(), &tex_dest, &dest_subres)) { output.pSample->Release(); tex_src->Release(); return false; }
					ID3D11Device * device;
					ID3D11DeviceContext * context;
					tex_src->GetDevice(&device);
					device->GetImmediateContext(&context);
					context->CopySubresourceRegion(tex_dest, dest_subres, 0, 0, 0, tex_src, src_subres, 0);
					context->Release();
					device->Release();
					tex_src->Release();
					tex_dest->Release();
					output.pSample->Release();
					return true;
				} else {
					auto sample = static_cast<MediaFoundationFrame *>(dest)->GetSample();
					IMFMediaBuffer * buffer;
					if (sample->GetBufferByIndex(0, &buffer) != S_OK) return false;
					if (buffer->SetCurrentLength(0) != S_OK) { buffer->Release(); return false; }
					buffer->Release();
					MFT_OUTPUT_DATA_BUFFER output;
					DWORD status;
					ZeroMemory(&output, sizeof(output));
					output.pSample = sample;
					if (_transform->ProcessOutput(0, 1, &output, &status) != S_OK) return false;
					return true;
				}
			}
			virtual bool Process(Graphics::ITexture * dest, const IVideoFrame * from, Graphics::SubresourceIndex subres) noexcept override
			{
				if (!IsInitialized()) {
					VideoObjectDesc desc;
					desc.Width = dest->GetWidth();
					desc.Height = dest->GetHeight();
					desc.FrameDuration = 1;
					desc.FramePresentation = 0;
					desc.TimeScale = 0;
					desc.Device = dest->GetParentDevice();
					if (!_output && !SetOutputFormat(dest->GetPixelFormat(), Codec::AlphaMode::Straight, desc)) return false;
					if (!_input && !SetInputFormat(from)) return false;
					if (!IsInitialized()) return false;
				}
				if (subres.mip_level) return false;
				if (dest->GetTextureType() != Graphics::TextureType::Type2D && dest->GetTextureType() != Graphics::TextureType::TypeArray2D) return false;
				if (dest->GetPixelFormat() != _output_gf) return false;
				auto sample = static_cast<const MediaFoundationFrame *>(from)->GetSample();
				if (_transform->ProcessInput(0, sample, 0) != S_OK) return false;
				if (_dxva) {
					MFT_OUTPUT_DATA_BUFFER output;
					DWORD status;
					ZeroMemory(&output, sizeof(output));
					if (_transform->ProcessOutput(0, 1, &output, &status) != S_OK) return false;
					if (output.pEvents) output.pEvents->Release();
					ID3D11Texture2D * surface;
					UINT src_subres;
					UINT dest_subres = D3D11CalcSubresource(subres.mip_level, subres.array_index, dest->GetMipmapCount());
					if (!_get_surface(output.pSample, &surface, &src_subres)) { output.pSample->Release(); return false; }
					auto device = Direct3D::GetInnerObject(_output_device);
					ID3D11DeviceContext * context;
					device->GetImmediateContext(&context);
					context->CopySubresourceRegion(Direct3D::GetInnerObject2D(dest), dest_subres, 0, 0, 0, surface, src_subres, 0);
					context->Release();
					surface->Release();
					output.pSample->Release();
					return true;
				} else {
					MFT_OUTPUT_DATA_BUFFER output;
					DWORD status;
					ZeroMemory(&output, sizeof(output));
					if (!_cached_sample) {
						IMFMediaBuffer * buffer;
						MFT_OUTPUT_STREAM_INFO info;
						if (_transform->GetOutputStreamInfo(0, &info) != S_OK) return false;
						if (MFCreateMemoryBuffer(info.cbSize, &buffer) != S_OK) return false;
						if (MFCreateSample(&_cached_sample) != S_OK) { buffer->Release(); return false; }
						if (_cached_sample->AddBuffer(buffer) != S_OK) { buffer->Release(); _cached_sample->Release(); _cached_sample = 0; return false; }
						buffer->Release();
					} else {
						IMFMediaBuffer * buffer;
						if (_cached_sample->GetBufferByIndex(0, &buffer) != S_OK) return false;
						if (buffer->SetCurrentLength(0) != S_OK) { buffer->Release(); return false; }
						buffer->Release();
					}
					output.pSample = _cached_sample;
					if (_transform->ProcessOutput(0, 1, &output, &status) != S_OK) { output.pSample->Release(); return false; }
					if (output.pEvents) output.pEvents->Release();
					auto device = Direct3D::GetInnerObject(dest->GetParentDevice());
					IMFMediaBuffer * buffer;
					if (_cached_sample->GetBufferByIndex(0, &buffer) != S_OK) return false;
					uint8 * pdata_src;
					if (buffer->Lock(&pdata_src, 0, 0) != S_OK) { buffer->Release(); return false; }
					ID3D11DeviceContext * context;
					device->GetImmediateContext(&context);
					D3D11_BOX box;
					box.left = box.top = box.front = 0;
					box.right = dest->GetWidth();
					box.bottom = dest->GetHeight();
					box.back = 1;
					UINT sr = D3D11CalcSubresource(subres.mip_level, subres.array_index, dest->GetMipmapCount());
					context->UpdateSubresource(Direct3D::GetInnerObject2D(dest), sr, &box, pdata_src, box.right * 4, 0);
					buffer->Unlock();
					buffer->Release();
					return true;
				}
			}
			IMFSample * Process(const IVideoFrame * frame) noexcept
			{
				auto input = static_cast<const MediaFoundationFrame *>(frame)->GetSample();
				if (_transform->ProcessInput(0, input, 0) != S_OK) return 0;
				if (_dxva) {
					MFT_OUTPUT_DATA_BUFFER output;
					DWORD status;
					ZeroMemory(&output, sizeof(output));
					if (_transform->ProcessOutput(0, 1, &output, &status) != S_OK) return 0;
					if (output.pEvents) output.pEvents->Release();
					return output.pSample;
				} else {
					if (!_cached_sample) {
						IMFMediaBuffer * buffer;
						MFT_OUTPUT_STREAM_INFO info;
						if (_transform->GetOutputStreamInfo(0, &info) != S_OK) return 0;
						if (MFCreateMemoryBuffer(info.cbSize, &buffer) != S_OK) return 0;
						if (MFCreateSample(&_cached_sample) != S_OK) { buffer->Release(); return 0; }
						if (_cached_sample->AddBuffer(buffer) != S_OK) { buffer->Release(); _cached_sample->Release(); _cached_sample = 0; return 0; }
						buffer->Release();
					} else {
						IMFMediaBuffer * buffer;
						if (_cached_sample->GetBufferByIndex(0, &buffer) != S_OK) return 0;
						if (buffer->SetCurrentLength(0) != S_OK) { buffer->Release(); return 0; }
						buffer->Release();
					}
					MFT_OUTPUT_DATA_BUFFER output;
					DWORD status;
					ZeroMemory(&output, sizeof(output));
					output.pSample = _cached_sample;
					if (_transform->ProcessOutput(0, 1, &output, &status) != S_OK) return 0;
					if (output.pEvents) output.pEvents->Release();
					_cached_sample->AddRef();
					return _cached_sample;
				}
			}
		};
		
		class MediaFoundationDecoder : public IVideoDecoder
		{
			struct h264_timestamp {
				uint64 present_at;
				uint64 duration;
			};

			SafePointer<IVideoCodec> _parent;
			string _format;
			VideoObjectDesc _desc;
			IMFTransform * _transform;
			IMFMediaType * _output_type;
			SafePointer<DataBlock> _codec_data;
			SafePointer<DataBlock> _h264_nalu_prefix;
			uint _h264_length_field;
			uint _time_scale;
			SafePointer<MediaFoundationFrame> _last_frame, _current_frame;
			bool _first_packet, _output_set;
			Array<h264_timestamp> _timecodes;

			void _transform_recreate(void)
			{
				_first_packet = true;
				if (_transform) _transform->Release();
				_transform = 0;
				if (_output_type) _output_type->Release();
				_output_type = 0;
				if (_format == VideoFormatH264) {
					if (CoCreateInstance(CLSID_CMSH264DecoderMFT, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_transform)) != S_OK) throw Exception();
					IMFAttributes * attr;
					if (_transform->GetAttributes(&attr) == S_OK) {
						attr->SetUINT32(MFT_DECODER_EXPOSE_OUTPUT_TYPES_IN_NATIVE_ORDER, TRUE);
						if (_desc.Device) {
							try {
								UINT32 d3d_aware;
								if (attr->GetUINT32(MF_SA_D3D11_AWARE, &d3d_aware) != S_OK || !d3d_aware) throw Exception();
								IMFDXGIDeviceManager * manager = EnableVideoAcceleration(_desc.Device);
								if (!manager) throw Exception();
								if (_transform->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(manager)) != S_OK) throw Exception();
							} catch (...) { _desc.Device->Release(); _desc.Device = 0; }
						}
						attr->Release();
					} else if (_desc.Device) { _desc.Device->Release(); _desc.Device = 0; }
					IMFMediaType * type;
					if (MFCreateMediaType(&type) != S_OK) throw Exception();
					if (type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video) != S_OK) { type->Release(); throw Exception(); }
					if (type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264_ES) != S_OK) { type->Release(); throw Exception(); }
					if (type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_MixedInterlaceOrProgressive) != S_OK) { type->Release(); throw Exception(); }
					if (MFSetAttributeSize(type, MF_MT_FRAME_SIZE, _desc.Width, _desc.Height) != S_OK) { type->Release(); throw Exception(); }
					if (_transform->SetInputType(0, type, 0) != S_OK) { type->Release(); throw Exception(); }
					type->Release();
					if (_desc.Device) {
						try {
							if (_transform->GetOutputAvailableType(0, 0, &type) != S_OK) throw Exception();
							if (_transform->SetOutputType(0, type, 0) != S_OK) { type->Release(); throw Exception(); }
							_output_type = type;
						} catch (...) {
							_desc.Device->Release(); _desc.Device = 0;
							_transform->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, 0);
						}
					}
					if (!_desc.Device) {
						if (_transform->GetOutputAvailableType(0, 0, &type) != S_OK) throw Exception();
						if (_transform->SetOutputType(0, type, 0) != S_OK) { type->Release(); throw Exception(); }
						_output_type = type;
					}
				} else if (_format == VideoFormatRGB) {
					if (_desc.Device) { _desc.Device->Release(); _desc.Device = 0; }
					auto stride = _desc.Width * 3;
					if (stride & 1) stride++;
					IMFMediaType * type;
					if (MFCreateMediaType(&type) != S_OK) throw Exception();
					if (type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video) != S_OK) { type->Release(); throw Exception(); }
					if (type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24) != S_OK) { type->Release(); throw Exception(); }
					if (type->SetUINT32(MF_MT_DEFAULT_STRIDE, stride) != S_OK) { type->Release(); throw Exception(); }
					if (MFSetAttributeSize(type, MF_MT_FRAME_SIZE, _desc.Width, _desc.Height) != S_OK) { type->Release(); throw Exception(); }
					if (MFSetAttributeRatio(type, MF_MT_FRAME_RATE, _desc.TimeScale, _desc.FrameDuration) != S_OK) { type->Release(); throw Exception(); }
					_output_type = type;
				} else throw InvalidFormatException();
			}
			bool _submit_packet(const Media::PacketBuffer & packet) noexcept
			{
				if (packet.PacketDataActuallyUsed) {
					if (!packet.PacketData) return false;
					if (_transform) {
						IMFSample * sample;
						IMFMediaBuffer * buffer;
						LPBYTE data_ptr;
						DWORD ex_mem = 0;
						if (_h264_length_field < 4) {
							int r = 0;
							while (r + _h264_length_field <= packet.PacketDataActuallyUsed) {
								uint length = 0;
								for (int i = 0; i < _h264_length_field; i++) {
									length <<= 8;
									length |= packet.PacketData->ElementAt(r);
									r++;
								}
								r += length;
								ex_mem += 4 - _h264_length_field;
							}
						}
						if (MFCreateMemoryBuffer(packet.PacketDataActuallyUsed + (_first_packet ? _h264_nalu_prefix->Length() : 0) + ex_mem, &buffer) != S_OK) return false;
						if (buffer->Lock(&data_ptr, 0, 0) != S_OK) { buffer->Release(); return false; }
						uint size = 0;
						int w, r;
						if (_first_packet) {
							for (int i = 0; i < _h264_nalu_prefix->Length(); i++) data_ptr[i] = _h264_nalu_prefix->ElementAt(i);
							w = _h264_nalu_prefix->Length();
							r = 0;
						} else w = r = 0;
						while (r + _h264_length_field <= packet.PacketDataActuallyUsed) {
							uint length = 0;
							for (int i = 0; i < _h264_length_field; i++) {
								length <<= 8;
								length |= packet.PacketData->ElementAt(r);
								r++;
							}
							if (r + length > packet.PacketDataActuallyUsed) break;
							data_ptr[w] = 0x00; data_ptr[w + 1] = 0x00; data_ptr[w + 2] = 0x00; data_ptr[w + 3] = 0x01;
							w += 4;
							for (int i = 0; i < int(length); i++) {
								data_ptr[w] = packet.PacketData->ElementAt(r);
								w++; r++;
							}
						}
						size = w;
						buffer->Unlock();
						if (buffer->SetCurrentLength(size) != S_OK) { buffer->Release(); return false; }
						if (MFCreateSample(&sample) != S_OK) { buffer->Release(); return false; }
						if (sample->AddBuffer(buffer) != S_OK) { buffer->Release(); sample->Release(); return false; }
						buffer->Release();
						if (sample->SetUINT32(MFSampleExtension_CleanPoint, packet.PacketIsKey ? 1 : 0) != S_OK) { sample->Release(); return false; }
						if (sample->SetSampleDuration(packet.PacketRenderDuration * 10000000 / _time_scale) != S_OK) { sample->Release(); return false; }
						if (sample->SetSampleTime(packet.PacketRenderTime * 10000000 / _time_scale) != S_OK) { sample->Release(); return false; }
						try { _timecodes << h264_timestamp{ packet.PacketRenderTime, packet.PacketRenderDuration }; } catch (...) { sample->Release(); return false; }
						if (_transform->ProcessInput(0, sample, 0) != S_OK) { sample->Release(); _timecodes.RemoveLast(); return false; }
						sample->Release();
					} else {
						if (!packet.PacketData) return false;
						int stride = _desc.Width * 3;
						if (stride & 1) stride++;
						int size = stride * _desc.Height;
						if (packet.PacketDataActuallyUsed < size) return false;
						IMFSample * sample;
						IMFMediaBuffer * buffer;
						LPBYTE data_ptr;
						if (MFCreateMemoryBuffer(size, &buffer) != S_OK) return false;
						if (buffer->Lock(&data_ptr, 0, 0) != S_OK) { buffer->Release(); return false; }
						for (int y = 0; y < _desc.Height; y++) for (int x = 0; x < _desc.Width; x++) {
							auto offs = 3 * x + y * stride;
							data_ptr[offs] = packet.PacketData->ElementAt(offs + 2);
							data_ptr[offs + 1] = packet.PacketData->ElementAt(offs + 1);
							data_ptr[offs + 2] = packet.PacketData->ElementAt(offs);
						}
						buffer->Unlock();
						if (buffer->SetCurrentLength(size) != S_OK) { buffer->Release(); return false; }
						if (MFCreateSample(&sample) != S_OK) { buffer->Release(); return false; }
						if (sample->AddBuffer(buffer) != S_OK) { buffer->Release(); sample->Release(); return false; }
						buffer->Release();
						if (sample->SetUINT32(MFSampleExtension_CleanPoint, packet.PacketIsKey ? 1 : 0) != S_OK) { sample->Release(); return false; }
						if (sample->SetSampleDuration(packet.PacketRenderDuration * 10000000 / _time_scale) != S_OK) { sample->Release(); return false; }
						if (sample->SetSampleTime(packet.PacketRenderTime * 10000000 / _time_scale) != S_OK) { sample->Release(); return false; }
						try {
							_last_frame = new MediaFoundationFrame(_output_type, sample);
							_last_frame->SetTimeScale(_desc.TimeScale);
							_last_frame->SetFramePresentation(packet.PacketRenderTime);
							_last_frame->SetFrameDuration(packet.PacketRenderDuration);
						} catch (...) { sample->Release(); return false; }
						sample->Release();
					}
					if (_first_packet) _first_packet = false;
				} else {
					if (_transform) {
						if (_transform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0) != S_OK) return false;
						if (_transform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0) != S_OK) return false;
					}
				}
				return true;
			}
			bool _last_frame_is_free(void) const noexcept { return !_last_frame || _last_frame->GetReferenceCount() <= 1; }
			bool _read_next_frame(void) noexcept
			{
				if (_transform) {
					_output_set = false;
					while (true) {
						MFT_OUTPUT_DATA_BUFFER output;
						DWORD status;
						ZeroMemory(&output, sizeof(output));
						if (_desc.Device) {
							if (_last_frame) _last_frame->SetSample(0);
						} else {
							if (_last_frame && _last_frame->GetFormat() == _output_type) {
								IMFMediaBuffer * buffer;
								output.pSample = _last_frame->GetSample();
								output.pSample->AddRef();
								if (output.pSample->GetBufferByIndex(0, &buffer) != S_OK) { output.pSample->Release(); return false; }
								if (buffer->SetCurrentLength(0) != S_OK) { output.pSample->Release(); buffer->Release(); return false; }
								buffer->Release();
							} else {
								_last_frame.SetReference(0);
								IMFMediaBuffer * buffer;
								MFT_OUTPUT_STREAM_INFO info;
								if (_transform->GetOutputStreamInfo(0, &info) != S_OK) return false;
								if (MFCreateMemoryBuffer(info.cbSize, &buffer) != S_OK) return false;
								if (MFCreateSample(&output.pSample) != S_OK) { buffer->Release(); return false; }
								if (output.pSample->AddBuffer(buffer) != S_OK) { buffer->Release(); output.pSample->Release(); return false; }
								buffer->Release();
							}
						}
						auto result = _transform->ProcessOutput(0, 1, &output, &status);
						if (result != S_OK && !_desc.Device) output.pSample->Release();
						if (result == MF_E_TRANSFORM_NEED_MORE_INPUT) return true;
						else if (result == MF_E_TRANSFORM_STREAM_CHANGE) {
							IMFMediaType * type;
							if (_transform->GetOutputAvailableType(0, 0, &type) != S_OK) return false;
							if (_transform->SetOutputType(0, type, 0) != S_OK) { type->Release(); return false; }
							if (_output_type) _output_type->Release();
							_output_type = type;
							_last_frame.SetReference(0);
							continue;
						} else if (result != S_OK) return false;
						if (output.pEvents) output.pEvents->Release();
						if (_last_frame) {
							if (_desc.Device) _last_frame->SetSample(output.pSample);
						} else {
							try {
								_last_frame = new MediaFoundationFrame(_output_type, output.pSample, _desc.Device);
							} catch (...) { output.pSample->Release(); return false; }
						}
						output.pSample->Release();
						auto tc = _timecodes.FirstElement();
						int ti = 0;
						for (int i = 1; i < _timecodes.Length(); i++) {
							if (_timecodes[i].present_at < tc.present_at) {
								tc = _timecodes[i];
								ti = i;
							}
						}
						_last_frame->SetFramePresentation(tc.present_at);
						_last_frame->SetFrameDuration(tc.duration);
						_last_frame->SetTimeScale(_time_scale);
						_timecodes.Remove(ti);
						swap(*_last_frame.InnerRef(), *_current_frame.InnerRef());
						_output_set = true;
						break;
					}
				} else {
					if (_last_frame) {
						_current_frame = _last_frame;
						_last_frame.SetReference(0);
						_output_set = true;
					} else _output_set = false;
				}
				return true;
			}
		public:
			MediaFoundationDecoder(IVideoCodec * codec, const Media::TrackFormatDesc & format, Graphics::IDevice * acceleration_device) : _timecodes(0x40)
			{
				if (format.GetTrackClass() != Media::TrackClass::Video) throw InvalidFormatException();
				if (MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET) != S_OK) throw Exception();
				try {
					_h264_length_field = 0;
					_output_set = false;
					_transform = 0;
					_output_type = 0;
					_parent.SetRetain(codec);
					_format = format.GetTrackCodec();
					auto & vf = format.As<Media::VideoTrackFormatDesc>();
					_time_scale = vf.GetFrameRateNumerator();
					_desc.Width = vf.GetWidth();
					_desc.Height = vf.GetHeight();
					_desc.FramePresentation = 0;
					_desc.FrameDuration = vf.GetFrameRateDenominator();
					_desc.TimeScale = vf.GetFrameRateNumerator();
					_desc.Device = acceleration_device;
					if (_desc.Device) _desc.Device->Retain();
					if (_format == VideoFormatH264) {
						_codec_data = new DataBlock(*vf.GetCodecMagic());
						_transform_recreate();
						auto & hdr = *reinterpret_cast<h264_header *>(_codec_data->GetBuffer());
						_h264_nalu_prefix = new DataBlock(0x100);
						_h264_length_field = (hdr.length_field_size & 0x03) + 1;
						int offs = 0;
						int nsps = hdr.data[offs] & 0x1F;
						offs++;
						for (int i = 0; i < nsps; i++) {
							int fl = (uint32(hdr.data[offs]) << 8) | uint32(hdr.data[offs + 1]);
							offs += 2;
							_h264_nalu_prefix->Append(0x00); _h264_nalu_prefix->Append(0x00); _h264_nalu_prefix->Append(0x01);
							for (int j = 0; j < fl; j++) { _h264_nalu_prefix->Append(hdr.data[offs]); offs++; }
						}
						int npps = hdr.data[offs];
						offs++;
						for (int i = 0; i < npps; i++) {
							int fl = (uint32(hdr.data[offs]) << 8) | uint32(hdr.data[offs + 1]);
							offs += 2;
							_h264_nalu_prefix->Append(0x00); _h264_nalu_prefix->Append(0x00); _h264_nalu_prefix->Append(0x01);
							for (int j = 0; j < fl; j++) { _h264_nalu_prefix->Append(hdr.data[offs]); offs++; }
						}
					} else if (_format == VideoFormatRGB) {
						_transform_recreate();
					} else throw InvalidFormatException();
				} catch (...) {
					if (_transform) _transform->Release();
					if (_output_type) _output_type->Release();
					if (_desc.Device) _desc.Device->Release();
					MFShutdown(); throw;
				}
			}
			virtual ~MediaFoundationDecoder(void) override
			{
				if (_transform) _transform->Release();
				if (_output_type) _output_type->Release();
				if (_desc.Device) _desc.Device->Release();
				MFShutdown();
			}
			virtual const VideoObjectDesc & GetObjectDescriptor(void) const noexcept override { return _desc; }
			virtual VideoObjectType GetObjectType(void) const noexcept override { return VideoObjectType::Decoder; }
			virtual handle GetBufferFormat(void) const noexcept override { return _output_type; }
			virtual IVideoCodec * GetParentCodec(void) const override { return _parent; }
			virtual string GetEncodedFormat(void) const override { return _format; }
			virtual bool Reset(void) noexcept override
			{
				try {
					_output_set = false;
					_timecodes.Clear();
					_last_frame.SetReference(0);
					_current_frame.SetReference(0);
					_first_packet = true;
					if (_transform && _transform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0) != S_OK) return false;
					return true;
				} catch (...) { return false; }
			}
			virtual bool IsOutputAvailable(void) const noexcept override { return _output_set; }
			virtual bool SupplyPacket(const Media::PacketBuffer & packet) noexcept override
			{
				if (_output_set || !_last_frame_is_free()) return false;
				if (!_submit_packet(packet)) return false;
				return _read_next_frame();
			}
			virtual bool ReadFrame(IVideoFrame ** frame) noexcept override
			{
				if (!frame || !_output_set || !_last_frame_is_free()) return false;
				*frame = _current_frame.Inner();
				_current_frame->Retain();
				return _read_next_frame();
			}
		};
		class MediaFoundationEncoder : public IVideoEncoder
		{
			SafePointer<IVideoCodec> _parent;
			string _format;
			VideoObjectDesc _desc;
			SafePointer<Media::VideoTrackFormatDesc> _track_desc;
			SafePointer<DataBlock> _codec_data;
			IMFTransform * _encoder;
			IMFMediaType * _input_type, * _primary_input_type;
			IMFSample * _sample;
			ICodecAPI * _codec_api;
			MediaFoundationFrameBlt * _recoder;
			Array<uint64> _render_indicies;
			uint _avg_byterate, _h264_profile, _max_keyframe_period;
			bool _output_set, _eos, _eos_pending, _first_frame;
			uint64 _frames_supplied;
			uint64 _frames_read;
			uint64 _base_encoder_time;

			void _encoder_recreate(void)
			{
				if (_encoder) _encoder->Release();
				_encoder = 0;
				if (_input_type) _input_type->Release();
				_input_type = 0;
				if (_format == VideoFormatH264) {
					UINT bitrate, profile;
					auto px_size = _desc.Width * _desc.Height;
					bitrate = uint32(5000000 * uint64(px_size) / (1080 * 1920));
					profile = eAVEncH264VProfile_Main;
					if (_avg_byterate) bitrate = _avg_byterate * 8;
					if (_h264_profile == Media::H264ProfileBase) profile = eAVEncH264VProfile_Base;
					else if (_h264_profile == Media::H264ProfileMain) profile = eAVEncH264VProfile_Main;
					else if (_h264_profile == Media::H264ProfileHigh) profile = eAVEncH264VProfile_High;
					if (CoCreateInstance(CLSID_CMSH264EncoderMFT, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_encoder)) != S_OK) throw Exception();
					IMFMediaType * type;
					if (MFCreateMediaType(&type) != S_OK) throw Exception();
					if (type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video) != S_OK) { type->Release(); throw Exception(); }
					if (type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264) != S_OK) { type->Release(); throw Exception(); }
					if (type->SetUINT32(MF_MT_AVG_BITRATE, bitrate) != S_OK) { type->Release(); throw Exception(); }
					if (MFSetAttributeSize(type, MF_MT_FRAME_SIZE, _desc.Width, _desc.Height) != S_OK) { type->Release(); throw Exception(); }
					if (MFSetAttributeRatio(type, MF_MT_FRAME_RATE, _desc.TimeScale, _desc.FrameDuration) != S_OK) { type->Release(); throw Exception(); }
					if (type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_MixedInterlaceOrProgressive) != S_OK) { type->Release(); throw Exception(); }
					if (type->SetUINT32(MF_MT_MPEG2_PROFILE, profile) != S_OK) { type->Release(); throw Exception(); }
					if (_encoder->SetOutputType(0, type, 0) != S_OK) { type->Release(); throw Exception(); }
					type->Release();
					if (MFCreateMediaType(&type) != S_OK) throw Exception();
					if (type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video) != S_OK) { type->Release(); throw Exception(); }
					if (type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV) != S_OK) { type->Release(); throw Exception(); }
					if (MFSetAttributeSize(type, MF_MT_FRAME_SIZE, _desc.Width, _desc.Height) != S_OK) { type->Release(); throw Exception(); }
					if (MFSetAttributeRatio(type, MF_MT_FRAME_RATE, _desc.TimeScale, _desc.FrameDuration) != S_OK) { type->Release(); throw Exception(); }
					if (_encoder->SetInputType(0, type, 0) != S_OK) { type->Release(); throw Exception(); }
					_input_type = type;
					if (_encoder->GetOutputCurrentType(0, &type) != S_OK) throw Exception();
					UINT32 blob_size;
					if (type->GetBlobSize(MF_MT_MPEG_SEQUENCE_HEADER, &blob_size) != S_OK) { type->Release(); throw Exception(); }
					SafePointer<DataBlock> blob;
					try {
						blob = new DataBlock(blob_size);
						blob->SetLength(blob_size);
					} catch (...) { type->Release(); throw; }
					if (type->GetBlob(MF_MT_MPEG_SEQUENCE_HEADER, blob->GetBuffer(), blob_size, 0) != S_OK) { type->Release(); throw Exception(); }
					type->Release();
					ObjectArray<DataBlock> sps(0x10), pps(0x10);
					int cp = 0;
					while (cp < blob->Length() - 2) {
						if (blob->ElementAt(cp) == 0x00 && blob->ElementAt(cp + 1) == 0x00 && blob->ElementAt(cp + 2) == 0x01) {
							cp += 3;
							int sp = cp;
							while (cp < blob->Length()) {
								if (cp < blob->Length() - 2 && blob->ElementAt(cp) == 0x00 && blob->ElementAt(cp + 1) == 0x00 && blob->ElementAt(cp + 2) == 0x01) break;
								else if (cp < blob->Length() - 3 && blob->ElementAt(cp) == 0x00 && blob->ElementAt(cp + 1) == 0x00 && blob->ElementAt(cp + 2) == 0x00 && blob->ElementAt(cp + 3) == 0x01) break;
								else cp++;
							}
							SafePointer<DataBlock> nalu = new DataBlock(0x100);
							while (sp < cp) { nalu->Append(blob->ElementAt(sp)); sp++; }
							if (nalu->Length()) {
								auto nt = nalu->FirstElement() & 0x1F;
								if (nt == 0x07) sps.Append(nalu);
								else if (nt == 0x08) pps.Append(nalu);
							}
						} else cp++;
					}
					if (!sps.Length() || !pps.Length()) throw InvalidFormatException();
					blob.SetReference(0);
					_codec_data = new DataBlock(0x100);
					_codec_data->Append(0x01);
					_codec_data->Append(sps.FirstElement()->ElementAt(1));
					_codec_data->Append(sps.FirstElement()->ElementAt(2));
					_codec_data->Append(sps.FirstElement()->ElementAt(3));
					_codec_data->Append(0xFF);
					_codec_data->Append(0xE0 | sps.Length());
					for (auto & nalu : sps) {
						_codec_data->Append((nalu.Length() & 0xFF00) >> 8);
						_codec_data->Append(nalu.Length() & 0xFF);
						_codec_data->Append(nalu);
					}
					_codec_data->Append(pps.Length());
					for (auto & nalu : pps) {
						_codec_data->Append((nalu.Length() & 0xFF00) >> 8);
						_codec_data->Append(nalu.Length() & 0xFF);
						_codec_data->Append(nalu);
					}
					if (_track_desc) _track_desc->SetCodecMagic(_codec_data);
				} else if (_format == VideoFormatRGB) {
					auto stride = _desc.Width * 3;
					if (stride & 1) stride++;
					IMFMediaType * type;
					if (MFCreateMediaType(&type) != S_OK) throw Exception();
					if (type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video) != S_OK) { type->Release(); throw Exception(); }
					if (type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24) != S_OK) { type->Release(); throw Exception(); }
					if (type->SetUINT32(MF_MT_DEFAULT_STRIDE, stride) != S_OK) { type->Release(); throw Exception(); }
					if (MFSetAttributeSize(type, MF_MT_FRAME_SIZE, _desc.Width, _desc.Height) != S_OK) { type->Release(); throw Exception(); }
					if (MFSetAttributeRatio(type, MF_MT_FRAME_RATE, _desc.TimeScale, _desc.FrameDuration) != S_OK) { type->Release(); throw Exception(); }
					_input_type = type;
				} else throw InvalidFormatException();
			}
			void _recoder_recreate(const MediaFoundationFrame * for_frame)
			{
				if (_recoder) _recoder->Release();
				_recoder = 0;
				if (_primary_input_type) _primary_input_type->Release();
				_primary_input_type = for_frame->GetFormat();
				_primary_input_type->AddRef();
				try {
					_recoder = new MediaFoundationFrameBlt;
					if (!_recoder->SetInputFormat(_primary_input_type, for_frame->GetObjectDescriptor().Device)) throw Exception();
					if (!_recoder->SetOutputFormat(_input_type, 0)) throw Exception();
				} catch (...) {
					if (_recoder) _recoder->Release();
					_recoder = 0;
					if (_primary_input_type) _primary_input_type->Release();
					_primary_input_type = 0;
					throw;
				}
			}
			bool _is_current_recoder_applicable(const MediaFoundationFrame * for_frame) const noexcept { return _primary_input_type && _primary_input_type == for_frame->GetFormat(); }
			bool _submit_frame(const IVideoFrame * frame, bool encode_keyframe) noexcept
			{
				if (_max_keyframe_period && _frames_supplied % _max_keyframe_period == 0) encode_keyframe = true;
				auto _frame = static_cast<const MediaFoundationFrame *>(frame);
				if (!_is_current_recoder_applicable(_frame)) {
					try { _recoder_recreate(_frame); } catch (...) { return false; }
				}
				IMFSample * sample = _recoder->Process(frame);
				if (!sample) return false;
				uint64 current_fake_clock = _frames_supplied * _desc.FrameDuration * 10000000 / _desc.TimeScale;
				uint64 next_fake_clock = (_frames_supplied + 1) * _desc.FrameDuration * 10000000 / _desc.TimeScale;
				if (sample->SetSampleTime(current_fake_clock) != S_OK) { sample->Release(); return false; }
				if (sample->SetSampleDuration(next_fake_clock - current_fake_clock) != S_OK) { sample->Release(); return false; }
				if (_encoder) {
					if (_codec_api && encode_keyframe) {
						VARIANT variant;
						VariantInit(&variant);
						variant.vt = VT_UI4;
						variant.ulVal = TRUE;
						_codec_api->SetValue(&CODECAPI_AVEncVideoForceKeyFrame, &variant);
						VariantClear(&variant);
					}
					if (_encoder->ProcessInput(0, sample, 0) != S_OK) { sample->Release(); return false; }
					sample->Release();
					try { _render_indicies << _frames_supplied; } catch (...) { return false; }
					_frames_supplied++;
					return true;
				} else {
					try { _render_indicies << _frames_supplied; } catch (...) { sample->Release(); return false; }
					if (_sample) _sample->Release();
					_sample = sample;
					_frames_supplied++;
					_output_set = true;
					return true;
				}
			}
			bool _read_next_packet(void) noexcept
			{
				if (_encoder) {
					_output_set = false;
					MFT_OUTPUT_DATA_BUFFER output;
					DWORD status;
					ZeroMemory(&output, sizeof(output));
					if (!_sample) {
						MFT_OUTPUT_STREAM_INFO info;
						if (_encoder->GetOutputStreamInfo(0, &info) != S_OK) return false;
						IMFMediaBuffer * buffer;
						if (MFCreateMemoryBuffer(info.cbSize, &buffer) != S_OK) return false;
						if (MFCreateSample(&_sample) != S_OK) { buffer->Release(); return false; }
						if (_sample->AddBuffer(buffer) != S_OK) { buffer->Release(); _sample->Release(); _sample = 0; return false; }
						buffer->Release();
					} else {
						IMFMediaBuffer * buffer;
						if (_sample->GetBufferByIndex(0, &buffer) != S_OK) return false;
						if (buffer->SetCurrentLength(0) != S_OK) { buffer->Release(); return false; }
						buffer->Release();
					}
					output.pSample = _sample;
					auto result = _encoder->ProcessOutput(0, 1, &output, &status);
					if (result == MF_E_TRANSFORM_NEED_MORE_INPUT) {
						if (_eos_pending) _eos = true;
						return true;
					} else if (result != S_OK) return false;
					if (output.pEvents) output.pEvents->Release();
					_output_set = true;
					return true;
				} else {
					if (!_output_set && _eos_pending) _eos = true;
					return true;
				}
			}
			bool _fill_packet(Media::PacketBuffer & packet) noexcept
			{
				if (_eos) {
					packet.PacketDecodeTime = _frames_read * _desc.FrameDuration;
					packet.PacketRenderTime = _frames_read * _desc.FrameDuration;
					packet.PacketRenderDuration = 0;
					packet.PacketIsKey = true;
					packet.PacketDataActuallyUsed = 0;
					_eos = false;
					_eos_pending = false;
					_output_set = false;
					_first_frame = true;
					_frames_read = _frames_supplied = _base_encoder_time = 0;
					return true;
				} else {
					LONGLONG time;
					if (_sample->GetSampleTime(&time) != S_OK) return false;
					if (_first_frame) {
						_first_frame = false;
						_base_encoder_time = time;
					}
					time -= _base_encoder_time;
					packet.PacketRenderTime = -1;
					for (int i = 0; i < _render_indicies.Length(); i++) {
						int64 co_time = _render_indicies[i] * _desc.FrameDuration * 10000000 / _desc.TimeScale;
						if (co_time - 1 <= time && time <= co_time + 1) {
							packet.PacketRenderTime = _render_indicies[i] * _desc.FrameDuration;
							_render_indicies.Remove(i);
							break;
						}
					}
					packet.PacketDecodeTime = _frames_read * _desc.FrameDuration;
					packet.PacketRenderDuration = _desc.FrameDuration;
					IMFMediaBuffer * buffer;
					UINT32 is_keyframe;
					DWORD raw_length;
					LPBYTE data_ptr;
					if (_sample->GetUINT32(MFSampleExtension_CleanPoint, &is_keyframe) != S_OK) is_keyframe = TRUE;
					if (_sample->GetBufferByIndex(0, &buffer) != S_OK) return false;
					if (buffer->GetCurrentLength(&raw_length) != S_OK) { buffer->Release(); return false; }
					try {
						if (!packet.PacketData) packet.PacketData = new DataBlock(1);
						if (packet.PacketData->Length() < raw_length) packet.PacketData->SetLength(raw_length);
					} catch (...) { buffer->Release(); return false; }
					if (buffer->Lock(&data_ptr, 0, 0) != S_OK) { buffer->Release(); return false; }
					if (_encoder) {
						packet.PacketDataActuallyUsed = 0;
						int cp = 0;
						while (cp < raw_length - 2) {
							if (data_ptr[cp] == 0x00 && data_ptr[cp + 1] == 0x00 && data_ptr[cp + 2] == 0x01) {
								cp += 3;
								int sp = cp;
								while (cp < raw_length) {
									if (cp < raw_length - 2 && data_ptr[cp] == 0x00 && data_ptr[cp + 1] == 0x00 && data_ptr[cp + 2] == 0x01) break;
									else if (cp < raw_length - 3 && data_ptr[cp] == 0x00 && data_ptr[cp + 1] == 0x00 && data_ptr[cp + 2] == 0x00 && data_ptr[cp + 3] == 0x01) break;
									else cp++;
								}
								uint nalu_length = cp - sp;
								packet.PacketData->ElementAt(packet.PacketDataActuallyUsed) = (nalu_length & 0xFF000000) >> 24;
								packet.PacketData->ElementAt(packet.PacketDataActuallyUsed + 1) = (nalu_length & 0xFF0000) >> 16;
								packet.PacketData->ElementAt(packet.PacketDataActuallyUsed + 2) = (nalu_length & 0xFF00) >> 8;
								packet.PacketData->ElementAt(packet.PacketDataActuallyUsed + 3) = nalu_length & 0xFF;
								packet.PacketDataActuallyUsed += 4;
								while (sp < cp) {
									packet.PacketData->ElementAt(packet.PacketDataActuallyUsed) = data_ptr[sp];
									packet.PacketDataActuallyUsed++;
									sp++;
								}
							} else cp++;
						}
					} else {
						packet.PacketDataActuallyUsed = raw_length;
						MemoryCopy(packet.PacketData->GetBuffer(), data_ptr, raw_length);
						int stride = _desc.Width * 3;
						if (stride & 1) stride++;
						for (int y = 0; y < _desc.Height; y++) {
							int base = y * stride;
							for (int x = 0; x < _desc.Width; x++) {
								int base2 = base + x * 3;
								swap(packet.PacketData->ElementAt(base2), packet.PacketData->ElementAt(base2 + 2));
							}
						}
						_output_set = false;
					}
					buffer->Unlock();
					buffer->Release();
					packet.PacketIsKey = is_keyframe ? true : false;
					_frames_read++;
					return true;
				}
			}
		public:
			MediaFoundationEncoder(IVideoCodec * codec, const string & format, const VideoObjectDesc & desc, uint num_options, const uint * options) : _render_indicies(0x100)
			{
				if (MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET) != S_OK) throw Exception();
				if (!desc.Width || !desc.Height || !desc.FrameDuration || !desc.TimeScale) throw Exception();
				try {
					_avg_byterate = _h264_profile = _max_keyframe_period = 0;
					for (uint i = 0; i < num_options; i++) {
						if (options[2 * i] == Media::MediaEncoderSuggestedBytesPerSecond) _avg_byterate = options[2 * i + 1];
						else if (options[2 * i] == Media::MediaEncoderH264Profile) _h264_profile = options[2 * i + 1];
						else if (options[2 * i] == Media::MediaEncoderMaxKeyframePeriod) _max_keyframe_period = options[2 * i + 1];
					}
					_encoder = 0;
					_recoder = 0;
					_codec_api = 0;
					_input_type = 0;
					_primary_input_type = 0;
					_sample = 0;
					_format = format;
					_parent.SetRetain(codec);
					_desc.Width = desc.Width;
					_desc.Height = desc.Height;
					_desc.TimeScale = desc.TimeScale;
					_desc.FrameDuration = desc.FrameDuration;
					_desc.FramePresentation = 0;
					_desc.Device = 0;
					_output_set = false;
					_eos = false;
					_eos_pending = false;
					_first_frame = true;
					_frames_supplied = _frames_read = _base_encoder_time = 0;
					if (format == VideoFormatH264) {
						_encoder_recreate();
						_track_desc = new Media::VideoTrackFormatDesc(format, desc.Width, desc.Height, desc.TimeScale, desc.FrameDuration);
						_track_desc->SetCodecMagic(_codec_data);
						if (_encoder->QueryInterface(IID_PPV_ARGS(&_codec_api)) != S_OK) _codec_api = 0;
					} else if (format == VideoFormatRGB) {
						_encoder_recreate();
						_track_desc = new Media::VideoTrackFormatDesc(format, desc.Width, desc.Height, desc.TimeScale, desc.FrameDuration);
					} else throw InvalidFormatException();
				} catch (...) {
					if (_encoder) _encoder->Release();
					if (_recoder) _recoder->Release();
					if (_input_type) _input_type->Release();
					if (_primary_input_type) _primary_input_type->Release();
					MFShutdown(); throw;
				}
			}
			virtual ~MediaFoundationEncoder(void) override
			{
				if (_encoder) _encoder->Release();
				if (_recoder) _recoder->Release();
				if (_codec_api) _codec_api->Release();
				if (_input_type) _input_type->Release();
				if (_primary_input_type) _primary_input_type->Release();
				if (_sample) _sample->Release();
				MFShutdown();
			}
			virtual const VideoObjectDesc & GetObjectDescriptor(void) const noexcept override { return _desc; }
			virtual VideoObjectType GetObjectType(void) const noexcept override { return VideoObjectType::Encoder; }
			virtual handle GetBufferFormat(void) const noexcept override { return _input_type; }
			virtual IVideoCodec * GetParentCodec(void) const override { return _parent; }
			virtual string GetEncodedFormat(void) const override { return _format; }
			virtual bool Reset(void) noexcept override
			{
				try {
					_render_indicies.Clear();
					_first_frame = true;
					_frames_supplied = _frames_read = _base_encoder_time = 0;
					_output_set = false;
					_eos = false;
					_eos_pending = false;
					if (_recoder) _recoder->Release();
					if (_primary_input_type) _primary_input_type->Release();
					if (_sample) _sample->Release();
					_sample = 0;
					_recoder = 0;
					_primary_input_type = 0;
					if (_encoder && _encoder->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0) != S_OK) return false;
					return true;
				} catch (...) { return false; }
			}
			virtual bool IsOutputAvailable(void) const noexcept override { return _output_set || _eos; }
			virtual const Media::VideoTrackFormatDesc & GetEncodedDescriptor(void) const noexcept override { return *_track_desc; }
			virtual const DataBlock * GetCodecMagic(void) noexcept override { return _codec_data; }
			virtual bool SupplyFrame(const IVideoFrame * frame, bool encode_keyframe) noexcept override
			{
				if (_output_set) return false;
				if (!_submit_frame(frame, encode_keyframe)) return false;
				return _read_next_packet();
			}
			virtual bool SupplyEndOfStream(void) noexcept override
			{
				if (_encoder && _encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0) != S_OK) return false;
				if (_encoder && _encoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0) != S_OK) return false;
				_eos_pending = true;
				if (!_output_set) return _read_next_packet();
				return true;
			}
			virtual bool ReadPacket(Media::PacketBuffer & packet) noexcept override
			{
				if ((!_sample || !_output_set) && !_eos) return false;
				if (!_fill_packet(packet)) return false;
				return _read_next_packet();
			}
		};
		class MediaFoundationCodec : public IVideoCodec
		{
		public:
			MediaFoundationCodec(void) {}
			virtual ~MediaFoundationCodec(void) override {}
			virtual bool CanEncode(const string & format) const noexcept override
			{
				if (format == VideoFormatH264 || format == VideoFormatRGB) return true;
				else return false;
			}
			virtual bool CanDecode(const string & format) const noexcept override
			{
				if (format == VideoFormatH264 || format == VideoFormatRGB) return true;
				else return false;
			}
			virtual Array<string> * GetFormatsCanEncode(void) const override
			{
				SafePointer< Array<string> > result = new Array<string>(4);
				result->Append(VideoFormatH264);
				result->Append(VideoFormatRGB);
				result->Retain();
				return result;
			}
			virtual Array<string> * GetFormatsCanDecode(void) const override
			{
				SafePointer< Array<string> > result = new Array<string>(4);
				result->Append(VideoFormatH264);
				result->Append(VideoFormatRGB);
				result->Retain();
				return result;
			}
			virtual string GetCodecName(void) const override { return L"Media Foundation Codec"; }
			virtual IVideoDecoder * CreateDecoder(const Media::TrackFormatDesc & format, Graphics::IDevice * acceleration_device) noexcept override
			{
				try {
					return new MediaFoundationDecoder(this, format, acceleration_device);
				} catch (...) { return 0; }
			}
			virtual IVideoEncoder * CreateEncoder(const string & format, const VideoObjectDesc & desc, uint num_options, const uint * options) noexcept override
			{
				try {
					return new MediaFoundationEncoder(this, format, desc, num_options, options);
				} catch (...) { return 0; }
			}
		};
		
		class ScreenCaptureDevice : public IVideoDevice
		{
			struct device_task {
				IVideoFrame ** frame;
				bool * result;
				IDispatchTask * task;
				Semaphore * open;
				device_task * next;
			};

			VideoObjectDesc _desc;
			HMONITOR _handle;
			MONITORINFOEXW _info;
			IMFMediaType * _type;
			IDXGIOutputDuplication * _duplication;
			SafePointer<Windows::IScreen> _screen;
			SafePointer<Thread> _thread;
			SafePointer<Semaphore> _counter, _access;
			SafePointer<Graphics::ITexture> _device_surface;
			int _state;
			bool _initialized;
			uint _stream_time;
			device_task * _first, * _last;

			void _task_process(device_task * task, IVideoFrame * retval, bool status) noexcept
			{
				if (retval) retval->Retain();
				*task->frame = retval;
				if (task->result) *task->result = status;
				if (task->open) {
					task->open->Open();
					task->open->Release();
				}
				if (task->task) {
					task->task->DoTask(0);
					task->task->Release();
				}
				delete task;
			}
			device_task * _dequeue_next(void) noexcept
			{
				_access->Wait();
				auto obj = _first;
				_first = _first->next;
				if (!_first) _last = 0;
				_access->Open();
				return obj;
			}
			void _drain(void) noexcept
			{
				while (_counter->TryWait()) {
					auto task = _dequeue_next();
					_task_process(task, 0, false);
				}
			}
			void _stop_thread(void) noexcept
			{
				device_task obj;
				ZeroMemory(&obj, sizeof(obj));
				_access->Wait();
				obj.next = _first;
				_first = &obj;
				if (!_last) _last = _first;
				_counter->Open();
				_access->Open();
				_thread->Wait();
				_thread.SetReference(0);
			}
			bool _queue_submit(IVideoFrame ** frame, bool * status, IDispatchTask * task, Semaphore * open) noexcept
			{
				if (!frame) return false;
				auto obj = new (std::nothrow) device_task;
				if (!obj) return false;
				obj->frame = frame;
				obj->result = status;
				obj->task = task;
				obj->open = open;
				obj->next = 0;
				if (task) task->Retain();
				if (open) open->Retain();
				_access->Wait();
				if (_last) _last->next = obj; else _first = _last = obj;
				_counter->Open();
				_access->Open();
				return true;
			}
			IMFSample * _capture(void) noexcept
			{
				if (_duplication) {
					IDXGIResource * duplicate;
					DXGI_OUTDUPL_FRAME_INFO info;
					if (_duplication->AcquireNextFrame(INFINITE, &info, &duplicate) == S_OK) {
						IMFSample * result = 0;
						ID3D11Texture2D * surface;
						if (duplicate->QueryInterface(IID_PPV_ARGS(&surface)) == S_OK) {
							D3D11_TEXTURE2D_DESC desc;
							surface->GetDesc(&desc);
							if (!_device_surface) {
								Graphics::TextureDesc td;
								td.Type = Graphics::TextureType::Type2D;
								td.Format = Graphics::PixelFormat::B8G8R8A8_unorm;
								td.Width = desc.Width;
								td.Height = desc.Height;
								td.MipmapCount = 1;
								td.Usage = Graphics::ResourceUsageShaderRead;
								td.MemoryPool = Graphics::ResourceMemoryPool::Default;
								_device_surface = _desc.Device->CreateTexture(td);
							}
							if (_device_surface) {
								auto surface_to = Direct3D::GetInnerObject2D(_device_surface);
								ID3D11DeviceContext * context;
								Direct3D::GetInnerObject(_desc.Device)->GetImmediateContext(&context);
								context->CopySubresourceRegion(surface_to, 0, 0, 0, 0, surface, 0, 0);
								context->Release();
								result = MediaFoundationFrameBlt::MakeDXGISample(_device_surface, Graphics::SubresourceIndex(0, 0));
								if (result) {
									result->SetSampleTime(0);
									result->SetSampleDuration(uint64(_desc.FrameDuration) * 10000000 / _desc.TimeScale);
								}
							}
							surface->Release();
						}
						duplicate->Release();
						_duplication->ReleaseFrame();
						return result;
					} else return 0;
				} else {
					IMFMediaBuffer * buffer;
					auto length = _desc.Width * _desc.Height * 4;
					LPBYTE data_ptr;
					if (MFCreateMemoryBuffer(length, &buffer) != S_OK) return 0;
					if (buffer->SetCurrentLength(length) != S_OK) { buffer->Release(); return 0; }
					if (buffer->Lock(&data_ptr, 0, 0) != S_OK) { buffer->Release(); return 0; }
					try {
						HDC dc = CreateDCW(_screen->GetName(), 0, 0, 0);
						if (!dc) throw Exception();
						int w = GetDeviceCaps(dc, HORZRES), h = GetDeviceCaps(dc, VERTRES);
						if (!w || !h) { DeleteDC(dc); throw Exception(); }
						HDC blit_dc = CreateCompatibleDC(dc);
						if (!blit_dc) { DeleteDC(dc); throw Exception(); }
						HBITMAP blit_bitmap = CreateCompatibleBitmap(dc, w, h);
						if (!blit_bitmap) { DeleteDC(dc); DeleteDC(blit_dc); throw Exception(); }
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
						GetDIBits(blit_dc, blit_bitmap, 0, h, data_ptr, reinterpret_cast<LPBITMAPINFO>(&hdr), DIB_RGB_COLORS);
						DeleteDC(blit_dc);
						DeleteObject(blit_bitmap);
					} catch (...) { buffer->Unlock(); buffer->Release(); return 0; }
					buffer->Unlock();
					IMFSample * result;
					if (MFCreateSample(&result) != S_OK) { buffer->Release(); return 0; }
					if (result->AddBuffer(buffer) != S_OK) { buffer->Release(); result->Release(); return 0; }
					buffer->Release();
					result->SetSampleTime(0);
					result->SetSampleDuration(uint64(_desc.FrameDuration) * 10000000 / _desc.TimeScale);
					return result;
				}
			}
			static int _dispatch_thread(void * arg)
			{
				auto self = reinterpret_cast<ScreenCaptureDevice *>(arg);
				uint64 total_duration_ms = 0;
				uint64 total_duration_units = 0;
				uint last_timestamp = GetTimerValue();
				int time_overcap = 0;
				int residual = 0;
				while (true) {
					if (total_duration_ms * self->_desc.TimeScale < total_duration_units * 1000) residual = 1; else residual = 0;
					auto time_ms = int(uint64(self->_desc.FrameDuration) * 1000 / self->_desc.TimeScale) + residual;
					total_duration_units += self->_desc.FrameDuration;
					total_duration_ms += time_ms;
					auto skip = time_overcap > time_ms;
					if (!skip) {
						auto sample = self->_capture();
						if (self->_counter->TryWait()) {
							auto task = self->_dequeue_next();
							if (task->frame) {
								SafePointer<IVideoFrame> frame;
								try {
									if (!sample) throw Exception();
									frame = new MediaFoundationFrame(self->_type, sample, self->_desc.Device);
									frame->SetTimeScale(self->_desc.TimeScale);
									frame->SetFrameDuration(self->_desc.FrameDuration);
									frame->SetFramePresentation(self->_stream_time * self->_desc.FrameDuration);
									self->_task_process(task, frame, true);
								} catch (...) { self->_task_process(task, 0, 0); }
							} else {
								self->_stream_time++;
								if (sample) sample->Release();
								return 0;
							}
						}
						self->_stream_time++;
						if (sample) sample->Release();
						auto current_time = GetTimerValue();
						int wait = time_ms - time_overcap - time_overcap / 2 - (current_time - last_timestamp);
						if (wait > 0) Sleep(wait);
					} else self->_stream_time++;
					auto new_timestamp = GetTimerValue();
					int present_time = new_timestamp - last_timestamp;
					time_overcap += present_time - time_ms;
					last_timestamp = new_timestamp;
				}
				return 0;
			}
		public:
			ScreenCaptureDevice(Windows::IScreen * screen, Graphics::IDevice * device)
			{
				_screen.SetRetain(screen);
				_counter = CreateSemaphore(0);
				_access = CreateSemaphore(1);
				if (!_counter || !_access) throw Exception();
				_handle = Windows::_get_monitor_handle(screen);
				_desc.Width = _desc.Height = _desc.TimeScale = _desc.FrameDuration = _desc.FramePresentation = 0;
				_desc.Device = 0;
				_type = 0;
				_info.cbSize = sizeof(_info);
				if (!GetMonitorInfoW(_handle, &_info)) throw Exception();
				_state = 0;
				_initialized = false;
				_stream_time = 0;
				_first = _last = 0;
				_duplication = 0;
				if (device) {
					UINT adapter_number = 0;
					IDXGIAdapter * adapter;
					IDXGIOutput1 * output_capture = 0;
					while (Direct3D::GetInnerObject(Direct3D::CommonFactory)->EnumAdapters(adapter_number, &adapter) == S_OK) {
						UINT output_number = 0;
						IDXGIOutput * output;
						while (adapter->EnumOutputs(output_number, &output) == S_OK) {
							DXGI_OUTPUT_DESC od;
							if (output->GetDesc(&od) == S_OK && od.Monitor == _handle) output->QueryInterface(IID_PPV_ARGS(&output_capture));
							output->Release();
							output_number++;
							if (output_capture) break;
						}
						adapter->Release();
						adapter_number++;
						if (output_capture) break;
					}
					if (output_capture) {
						if (output_capture->DuplicateOutput(Direct3D::GetInnerObject(device), &_duplication) == S_OK) {
							_desc.Device = device;
							_desc.Device->Retain();
						}
						output_capture->Release();
					}
				}
			}
			virtual ~ScreenCaptureDevice(void) override
			{
				if (_desc.Device) _desc.Device->Release();
				if (_duplication) _duplication->Release();
				if (_initialized) StopProcessing();
				if (_type) _type->Release();
			}
			virtual const VideoObjectDesc & GetObjectDescriptor(void) const noexcept override { return _desc; }
			virtual VideoObjectType GetObjectType(void) const noexcept override { return VideoObjectType::Device; }
			virtual handle GetBufferFormat(void) const noexcept override { return _type; }
			virtual string GetDeviceIdentifier(void) const override { return L""; }
			virtual Array<VideoObjectDesc> * GetSupportedFrameFormats(void) const noexcept override
			{
				try {
					SafePointer< Array<VideoObjectDesc> > result = new Array<VideoObjectDesc>(0x10);
					VideoObjectDesc desc;
					int width, height;
					width = _info.rcMonitor.right - _info.rcMonitor.left;
					height = _info.rcMonitor.bottom - _info.rcMonitor.top;
					desc.Width = width;
					desc.Height = height;
					desc.TimeScale = 30;
					desc.FrameDuration = 1;
					desc.FramePresentation = 0;
					desc.Device = 0;
					result->Append(desc);
					desc.TimeScale = 1;
					result->Append(desc);
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			virtual bool SetFrameFormat(const VideoObjectDesc & desc) noexcept override
			{
				if (_initialized) return false;
				if (desc.Width != _info.rcMonitor.right - _info.rcMonitor.left) return false;
				if (desc.Height != _info.rcMonitor.bottom - _info.rcMonitor.top) return false;
				if (desc.TimeScale > 30 * desc.FrameDuration) return false;
				if (desc.TimeScale < desc.FrameDuration) return false;
				auto type = MediaFoundationFrameBlt::ProduceFormat(Graphics::PixelFormat::B8G8R8A8_unorm, Codec::AlphaMode::Straight, desc);
				if (!type) return false;
				if (_type) _type->Release();
				_type = type;
				_desc.Width = desc.Width;
				_desc.Height = desc.Height;
				_desc.TimeScale = desc.TimeScale;
				_desc.FrameDuration = desc.FrameDuration;
				return true;
			}
			virtual bool GetSupportedFrameRateRange(uint * min_rate_numerator, uint * min_rate_denominator, uint * max_rate_numerator, uint * max_rate_denominator) const noexcept override
			{
				if (_type) {
					if (min_rate_numerator) *min_rate_numerator = 1;
					if (min_rate_denominator) *min_rate_denominator = 1;
					if (max_rate_numerator) *max_rate_numerator = 30;
					if (max_rate_denominator) *max_rate_denominator = 1;
					return true;
				} else return false;
			}
			virtual bool SetFrameRate(uint rate_numerator, uint rate_denominator) noexcept override
			{
				if (_initialized || !_type) return false;
				if (rate_denominator > 30 * rate_numerator) return false;
				if (rate_denominator < rate_numerator) return false;
				auto type = MediaFoundationFrameBlt::ProduceFormat(Graphics::PixelFormat::B8G8R8A8_unorm, Codec::AlphaMode::Straight, _desc);
				if (!type) return false;
				if (_type) _type->Release();
				_type = type;
				_desc.TimeScale = rate_numerator;
				_desc.FrameDuration = rate_denominator;
				return true;
			}
			virtual bool Initialize(void) noexcept override { if (_initialized) return true; _initialized = true; return true; }
			virtual bool StartProcessing(void) noexcept override
			{
				if (!_initialized && !Initialize()) return false;
				if (_state == 1) return true;
				try { _thread = CreateThread(_dispatch_thread, this); } catch (...) { return false; }
				_state = 1;
				_stream_time = 0;
				return true;
			}
			virtual bool PauseProcessing(void) noexcept override
			{
				if (!_initialized && !Initialize()) return false;
				if (!_state) return false;
				if (_state == 2) return true;
				_stop_thread();
				_state = 2;
				return true;
			}
			virtual bool StopProcessing(void) noexcept override
			{
				if (!_initialized && !Initialize()) return false;
				if (!_state) return true;
				if (!PauseProcessing()) return false;
				_drain();
				_state = 0;
				return true;
			}
			virtual bool ReadFrame(IVideoFrame ** frame) noexcept override
			{
				if (!frame) return false;
				SafePointer<Semaphore> semaphore;
				try {
					semaphore = CreateSemaphore(0);
					if (!semaphore) return false;
				} catch (...) { return false; }
				bool status;
				if (!ReadFrameAsync(frame, &status, semaphore)) return false;
				semaphore->Wait();
				return status;
			}
			virtual bool ReadFrameAsync(IVideoFrame ** frame, bool * read_status) noexcept override { return _queue_submit(frame, read_status, 0, 0); }
			virtual bool ReadFrameAsync(IVideoFrame ** frame, bool * read_status, IDispatchTask * execute_on_processed) noexcept override { return _queue_submit(frame, read_status, execute_on_processed, 0); }
			virtual bool ReadFrameAsync(IVideoFrame ** frame, bool * read_status, Semaphore * open_on_processed) noexcept override { return _queue_submit(frame, read_status, 0, open_on_processed); }
		};
		class MediaFoundationDevice : public IVideoDevice
		{
			struct device_task {
				IVideoFrame ** frame;
				bool * result;
				IDispatchTask * task;
				Semaphore * open;
				device_task * next;
			};
			VideoObjectDesc _desc;
			IMFMediaSource * _source;
			IMFMediaType * _type;
			IMFMediaStream * _stream;
			IMFPresentationDescriptor * _presentation;
			SafePointer<Thread> _thread;
			SafePointer<Semaphore> _counter, _access;
			string _ident;
			int _state;
			bool _initialized;
			bool _thread_dead;
			uint _stream_time;
			device_task * _first, * _last;

			void _dequeue_next(IVideoFrame * retval, bool status) noexcept
			{
				_access->Wait();
				auto obj = _first;
				_first = _first->next;
				if (!_first) _last = 0;
				_access->Open();
				if (retval) retval->Retain();
				*obj->frame = retval;
				if (obj->result) *obj->result = status;
				if (obj->open) {
					obj->open->Open();
					obj->open->Release();
				}
				if (obj->task) {
					obj->task->DoTask(0);
					obj->task->Release();
				}
				delete obj;
			}
			void _drain(bool thread_failure_flag) noexcept
			{
				if (thread_failure_flag) _thread_dead = true;
				while (_counter->TryWait()) _dequeue_next(0, false);
			}
			void _output_sample(IMFSample * sample) noexcept
			{
				SafePointer<IVideoFrame> result;
				bool status;
				try {
					result = new MediaFoundationFrame(_type, sample);
					result->SetTimeScale(_desc.TimeScale);
					result->SetFrameDuration(_desc.FrameDuration);
					result->SetFramePresentation(_stream_time * _desc.FrameDuration);
					_stream_time++;
					status = true;
				} catch (...) { result.SetReference(0); status = false; }
				if (_counter->TryWait()) _dequeue_next(result, status);
			}
			bool _queue_submit(IVideoFrame ** frame, bool * status, IDispatchTask * task, Semaphore * open) noexcept
			{
				if (!frame || _thread_dead) return false;
				auto obj = new (std::nothrow) device_task;
				if (!obj) return false;
				obj->frame = frame;
				obj->result = status;
				obj->task = task;
				obj->open = open;
				obj->next = 0;
				if (task) task->Retain();
				if (open) open->Retain();
				_access->Wait();
				if (_last) _last->next = obj; else _first = _last = obj;
				_counter->Open();
				_access->Open();
				return true;
			}
			static int _dispatch_thread(void * arg)
			{
				auto self = reinterpret_cast<MediaFoundationDevice *>(arg);
				while (true) {
					IMFMediaEvent * event;
					HRESULT status;
					if (!self->_stream) status = self->_source->GetEvent(0, &event);
					else status = self->_stream->GetEvent(0, &event);
					if (status == S_OK) {
						MediaEventType type;
						if (event->GetType(&type) == S_OK) {
							if (type == MEStreamStarted) {
								if (self->_stream->RequestSample(0) != S_OK) {
									self->_drain(true);
									event->Release();
									return 1;
								}
							} else if (type == MEStreamStopped) {
								self->_drain(false);
							} else if (type == MEEndOfStream) {
								event->Release();
								return 0;
							} else if (type == MEMediaSample) {
								IMFSample * sample = 0;
								PROPVARIANT var;
								PropVariantInit(&var);
								if (event->GetValue(&var) == S_OK) {
									if (var.vt == VT_UNKNOWN) var.punkVal->QueryInterface(IID_PPV_ARGS(&sample));
								}
								PropVariantClear(&var);
								if (sample) {
									self->_output_sample(sample);
									sample->Release();
								}
								if (self->_stream->RequestSample(0) != S_OK) {
									self->_drain(true);
									event->Release();
									return 1;
								}
							} else if (type == MENewStream) {
								PROPVARIANT var;
								PropVariantInit(&var);
								if (event->GetValue(&var) == S_OK) {
									if (var.vt == VT_UNKNOWN) var.punkVal->QueryInterface(IID_PPV_ARGS(&self->_stream));
								}
								PropVariantClear(&var);
								if (!self->_stream) {
									self->_drain(true);
									event->Release();
									return 1;
								}
							} else if (type == MEError) {
								self->_drain(true);
								event->Release();
								return 1;
							}
						} else {
							self->_drain(true);
							event->Release();
							return 1;
						}
						event->Release();
					} else {
						self->_drain(true);
						return 1;
					}
				}
				return 0;
			}
			void _init_device_from_id(const string & identifier)
			{
				_counter = CreateSemaphore(0);
				_access = CreateSemaphore(1);
				if (!_counter || !_access) throw Exception();
				_desc.Width = _desc.Height = _desc.FramePresentation = _desc.FrameDuration = _desc.TimeScale = 0;
				_desc.Device = 0;
				_type = 0;
				_presentation = 0;
				_stream = 0;
				_state = 0;
				_stream_time = 0;
				_first = _last = 0;
				_ident = identifier;
				_initialized = false;
				_thread_dead = false;
				IMFAttributes * attr;
				if (MFCreateAttributes(&attr, 2) != S_OK) throw Exception();
				if (attr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID) != S_OK) { attr->Release(); throw Exception(); }
				if (attr->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, _ident) != S_OK) { attr->Release(); throw Exception(); }
				if (MFCreateDeviceSource(attr, &_source) != S_OK) { attr->Release(); throw Exception(); }
				attr->Release();
				if (_source->CreatePresentationDescriptor(&_presentation) != S_OK) { _source->Shutdown(); _source->Release(); _source = 0; throw Exception(); }
			}
		public:
			MediaFoundationDevice(IVideoFactory * factory)
			{
				if (MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET) != S_OK) throw Exception();
				try {
					SafePointer< Volumes::Dictionary<string, string> > list = factory->GetAvailableDevices();
					if (!list || !list->GetRoot()) throw Exception();
					_init_device_from_id(list->GetFirst()->GetValue().key);
				} catch (...) { MFShutdown(); throw; }
			}
			MediaFoundationDevice(const string & identifier)
			{
				if (MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET) != S_OK) throw Exception();
				try { _init_device_from_id(identifier); } catch (...) { MFShutdown(); throw; }
			}
			virtual ~MediaFoundationDevice(void) override
			{
				if (_initialized) {
					StopProcessing();
					if (_source->QueueEvent(MEEndOfStream, GUID_NULL, S_OK, 0) != S_OK) throw Exception();
					if (_stream && _stream->QueueEvent(MEEndOfStream, GUID_NULL, S_OK, 0) != S_OK) throw Exception();
					_thread->Wait();
					_thread.SetReference(0);
				}
				_drain(false);
				if (_stream) _stream->Release();
				if (_presentation) _presentation->Release();
				if (_type) _type->Release();
				if (_source) {
					_source->Shutdown();
					_source->Release();
				}
				MFShutdown();
			}
			virtual const VideoObjectDesc & GetObjectDescriptor(void) const noexcept override { return _desc; }
			virtual VideoObjectType GetObjectType(void) const noexcept override { return VideoObjectType::Device; }
			virtual handle GetBufferFormat(void) const noexcept override { return _type; }
			virtual string GetDeviceIdentifier(void) const override { return _ident; }
			virtual Array<VideoObjectDesc> * GetSupportedFrameFormats(void) const noexcept override
			{
				try {
					SafePointer< Array<VideoObjectDesc> > result = new Array<VideoObjectDesc>;
					IMFStreamDescriptor * stream;
					IMFMediaTypeHandler * handler;
					IMFMediaType * type;
					BOOL selected;
					auto status = _presentation->GetStreamDescriptorByIndex(0, &selected, &stream);
					if (status != S_OK) throw Exception();
					status = stream->GetMediaTypeHandler(&handler);
					stream->Release();
					if (status != S_OK) throw Exception();
					DWORD count;
					if (handler->GetMediaTypeCount(&count) != S_OK) { handler->Release(); throw Exception(); }
					for (int i = 0; i < count; i++) {
						if (handler->GetMediaTypeByIndex(i, &type) == S_OK) {
							UINT32 width, height;
							UINT32 rate_num, rate_denom;
							try {
								if (MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width, &height) != S_OK) throw Exception();
								if (MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &rate_num, &rate_denom) != S_OK) throw Exception();
								bool present = false;
								for (auto & e : result->Elements()) {
									if (e.Width == width && e.Height == height && e.FrameDuration == rate_denom && e.TimeScale == rate_num) { present = true; break; }
								}
								if (!present) {
									VideoObjectDesc obj;
									obj.Width = width;
									obj.Height = height;
									obj.FrameDuration = rate_denom;
									obj.TimeScale = rate_num;
									obj.FramePresentation = 0;
									obj.Device = 0;
									result->Append(obj);
								}
							} catch (...) {}
							type->Release();
						}
					}
					handler->Release();
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			virtual bool SetFrameFormat(const VideoObjectDesc & vd) noexcept override
			{
				if (_initialized) return false;
				try {
					IMFStreamDescriptor * stream;
					IMFMediaTypeHandler * handler;
					IMFMediaType * type = 0;
					BOOL selected;
					auto status = _presentation->GetStreamDescriptorByIndex(0, &selected, &stream);
					if (status != S_OK) throw Exception();
					status = stream->GetMediaTypeHandler(&handler);
					stream->Release();
					if (status != S_OK) throw Exception();
					DWORD count;
					if (handler->GetMediaTypeCount(&count) != S_OK) { handler->Release(); throw Exception(); }
					for (int i = 0; i < count; i++) {
						if (handler->GetMediaTypeByIndex(i, &type) == S_OK) {
							UINT32 width, height;
							UINT32 rate_num, rate_denom;
							try {
								if (MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width, &height) != S_OK) throw Exception();
								if (MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &rate_num, &rate_denom) != S_OK) throw Exception();
								if (width == vd.Width && height == vd.Height && rate_num == vd.TimeScale && rate_denom == vd.FrameDuration) break;
							} catch (...) {}
							type->Release();
							type = 0;
						}
					}
					if (handler->SetCurrentMediaType(type) != S_OK) { type->Release(); handler->Release(); throw Exception(); }
					if (_type) _type->Release();
					_type = type;
					handler->Release();
					_desc.Width = vd.Width;
					_desc.Height = vd.Height;
					_desc.FrameDuration = vd.FrameDuration;
					_desc.TimeScale = vd.TimeScale;
					return true;
				} catch (...) { return false; }
			}
			virtual bool GetSupportedFrameRateRange(uint * min_rate_numerator, uint * min_rate_denominator, uint * max_rate_numerator, uint * max_rate_denominator) const noexcept override
			{
				if (!_type) return false;
				UINT32 mr_num, mr_den, xr_num, xr_den;
				if (MFGetAttributeRatio(_type, MF_MT_FRAME_RATE_RANGE_MIN, &mr_num, &mr_den) != S_OK) return false;
				if (MFGetAttributeRatio(_type, MF_MT_FRAME_RATE_RANGE_MAX, &xr_num, &xr_den) != S_OK) return false;
				if (min_rate_numerator) *min_rate_numerator = mr_num;
				if (min_rate_denominator) *min_rate_denominator = mr_den;
				if (max_rate_numerator) *max_rate_numerator = xr_num;
				if (max_rate_denominator) *max_rate_denominator = xr_den;
				return true;
			}
			virtual bool SetFrameRate(uint rate_numerator, uint rate_denominator) noexcept override
			{
				if (_initialized || !_type) return false;
				try {
					IMFStreamDescriptor * stream;
					IMFMediaTypeHandler * handler;
					IMFMediaType * type;
					BOOL selected;
					auto status = _presentation->GetStreamDescriptorByIndex(0, &selected, &stream);
					if (status != S_OK) throw Exception();
					status = stream->GetMediaTypeHandler(&handler);
					stream->Release();
					if (status != S_OK) throw Exception();
					if (MFCreateMediaType(&type) != S_OK) { handler->Release(); throw Exception(); }
					if (_type->CopyAllItems(type) != S_OK) { handler->Release(); type->Release(); throw Exception(); }
					if (MFSetAttributeRatio(type, MF_MT_FRAME_RATE, rate_numerator, rate_denominator) != S_OK) { handler->Release(); type->Release(); throw Exception(); }
					if (handler->SetCurrentMediaType(type) != S_OK) { handler->Release(); type->Release(); throw Exception(); }
					handler->Release();
					_type->Release();
					_type = type;
					_desc.FrameDuration = rate_denominator;
					_desc.TimeScale = rate_numerator;
					return true;
				} catch (...) { return false; }
			}
			virtual bool Initialize(void) noexcept override
			{
				if (_initialized) return true;
				if (_presentation->SelectStream(0) != S_OK) return false;
				try {
					_thread = CreateThread(_dispatch_thread, this);
					if (!_thread) return false;
				} catch (...) { return false; }
				_initialized = true;
				return true;
			}
			virtual bool StartProcessing(void) noexcept override
			{
				if (_thread_dead) return false;
				if (!_initialized && !Initialize()) return false;
				if (_state == 1) return true;
				PROPVARIANT var;
				var.vt = VT_EMPTY;
				if (_source->Start(_presentation, 0, &var) != S_OK) return false;
				_state = 1;
				_stream_time = 0;
				return true;
			}
			virtual bool PauseProcessing(void) noexcept override
			{
				if (_thread_dead) return false;
				if (!_initialized && !Initialize()) return false;
				if (!_state) return false;
				if (_state == 2) return true;
				if (_source->Pause() != S_OK) return false;
				_state = 2;
				return true;
			}
			virtual bool StopProcessing(void) noexcept override
			{
				if (_thread_dead) return false;
				if (!_initialized && !Initialize()) return false;
				if (!_state) return true;
				if (_source->Stop() != S_OK) return false;
				_state = 0;
				return true;
			}
			virtual bool ReadFrame(IVideoFrame ** frame) noexcept override
			{
				if (!frame || _thread_dead) return false;
				SafePointer<Semaphore> semaphore;
				try {
					semaphore = CreateSemaphore(0);
					if (!semaphore) return false;
				} catch (...) { return false; }
				bool status;
				if (!ReadFrameAsync(frame, &status, semaphore)) return false;
				semaphore->Wait();
				return status;
			}
			virtual bool ReadFrameAsync(IVideoFrame ** frame, bool * read_status) noexcept override { return _queue_submit(frame, read_status, 0, 0); }
			virtual bool ReadFrameAsync(IVideoFrame ** frame, bool * read_status, IDispatchTask * execute_on_processed) noexcept override { return _queue_submit(frame, read_status, execute_on_processed, 0); }
			virtual bool ReadFrameAsync(IVideoFrame ** frame, bool * read_status, Semaphore * open_on_processed) noexcept override { return _queue_submit(frame, read_status, 0, open_on_processed); }
		};
		class MediaFoundationFactory : public IVideoFactory
		{
		public:
			virtual Volumes::Dictionary<string, string> * GetAvailableDevices(void) noexcept override
			{
				SafePointer< Volumes::Dictionary<string, string> > result;
				try { result = new Volumes::Dictionary<string, string>; } catch (...) { return 0; }
				IMFAttributes * attr;
				IMFActivate ** devices;
				UINT32 count;
				if (MFCreateAttributes(&attr, 1) != S_OK) return 0;
				if (attr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID) != S_OK) { attr->Release(); return 0; }
				if (MFEnumDeviceSources(attr, &devices, &count) != S_OK) { attr->Release(); return 0; }
				attr->Release();
				for (int i = 0; i < count; i++) {
					UINT32 name_len, id_len;
					DynamicString name, id;
					if (devices[i]->GetStringLength(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name_len) == S_OK) {
						if (devices[i]->GetStringLength(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &id_len) == S_OK) {
							try {
								name.ReserveLength(name_len + 1);
								if (devices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, name, name.ReservedLength(), 0) != S_OK) throw Exception();
								id.ReserveLength(id_len + 1);
								if (devices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, id, id.ReservedLength(), 0) != S_OK) throw Exception();
								result->Append(id, name);
							} catch (...) {}
						}
					}
					devices[i]->Release();
				}
				CoTaskMemFree(devices);
				result->Retain();
				return result;
			}
			virtual IVideoDevice * CreateDevice(const string & identifier, Graphics::IDevice * acceleration_device) noexcept override { try { return new MediaFoundationDevice(identifier); } catch (...) { return 0; } }
			virtual IVideoDevice * CreateDefaultDevice(Graphics::IDevice * acceleration_device) noexcept override { try { return new MediaFoundationDevice(this); } catch (...) { return 0; } }
			virtual IVideoDevice * CreateScreenCaptureDevice(Windows::IScreen * screen, Graphics::IDevice * acceleration_device) noexcept override { try { return new ScreenCaptureDevice(screen, acceleration_device); } catch (...) { return 0; } }
			virtual IVideoFrame * CreateFrame(Codec::Frame * frame, Graphics::IDevice * acceleration_device) noexcept override
			{
				try {
					VideoObjectDesc desc;
					desc.Width = frame->GetWidth();
					desc.Height = frame->GetHeight();
					if (acceleration_device) {
						SafePointer<Graphics::ITexture> texture = Graphics::LoadTexture(acceleration_device, frame, 1,
							Graphics::ResourceUsageShaderRead, Graphics::PixelFormat::B8G8R8A8_unorm, Graphics::ResourceMemoryPool::Default);
						if (!texture) return CreateFrame(frame, 0);
						IMFSample * sample = MediaFoundationFrameBlt::MakeDXGISample(texture, Graphics::SubresourceIndex(0, 0));
						if (!sample) return CreateFrame(frame, 0);
						IMFMediaType * type = MediaFoundationFrameBlt::ProduceFormat(Graphics::PixelFormat::B8G8R8A8_unorm, Codec::AlphaMode::Straight, desc);
						if (!type) { sample->Release(); return 0; }
						SafePointer<IVideoFrame> result;
						desc.Device = acceleration_device;
						try { result = new MediaFoundationFrame(type, sample, desc.Device); } catch (...) {}
						sample->Release();
						type->Release();
						if (!result) return 0;
						result->Retain();
						return result;
					} else {
						SafePointer<Codec::Frame> image;
						bool convert = false;
						if (frame->GetPixelFormat() != Codec::PixelFormat::B8G8R8A8) convert = true;
						else if (frame->GetScanOrigin() != Codec::ScanOrigin::TopDown) convert = true;
						else if (frame->GetScanLineLength() != frame->GetWidth() * 4) convert = true;
						if (convert) {
							image = frame->ConvertFormat(Codec::PixelFormat::B8G8R8A8, Codec::ScanOrigin::TopDown);
						} else image.SetRetain(frame);
						auto length = image->GetHeight() * image->GetScanLineLength();
						IMFMediaBuffer * buffer;
						if (MFCreateMemoryBuffer(length, &buffer) != S_OK) return 0;
						if (buffer->SetCurrentLength(length) != S_OK) { buffer->Release(); return 0; }
						LPBYTE data_ptr;
						if (buffer->Lock(&data_ptr, 0, 0) != S_OK) { buffer->Release(); return 0; }
						MemoryCopy(data_ptr, image->GetData(), length);
						buffer->Unlock();
						IMFSample * sample;
						if (MFCreateSample(&sample) != S_OK) { buffer->Release(); return 0; }
						if (sample->AddBuffer(buffer) != S_OK) { buffer->Release(); sample->Release(); return 0; }
						buffer->Release();
						IMFMediaType * type = MediaFoundationFrameBlt::ProduceFormat(Graphics::PixelFormat::B8G8R8A8_unorm, image->GetAlphaMode(), desc);
						if (!type) { sample->Release(); return 0; }
						SafePointer<IVideoFrame> result;
						try { result = new MediaFoundationFrame(type, sample); } catch (...) {}
						sample->SetSampleTime(0);
						sample->SetSampleDuration(1);
						sample->Release();
						type->Release();
						if (!result) return 0;
						result->Retain();
						return result;
					}
				} catch (...) { return 0; }
			}
			virtual IVideoFrame * CreateFrame(Graphics::PixelFormat format, Codec::AlphaMode alpha, const VideoObjectDesc & desc) noexcept override
			{
				if (format != Graphics::PixelFormat::B8G8R8A8_unorm) return 0;
				if (!desc.Width || !desc.Height) return 0;
				if (desc.Device) {
					Graphics::TextureDesc txdesc;
					txdesc.Type = Graphics::TextureType::Type2D;
					txdesc.Format = Graphics::PixelFormat::B8G8R8A8_unorm;
					txdesc.Width = desc.Width;
					txdesc.Height = desc.Height;
					txdesc.MipmapCount = 1;
					txdesc.Usage = Graphics::ResourceUsageShaderRead;
					txdesc.MemoryPool = Graphics::ResourceMemoryPool::Default;
					SafePointer<Graphics::ITexture> texture = desc.Device->CreateTexture(txdesc);
					if (!texture) {
						VideoObjectDesc alt = desc;
						alt.Device = 0;
						return CreateFrame(format, alpha, alt);
					}
					IMFSample * sample = MediaFoundationFrameBlt::MakeDXGISample(texture, Graphics::SubresourceIndex(0, 0));
					if (!sample) {
						VideoObjectDesc alt = desc;
						alt.Device = 0;
						return CreateFrame(format, alpha, alt);
					}
					IMFMediaType * type = MediaFoundationFrameBlt::ProduceFormat(format, alpha, desc);
					if (!type) { sample->Release(); return 0; }
					SafePointer<IVideoFrame> result;
					try { result = new MediaFoundationFrame(type, sample, desc.Device); } catch (...) {}
					sample->Release();
					type->Release();
					if (!result) return 0;
					result->SetTimeScale(desc.TimeScale);
					result->SetFramePresentation(desc.FramePresentation);
					result->SetFrameDuration(desc.FrameDuration);
					result->Retain();
					return result;
				} else {
					auto length = desc.Height * desc.Width * 4;
					IMFMediaBuffer * buffer;
					if (MFCreateMemoryBuffer(length, &buffer) != S_OK) return 0;
					if (buffer->SetCurrentLength(length) != S_OK) { buffer->Release(); return 0; }
					IMFSample * sample;
					if (MFCreateSample(&sample) != S_OK) { buffer->Release(); return 0; }
					if (sample->AddBuffer(buffer) != S_OK) { buffer->Release(); sample->Release(); return 0; }
					buffer->Release();
					IMFMediaType * type = MediaFoundationFrameBlt::ProduceFormat(format, alpha, desc);
					if (!type) { sample->Release(); return 0; }
					SafePointer<IVideoFrame> result;
					try { result = new MediaFoundationFrame(type, sample); } catch (...) {}
					sample->SetSampleTime(0);
					sample->SetSampleDuration(1);
					sample->Release();
					type->Release();
					if (!result) return 0;
					result->Retain();
					return result;
				}
			}
			virtual IVideoFrameBlt * CreateFrameBlt(void) noexcept override { try { return new MediaFoundationFrameBlt; } catch (...) { return 0; } }
		};

		SafePointer<IVideoCodec> _system_codec;
		IVideoCodec * InitializeSystemCodec(void)
		{
			if (!_system_codec) {
				_system_codec = new MediaFoundationCodec;
				RegisterCodec(_system_codec);
			}
			return _system_codec;
		}
		IVideoFactory * CreateSystemVideoFactory(void) { return new MediaFoundationFactory; }
	}
}