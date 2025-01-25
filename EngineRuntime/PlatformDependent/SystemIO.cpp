#include "../Interfaces/SystemIO.h"
#include "../Miscellaneous/DynamicString.h"
#include "../Syntax/Regular.h"

#include <Windows.h>

#undef CreateFile
#undef MoveFile
#undef SetCurrentDirectory
#undef GetCurrentDirectory
#undef CreateDirectory
#undef RemoveDirectory
#undef CreateSymbolicLink
#undef CreateHardLink

namespace Engine
{
	namespace IO
	{
		uint WinErrorToEngineError(DWORD code)
		{
			if (code == ERROR_SUCCESS) return Error::Success;
			else if (code == ERROR_FILE_NOT_FOUND) return Error::FileNotFound;
			else if (code == ERROR_PATH_NOT_FOUND) return Error::PathNotFound;
			else if (code == ERROR_TOO_MANY_OPEN_FILES) return Error::TooManyOpenFiles;
			else if (code == ERROR_ACCESS_DENIED) return Error::AccessDenied;
			else if (code == ERROR_INVALID_HANDLE) return Error::InvalidHandle;
			else if (code == ERROR_NOT_ENOUGH_MEMORY) return Error::NotEnoughMemory;
			else if (code == ERROR_OUTOFMEMORY) return Error::NotEnoughMemory;
			else if (code == ERROR_INVALID_DRIVE) return Error::InvalidDevice;
			else if (code == ERROR_CURRENT_DIRECTORY) return Error::DirectoryIsCurrent;
			else if (code == ERROR_NOT_SAME_DEVICE) return Error::NotSameDevice;
			else if (code == ERROR_WRITE_PROTECT) return Error::IsReadOnly;
			else if (code == ERROR_BAD_UNIT) return Error::InvalidDevice;
			else if (code == ERROR_WRITE_FAULT) return Error::WriteFailure;
			else if (code == ERROR_READ_FAULT) return Error::ReadFailure;
			else if (code == ERROR_SHARING_VIOLATION) return Error::AccessDenied;
			else if (code == ERROR_LOCK_VIOLATION) return Error::AccessDenied;
			else if (code == ERROR_HANDLE_DISK_FULL) return Error::NoDiskSpace;
			else if (code == ERROR_NOT_SUPPORTED) return Error::NotImplemented;
			else if (code == ERROR_DEV_NOT_EXIST) return Error::InvalidDevice;
			else if (code == ERROR_FILE_EXISTS) return Error::FileExists;
			else if (code == ERROR_CANNOT_MAKE) return Error::CreateFailure;
			else if (code == ERROR_OPEN_FAILED) return Error::OpenFailure;
			else if (code == ERROR_DISK_FULL) return Error::NoDiskSpace;
			else if (code == ERROR_CALL_NOT_IMPLEMENTED) return Error::NotImplemented;
			else if (code == ERROR_INVALID_NAME) return Error::BadPathName;
			else if (code == ERROR_DIR_NOT_EMPTY) return Error::DirectoryNotEmpty;
			else if (code == ERROR_BAD_PATHNAME) return Error::BadPathName;
			else if (code == ERROR_ALREADY_EXISTS) return Error::FileExists;
			else if (code == ERROR_FILENAME_EXCED_RANGE) return Error::FileNameTooLong;
			else if (code == ERROR_FILE_TOO_LARGE) return Error::FileTooLarge;
			else if (code == ERROR_DIRECTORY) return Error::BadPathName;
			return Error::Unknown;
		}
		string ExpandPath(const string & path)
		{
			string Path = Path::NormalizePath(path);
			DynamicString exp(0x100);
			exp.ReserveLength(0x1000);
			do {
				auto result = GetFullPathNameW(Path, exp.ReservedLength(), exp, 0);
				if (!result) throw Exception();
				if (result > uint(exp.ReservedLength())) exp.ReserveLength(result);
				else break;
			} while (true);
			return exp;
		}
		handle CreateFile(const string & path, Streaming::FileAccess access, Streaming::FileCreationMode mode)
		{
			DWORD Access = 0;
			DWORD Share = 0;
			if (access == Streaming::FileAccess::AccessRead) {
				Access = GENERIC_READ;
				Share = FILE_SHARE_READ;
			} else if (access == Streaming::FileAccess::AccessWrite) {
				Access = GENERIC_WRITE;
			} else if (access == Streaming::FileAccess::AccessReadWrite) {
				Access = GENERIC_READ | GENERIC_WRITE;
			} else if (access == Streaming::FileAccess::AccessNo) {
				Share = FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE;
			} else throw InvalidArgumentException();
			DWORD Creation = 0;
			if (mode == Streaming::FileCreationMode::CreateAlways) Creation = CREATE_ALWAYS;
			else if (mode == Streaming::FileCreationMode::CreateNew) Creation = CREATE_NEW;
			else if (mode == Streaming::FileCreationMode::OpenAlways) Creation = OPEN_ALWAYS;
			else if (mode == Streaming::FileCreationMode::OpenExisting) Creation = OPEN_EXISTING;
			else if (mode == Streaming::FileCreationMode::TruncateExisting) Creation = TRUNCATE_EXISTING;
			else throw InvalidArgumentException();
			DWORD Flags = FILE_ATTRIBUTE_NORMAL;
			handle file = CreateFileW(Path::NormalizePath(path), Access, Share, 0, Creation, Flags, 0);
			if (file == INVALID_HANDLE_VALUE) throw FileAccessException(WinErrorToEngineError(GetLastError()));
			return file;
		}
		void CreatePipe(handle * pipe_in, handle * pipe_out) { if (!::CreatePipe(pipe_out, pipe_in, 0, 0)) throw FileAccessException(WinErrorToEngineError(GetLastError())); }
		handle GetStandardOutput(void)
		{
			auto ret = GetStdHandle(STD_OUTPUT_HANDLE);
			if (ret == INVALID_HANDLE_VALUE || !ret) return InvalidHandle;
			return ret;
		}
		handle GetStandardInput(void)
		{
			auto ret = GetStdHandle(STD_INPUT_HANDLE);
			if (ret == INVALID_HANDLE_VALUE || !ret) return InvalidHandle;
			return ret;
		}
		handle GetStandardError(void)
		{
			auto ret = GetStdHandle(STD_ERROR_HANDLE);
			if (ret == INVALID_HANDLE_VALUE || !ret) return InvalidHandle;
			return ret;
		}
		void SetStandardOutput(handle file)
		{
			if (file != InvalidHandle) {
				handle dup;
				if (!DuplicateHandle(GetCurrentProcess(), file, GetCurrentProcess(), &dup, 0, TRUE, DUPLICATE_SAME_ACCESS)) throw FileAccessException(WinErrorToEngineError(GetLastError()));
				CloseHandle(GetStdHandle(STD_OUTPUT_HANDLE));
				SetStdHandle(STD_OUTPUT_HANDLE, dup);
			} else {
				CloseHandle(GetStdHandle(STD_OUTPUT_HANDLE));
				SetStdHandle(STD_OUTPUT_HANDLE, 0);
			}
		}
		void SetStandardInput(handle file)
		{
			if (file != InvalidHandle) {
				handle dup;
				if (!DuplicateHandle(GetCurrentProcess(), file, GetCurrentProcess(), &dup, 0, TRUE, DUPLICATE_SAME_ACCESS)) throw FileAccessException(WinErrorToEngineError(GetLastError()));
				CloseHandle(GetStdHandle(STD_INPUT_HANDLE));
				SetStdHandle(STD_INPUT_HANDLE, dup);
			} else {
				CloseHandle(GetStdHandle(STD_INPUT_HANDLE));
				SetStdHandle(STD_INPUT_HANDLE, 0);
			}
		}
		void SetStandardError(handle file)
		{
			if (file != InvalidHandle) {
				handle dup;
				if (!DuplicateHandle(GetCurrentProcess(), file, GetCurrentProcess(), &dup, 0, TRUE, DUPLICATE_SAME_ACCESS)) throw FileAccessException(WinErrorToEngineError(GetLastError()));
				CloseHandle(GetStdHandle(STD_ERROR_HANDLE));
				SetStdHandle(STD_ERROR_HANDLE, dup);
			} else {
				CloseHandle(GetStdHandle(STD_ERROR_HANDLE));
				SetStdHandle(STD_ERROR_HANDLE, 0);
			}
		}
		handle CloneHandle(handle file)
		{
			if (file == InvalidHandle) return InvalidHandle;
			handle result;
			if (!DuplicateHandle(GetCurrentProcess(), file, GetCurrentProcess(), &result, 0, FALSE, DUPLICATE_SAME_ACCESS)) throw FileAccessException(WinErrorToEngineError(GetLastError()));
			return result;
		}
		void CloseHandle(handle file)
		{
			if (file == InvalidHandle) return;
			if (!::CloseHandle(file)) throw FileAccessException(WinErrorToEngineError(GetLastError()));
		}
		void Flush(handle file) { if (!FlushFileBuffers(file)) throw FileAccessException(WinErrorToEngineError(GetLastError())); }
		uint64 GetFileSize(handle file)
		{
			LARGE_INTEGER v;
			if (!GetFileSizeEx(file, &v)) throw FileAccessException(WinErrorToEngineError(GetLastError()));
			return uint64(v.QuadPart);
		}
		void MoveFile(const string & from, const string & to) { if (!MoveFileW(Path::NormalizePath(from), Path::NormalizePath(to))) throw FileAccessException(WinErrorToEngineError(GetLastError())); }
		bool FileExists(const string & path)
		{
			handle file = CreateFileW(Path::NormalizePath(path), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
			if (file == INVALID_HANDLE_VALUE) return false;
			CloseHandle(file);
			return true;
		}
		void ReadFile(handle file, void * to, uint32 amount)
		{
			DWORD Read;
			if (!::ReadFile(file, to, amount, &Read, 0) && GetLastError() != ERROR_BROKEN_PIPE) throw FileAccessException(WinErrorToEngineError(GetLastError()));
			if (Read != amount) throw FileReadEndOfFileException(Read);
		}
		void WriteFile(handle file, const void * data, uint32 amount)
		{
			DWORD Written;
			if (!::WriteFile(file, data, amount, &Written, 0)) throw FileAccessException(WinErrorToEngineError(GetLastError()));
			if (Written != amount) throw FileAccessException(Error::FileTooLarge);
		}
		int64 Seek(handle file, int64 position, Streaming::SeekOrigin origin)
		{
			LARGE_INTEGER set, get;
			set.QuadPart = position;
			DWORD org = FILE_BEGIN;
			if (origin == Streaming::SeekOrigin::Current) org = FILE_CURRENT;
			else if (origin == Streaming::SeekOrigin::End) org = FILE_END;
			if (!SetFilePointerEx(file, set, &get, org)) throw FileAccessException(WinErrorToEngineError(GetLastError()));
			return get.QuadPart;
		}
		void SetFileSize(handle file, uint64 size)
		{
			LARGE_INTEGER pos, len;
			len.QuadPart = int64(size);
			if (!SetFilePointerEx(file, { 0, 0 }, &pos, FILE_CURRENT)) throw FileAccessException(WinErrorToEngineError(GetLastError()));
			if (!SetFilePointerEx(file, len, 0, FILE_BEGIN)) throw FileAccessException(WinErrorToEngineError(GetLastError()));
			if (!SetEndOfFile(file)) { SetFilePointerEx(file, pos, 0, FILE_BEGIN); throw FileAccessException(WinErrorToEngineError(GetLastError())); }
			SetFilePointerEx(file, pos, 0, FILE_BEGIN);
		}
		void RemoveFile(const string & path)
		{
			if (!DeleteFileW(Path::NormalizePath(path))) throw FileAccessException(WinErrorToEngineError(GetLastError()));
		}
		void SetCurrentDirectory(const string & path)
		{
			if (!SetCurrentDirectoryW(ExpandPath(path))) throw FileAccessException(WinErrorToEngineError(GetLastError()));
		}
		string GetCurrentDirectory(void)
		{
			DynamicString exp(0x100);
			exp.ReserveLength(0x1000);
			do {
				auto result = GetCurrentDirectoryW(exp.ReservedLength(), exp);
				if (!result) throw Exception();
				if (result > uint(exp.ReservedLength())) exp.ReserveLength(result);
				else break;
			} while (true);
			return exp;
		}
		void CreateDirectory(const string & path)
		{
			if (!CreateDirectoryW(Path::NormalizePath(path), 0)) {
				if (GetLastError() == ERROR_ALREADY_EXISTS) throw DirectoryAlreadyExistsException();
				throw FileAccessException(WinErrorToEngineError(GetLastError()));
			}
		}
		void RemoveDirectory(const string & path)
		{
			if (!RemoveDirectoryW(Path::NormalizePath(path))) throw FileAccessException(WinErrorToEngineError(GetLastError()));
		}
		void CreateSymbolicLink(const string & at, const string & to)
		{
			auto to_norm = Path::NormalizePath(to);
			auto attr = GetFileAttributesW(to_norm);
			if (attr == INVALID_FILE_ATTRIBUTES) throw FileAccessException(WinErrorToEngineError(GetLastError()));
			if (attr & FILE_ATTRIBUTE_DIRECTORY) {
				if (!CreateSymbolicLinkW(Path::NormalizePath(at), to_norm, SYMBOLIC_LINK_FLAG_DIRECTORY)) throw FileAccessException(WinErrorToEngineError(GetLastError()));
			} else {
				if (!CreateSymbolicLinkW(Path::NormalizePath(at), to_norm, 0)) throw FileAccessException(WinErrorToEngineError(GetLastError()));
			}
		}
		void CreateHardLink(const string & at, const string & to)
		{
			if (!CreateHardLinkW(Path::NormalizePath(at), Path::NormalizePath(to), 0)) throw FileAccessException(WinErrorToEngineError(GetLastError()));
		}
		FileType GetFileType(const string & file)
		{
			auto hobj = CreateFileW(Path::NormalizePath(file), 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, 0);
			if (hobj == INVALID_HANDLE_VALUE) throw FileAccessException(WinErrorToEngineError(GetLastError()));
			auto result = FileType::Unknown;
			FILE_ATTRIBUTE_TAG_INFO info;
			if (GetFileInformationByHandleEx(hobj, FileAttributeTagInfo, &info, sizeof(info))) {
				if (info.ReparseTag == IO_REPARSE_TAG_SYMLINK) result = FileType::SymbolicLink;
				else if (info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) result = FileType::Directory;
				else result = FileType::Regular;
			}
			CloseHandle(hobj);
			return result;
		}
		string GetSymbolicLinkDestination(const string & file)
		{
			DynamicString str;
			auto hobj = CreateFileW(Path::NormalizePath(file), 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
			if (hobj == INVALID_HANDLE_VALUE) throw FileAccessException(WinErrorToEngineError(GetLastError()));
			auto len = GetFinalPathNameByHandleW(hobj, 0, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
			if (!len) throw FileAccessException(WinErrorToEngineError(GetLastError()));
			try {
				str.ReserveLength(len);
				auto res = GetFinalPathNameByHandleW(hobj, str, str.ReservedLength(), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
				if (!res) throw FileAccessException(WinErrorToEngineError(GetLastError()));
			} catch (...) {
				CloseHandle(hobj);
				throw;
			}
			CloseHandle(hobj);
			auto result = str.ToString();
			if (result.Fragment(0, 4) == L"\\\\?\\") result = result.Fragment(4, -1);
			return result;
		}
		void GetVolumeSpace(const string & volume, uint64 * total_bytes, uint64 * free_bytes, uint64 * user_available_bytes)
		{
			ULARGE_INTEGER total, free, user;
			if (!GetDiskFreeSpaceExW(Path::NormalizePath(volume), &user, &total, &free)) throw FileAccessException(WinErrorToEngineError(GetLastError()));
			if (total_bytes) *total_bytes = total.QuadPart;
			if (free_bytes) *free_bytes = free.QuadPart;
			if (user_available_bytes) *user_available_bytes = user.QuadPart;
		}
		string GetExecutablePath(void)
		{
			DynamicString path;
			path.ReserveLength(0x1000);
			do {
				SetLastError(ERROR_SUCCESS);
				if (!GetModuleFileNameW(0, path, path.ReservedLength())) throw Exception();
				if (GetLastError() == ERROR_SUCCESS) return path;
				else if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) throw Exception();
				path.ReserveLength(path.ReservedLength() * 2);
			} while (true);
			return path;
		}
		namespace Search
		{
			namespace SearchHelper
			{
				void FillFiles(Array<string> * to, const string & path, const string & filter, const string & prefix, bool recursive)
				{
					if (recursive) {
						SafePointer< Array<string> > subs = GetDirectories(path + L"\\*");
						for (int i = 0; i < subs->Length(); i++) {
							FillFiles(to, path + L"\\" + subs->ElementAt(i), filter, prefix + subs->ElementAt(i) + string(PathDirectorySeparator), recursive);
						}
					}
					WIN32_FIND_DATAW Find;
					HANDLE Search = FindFirstFileW(path + L"\\*", &Find);
					if (Search != INVALID_HANDLE_VALUE) {
						BOOL Continue = TRUE;
						while (Continue) {
							if ((Find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && Syntax::MatchFilePattern(Find.cFileName, filter)) {
								to->Append(prefix + Find.cFileName);
							}
							Continue = FindNextFileW(Search, &Find);
						}
						FindClose(Search);
					}
				}
			}
			Array<string> * GetFiles(const string & path, bool recursive)
			{
				SafePointer< Array<string> > result = new Array<string>(0x10);
				auto epath = ExpandPath(path);
				SearchHelper::FillFiles(result, Path::GetDirectory(epath), Path::GetFileName(epath), L"", recursive);
				result->Retain();
				return result;
			}
			Array<string> * GetDirectories(const string & path)
			{
				SafePointer< Array<string> > result = new Array<string>(0x10);
				auto epath = ExpandPath(path);
				auto filter = Path::GetFileName(epath);
				WIN32_FIND_DATAW Find;
				HANDLE Search = FindFirstFileW(Path::GetDirectory(epath) + L"\\*", &Find);
				if (Search != INVALID_HANDLE_VALUE) {
					BOOL Continue = TRUE;
					while (Continue) {
						if ((Find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && Syntax::MatchFilePattern(Find.cFileName, filter) &&
							StringCompare(Find.cFileName, L".") != 0 && StringCompare(Find.cFileName, L"..") != 0) {
							result->Append(Find.cFileName);
						}
						Continue = FindNextFileW(Search, &Find);
					}
					FindClose(Search);
				}
				result->Retain();
				return result;
			}
			Array<Volume> * GetVolumes(void)
			{
				SafePointer< Array<Volume> > result = new Array<Volume>(0x10);
				auto drives = GetLogicalDrives();
				widechar drive = L'A';
				while (drives) {
					if (drives & 1) {
						DynamicString label;
						Volume vol;
						vol.Path = string(drive) + L":";
						label.ReserveLength(MAX_PATH + 1);
						if (GetVolumeInformationW(vol.Path, label, label.ReservedLength(), 0, 0, 0, 0, 0)) vol.Label = label.ToString();
						result->Append(vol);
					}
					drive++;
					drives = (drives >> 1);
				}
				result->Retain();
				return result;
			}
		}
		namespace DateTime
		{
			Time GetFileCreationTime(handle file)
			{
				FILETIME time;
				GetFileTime(file, &time, 0, 0);
				return Time::FromWindowsTime(*reinterpret_cast<uint64 *>(&time));
			}
			Time GetFileAccessTime(handle file)
			{
				FILETIME time;
				GetFileTime(file, 0, &time, 0);
				return Time::FromWindowsTime(*reinterpret_cast<uint64 *>(&time));
			}
			Time GetFileAlterTime(handle file)
			{
				FILETIME time;
				GetFileTime(file, 0, 0, &time);
				return Time::FromWindowsTime(*reinterpret_cast<uint64 *>(&time));
			}
			void SetFileCreationTime(handle file, Time time)
			{
				FILETIME os;
				*reinterpret_cast<uint64 *>(&os) = time.ToWindowsTime();
				SetFileTime(file, &os, 0, 0);
			}
			void SetFileAccessTime(handle file, Time time)
			{
				FILETIME os;
				*reinterpret_cast<uint64 *>(&os) = time.ToWindowsTime();
				SetFileTime(file, 0, &os, 0);
			}
			void SetFileAlterTime(handle file, Time time)
			{
				FILETIME os;
				*reinterpret_cast<uint64 *>(&os) = time.ToWindowsTime();
				SetFileTime(file, 0, 0, &os);
			}
		}
	}
}