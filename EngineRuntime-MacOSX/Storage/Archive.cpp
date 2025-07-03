#include "Archive.h"

#include "Registry.h"
#include "Chain.h"
#include "../Miscellaneous/DynamicString.h"

namespace Engine
{
	namespace Storage
	{
		namespace Archives
		{
			ENGINE_PACKED_STRUCTURE(ArchiveHeader32)
				uint8 Signature[8];	// ecs.1.0
				uint32 SignatureEx;	// 0x80000005
				uint32 FileCount;
				uint32 BaseOffset;
				uint32 StringsOffset;
				uint32 StringsSize;
			ENGINE_END_PACKED_STRUCTURE
			ENGINE_PACKED_STRUCTURE(ArchiveHeader64)
				uint8 Signature[8];	// ecs.1.0
				uint32 SignatureEx;	// 0xC0000005
				uint32 Reserved;	// 0
				uint64 FileCount;
				uint64 BaseOffset;
				uint64 StringsOffset;
				uint64 StringsSize;
				uint64 Unused1;		// 0
				uint64 Unused2;		// 0
			ENGINE_END_PACKED_STRUCTURE
			ENGINE_PACKED_STRUCTURE(ArchiveFileHeader32)
				uint32 FileOffset;
				uint32 FileSize;
				uint32 FileType;
				uint32 FileID;
				uint32 Custom;
				uint32 NameOffset;
			ENGINE_END_PACKED_STRUCTURE
			ENGINE_PACKED_STRUCTURE(ArchiveFileHeader64)
				uint64 FileOffset;
				uint64 FileSize;
				uint32 FileType;
				uint32 FileID;
				uint32 Custom;
				uint32 NameOffset;
				uint64 Unused1;		// 0
				uint64 Unused2;		// 0
			ENGINE_END_PACKED_STRUCTURE
			struct ArchiveFileDescriptor
			{
				uint64 Offset;
				uint64 Size;
				uint32 Type;
				uint32 ID;
				uint32 Custom;
				string Name;
			};
			class Archive : public Storage::Archive
			{
				SafePointer<Streaming::Stream> inner;
				SafePointer<Registry> metadata;
				Array<ArchiveFileDescriptor> files;
				static void ValidateMemoryString(const uint8 * data, int size, int data_at)
				{
					if (data_at < 0 || data_at > size || size - data_at < 2) throw InvalidFormatException();
					int pos = data_at;
					while (size - pos >= 2 && *reinterpret_cast<const uint16 *>(data + pos)) pos += 2;
					if (size - data_at < 2) throw InvalidFormatException();
				}
				template <class ArchiveHeader, class ArchiveFileHeader> void LoadArchiveData(void)
				{
					ArchiveHeader hdr;
					inner->Read(&hdr, sizeof(hdr));
					if (hdr.FileCount > 0x7FFFFFFFU || hdr.StringsSize > 0x7FFFFFFFU) throw InvalidFormatException();
					files = Array<ArchiveFileDescriptor>(max(uint32(hdr.FileCount), 1U));
					Array<uint8> strings(1);
					strings.SetLength(uint32(hdr.StringsSize));
					uint64 pos = inner->Seek(0, Streaming::Current);
					inner->Seek(hdr.StringsOffset, Streaming::Begin);
					inner->Read(strings.GetBuffer(), uint32(hdr.StringsSize));
					inner->Seek(pos, Streaming::Begin);
					for (uint32 i = 0; i < uint32(hdr.FileCount); i++) {
						ArchiveFileHeader fhdr;
						inner->Read(&fhdr, sizeof(fhdr));
						ArchiveFileDescriptor file;
						file.Offset = fhdr.FileOffset + hdr.BaseOffset;
						file.Size = fhdr.FileSize;
						file.Type = fhdr.FileType;
						file.ID = fhdr.FileID;
						file.Custom = fhdr.Custom;
						ValidateMemoryString(strings.GetBuffer(), strings.Length(), fhdr.NameOffset);
						file.Name = string(strings.GetBuffer() + fhdr.NameOffset, -1, Encoding::UTF16);
						files << file;
					}
				}
			public:
				static string NormalizeName(const string & name)
				{
					DynamicString normalized;
					int pos = 0;
					while (pos < name.Length()) {
						while (name[pos] == L'\\' || name[pos] == L'/') pos++;
						while (name[pos] != L'\\' && name[pos] != L'/' && name[pos] != 0) { normalized += name[pos]; pos++; }
						if (name[pos] != 0) { normalized += L'\\'; pos++; }
					}
					return normalized;
				}
				static uint32 FileTypeFromString(const widechar * text)
				{
					int offs = 0;
					uint32 result = 0;
					for (int i = 0; i < min(StringLength(text), 4); i++) { if (text[i] == 32) break; result |= uint32(text[i] & 0xFF) << offs; offs += 8; }
					return result;
				}
				static string FileTypeToString(uint32 code) { return string(&code, 4, Encoding::ANSI); }
			public:
				Archive(Streaming::Stream * source, ArchiveMetadataUsage usage)
				{
					inner.SetRetain(source);
					uint64 signature;
					uint32 subtype;
					uint32 next;
					source->Seek(0, Streaming::Begin);
					source->Read(&signature, 8);
					source->Read(&subtype, 4);
					source->Read(&next, 4);
					bool is64 = false;
					if (MemoryCompare(&signature, "ecs.1.0", 8) != 0) throw InvalidFormatException();
					if (subtype == 0xC0000005) { is64 = true; if (next) throw InvalidFormatException(); }
					else if (subtype != 0x80000005) throw InvalidFormatException();
					source->Seek(0, Streaming::Begin);
					if (is64) LoadArchiveData<ArchiveHeader64, ArchiveFileHeader64>();
					else LoadArchiveData<ArchiveHeader32, ArchiveFileHeader32>();
					if (usage == ArchiveMetadataUsage::LoadMetadata) {
						if (files.LastElement().Name == L"" && files.LastElement().Type == FileTypeFromString(L"AMDE") && files.LastElement().ID == 0xFFFFFFFF && files.LastElement().Custom == 0) {
							SafePointer<Streaming::Stream> metadata_stream = QueryFileStream(files.Length(), ArchiveStream::Native);
							if (metadata_stream) metadata = LoadRegistry(metadata_stream);
						}
					}
				}
				~Archive(void) override {}
				virtual int GetFileCount(void) override { return files.Length(); }
				virtual ArchiveFile FindArchiveFile(const string & name) override
				{
					string normalized = NormalizeName(name);
					if (!normalized.Length()) return 0;
					for (int i = 0; i < files.Length(); i++) if (string::CompareIgnoreCase(normalized, files[i].Name) == 0) return i + 1;
					return 0;
				}
				virtual ArchiveFile FindArchiveFile(const widechar * type, uint32 file_id) override
				{
					auto type_dword = FileTypeFromString(type);
					if (type_dword == 0 || file_id == 0) return 0;
					for (int i = 0; i < files.Length(); i++) if (files[i].Type == type_dword && files[i].ID == file_id) return i + 1;
					return 0;
				}
				virtual Streaming::Stream * QueryFileStream(ArchiveFile file) override { return QueryFileStream(file, ArchiveStream::MetadataBased); }
				virtual Streaming::Stream * QueryFileStream(ArchiveFile file, ArchiveStream stream) override
				{
					if (file == 0 || file > files.Length()) return 0;
					if (stream == ArchiveStream::Native) {
						return new Streaming::FragmentStream(inner, files[file - 1].Offset, files[file - 1].Size);
					} else if (stream == ArchiveStream::ForceDecompressed) {
						SafePointer<Streaming::Stream> native = new Streaming::FragmentStream(inner, files[file - 1].Offset, files[file - 1].Size);
						return new DecompressionStream(native);
					} else if (stream == ArchiveStream::MetadataBased) {
						if (IsFileCompressed(file)) return QueryFileStream(file, ArchiveStream::ForceDecompressed);
						else return QueryFileStream(file, ArchiveStream::Native);
					}
					return 0;
				}
				virtual string GetFileName(ArchiveFile file) override
				{
					if (file == 0 || file > files.Length()) throw InvalidArgumentException();
					return files[file - 1].Name;
				}
				virtual string GetFileType(ArchiveFile file) override
				{
					if (file == 0 || file > files.Length()) throw InvalidArgumentException();
					return FileTypeToString(files[file - 1].Type);
				}
				virtual uint32 GetFileID(ArchiveFile file) override
				{
					if (file == 0 || file > files.Length()) throw InvalidArgumentException();
					return files[file - 1].ID;
				}
				virtual uint32 GetFileCustomData(ArchiveFile file) override
				{
					if (file == 0 || file > files.Length()) throw InvalidArgumentException();
					return files[file - 1].Custom;
				}
				virtual bool HasMetadata(void) override { return metadata; }
				virtual Time GetFileCreationTime(ArchiveFile file) override
				{
					if (file == 0 || file > files.Length()) throw InvalidArgumentException();
					if (metadata) return metadata->GetValueTime(string(file) + L"\\Creation Time"); else return 0;
				}
				virtual Time GetFileAccessTime(ArchiveFile file) override
				{
					if (file == 0 || file > files.Length()) throw InvalidArgumentException();
					if (metadata) return metadata->GetValueTime(string(file) + L"\\Access Time"); else return 0;
				}
				virtual Time GetFileAlterTime(ArchiveFile file) override
				{
					if (file == 0 || file > files.Length()) throw InvalidArgumentException();
					if (metadata) return metadata->GetValueTime(string(file) + L"\\Alter Time"); else return 0;
				}
				virtual bool IsFileCompressed(ArchiveFile file) override
				{
					if (file == 0 || file > files.Length()) throw InvalidArgumentException();
					if (metadata) return metadata->GetValueBoolean(string(file) + L"\\Compressed"); else return false;
				}
				virtual string GetFileAttribute(ArchiveFile file, const string & key) override
				{
					if (file == 0 || file > files.Length()) throw InvalidArgumentException();
					if (metadata) return metadata->GetValueString(string(file) + L"\\_" + key); else return L"";
				}
				virtual Array<string> * GetFileAttributes(ArchiveFile file) override
				{
					if (file == 0 || file > files.Length()) throw InvalidArgumentException();
					SafePointer< Array<string> > result = new Array<string>(0x10);
					if (metadata) {
						SafePointer<RegistryNode> node = metadata->OpenNode(string(file));
						if (node) {
							auto & names = node->GetValues();
							for (int i = 0; i < names.Length(); i++) if (names[i][0] == L'_') result->Append(names[i].Fragment(1, -1));
						}
					}
					result->Retain();
					return result;
				}
				virtual string ToString(void) const override { return L"Archive"; }
			};
			class NewArchive : public Storage::NewArchive
			{
				struct ArchiveFileInfo
				{
					uint64 Offset;
					uint64 Size;
					uint32 NameOffset;
					uint32 Type;
					uint32 ID;
					uint32 Custom;
					uint32 SetMask; // 1 - data set, 2 - name set
				};
				Array<ArchiveFileInfo> headers;
				Array<uint16> strings;
				SafePointer<Registry> metadata;
				SafePointer<Streaming::Stream> output;
				bool long_format;
				uint32 header_length;
				uint32 file_header_length;
				uint64 base_offset;

				uint32 string_alloc(const string & text)
				{
					Array<uint16> utf16(0x10);
					utf16.SetLength(text.GetEncodedLength(Encoding::UTF16) + 1);
					text.Encode(utf16.GetBuffer(), Encoding::UTF16, true);
					for (int i = 0; i <= strings.Length() - utf16.Length(); i++) {
						bool match = true;
						for (int j = 0; j < utf16.Length(); j++) {
							if (strings[i + j] != utf16[j]) { match = false; break; }
						}
						if (match) return i * 2;
					}
					uint32 result = strings.Length() * 2;
					strings << utf16;
					return result;
				}
				void mark_compressed(ArchiveFile file, bool flag = true)
				{
					if (metadata) {
						metadata->CreateNode(string(file));
						SafePointer<RegistryNode> node = metadata->OpenNode(string(file));
						node->CreateValue(L"Compressed", RegistryValueType::Boolean);
						node->SetValue(L"Compressed", flag);
					}
				}
				template <class ArchiveHeader, class ArchiveFileHeader, class Size> void finalize(void)
				{
					ArchiveHeader hdr;
					ZeroMemory(&hdr, sizeof(hdr));
					MemoryCopy(hdr.Signature, "ecs.1.0", 8);
					hdr.SignatureEx = long_format ? 0xC0000005 : 0x80000005;
					hdr.FileCount = headers.Length();
					hdr.BaseOffset = Size(base_offset);
					hdr.StringsOffset = Size(output->Seek(0, Streaming::Current));
					hdr.StringsSize = Size(strings.Length() * 2);
					output->Write(strings.GetBuffer(), strings.Length() * 2);
					uint64 end = output->Seek(0, Streaming::Current);
					output->Seek(0, Streaming::Begin);
					output->Write(&hdr, sizeof(hdr));
					for (int i = 0; i < headers.Length(); i++) {
						ArchiveFileHeader fhdr;
						ZeroMemory(&fhdr, sizeof(fhdr));
						fhdr.FileOffset = Size(headers[i].Offset);
						fhdr.FileSize = Size(headers[i].Size);
						fhdr.FileType = headers[i].Type;
						fhdr.FileID = headers[i].ID;
						fhdr.Custom = headers[i].Custom;
						fhdr.NameOffset = headers[i].NameOffset;
						output->Write(&fhdr, sizeof(fhdr));
					}
					output->Seek(end, Streaming::Begin);
				}
			public:
				NewArchive(Streaming::Stream * at, int num_files, uint flags) : headers(num_files), strings(0x100)
				{
					if (flags & NewArchiveFlags::CreateMetadata) metadata = CreateRegistry();
					if (flags & NewArchiveFlags::UseFormat32) long_format = false; else long_format = true;
					output.SetRetain(at);
					if (long_format) {
						header_length = sizeof(ArchiveHeader64);
						file_header_length = sizeof(ArchiveFileHeader64);
					} else {
						header_length = sizeof(ArchiveHeader32);
						file_header_length = sizeof(ArchiveFileHeader32);
					}
					if (metadata) num_files++;
					headers.SetLength(num_files);
					ZeroMemory(headers.GetBuffer(), sizeof(ArchiveFileInfo) * num_files);
					base_offset = header_length + num_files * file_header_length;
					output->Seek(0, Streaming::Begin);
					output->SetLength(base_offset);
					output->Seek(base_offset, Streaming::Begin);
				}
				~NewArchive(void) override {}
				virtual int GetFileCount(void) override { return headers.Length() - (metadata ? 1 : 0); }
				virtual bool IsLongFormat(void) override { return long_format; }
				virtual bool HasMetadata(void) override { return metadata; }
				virtual void SetFileName(ArchiveFile file, const string & name) override
				{
					if (file <= 0 || file > GetFileCount()) throw InvalidArgumentException();
					if (headers[file - 1].SetMask & 2) throw InvalidStateException();
					if (!output) throw InvalidStateException();
					headers[file - 1].NameOffset = string_alloc(Archive::NormalizeName(name));
					headers[file - 1].SetMask |= 2;
				}
				virtual void SetFileType(ArchiveFile file, const string & type) override
				{
					if (file <= 0 || file > GetFileCount()) throw InvalidArgumentException();
					if (!output) throw InvalidStateException();
					headers[file - 1].Type = Archive::FileTypeFromString(type);
				}
				virtual void SetFileID(ArchiveFile file, uint32 id) override
				{
					if (file <= 0 || file > GetFileCount()) throw InvalidArgumentException();
					if (!output) throw InvalidStateException();
					headers[file - 1].ID = id;
				}
				virtual void SetFileCustom(ArchiveFile file, uint32 custom) override
				{
					if (file <= 0 || file > GetFileCount()) throw InvalidArgumentException();
					if (!output) throw InvalidStateException();
					headers[file - 1].Custom = custom;
				}
				virtual void SetFileCreationTime(ArchiveFile file, Time time) override
				{
					if (file <= 0 || file > GetFileCount()) throw InvalidArgumentException();
					if (!output) throw InvalidStateException();
					if (metadata) {
						metadata->CreateNode(string(file));
						SafePointer<RegistryNode> node = metadata->OpenNode(string(file));
						node->CreateValue(L"Creation Time", RegistryValueType::Time);
						node->SetValue(L"Creation Time", time);
					}
				}
				virtual void SetFileAccessTime(ArchiveFile file, Time time) override
				{
					if (file <= 0 || file > GetFileCount()) throw InvalidArgumentException();
					if (!output) throw InvalidStateException();
					if (metadata) {
						metadata->CreateNode(string(file));
						SafePointer<RegistryNode> node = metadata->OpenNode(string(file));
						node->CreateValue(L"Access Time", RegistryValueType::Time);
						node->SetValue(L"Access Time", time);
					}
				}
				virtual void SetFileAlterTime(ArchiveFile file, Time time) override
				{
					if (file <= 0 || file > GetFileCount()) throw InvalidArgumentException();
					if (!output) throw InvalidStateException();
					if (metadata) {
						metadata->CreateNode(string(file));
						SafePointer<RegistryNode> node = metadata->OpenNode(string(file));
						node->CreateValue(L"Alter Time", RegistryValueType::Time);
						node->SetValue(L"Alter Time", time);
					}
				}
				virtual void SetFileAttribute(ArchiveFile file, const string & key, const string & value) override
				{
					if (file <= 0 || file > GetFileCount()) throw InvalidArgumentException();
					if (!output) throw InvalidStateException();
					if (metadata) {
						metadata->CreateNode(string(file));
						SafePointer<RegistryNode> node = metadata->OpenNode(string(file));
						string path = L"_" + key;
						node->CreateValue(path, RegistryValueType::String);
						node->SetValue(path, value);
					}
				}
				virtual void SetFileData(ArchiveFile file, const void * data, int length) override
				{
					if (file <= 0 || file > GetFileCount()) throw InvalidArgumentException();
					if (headers[file - 1].SetMask & 1) throw InvalidStateException();
					if (!output) throw InvalidStateException();
					headers[file - 1].Size = length;
					headers[file - 1].Offset = output->Seek(0, Streaming::Current) - base_offset;
					output->Write(data, length);
					headers[file - 1].SetMask |= 1;
				}
				virtual void SetFileData(ArchiveFile file, Streaming::Stream * source) override
				{
					if (file <= 0 || file > GetFileCount()) throw InvalidArgumentException();
					if (headers[file - 1].SetMask & 1) throw InvalidStateException();
					if (!output) throw InvalidStateException();
					uint64 pos_begin = output->Seek(0, Streaming::Current);
					source->CopyTo(output);
					uint64 pos_end = output->Seek(0, Streaming::Current);
					headers[file - 1].Size = pos_end - pos_begin;
					headers[file - 1].Offset = pos_begin - base_offset;
					headers[file - 1].SetMask |= 1;
				}
				virtual void SetFileData(ArchiveFile file, Streaming::Stream * source, MethodChain chain, CompressionQuality quality, ThreadPool * pool = 0) override
				{
					if (file <= 0 || file > GetFileCount()) throw InvalidArgumentException();
					if (headers[file - 1].SetMask & 1) throw InvalidStateException();
					if (!output) throw InvalidStateException();
					uint64 pos_begin = output->Seek(0, Streaming::Current);
					ChainCompress(output, source, chain, quality, pool);
					uint64 pos_end = output->Seek(0, Streaming::Current);
					headers[file - 1].Size = pos_end - pos_begin;
					headers[file - 1].Offset = pos_begin - base_offset;
					mark_compressed(file);
					headers[file - 1].SetMask |= 1;
				}
				virtual void SetFileData(ArchiveFile file, Streaming::Stream * source, MethodChain chain, CompressionQuality quality, ThreadPool * pool, uint32 block_size) override
				{
					if (file <= 0 || file > GetFileCount()) throw InvalidArgumentException();
					if (headers[file - 1].SetMask & 1) throw InvalidStateException();
					if (!output) throw InvalidStateException();
					uint64 pos_begin = output->Seek(0, Streaming::Current);
					ChainCompress(output, source, chain, quality, pool, block_size);
					uint64 pos_end = output->Seek(0, Streaming::Current);
					headers[file - 1].Size = pos_end - pos_begin;
					headers[file - 1].Offset = pos_begin - base_offset;
					mark_compressed(file);
					headers[file - 1].SetMask |= 1;
				}
				virtual void SetFileCompressionFlag(ArchiveFile file, bool flag) override
				{
					if (file <= 0 || file > GetFileCount()) throw InvalidArgumentException();
					if (headers[file - 1].SetMask & 1) throw InvalidStateException();
					if (!output) throw InvalidStateException();
					mark_compressed(file, flag);
				}
				virtual void Finalize(void) override
				{
					if (!output) throw InvalidStateException();
					for (int i = 1; i <= GetFileCount(); i++) {
						if ((headers[i - 1].SetMask & 2) == 0) SetFileName(i, L"");
						if ((headers[i - 1].SetMask & 1) == 0) { headers[i - 1].Offset = 0; headers[i - 1].Size = 0; }
					}
					if (metadata) {
						Streaming::MemoryStream metadata_serialized(0x1000);
						metadata->Save(&metadata_serialized);
						metadata_serialized.Seek(0, Streaming::Begin);
						headers.LastElement().NameOffset = string_alloc(L"");
						headers.LastElement().Type = Archive::FileTypeFromString(L"AMDE");
						headers.LastElement().ID = 0xFFFFFFFF;
						headers.LastElement().Custom = 0;
						uint64 pos_begin = output->Seek(0, Streaming::Current);
						metadata_serialized.CopyTo(output);
						uint64 pos_end = output->Seek(0, Streaming::Current);
						headers.LastElement().Size = pos_end - pos_begin;
						headers.LastElement().Offset = pos_begin - base_offset;
						headers.LastElement().SetMask |= 3;
					}
					uint64 pos = output->Seek(0, Streaming::Current);
					if (!long_format && pos + strings.Length() * 2 > 0xFFFFFFFF) throw InvalidFormatException();
					if (long_format) finalize<ArchiveHeader64, ArchiveFileHeader64, uint64>();
					else finalize<ArchiveHeader32, ArchiveFileHeader32, uint32>();
					output.SetReference(0);
				}
				virtual string ToString(void) const override { return L"NewArchive"; }
			};
		}
		Archive * OpenArchive(Streaming::Stream * at) noexcept { return OpenArchive(at, ArchiveMetadataUsage::LoadMetadata); }
		Archive * OpenArchive(Streaming::Stream * at, ArchiveMetadataUsage metadata_usage) noexcept { try { return new Archives::Archive(at, metadata_usage); } catch (...) { return 0; } }
		NewArchive * CreateArchive(Streaming::Stream * at, int num_files, uint flags)
		{
			if (num_files < 0 || !at) throw InvalidArgumentException();
			return new Archives::NewArchive(at, num_files, flags);
		}
	}
}