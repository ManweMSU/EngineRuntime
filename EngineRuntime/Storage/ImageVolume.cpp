#include "ImageVolume.h"

#include "Chain.h"
#include "../Math/MathBase.h"
#include "../UserInterface/ShapeBase.h"

namespace Engine
{
	namespace Storage
	{
		ENGINE_PACKED_STRUCTURE(ImageVolumeHeader)
			uint8 Signature[8];	// ecs.1.0
			uint32 SignatureEx;	// 0x80000006
			uint32 Version;		// 0
			uint32 FrameCount;
		ENGINE_END_PACKED_STRUCTURE
		ENGINE_PACKED_STRUCTURE(ImageVolumeFrameHeader)
			uint32 Width;
			uint32 Height;
			uint32 Usage;			// 0 - Color, 1 - Normal, 2 - Luminescence
			int32 HotPointX;
			int32 HotPointY;
			int32 Duration;
			double DpiUsage;
			// bits 0...3: pixel format: 0 - B8G8R8A8, 1 - B8G8R8, 2 - P8, palette in B8G8R8A8
			// bits 4...7: compression method: 0 - no compression, 16 - chain compression
			uint32 DataCompression;
			uint32 DataOffset;
			uint32 DataSize;
		ENGINE_END_PACKED_STRUCTURE

		class VolumeCodec : public Codec::ICodec
		{
		public:
			bool load_best_dpi_only;

			VolumeCodec(void) { load_best_dpi_only = false; }
			~VolumeCodec(void) override {}

			virtual void EncodeFrame(Streaming::Stream * stream, Engine::Codec::Frame * frame, const string & format) override
			{
				SafePointer<Engine::Codec::Image> Fake = new Engine::Codec::Image;
				Fake->Frames.Append(frame);
				EncodeImage(stream, Fake, format);
			}
			virtual void EncodeImage(Streaming::Stream * stream, Engine::Codec::Image * image, const string & format) override
			{
				if (format != L"EIWV") throw InvalidArgumentException();
				if (!image->Frames.Length()) throw InvalidArgumentException();
				ImageVolumeHeader hdr;
				Array<ImageVolumeFrameHeader> fhdr(0x10);
				fhdr.SetLength(image->Frames.Length());
				uint64 base = sizeof(ImageVolumeHeader) + sizeof(ImageVolumeFrameHeader) * image->Frames.Length();
				stream->SetLength(base);
				stream->Seek(base, Streaming::Begin);
				MemoryCopy(&hdr.Signature, "ecs.1.0", 8);
				hdr.SignatureEx = 0x80000006;
				hdr.Version = 0;
				hdr.FrameCount = image->Frames.Length();
				for (int i = 0; i < image->Frames.Length(); i++) {
					Engine::Codec::Frame & frame = image->Frames[i];
					SafePointer<Engine::Codec::Frame> conv = frame.ConvertFormat(Engine::Codec::PixelFormat::B8G8R8A8, Engine::Codec::AlphaMode::Normal, Engine::Codec::ScanOrigin::BottomUp);
					fhdr[i].Width = conv->GetWidth();
					fhdr[i].Height = conv->GetHeight();
					if (conv->Usage == Engine::Codec::FrameUsage::ColorMap) fhdr[i].Usage = 0;
					else if (conv->Usage == Engine::Codec::FrameUsage::NormalMap) fhdr[i].Usage = 1;
					else if (conv->Usage == Engine::Codec::FrameUsage::LightMap) fhdr[i].Usage = 2;
					else fhdr[i].Usage = 0;
					fhdr[i].HotPointX = conv->HotPointX;
					fhdr[i].HotPointY = conv->HotPointY;
					fhdr[i].Duration = conv->Duration;
					fhdr[i].DpiUsage = conv->DpiUsage;
					fhdr[i].DataOffset = uint32(stream->Seek(0, Streaming::Current));
					// Checking for best pixel format: RGBA, RGB or indexed
					int s = fhdr[i].Width * fhdr[i].Height;
					Array<uint32> clrused(0x100);
					bool alphaused = false;
					for (int j = 0; j < s; j++) {
						uint32 clr = reinterpret_cast<const uint32 *>(conv->GetData())[j];
						if ((clr & 0xFF000000) != 0xFF000000) alphaused = true;
						if (clrused.Length() <= 0x100) {
							bool found = false;
							for (int k = 0; k < clrused.Length(); k++) {
								if (clrused[k] == clr) { found = true; break; }
							}
							if (!found) clrused << clr;
						}
					}
					Streaming::MemoryStream data(0x10000);
					// Serializing data
					if (clrused.Length() <= 0x100) {
						fhdr[i].DataCompression = 18;
						uint8 clrvol = uint8(clrused.Length());
						data.Write(&clrvol, 1);
						data.Write(clrused.GetBuffer(), clrused.Length() * 4);
						for (int j = 0; j < s; j++) {
							uint32 clr = reinterpret_cast<const uint32 *>(conv->GetData())[j];
							for (int k = 0; k < clrused.Length(); k++) {
								if (clrused[k] == clr) {
									uint8 v = uint8(k);
									data.Write(&v, 1);
								}
							}
						}
					} else if (!alphaused) {
						fhdr[i].DataCompression = 17;
						for (int j = 0; j < s; j++) {
							uint8 v = (reinterpret_cast<const uint32 *>(conv->GetData())[j]) & 0xFF;
							data.Write(&v, 1);
						}
						for (int j = 0; j < s; j++) {
							uint8 v = ((reinterpret_cast<const uint32 *>(conv->GetData())[j]) & 0xFF00) >> 8;
							data.Write(&v, 1);
						}
						for (int j = 0; j < s; j++) {
							uint8 v = ((reinterpret_cast<const uint32 *>(conv->GetData())[j]) & 0xFF0000) >> 16;
							data.Write(&v, 1);
						}
					} else {
						fhdr[i].DataCompression = 16;
						for (int j = 0; j < s; j++) {
							uint8 v = (reinterpret_cast<const uint32 *>(conv->GetData())[j]) & 0xFF;
							data.Write(&v, 1);
						}
						for (int j = 0; j < s; j++) {
							uint8 v = ((reinterpret_cast<const uint32 *>(conv->GetData())[j]) & 0xFF00) >> 8;
							data.Write(&v, 1);
						}
						for (int j = 0; j < s; j++) {
							uint8 v = ((reinterpret_cast<const uint32 *>(conv->GetData())[j]) & 0xFF0000) >> 16;
							data.Write(&v, 1);
						}
						for (int j = 0; j < s; j++) {
							uint8 v = ((reinterpret_cast<const uint32 *>(conv->GetData())[j]) & 0xFF000000) >> 24;
							data.Write(&v, 1);
						}
					}
					// Using differential encoding
					uint8 * data_ptr = reinterpret_cast<uint8 *>(data.GetBuffer());
					for (int j = int32(data.Length()) - 1; j > 0; j--) data_ptr[j] -= data_ptr[j - 1];
					// Compressing
					data.Seek(0, Streaming::Begin);
					Streaming::MemoryStream com(0x10000);
					ChainCompress(&com, &data, MethodChain(CompressionMethod::RunLengthEncoding8bit, CompressionMethod::Huffman), CompressionQuality::Sequential, 0, 0x10000);
					// Writing
					com.Seek(0, Streaming::Begin);
					fhdr[i].DataSize = uint32(com.Length());
					com.CopyTo(stream);
				}
				stream->Seek(0, Streaming::Begin);
				stream->Write(&hdr, sizeof(hdr));
				stream->Write(fhdr.GetBuffer(), sizeof(ImageVolumeFrameHeader) * image->Frames.Length());
			}
			virtual Engine::Codec::Frame * DecodeFrame(Streaming::Stream * stream) override
			{
				SafePointer<Engine::Codec::Image> Decoded = DecodeImage(stream);
				if (!Decoded->Frames.Length()) throw InvalidFormatException();
				Engine::Codec::Frame * Result = Decoded->Frames.ElementAt(0);
				Result->Retain();
				return Result;
			}
			virtual Engine::Codec::Image * DecodeImage(Streaming::Stream * stream) override
			{
				ImageVolumeHeader hdr;
				Array<ImageVolumeFrameHeader> fhdr(0x10);
				stream->Read(&hdr, sizeof(hdr));
				if (MemoryCompare(&hdr.Signature, "ecs.1.0", 8) != 0 || hdr.SignatureEx != 0x80000006 || hdr.Version != 0 || hdr.FrameCount == 0 || hdr.FrameCount > 0x00FFFFFFU) throw InvalidFormatException();
				fhdr.SetLength(hdr.FrameCount);
				stream->Read(fhdr.GetBuffer(), sizeof(ImageVolumeFrameHeader) * hdr.FrameCount);
				if (load_best_dpi_only) {
					int best_match_index = -1;
					double best_match = 0.0;
					for (int i = 0; i < fhdr.Length(); i++) {
						double res = Math::abs(fhdr[i].DpiUsage - UI::CurrentScaleFactor);
						if (res < best_match || best_match_index < 0) { best_match = res; best_match_index = i; }
					}
					if (best_match_index > 0) fhdr.SwapAt(0, best_match_index);
					if (fhdr.Length() > 1) fhdr.SetLength(1);
				}
				SafePointer<Engine::Codec::Image> result = new Engine::Codec::Image;
				for (int i = 0; i < fhdr.Length(); i++) {
					Streaming::FragmentStream source(stream, fhdr[i].DataOffset, fhdr[i].DataSize);
					Streaming::MemoryStream dec(0x10000);
					if ((fhdr[i].DataCompression & 0xF0) == 16) ChainDecompress(&dec, &source); else source.CopyTo(&dec);
					if (dec.Length() > 0x7FFFFFFFU || uint64(fhdr[i].Width) * uint64(fhdr[i].Height) > 0x1FFFFFFFU) throw InvalidFormatException();
					uint8 * data_ptr = reinterpret_cast<uint8 *>(dec.GetBuffer());
					int32 data_len = int32(dec.Length());
					for (int j = 1; j < data_len; j++) data_ptr[j] += data_ptr[j - 1];
					Array<Color> pixel(0x10000);
					pixel.SetLength(fhdr[i].Width * fhdr[i].Height);
					int s = fhdr[i].Width * fhdr[i].Height;
					dec.Seek(0, Streaming::Begin);
					if ((fhdr[i].DataCompression & 0xF) == 0) {
						for (int j = 0; j < s; j++) {
							uint8 v;
							dec.Read(&v, 1);
							pixel[j].r = v;
						}
						for (int j = 0; j < s; j++) {
							uint8 v;
							dec.Read(&v, 1);
							pixel[j].g = v;
						}
						for (int j = 0; j < s; j++) {
							uint8 v;
							dec.Read(&v, 1);
							pixel[j].b = v;
						}
						for (int j = 0; j < s; j++) {
							uint8 v;
							dec.Read(&v, 1);
							pixel[j].a = v;
						}
					} else if ((fhdr[i].DataCompression & 0xF) == 1) {
						for (int j = 0; j < s; j++) {
							uint8 v;
							dec.Read(&v, 1);
							pixel[j].r = v;
							pixel[j].a = 0xFF;
						}
						for (int j = 0; j < s; j++) {
							uint8 v;
							dec.Read(&v, 1);
							pixel[j].g = v;
						}
						for (int j = 0; j < s; j++) {
							uint8 v;
							dec.Read(&v, 1);
							pixel[j].b = v;
						}
					} else if ((fhdr[i].DataCompression & 0xF) == 2) {
						uint32 clrused = 0;
						dec.Read(&clrused, 1);
						if (clrused == 0) clrused = 0x100;
						Array<uint32> plt(0x100);
						plt.SetLength(0x100);
						ZeroMemory(plt.GetBuffer(), 0x400);
						dec.Read(plt.GetBuffer(), 4 * clrused);
						for (int j = 0; j < s; j++) {
							uint32 v = 0;
							dec.Read(&v, 1);
							pixel[j].Value = plt[v];
						}
					} else throw InvalidFormatException();
					SafePointer<Engine::Codec::Frame> decoded = new Engine::Codec::Frame(fhdr[i].Width, fhdr[i].Height, fhdr[i].Width * 4, Engine::Codec::PixelFormat::B8G8R8A8, Engine::Codec::AlphaMode::Normal, Engine::Codec::ScanOrigin::BottomUp);
					if (fhdr[i].Usage == 1) decoded->Usage = Engine::Codec::FrameUsage::NormalMap;
					else if (fhdr[i].Usage == 2) decoded->Usage = Engine::Codec::FrameUsage::LightMap;
					decoded->HotPointX = fhdr[i].HotPointX;
					decoded->HotPointY = fhdr[i].HotPointY;
					decoded->Duration = fhdr[i].Duration;
					decoded->DpiUsage = fhdr[i].DpiUsage;
					MemoryCopy(decoded->GetData(), pixel.GetBuffer(), 4 * s);
					result->Frames.Append(decoded);
				}
				result->Retain();
				return result;
			}
			virtual bool IsImageCodec(void) override { return true; }
			virtual bool IsFrameCodec(void) override { return true; }
			virtual string GetCodecName(void) override { return L"Engine Volume Codec"; }
			virtual string ExamineData(Streaming::Stream * stream) override
			{
				try {
					ImageVolumeHeader hdr;
					stream->Read(&hdr, sizeof(hdr));
					stream->Seek(0, Streaming::Begin);
					if (MemoryCompare(&hdr.Signature, "ecs.1.0", 8) == 0 && hdr.SignatureEx == 0x80000006 && hdr.Version == 0 && hdr.FrameCount != 0) return L"EIWV";
					else return L"";
				} catch (...) { return L""; }
			}
			virtual bool CanEncode(const string & format) override { return format == L"EIWV"; }
			virtual bool CanDecode(const string & format) override { return format == L"EIWV"; }
		};

		Codec::ICodec * _VolumeCodec = 0;
		Codec::ICodec * CreateVolumeCodec(void) { if (!_VolumeCodec) { _VolumeCodec = new VolumeCodec(); _VolumeCodec->Release(); } return _VolumeCodec; }
		void SetVolumeCodecLoadBestDpiOnly(bool only) { if (_VolumeCodec) static_cast<VolumeCodec *>(_VolumeCodec)->load_best_dpi_only = only; }
		bool IsVolumeCodecLoadsBestDpiOnly(void) { if (_VolumeCodec) return static_cast<VolumeCodec *>(_VolumeCodec)->load_best_dpi_only; else return false; }
	}
}