#pragma once

#include "PresentationCore.h"
#include "../Streaming.h"
#include "../Miscellaneous/Volumes.h"

namespace Engine
{
	namespace Graphics
	{
		class IDevice;
		class IDeviceChild;
		class IShader;
		class IShaderLibrary;
		class IPipelineState;
		class ISamplerState;
		class IDeviceResource;
		class IBuffer;
		class ITexture;
		class IWindowLayer;
		class IDeviceContext;
		class IDeviceResourceHandle;
		class IDeviceFactory;
		class I2DDeviceContext;

		enum class DeviceClass { Unknown = 0, Integrated = 1, Discrete = 2, Software = 3 };
		enum class PixelFormatUsage { ShaderRead = 0, ShaderSample = 1, RenderTarget = 2, BlendRenderTarget = 3, DepthStencil = 4, WindowSurface = 5, RenderTarget2D = 6, BitmapSource = 7, VideoIO = 8 };
		enum class PixelFormat : uint32 {
			Invalid = 0x00000000,
			// Color formats
			// 8 bpp
			A8_unorm = 0x81100001,
			R8_unorm = 0x81100002, R8_snorm = 0x81100003, R8_uint = 0x81100004, R8_sint = 0x81100005,

			// 16 bpp
			R16_unorm = 0x82100001, R16_snorm = 0x82100002, R16_uint = 0x82100003, R16_sint = 0x82100004, R16_float = 0x82100005,
			R8G8_unorm = 0x82200001, R8G8_snorm = 0x82200002, R8G8_uint = 0x82200003, R8G8_sint = 0x82200004,
			B5G6R5_unorm = 0x82300001, R5G6B5_unorm = 0x82300002,
			B5G5R5A1_unorm = 0x82400001, R5G5B5A1_unorm = 0x82400002, A1B5G5R5_unorm = 0x82400003, A1R5G5B5_unorm = 0x82400004,
			B4G4R4A4_unorm = 0x82400005, R4G4B4A4_unorm = 0x82400006, A4B4G4R4_unorm = 0x82400007, A4R4G4B4_unorm = 0x82400008,

			// 32 bpp
			R32_uint = 0x83100001, R32_sint = 0x83100002, R32_float = 0x83100003,
			R16G16_unorm = 0x83200001, R16G16_snorm = 0x83200002, R16G16_uint = 0x83200003, R16G16_sint = 0x83200004, R16G16_float = 0x83200005,
			B8G8R8A8_unorm = 0x83400001,
			R8G8B8A8_unorm = 0x83400002, R8G8B8A8_snorm = 0x83400003, R8G8B8A8_uint = 0x83400004, R8G8B8A8_sint = 0x83400005,
			R10G10B10A2_unorm = 0x83400006, R10G10B10A2_uint = 0x83400007, A2R10G10B10_unorm = 0x83400008, A2R10G10B10_uint = 0x83400009,
			R11G11B10_float = 0x83300001, B10R11G11_float = 0x83300003,
			R9G9B9E5_float = 0x83300002, E5B9G9R9_float = 0x83300004,

			// 64 bpp
			R32G32_uint = 0x84200001, R32G32_sint = 0x84200002, R32G32_float = 0x84200003,
			R16G16B16A16_unorm = 0x84400001, R16G16B16A16_snorm = 0x84400002, R16G16B16A16_uint = 0x84400003, R16G16B16A16_sint = 0x84400004, R16G16B16A16_float = 0x84400005,

			// 128 bpp
			R32G32B32A32_uint = 0x85400001, R32G32B32A32_sint = 0x85400002, R32G32B32A32_float = 0x85400003,

			// Depth/Stencil formats
			D16_unorm = 0x42100001, D24_unorm = 0x42100002, D32_float = 0x43100001, D16S8_unorm = 0x42200001, D24S8_unorm = 0x43200001, D32S8_float = 0x44200001
		};
		enum class SamplerFilter { Point = 0, Linear = 1, Anisotropic = 2 };
		enum class SamplerAddressMode { Wrap = 0, Mirror = 1, Clamp = 2, Border = 3 };
		enum class ShaderType { Unknown = 0, Vertex = 1, Pixel = 2 };
		enum class ShaderError { Success = 0, Unknown = 1, IO = 2, InvalidContainerData = 3, NoCompiler = 4, NoPlatformVersion = 5, Compilation = 6 };
		enum class BlendingFactor {
			Zero = 0, One = 1,
			OverColor = 2, InvertedOverColor = 3, OverAlpha = 4, InvertedOverAlpha = 5,
			BaseColor = 6, InvertedBaseColor = 7, BaseAlpha = 8, InvertedBaseAlpha = 9,
			SecondaryColor = 10, InvertedSecondaryColor = 11, SecondaryAlpha = 12, InvertedSecondaryAlpha = 13,
			OverAlphaSaturated = 14
		};
		enum class BlendingFunction { Add = 0, SubtractOverFromBase = 1, SubtractBaseFromOver = 2, Min = 3, Max = 4 };
		enum class CompareFunction { Always = 0, Lesser = 1, Greater = 2, Equal = 3, LesserEqual = 4, GreaterEqual = 5, NotEqual = 6, Never = 7 };
		enum class StencilFunction { Keep = 0, SetZero = 1, Replace = 2, IncrementWrap = 3, DecrementWrap = 4, IncrementClamp = 5, DecrementClamp = 6, Invert = 7 };
		enum class FillMode { Solid = 0, Wireframe = 1 };
		enum class CullMode { None = 0, Front = 1, Back = 2 };
		enum class ResourceType { Buffer = 0, Texture = 1 };
		enum class ResourceMemoryPool { Default = 0, Immutable = 1, Shared = 2 };
		enum class TextureType { Type1D = 0, TypeArray1D = 1, Type2D = 2, TypeArray2D = 3, TypeCube = 4, TypeArrayCube = 5, Type3D = 6 };
		enum class PrimitiveTopology { PointList = 0, LineList = 1, LineStrip = 2, TriangleList = 3, TriangleStrip = 4 };
		enum class TextureLoadAction { DontCare = 0, Load = 1, Clear = 2 };
		enum class IndexBufferFormat { UInt16 = 0, UInt32 = 1 };
		
		enum RenderTargetFlags : uint {
			RenderTargetFlagBlendingEnabled = 0x00000001,
			RenderTargetFlagRestrictWriteRed = 0x00000002,
			RenderTargetFlagRestrictWriteGreen = 0x00000004,
			RenderTargetFlagRestrictWriteBlue = 0x00000008,
			RenderTargetFlagRestrictWriteAlpha = 0x00000010
		};
		enum DepthStencilFlags : uint {
			DepthStencilFlagDepthTestEnabled = 0x00000001,
			DepthStencilFlagStencilTestEnabled = 0x00000004,
			DepthStencilFlagDepthWriteEnabled = 0x00000002
		};
		enum ResourceUsage : uint {
			ResourceUsageShaderRead = 0x00000001,
			ResourceUsageShaderWrite = 0x00000002,
			ResourceUsageConstantBuffer = 0x00000004,
			ResourceUsageIndexBuffer = 0x00000008,
			ResourceUsageRenderTarget = 0x00000010,
			ResourceUsageDepthStencil = 0x00000020,
			ResourceUsageCPURead = 0x00000040,
			ResourceUsageCPUWrite = 0x00000080,
			ResourceUsageVideoRead = 0x00000100,
			ResourceUsageVideoWrite = 0x00000200,
			ResourceUsageShaderAll = ResourceUsageShaderRead | ResourceUsageShaderWrite,
			ResourceUsageCPUAll = ResourceUsageCPURead | ResourceUsageCPUWrite,
			ResourceUsageVideoAll = ResourceUsageVideoRead | ResourceUsageVideoWrite,
			ResourceUsageBufferMask = ResourceUsageShaderAll | ResourceUsageConstantBuffer | ResourceUsageIndexBuffer | ResourceUsageCPUAll,
			ResourceUsageTextureMask = ResourceUsageShaderAll | ResourceUsageRenderTarget | ResourceUsageDepthStencil | ResourceUsageCPUAll | ResourceUsageVideoAll
		};
		enum WindowLayerAttributes : uint {
			WindowLayerAttributeAlphaChannelIgnore			= 0x01000000,
			WindowLayerAttributeAlphaChannelStraight		= 0x02000000,
			WindowLayerAttributeAlphaChannelPremultiplied	= 0x04000000,
			WindowLayerAttributeExtendedDynamicRange		= 0x08000000,
			WindowLayerAttributeMask						= 0xFF000000
		};

		struct SamplerDesc
		{
			SamplerFilter MinificationFilter;
			SamplerFilter MagnificationFilter;
			SamplerFilter MipFilter;
			SamplerAddressMode AddressU;
			SamplerAddressMode AddressV;
			SamplerAddressMode AddressW;
			uint32 MaximalAnisotropy;
			float MinimalLOD;
			float MaximalLOD;
			float BorderColor[4];
		};
		struct RenderTargetDesc
		{
			PixelFormat Format;
			uint32 Flags;
			BlendingFunction BlendRGB;
			BlendingFunction BlendAlpha;
			BlendingFactor BaseFactorRGB;
			BlendingFactor BaseFactorAlpha;
			BlendingFactor OverFactorRGB;
			BlendingFactor OverFactorAlpha;
		};
		struct StencilDesc
		{
			CompareFunction TestFunction;
			StencilFunction OnStencilTestFailed;
			StencilFunction OnDepthTestFailed;
			StencilFunction OnTestsPassed;
		};
		struct DepthStencilDesc
		{
			PixelFormat Format;
			uint32 Flags;
			CompareFunction DepthTestFunction;
			uint8 StencilWriteMask;
			uint8 StencilReadMask;
			StencilDesc FrontStencil;
			StencilDesc BackStencil;
		};
		struct RasterizationDesc
		{
			FillMode Fill;
			CullMode Cull;
			bool FrontIsCounterClockwise;
			int DepthBias;
			float DepthBiasClamp;
			float SlopeScaledDepthBias;
			bool DepthClipEnable;
		};
		struct PipelineStateDesc
		{
			IShader * VertexShader;
			IShader * PixelShader;
			uint32 RenderTargetCount;
			RenderTargetDesc RenderTarget[8];
			DepthStencilDesc DepthStencil;
			RasterizationDesc Rasterization;
			PrimitiveTopology Topology;
		};
		struct BufferDesc
		{
			uint32 Length;
			uint32 Stride;
			uint32 Usage;
			ResourceMemoryPool MemoryPool;
		};
		struct TextureDesc
		{
			TextureType Type;
			PixelFormat Format;
			uint32 Width;
			uint32 Height;
			union { uint32 Depth; uint32 ArraySize; };
			uint32 MipmapCount;
			uint32 Usage;
			ResourceMemoryPool MemoryPool;
		};
		struct WindowLayerDesc
		{
			PixelFormat Format;
			uint32 Width;
			uint32 Height;
			uint32 Usage;
		};
		struct ResourceInitDesc
		{
			const void * Data;
			intptr DataPitch;
			intptr DataSlicePitch;
		};
		struct ResourceDataDesc
		{
			void * Data;
			intptr DataPitch;
			intptr DataSlicePitch;
		};
		struct RenderTargetViewDesc
		{
			ITexture * Texture;
			TextureLoadAction LoadAction;
			float ClearValue[4];
		};
		struct DepthStencilViewDesc
		{
			ITexture * Texture;
			TextureLoadAction DepthLoadAction;
			TextureLoadAction StencilLoadAction;
			float DepthClearValue;
			uint8 StencilClearValue;
		};

		class VolumeIndex
		{
		public:
			uint32 x, y, z;
			VolumeIndex(void);
			VolumeIndex(uint32 sx);
			VolumeIndex(uint32 sx, uint32 sy);
			VolumeIndex(uint32 sx, uint32 sy, uint32 sz);
		};
		class SubresourceIndex
		{
		public:
			uint32 mip_level, array_index;
			SubresourceIndex(void);
			SubresourceIndex(uint32 mip, uint32 index);
		};

		class IDevice : public Object
		{
		public:
			virtual string GetDeviceName(void) noexcept = 0;
			virtual uint64 GetDeviceIdentifier(void) noexcept = 0;
			virtual bool DeviceIsValid(void) noexcept = 0;
			virtual void GetImplementationInfo(string & tech, uint32 & version_major, uint32 & version_minor) noexcept = 0;
			virtual DeviceClass GetDeviceClass(void) noexcept = 0;
			virtual uint64 GetDeviceMemory(void) noexcept = 0;
			virtual bool GetDevicePixelFormatSupport(PixelFormat format, PixelFormatUsage usage) noexcept = 0;
			virtual IShaderLibrary * LoadShaderLibrary(const void * data, int length) noexcept = 0;
			virtual IShaderLibrary * LoadShaderLibrary(const DataBlock * data) noexcept = 0;
			virtual IShaderLibrary * LoadShaderLibrary(Streaming::Stream * stream) noexcept = 0;
			virtual IShaderLibrary * CompileShaderLibrary(const void * data, int length, ShaderError * error) noexcept = 0;
			virtual IShaderLibrary * CompileShaderLibrary(const DataBlock * data, ShaderError * error) noexcept = 0;
			virtual IShaderLibrary * CompileShaderLibrary(Streaming::Stream * stream, ShaderError * error) noexcept = 0;
			virtual IDeviceContext * GetDeviceContext(void) noexcept = 0;
			virtual IPipelineState * CreateRenderingPipelineState(const PipelineStateDesc & desc) noexcept = 0;
			virtual ISamplerState * CreateSamplerState(const SamplerDesc & desc) noexcept = 0;
			virtual IBuffer * CreateBuffer(const BufferDesc & desc) noexcept = 0;
			virtual IBuffer * CreateBuffer(const BufferDesc & desc, const ResourceInitDesc & init) noexcept = 0;
			virtual ITexture * CreateTexture(const TextureDesc & desc) noexcept = 0;
			virtual ITexture * CreateTexture(const TextureDesc & desc, const ResourceInitDesc * init) noexcept = 0;
			virtual ITexture * CreateRenderTargetView(ITexture * texture, uint32 mip_level, uint32 array_offset_or_depth) noexcept = 0;
			virtual IDeviceResource * OpenResource(IDeviceResourceHandle * handle) noexcept = 0;
			virtual IWindowLayer * CreateWindowLayer(Windows::ICoreWindow * window, const WindowLayerDesc & desc) noexcept = 0;
		};
		class IDeviceChild : public Object
		{
		public:
			virtual IDevice * GetParentDevice(void) noexcept = 0;
		};
		class IShader : public IDeviceChild
		{
		public:
			virtual string GetName(void) noexcept = 0;
			virtual ShaderType GetType(void) noexcept = 0;
		};
		class IShaderLibrary : public IDeviceChild
		{
		public:
			virtual Array<string> * GetShaderNames(void) noexcept = 0;
			virtual IShader * CreateShader(const string & name) noexcept = 0;
		};
		class IPipelineState : public IDeviceChild {};
		class ISamplerState : public IDeviceChild {};
		class IDeviceResource : public IDeviceChild
		{
		public:
			virtual ResourceType GetResourceType(void) noexcept = 0;
			virtual ResourceMemoryPool GetMemoryPool(void) noexcept = 0;
			virtual uint32 GetResourceUsage(void) noexcept = 0;
		};
		class IBuffer : public IDeviceResource
		{
		public:
			virtual uint32 GetLength(void) noexcept = 0;
		};
		class ITexture : public IDeviceResource
		{
		public:
			virtual TextureType GetTextureType(void) noexcept = 0;
			virtual PixelFormat GetPixelFormat(void) noexcept = 0;
			virtual uint32 GetWidth(void) noexcept = 0;
			virtual uint32 GetHeight(void) noexcept = 0;
			virtual uint32 GetDepth(void) noexcept = 0;
			virtual uint32 GetMipmapCount(void) noexcept = 0;
			virtual uint32 GetArraySize(void) noexcept = 0;
		};
		class IWindowLayer : public IDeviceChild
		{
		public:
			virtual bool Present(void) noexcept = 0;
			virtual ITexture * QuerySurface(void) noexcept = 0;
			virtual bool ResizeSurface(uint32 width, uint32 height) noexcept = 0;
			virtual bool SwitchToFullscreen(void) noexcept = 0;
			virtual bool SwitchToWindow(void) noexcept = 0;
			virtual bool IsFullscreen(void) noexcept = 0;
			virtual uint GetLayerAttributes(void) noexcept = 0;
		};
		class IDeviceContext : public IDeviceChild
		{
		public:
			virtual bool BeginRenderingPass(uint32 rtc, const RenderTargetViewDesc * rtv, const DepthStencilViewDesc * dsv) noexcept = 0;
			virtual bool Begin2DRenderingPass(const RenderTargetViewDesc & rtv) noexcept = 0;
			virtual bool BeginMemoryManagementPass(void) noexcept = 0;
			virtual bool EndCurrentPass(void) noexcept = 0;
			virtual void Flush(void) noexcept = 0;

			virtual void SetRenderingPipelineState(IPipelineState * state) noexcept = 0;
			virtual void SetViewport(float top_left_x, float top_left_y, float width, float height, float min_depth, float max_depth) noexcept = 0;
			virtual void SetVertexShaderResource(uint32 at, IDeviceResource * resource) noexcept = 0;
			virtual void SetVertexShaderConstant(uint32 at, IBuffer * buffer) noexcept = 0;
			virtual void SetVertexShaderConstant(uint32 at, const void * data, int length) noexcept = 0;
			virtual void SetVertexShaderSamplerState(uint32 at, ISamplerState * sampler) noexcept = 0;
			virtual void SetPixelShaderResource(uint32 at, IDeviceResource * resource) noexcept = 0;
			virtual void SetPixelShaderConstant(uint32 at, IBuffer * buffer) noexcept = 0;
			virtual void SetPixelShaderConstant(uint32 at, const void * data, int length) noexcept = 0;
			virtual void SetPixelShaderSamplerState(uint32 at, ISamplerState * sampler) noexcept = 0;
			virtual void SetIndexBuffer(IBuffer * index, IndexBufferFormat format) noexcept = 0;
			virtual void SetStencilReferenceValue(uint8 ref) noexcept = 0;
			virtual void DrawPrimitives(uint32 vertex_count, uint32 first_vertex) noexcept = 0;
			virtual void DrawInstancedPrimitives(uint32 vertex_count, uint32 first_vertex, uint32 instance_count, uint32 first_instance) noexcept = 0;
			virtual void DrawIndexedPrimitives(uint32 index_count, uint32 first_index, uint32 base_vertex) noexcept = 0;
			virtual void DrawIndexedInstancedPrimitives(uint32 index_count, uint32 first_index, uint32 base_vertex, uint32 instance_count, uint32 first_instance) noexcept = 0;

			virtual I2DDeviceContext * Get2DContext(void) noexcept = 0;

			virtual void GenerateMipmaps(ITexture * texture) noexcept = 0;
			virtual void CopyResourceData(IDeviceResource * dest, IDeviceResource * src) noexcept = 0;
			virtual void CopySubresourceData(IDeviceResource * dest, SubresourceIndex dest_subres, VolumeIndex dest_origin, IDeviceResource * src, SubresourceIndex src_subres, VolumeIndex src_origin, VolumeIndex size) noexcept = 0;
			virtual void UpdateResourceData(IDeviceResource * dest, SubresourceIndex subres, VolumeIndex origin, VolumeIndex size, const ResourceInitDesc & src) noexcept = 0;
			virtual void QueryResourceData(const ResourceDataDesc & dest, IDeviceResource * src, SubresourceIndex subres, VolumeIndex origin, VolumeIndex size) noexcept = 0;

			virtual bool AcquireSharedResource(IDeviceResource * rsrc) noexcept = 0;
			virtual bool AcquireSharedResource(IDeviceResource * rsrc, uint32 timeout) noexcept = 0;
			virtual bool ReleaseSharedResource(IDeviceResource * rsrc) noexcept = 0;
		};
		class IDeviceResourceHandle : public Object
		{
		public:
			virtual uint64 GetDeviceIdentifier(void) noexcept = 0;
			virtual DataBlock * Serialize(void) noexcept = 0;
		};
		class IDeviceFactory : public Object
		{
		public:
			virtual Volumes::Dictionary<uint64, string> * GetAvailableDevices(void) noexcept = 0;
			virtual IDevice * CreateDevice(uint64 identifier) noexcept = 0;
			virtual IDevice * CreateDefaultDevice(void) noexcept = 0;
			virtual IDeviceResourceHandle * QueryResourceHandle(IDeviceResource * resource) noexcept = 0;
			virtual IDeviceResourceHandle * OpenResourceHandle(const DataBlock * data) noexcept = 0;
		};

		bool IsColorFormat(PixelFormat format);
		bool IsDepthStencilFormat(PixelFormat format);
		int GetFormatChannelCount(PixelFormat format);
		int GetFormatBitsPerPixel(PixelFormat format);
	}
}

#include "GraphicsBase.h"