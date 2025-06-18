#pragma once

#include "../Graphics/PresentationCore.h"
#include "../Graphics/GraphicsBase.h"

#include <dwmapi.h>
#include <d3d11.h>

namespace Engine
{
	namespace Windows
	{
		typedef void (* func_RenderLayerCallback) (IPresentationEngine * engine, ICoreWindow * window);
		enum WindowBackgroundModes : uint {
			WindowBackgroundModeSetMargins		= 0x01,
			WindowBackgroundModeSetBlurBehind	= 0x02,
			WindowBackgroundModeClearBackground	= 0x04,
			WindowBackgroundModeResetBackground	= 0x08
		};
		enum CreateWindowLayersFlags : uint {
			CreateWindowLayersAlphaModeIgnore			= 0x0001,
			CreateWindowLayersAlphaModePremultiplied	= 0x0002,
			CreateWindowLayersAlphaModeStraight			= 0x0004,
			CreateWindowLayersTransparentBackground		= 0x0010,
			CreateWindowLayersBlurBehind				= 0x0020,
			CreateWindowLayersFormatB8G8R8A8			= 0x0100,
			CreateWindowLayersFormatR8G8B8A8			= 0x0200,
			CreateWindowLayersFormatR10G10B10A2			= 0x0400,
			CreateWindowLayersFormatR16G16B16A16		= 0x0800
		};
		struct CreateWindowLayersDesc
		{
			HWND window;
			uint flags;
			ID3D11Device * device;
			uint width;
			uint height;
			double deviation;
		};
		class IWindowLayers : public Object
		{
		public:
			virtual bool Resize(uint width, uint height) noexcept = 0;
			virtual bool BeginDraw(ID3D11Texture2D ** surface, uint * orgx, uint * orgy) noexcept = 0;
			virtual bool EndDraw(void) noexcept = 0;
		};

		void GetWindowSurfaceFlags(ICoreWindow * window, uint * mode, Color * clear_color, MARGINS ** margins) noexcept;
		void SetWindowRenderingCallback(ICoreWindow * window, func_RenderLayerCallback callback) noexcept;
		void SetWindowUserRenderingCallback(ICoreWindow * window) noexcept;
		void SetWindowLayers(ICoreWindow * window, IWindowLayers * layers) noexcept;
		void GetWindowLayers(ICoreWindow * window, IWindowLayers ** layers, double * factor) noexcept;

		bool LayeredSubsystemInitialize(void) noexcept;
		IWindowLayers * CreateWindowLayers(const CreateWindowLayersDesc & desc) noexcept;
	}
}