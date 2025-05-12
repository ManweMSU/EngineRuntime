#pragma once

#include "../Graphics/GraphicsBase.h"
#include "CoreX11.h"

#include <X11/extensions/Xrender.h>

namespace Engine
{
	namespace X11
	{
		class IXTexture : public UI::ITexture
		{
		public:
			virtual Codec::Frame * GetFrameSurface(void) noexcept = 0;
			virtual Pixmap GetPixmapSurface(void) noexcept = 0;
		};
		class IXRenderingDevice : public UI::ITextureRenderingDevice
		{
		public:
			virtual bool SetRenderTarget(Drawable surface, int width, int height) noexcept = 0;
			virtual bool SetRenderTarget(Drawable surface, int width, int height, XRenderPictFormat * format) noexcept = 0;
			virtual bool SetPixmapSurface(Pixmap pixmap) noexcept = 0;
			virtual void UnsetRenderTarget(void) noexcept = 0;
			virtual bool XBeginDraw(void) noexcept = 0;
			virtual bool XEndDraw(void) noexcept = 0;
			virtual bool GetState(void) noexcept = 0;
		};

		IXRenderingDevice * CreateXDevice(XServerConnection * con) noexcept;
		Cursor LoadCursor(XServerConnection * con, Codec::Frame * frame) noexcept;
		Pixmap LoadPixmap(XServerConnection * con, Codec::Frame * frame) noexcept;
		Pixmap CreatePixmap(XServerConnection * con, int width, int height, UI::Color color) noexcept;
		Codec::Frame * QueryPixmapSurface(XServerConnection * con, Pixmap pixmap, int width, int height, int xorg = 0, int yorg = 0) noexcept;

		UI::ITexture * LoadTexture(XServerConnection * con, Codec::Frame * source) noexcept;
		UI::IFont * LoadFont(XServerConnection * con, const string & face_name, int height, int weight, bool italic, bool underline, bool strikeout) noexcept;
		Array<string> * GetFontFamilies(XServerConnection * con) noexcept;
		UI::ITextureRenderingDevice * CreateTextureRenderingDevice(XServerConnection * con, int width, int height, UI::Color color) noexcept;
		UI::ITextureRenderingDevice * CreateTextureRenderingDevice(XServerConnection * con, Codec::Frame * frame) noexcept;

		UI::IObjectFactory * CreateGraphicsObjectFactory(void);
	}
}