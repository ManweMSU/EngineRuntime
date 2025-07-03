#include "Streaming.h"

#include "Miscellaneous/DynamicString.h"

namespace Engine
{
	namespace Streaming
	{
		using namespace ::Engine::IO;

		void Stream::CopyTo(Stream * to, uint64 length)
		{
			constexpr uint64 buflen = 0x100000;
			uint8 * buffer = new (std::nothrow) uint8[buflen];
			if (!buffer) throw OutOfMemoryException();
			try {
				uint64 pending = length;
				while (pending) {
					uint32 amount = uint32(min(buflen, pending));
					Read(buffer, amount);
					to->Write(buffer, amount);
					pending -= amount;
				}
			}
			catch (...) {
				delete[] buffer;
				throw;
			}
			delete[] buffer;
		}
		void Stream::CopyTo(Stream * to) { CopyTo(to, Length() - Seek(0, Current)); }
		void Stream::CopyToUntilEof(Stream * to)
		{
			constexpr uint64 buflen = 0x100000;
			uint8 * buffer = new (std::nothrow) uint8[buflen];
			if (!buffer) throw OutOfMemoryException();
			try {
				while (true) {
					Read(buffer, buflen);
					to->Write(buffer, buflen);
				}
			} catch (FileReadEndOfFileException & e) {
				if (e.DataRead) to->Write(buffer, e.DataRead);
			} catch (...) {
				delete[] buffer;
				throw;
			}
			delete[] buffer;
		}
		Array<uint8>* Stream::ReadAll(void)
		{
			uint64 current = Seek(0, Current);
			uint64 length = Length();
			if (length - current > 0x7FFFFFFF) throw OutOfMemoryException();
			int len = int(length - current);
			SafePointer< Array<uint8> > result = new (std::nothrow) Array<uint8>(len);
			if (!result) throw OutOfMemoryException();
			result->SetLength(len);
			Read(result->GetBuffer(), len);
			result->Retain();
			return result;
		}
		Array<uint8> * Stream::ReadBlock(uint32 length)
		{
			SafePointer< Array<uint8> > result = new (std::nothrow) Array<uint8>(length);
			if (!result) throw OutOfMemoryException();
			result->SetLength(length);
			try {
				Read(result->GetBuffer(), length);
			} catch (IO::FileReadEndOfFileException & e) {
				result->SetLength(e.DataRead);
			}
			result->Retain();
			return result;
		}
		void Stream::WriteArray(const Array<uint8>* data) { Write(data->GetBuffer(), data->Length()); }
		void Stream::RelocateData(uint64 offset_from, uint64 offset_to, uint64 length)
		{
			if (offset_to > offset_from) {
				DataBlock block(0x10000);
				block.SetLength(0x100000);
				uint64 data_left = length;
				while (data_left) {
					uint32 current = uint32(min(data_left, uint64(block.Length())));
					Seek(offset_from + data_left - current, Begin);
					Read(block.GetBuffer(), current);
					Seek(offset_to + data_left - current, Begin);
					Write(block.GetBuffer(), current);
					data_left -= current;
				}
			} else if (offset_to < offset_from) {
				DataBlock block(0x10000);
				block.SetLength(0x100000);
				uint64 data_left = length;
				while (data_left) {
					uint32 current = uint32(min(data_left, uint64(block.Length())));
					Seek(offset_from + length - data_left, Begin);
					Read(block.GetBuffer(), current);
					Seek(offset_to + length - data_left, Begin);
					Write(block.GetBuffer(), current);
					data_left -= current;
				}
			}
		}
		FileStream::FileStream(const string & path, FileAccess access, FileCreationMode mode) : owned(true) { file = CreateFile(path, access, mode); }
		FileStream::FileStream(handle file_handle, bool take_control) : owned(take_control) { file = file_handle; }
		void FileStream::Read(void * buffer, uint32 length) { ReadFile(file, buffer, length); }
		void FileStream::Write(const void * data, uint32 length) { WriteFile(file, data, length); }
		int64 FileStream::Seek(int64 position, SeekOrigin origin) { return IO::Seek(file, position, origin); }
		uint64 FileStream::Length(void) { return GetFileSize(file); }
		void FileStream::SetLength(uint64 length) { SetFileSize(file, length); }
		void FileStream::Flush(void) { IO::Flush(file); }
		handle FileStream::Handle(void) const { return file; }
		handle & FileStream::Handle(void) { return file; }
		FileStream::~FileStream(void) { if (owned) CloseHandle(file); }
		string FileStream::ToString(void) const { return L"FileStream"; }

		MemoryStream::MemoryStream(const void * source, int length) : data(0x10000), pointer(0) { data.SetLength(length); MemoryCopy(data, source, length); }
		MemoryStream::MemoryStream(const void * source, int length, int block) : data(block), pointer(0) { data.SetLength(length); MemoryCopy(data, source, length); }
		MemoryStream::MemoryStream(Stream * source, int length) : data(0x10000), pointer(0) { data.SetLength(length); source->Read(data, length); }
		MemoryStream::MemoryStream(Stream * source, int length, int block) : data(block), pointer(0) { data.SetLength(length); source->Read(data, length); }
		MemoryStream::MemoryStream(int block) : data(block), pointer(0) {}
		void MemoryStream::Read(void * buffer, uint32 length)
		{
			if (length > uint32(data.Length() - pointer)) {
				length = (pointer >= data.Length()) ? 0 : (uint32(data.Length()) - pointer);
				MemoryCopy(buffer, data.GetBuffer() + pointer, length);
				pointer += length;
				throw FileReadEndOfFileException(length);
			}
			MemoryCopy(buffer, data.GetBuffer() + pointer, length);
			pointer += length;
		}
		void MemoryStream::Write(const void * _data, uint32 length)
		{
			if (length > uint32(data.Length() - pointer)) {
				if (length > 0x7FFFFFFFU) throw InvalidArgumentException();
				if (uint32(pointer) + length > 0x7FFFFFFFU) throw FileAccessException(Error::FileTooLarge);
				data.SetLength(pointer + int(length));
			}
			MemoryCopy(data.GetBuffer() + pointer, _data, length);
			pointer += int(length);
		}
		int64 MemoryStream::Seek(int64 position, SeekOrigin origin)
		{
			int64 newpos = position;
			if (origin == Current) newpos += int64(pointer);
			else if (origin == End) newpos += int64(data.Length());
			if (newpos < 0 || newpos > int64(data.Length())) throw InvalidArgumentException();
			pointer = int32(newpos);
			return pointer;
		}
		uint64 MemoryStream::Length(void) { return uint64(data.Length()); }
		void MemoryStream::SetLength(uint64 length)
		{
			if (length > 0x7FFFFFFFU) throw InvalidArgumentException();
			data.SetLength(int(length));
		}
		void MemoryStream::Flush(void) {}
		MemoryStream::~MemoryStream(void) {}
		string MemoryStream::ToString(void) const { return L"MemoryStream"; }
		void * MemoryStream::GetBuffer(void) { return data.GetBuffer(); }
		const void * MemoryStream::GetBuffer(void) const { return data.GetBuffer(); }
		TextWriter::TextWriter(Stream * Dest) { dest = Dest; dest->Retain(); coding = TextFileEncoding; }
		TextWriter::TextWriter(Stream * Dest, Encoding encoding) { dest = Dest; dest->Retain(); coding = encoding; }
		void TextWriter::Write(const string & text) const
		{
			auto data = text.EncodeSequence(coding, false);
			dest->Write(data->GetBuffer(), data->Length());
			data->Release();
		}
		void TextWriter::WriteLine(const string & text) const { Write(text + LineFeedSequence); }
		void TextWriter::WriteEncodingSignature(void) const
		{
			if (coding == Encoding::ANSI) {}
			else if (coding == Encoding::UTF8) { dest->Write("\xEF\xBB\xBF", 3); }
			else if (coding == Encoding::UTF16) { dest->Write("\xFF\xFE", 2); }
			else if (coding == Encoding::UTF32) { dest->Write("\xFF\xFE\x00\x00", 4); }
		}
		TextWriter::~TextWriter(void) { dest->Release(); }
		string TextWriter::ToString(void) const { return L"TextWriter"; }
		void ITextWriter::LineFeed(void) const { WriteLine(L""); }
		ITextWriter & ITextWriter::operator<<(const string & text) { Write(text); return *this; }
		const ITextWriter & ITextWriter::operator<<(const string & text) const { Write(text); return *this; }
		FragmentStream::FragmentStream(Stream * Inner, uint64 From, uint64 Length) : inner(Inner), begin(From), end(From + Length), pointer(0) { inner->Retain(); }
		void FragmentStream::Read(void * buffer, uint32 length)
		{
			if (uint64(length) > end - begin - pointer) {
				length = (uint64(pointer) >= end - begin) ? 0 : uint32(end - begin - pointer);
				auto pos = inner->Seek(0, Current);
				inner->Seek(begin + pointer, Begin);
				try {
					inner->Read(buffer, length);
				}
				catch (...) { inner->Seek(pos, Begin); throw; }
				inner->Seek(pos, Begin);
				pointer += length;
				throw FileReadEndOfFileException(length);
			}
			auto pos = inner->Seek(0, Current);
			inner->Seek(begin + pointer, Begin);
			try {
				inner->Read(buffer, length);
			} catch (...) { inner->Seek(pos, Begin); throw; }
			pointer += length;
		}
		void FragmentStream::Write(const void * data, uint32 length) { throw FileAccessException(Error::IsReadOnly); }
		int64 FragmentStream::Seek(int64 position, SeekOrigin origin)
		{
			int64 newpos = position;
			if (origin == Current) newpos += pointer;
			else if (origin == End) newpos += end - begin;
			if (newpos < 0 || uint64(newpos) > end - begin) throw InvalidArgumentException();
			pointer = newpos;
			return pointer;
		}
		uint64 FragmentStream::Length(void) { return end - begin; }
		void FragmentStream::SetLength(uint64 length) { throw FileAccessException(Error::IsReadOnly); }
		void FragmentStream::Flush(void) {}
		FragmentStream::~FragmentStream(void) { inner->Release(); }
		string FragmentStream::ToString(void) const { return L"FragmentStream"; }
		TextReader::TextReader(Stream * Source) { source = Source; source->Retain(); coding = Encoding::Unknown; eof = false; }
		TextReader::TextReader(Stream * Source, Encoding encoding) { source = Source; source->Retain(); coding = encoding; eof = false; }
		uint32 TextReader::ReadChar(void) const
		{
			try {
				if (coding == Encoding::Unknown) {
					uint8 byte;
					source->Read(&byte, 1);
					if (byte == 0xEF) {
						uint16 word;
						source->Read(&word, 2);
						if (word == 0xBFBB) coding = Encoding::UTF8;
						else throw InvalidFormatException();
					} else if (byte == 0xFF) {
						source->Read(&byte, 1);
						if (byte != 0xFE) throw InvalidFormatException();
						coding = Encoding::UTF16;
					} else {
						coding = Encoding::ANSI;
						return byte;
					}
				}
				if (coding == Encoding::ANSI) {
					uint8 byte;
					source->Read(&byte, 1);
					return byte;
				} else if (coding == Encoding::UTF8) {
					uint32 code = 0xFEFF;
					while (code == 0xFEFF) {
						code = 0;
						source->Read(&code, 1);
						if (code & 0x80) {
							code &= 0x7F;
							if (code & 0x20) {
								code &= 0x1F;
								if (code & 0x10) {
									code &= 0x07;
									code <<= 18;
									uint8 c2, c3, c4;
									source->Read(&c2, 1);
									source->Read(&c3, 1);
									source->Read(&c4, 1);
									code |= (c2 & 0x3F) << 12;
									code |= (c3 & 0x3F) << 6;
									code |= (c4 & 0x3F);
								} else {
									code &= 0x0F;
									code <<= 12;
									uint8 c2, c3;
									source->Read(&c2, 1);
									source->Read(&c3, 1);
									code |= (c2 & 0x3F) << 6;
									code |= (c3 & 0x3F);
								}
							} else {
								code &= 0x1F;
								code <<= 6;
								uint8 c2;
								source->Read(&c2, 1);
								code |= (c2 & 0x3F);
							}
						}
					}
					return code;
				} else if (coding == Encoding::UTF16) {
					uint32 code = 0;
					source->Read(&code, 2);
					if (code == 0) {
						coding = Encoding::UTF32;
						source->Read(&code, 4);
						return code;
					} else {
						if ((code & 0xFC00) == 0xD800) {
							code &= 0x03FF;
							code <<= 10;
							uint16 c;
							source->Read(&c, 2);
							code |= (c & 0x3FF);
							code += 0x10000;
						}
						return code;
					}
				} else if (coding == Encoding::UTF32) {
					uint32 code;
					source->Read(&code, 4);
					return code;
				} else return 0;
			}
			catch (FileReadEndOfFileException &) { eof = true; return 0xFFFFFFFF; }
		}
		string ITextReader::ReadLine(void) const
		{
			DynamicString result;
			uint32 chr;
			do {
				chr = ReadChar();
				if (chr != 0xFFFFFFFF && (chr >= 0x20 || chr == L'\t')) {
					result << string(&chr, 1, Encoding::UTF32);
				}
			} while (chr != 0xFFFFFFFF && chr != L'\n');
			return result.ToString();
		}
		string ITextReader::ReadAll(void) const
		{
			DynamicString result;
			uint32 chr;
			do {
				chr = ReadChar();
				if (chr != 0xFFFFFFFF) {
					result << string(&chr, 1, Encoding::UTF32);
				}
			} while (chr != 0xFFFFFFFF);
			return result.ToString();
		}
		bool ITextReader::EofReached(void) const { return eof; }
		Encoding TextReader::GetEncoding(void) const { return coding; }
		TextReader::~TextReader(void) { source->Release(); }
		string TextReader::ToString(void) const { return L"TextReader"; }
		ITextReader & ITextReader::operator >> (string & str) { str = ReadLine(); return *this; }
		const ITextReader & ITextReader::operator >> (string & str) const { str = ReadLine(); return *this; }
	}
	namespace IO {
		void CreateDirectoryTree(const string & path)
		{
			string full = ExpandPath(path);
			for (int i = 0; i < full.Length(); i++) {
				if (full[i] == L'/' || full[i] == L'\\') {
					try { CreateDirectory(full.Fragment(0, i)); } catch (...) {}
				}
			}
			try {
				CreateDirectory(path);
			}
			catch (DirectoryAlreadyExistsException &) {}
		}
		void RemoveEntireDirectory(const string & path)
		{
			string full = ExpandPath(path);
			SafePointer< Array<string> > files = Search::GetFiles(full + L"\\*");
			for (int i = 0; i < files->Length(); i++) RemoveFile(full + L"\\" + files->ElementAt(i));
			files = Search::GetDirectories(full + L"\\*");
			for (int i = 0; i < files->Length(); i++) RemoveEntireDirectory(full + L"\\" + files->ElementAt(i));
			RemoveDirectory(full);
		}
	}
}