#include "../Interfaces/SystemGraphics.h"

#include "../PlatformDependent/SystemCodecs.h"
#include "../PlatformDependent/Direct2D.h"
#include "../PlatformDependent/Direct3D.h"
#include "../ImageCodec/IconCodec.h"
#include "../Storage/ImageVolume.h"

namespace Engine
{
	namespace Graphics
	{
		I2DDeviceContextFactory * CreateDeviceContextFactory(void)
		{
			Codec::InitializeDefaultCodecs();
			Direct2D::InitializeFactory();
			Direct2D::CommonFactory->Retain();
			return Direct2D::CommonFactory;
		}
		IDeviceFactory * CreateDeviceFactory(void) { if (!Direct3D::CommonFactory) Direct3D::CreateCommonDeviceFactory(); if (Direct3D::CommonFactory) Direct3D::CommonFactory->Retain(); return Direct3D::CommonFactory; }
		IDevice * GetCommonDevice(void) { if (!Direct3D::CommonDevice) Direct3D::CreateCommonDevice(); return Direct3D::CommonDevice; }
		void ResetCommonDevice(void) { Direct3D::CommonDevice.SetReference(0); Direct3D::CreateCommonDevice(); }
	}
	namespace Codec
	{
		void InitializeDefaultCodecs(void)
		{
			WIC::CreateWICodec();
			Codec::CreateIconCodec();
			Storage::CreateVolumeCodec();
		}
	}
}