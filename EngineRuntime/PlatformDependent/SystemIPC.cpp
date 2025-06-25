#include "../Interfaces/SystemIPC.h"

#include <Windows.h>

#undef CreateNamedPipe

namespace Engine
{
	namespace IO
	{
		uint WinErrorToEngineError(DWORD code);
	}
	namespace IPC
	{
		class SystemConnection : public IConnection
		{
			HANDLE _io;
			OVERLAPPED _async_in, _async_out;
			bool _threaded_mode;
		private:
			uint32 _internal_read(void * buffer, uint32 length)
			{
				DWORD read;
				if (_threaded_mode) {
					if (ReadFile(_io, buffer, length, &read, &_async_in)) return read;
					if (GetLastError() == ERROR_IO_PENDING) {
						if (GetOverlappedResult(_io, &_async_in, &read, TRUE)) return read;
					}
				} else {
					if (ReadFile(_io, buffer, length, &read, 0)) return read;
				}
				throw IO::FileAccessException(IO::WinErrorToEngineError(GetLastError()));
			}
			uint32 _internal_write(const void * buffer, uint32 length)
			{
				DWORD written;
				if (_threaded_mode) {
					if (WriteFile(_io, buffer, length, &written, &_async_out)) return written;
					if (GetLastError() == ERROR_IO_PENDING) {
						if (GetOverlappedResult(_io, &_async_out, &written, TRUE)) return written;
					}
				} else {
					if (WriteFile(_io, buffer, length, &written, 0)) return written;
				}
				throw IO::FileAccessException(IO::WinErrorToEngineError(GetLastError()));
			}
		public:
			SystemConnection(const string & listener_name, uint flags, uint * error)
			{
				try {
					auto path = L"\\\\.\\pipe\\" + listener_name;
					DWORD flag = 0;
					if (flags & CreateConnectionMultiThreaded) {
						flag = FILE_FLAG_OVERLAPPED;
						_threaded_mode = true;
					} else _threaded_mode = false;
					_io = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, flag, 0);
				} catch (...) { if (error) *error = ErrorAllocation; throw; }
				if (_io == INVALID_HANDLE_VALUE) {
					if (error) {
						auto werror = GetLastError();
						if (werror == ERROR_FILE_NOT_FOUND) *error = ErrorDoesNotExist;
						else if (werror == ERROR_BAD_PATHNAME || werror == ERROR_INVALID_NAME) *error = ErrorBadFileName;
						else *error = ErrorAllocation;
					}
					throw IO::FileAccessException();
				}
				if (_threaded_mode) {
					_async_in.hEvent = CreateEventW(0, TRUE, FALSE, L"");
					if (!_async_in.hEvent) { CloseHandle(_io); if (error) *error = ErrorAllocation; throw OutOfMemoryException(); }
					_async_out.hEvent = CreateEventW(0, TRUE, FALSE, L"");
					if (!_async_out.hEvent) { CloseHandle(_io); CloseHandle(_async_in.hEvent); if (error) *error = ErrorAllocation; throw OutOfMemoryException(); }
					_async_in.Offset = _async_in.OffsetHigh = _async_out.Offset = _async_out.OffsetHigh = 0;
				}
				if (error) *error = ErrorSuccess;
			}
			SystemConnection(HANDLE io, bool threaded) : _io(io), _threaded_mode(threaded)
			{
				if (_threaded_mode) {
					_async_in.hEvent = CreateEventW(0, TRUE, FALSE, L"");
					if (!_async_in.hEvent) throw OutOfMemoryException();
					_async_out.hEvent = CreateEventW(0, TRUE, FALSE, L"");
					if (!_async_out.hEvent) { CloseHandle(_async_in.hEvent); throw OutOfMemoryException(); }
					_async_in.Offset = _async_in.OffsetHigh = _async_out.Offset = _async_out.OffsetHigh = 0;
				}
			}
			virtual ~SystemConnection(void) override { CloseHandle(_io); if (_threaded_mode) { CloseHandle(_async_in.hEvent); CloseHandle(_async_out.hEvent); } }
			virtual void Read(void * buffer, uint32 length) override { uint bytes; if ((bytes = _internal_read(buffer, length)) != length) throw IO::FileReadEndOfFileException(bytes); }
			virtual void Write(const void * data, uint32 length) override { if (_internal_write(data, length) != length) throw IO::FileAccessException(IO::Error::WriteFailure); }
			virtual int64 Seek(int64 position, Streaming::SeekOrigin origin) override { throw Exception(); }
			virtual uint64 Length(void) override { throw Exception(); }
			virtual void SetLength(uint64 length) override { throw Exception(); }
			virtual void Flush(void) override {}
			virtual handle GetIOHandle(void) noexcept override { return _threaded_mode ? IO::InvalidHandle : _io; }
		};
		class SystemConnectionListener : public IConnectionListener
		{
			string _path, _name;
			HANDLE _io;
			OVERLAPPED _async;
			bool _threaded_mode;
		public:
			SystemConnectionListener(const string & listener_name, uint flags, uint * error)
			{
				try {
					_name = listener_name;
					_path = L"\\\\.\\pipe\\" + listener_name;
					DWORD flag = PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE;
					if (flags & CreateConnectionMultiThreaded) {
						flag |= FILE_FLAG_OVERLAPPED;
						_threaded_mode = true;
					} else _threaded_mode = false;
					_io = CreateNamedPipeW(_path, flag, PIPE_REJECT_REMOTE_CLIENTS, PIPE_UNLIMITED_INSTANCES, 512, 512, 0, 0);
				} catch (...) { if (error) *error = ErrorAllocation; throw; }
				if (_io == INVALID_HANDLE_VALUE) {
					if (error) {
						auto werror = GetLastError();
						if (werror == ERROR_ACCESS_DENIED) *error = ErrorAlreadyExists;
						else if (werror == ERROR_BAD_PATHNAME || werror == ERROR_INVALID_NAME) *error = ErrorBadFileName;
						else *error = ErrorAllocation;
					}
					throw IO::FileAccessException();
				}
				if (_threaded_mode) {
					_async.hEvent = CreateEventW(0, TRUE, FALSE, L"");
					if (!_async.hEvent) { CloseHandle(_io); if (error) *error = ErrorAllocation; throw OutOfMemoryException(); }
					_async.Offset = _async.OffsetHigh = 0;
				}
				if (error) *error = ErrorSuccess;
			}
			virtual ~SystemConnectionListener(void) override { CloseHandle(_io); if (_threaded_mode) CloseHandle(_async.hEvent); }
			virtual IConnection * Accept(void) noexcept override
			{
				bool connected = false;
				if (_threaded_mode) {
					if (ConnectNamedPipe(_io, &_async)) {
						connected = true;
					} else if (GetLastError() == ERROR_IO_PENDING) {
						DWORD size;
						if (GetOverlappedResult(_io, &_async, &size, TRUE)) connected = true;
					}
				} else {
					if (ConnectNamedPipe(_io, 0)) connected = true;
				}
				if (connected || GetLastError() == ERROR_PIPE_CONNECTED) {
					DWORD flag = PIPE_ACCESS_DUPLEX;
					if (_threaded_mode) flag |= FILE_FLAG_OVERLAPPED;
					auto new_io = CreateNamedPipeW(_path, flag, PIPE_REJECT_REMOTE_CLIENTS, PIPE_UNLIMITED_INSTANCES, 512, 512, 0, 0);
					if (new_io == INVALID_HANDLE_VALUE) return 0;
					swap(_io, new_io);
					try {
						SafePointer<SystemConnection> con = new SystemConnection(new_io, _threaded_mode);
						con->Retain();
						return con;
					} catch (...) { CloseHandle(new_io); return 0; }
				} else return 0;
			}
			virtual string GetOSPath(void) override { return _path; }
			virtual string GetListenerName(void) override { return _name; }
		};
		class SystemSharedMemory : public ISharedMemory
		{
			HANDLE _file;
			string _name;
			void * _lock_addr;
			uint _length;
		public:
			SystemSharedMemory(const string & segment_name, uint length, uint flags, uint * error) : _lock_addr(0), _length(length)
			{
				try { _name = segment_name; } catch (...) { if (error) *error = ErrorAllocation; throw; }
				if (!length) { if (error) *error = ErrorInvalidArgument; throw Exception(); }
				if (flags & SharedMemoryCreateNew) {
					_file = CreateFileMappingW(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, length, _name);
					if (_file && GetLastError() == ERROR_ALREADY_EXISTS) {
						CloseHandle(_file);
						if (error) *error = ErrorAlreadyExists;
						throw Exception();
					} else if (!_file) {
						if (error) {
							auto werror = GetLastError();
							if (werror == ERROR_BAD_PATHNAME || werror == ERROR_INVALID_NAME) *error = ErrorBadFileName;
							else *error = ErrorAllocation;
						}
						throw Exception();
					}
				} else {
					_file = CreateFileMappingW(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, length, _name);
					if (_file && GetLastError() != ERROR_ALREADY_EXISTS) {
						CloseHandle(_file);
						if (error) *error = ErrorDoesNotExist;
						throw Exception();
					} else if (!_file) {
						if (error) {
							auto werror = GetLastError();
							if (werror == ERROR_BAD_PATHNAME || werror == ERROR_INVALID_NAME) *error = ErrorBadFileName;
							else *error = ErrorAllocation;
						}
						throw Exception();
					}
				}
				if (error) *error = ErrorSuccess;
			}
			virtual ~SystemSharedMemory(void) override { if (_lock_addr) Unmap(); CloseHandle(_file); }
			virtual string GetSegmentName(void) override { return _name; }
			virtual uint GetLength(void) noexcept override { return _length; }
			virtual bool Map(void ** pdata, uint map_flags) noexcept override
			{
				if (_lock_addr) return false;
				DWORD flags = 0;
				if (map_flags & SharedMemoryMapRead) flags |= FILE_MAP_READ;
				if (map_flags & SharedMemoryMapWrite) flags |= FILE_MAP_WRITE;
				if (!flags) return false;
				_lock_addr = MapViewOfFile(_file, flags, 0, 0, _length);
				if (!_lock_addr) return false;
				if (pdata) *pdata = _lock_addr;
				return true;
			}
			virtual void Unmap(void) noexcept override { if (_lock_addr) UnmapViewOfFile(_lock_addr); _lock_addr = 0; }
		};
		class SystemSharedLock : public ISharedLock
		{
			HANDLE _semaphore;
			string _name;
			bool _locked;
		public:
			SystemSharedLock(const string & lock_name, uint * error) : _locked(false)
			{
				try {
					_name = lock_name;
					_semaphore = CreateSemaphoreW(0, 1, 1, _name);
				} catch (...) { if (error) *error = ErrorAllocation; throw; }
				if (!_semaphore) {
					if (error) {
						auto werror = GetLastError();
						if (werror == ERROR_INVALID_HANDLE) *error = ErrorAlreadyExists;
						else if (werror == ERROR_BAD_PATHNAME || werror == ERROR_INVALID_NAME) *error = ErrorBadFileName;
						else *error = ErrorAllocation;
					}
					throw Exception();
				}
				if (error) *error = ErrorSuccess;
			}
			virtual ~SystemSharedLock(void) override { Open(); CloseHandle(_semaphore); }
			virtual string GetLockName(void) override { return _name; }
			virtual bool TryWait(void) noexcept override
			{
				if (_locked) return false;
				auto result = WaitForSingleObject(_semaphore, 0) == WAIT_OBJECT_0;
				if (result) _locked = true;
				return result;
			}
			virtual void Open(void) noexcept override { if (_locked) { ReleaseSemaphore(_semaphore, 1, 0); _locked = false; } }
		};

		IConnection * Connect(const string & listener_name, uint flags, uint * error) noexcept { try { if (error) *error = ErrorAllocation; return new SystemConnection(listener_name, flags, error); } catch (...) { return 0; } }
		IConnectionListener * CreateConnectionListener(const string & listener_name, uint flags, uint * error) noexcept { if (error) *error = ErrorAllocation; try { return new SystemConnectionListener(listener_name, flags, error); } catch (...) { return 0; } }
		ISharedMemory * CreateSharedMemory(const string & segment_name, uint length, uint flags, uint * error) noexcept { if (error) *error = ErrorAllocation; try { return new SystemSharedMemory(segment_name, length, flags, error); } catch (...) { return 0; } }
		ISharedLock * CreateSharedLock(const string & lock_name, uint * error) noexcept { try { if (error) *error = ErrorAllocation; return new SystemSharedLock(lock_name, error); } catch (...) { return 0; } }

		void DestroyConnectionListener(const string & name) noexcept {}
		void DestroySharedMemory(const string & name) noexcept {}
		void DestroySharedLock(const string & name) noexcept {}
	}
}