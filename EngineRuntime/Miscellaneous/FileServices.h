#pragma once

#include "../EngineBase.h"

namespace Engine
{
	namespace Streaming
	{
		enum FileAccess { AccessNo, AccessRead, AccessWrite, AccessReadWrite };
		enum FileCreationMode { CreateAlways, CreateNew, OpenAlways, OpenExisting, TruncateExisting };
		enum SeekOrigin { Begin, Current, End };
	}
	namespace IO
	{
		namespace Error { enum ErrorCode {
			Success = 0x0000,
			Unknown = 0x0001,
			FileNotFound = 0x0002,
			PathNotFound = 0x0003,
			TooManyOpenFiles = 0x0004,
			AccessDenied = 0x0005,
			InvalidHandle = 0x0006,
			NotEnoughMemory = 0x0007,
			InvalidDevice = 0x0008,
			IsReadOnly = 0x0009,
			NoDiskSpace = 0x000A,
			FileExists = 0x000B,
			NotImplemented = 0x000C,
			DirectoryNotEmpty = 0x000D,
			DirectoryIsCurrent = 0x000E,
			NotSameDevice = 0x000F,
			BadPathName = 0x0010,
			FileNameTooLong = 0x0011,
			FileTooLarge = 0x0012,
			ReadFailure = 0x0013,
			WriteFailure = 0x0014,
			CreateFailure = 0x0015,
			OpenFailure = 0x0016,
		}; }

		enum class FileType { Unknown, Regular, Directory, SymbolicLink };

		extern const handle InvalidHandle;

#ifdef ENGINE_WINDOWS
		constexpr widechar PathDirectorySeparator = L'\\';
		constexpr const widechar * LineFeedSequence = L"\r\n";
		constexpr Encoding TextFileEncoding = Encoding::UTF8;
#endif
#ifdef ENGINE_UNIX
		constexpr widechar PathDirectorySeparator = L'/';
		constexpr const widechar * LineFeedSequence = L"\n";
		constexpr Encoding TextFileEncoding = Encoding::UTF8;
#endif
		class FileAccessException : public Exception { public: uint code; FileAccessException(uint ec); FileAccessException(void); string ToString(void) const override; };
		class FileReadEndOfFileException : public Exception { public: uint32 DataRead; FileReadEndOfFileException(uint32 data_read); string ToString(void) const override; };
		class DirectoryAlreadyExistsException : public Exception { public: string ToString(void) const override; };
		class FileFormatException : public Exception { public: string ToString(void) const override; };

		namespace Path
		{
			string NormalizePath(const string & path);
			string GetExtension(const string & path);
			string GetFileName(const string & path);
			string GetDirectory(const string & path);
			string GetFileNameWithoutExtension(const string & path);
		}
	}
}