#include "../Interfaces/SystemIPC.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

namespace Engine
{
	namespace IPC
	{
		Array<char> * _encode_utf8(const string & str)
		{
			SafePointer< Array<char> > result = new Array<char>(1);
			result->SetLength(str.GetEncodedLength(Encoding::UTF8) + 1);
			str.Encode(result->GetBuffer(), Encoding::UTF8, true);
			result->Retain();
			return result;
		}

		class SystemConnection : public IConnection
		{
			int _socket;
		public:
			SystemConnection(const string & listener_name, uint flags, uint * error)
			{
				SafePointer< Array<char> > path;
				try {
					auto public_path = L"/tmp/pipe_" + listener_name;
					path = _encode_utf8(public_path);
				} catch (...) { if (error) *error = ErrorAllocation; throw; }
				sockaddr_un addr;
				ZeroMemory(&addr, sizeof(addr));
				addr.sun_len = sizeof(addr);
				addr.sun_family = PF_LOCAL;
				if (path->Length() > sizeof(addr.sun_path)) { if (error) *error = ErrorBadFileName; throw InvalidArgumentException(); }
				MemoryCopy(&addr.sun_path, path->GetBuffer(), path->Length());
				_socket = socket(PF_LOCAL, SOCK_STREAM, 0);
				if (_socket < 0) { if (error) *error = ErrorAllocation; throw Exception(); }
				int status;
				while (true) { if ((status = connect(_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr))) >= 0 || errno != EINTR) break; }
				if (status < 0) {
					if (error) {
						if (errno == EADDRNOTAVAIL || errno == ENAMETOOLONG) *error = ErrorBadFileName;
						else if (errno == ECONNREFUSED || errno == ENOENT) *error = ErrorDoesNotExist;
						else *error = ErrorAllocation;
					}
					close(_socket);
					throw Exception();
				}
				*error = ErrorSuccess;
			}
			SystemConnection(int io) : _socket(io) {}
			virtual ~SystemConnection(void) override { shutdown(_socket, SHUT_RDWR); close(_socket); }
			virtual void Read(void * buffer, uint32 length) override { IO::ReadFile(reinterpret_cast<handle>(sintptr(_socket)), buffer, length); }
			virtual void Write(const void * data, uint32 length) override { IO::WriteFile(reinterpret_cast<handle>(sintptr(_socket)), data, length); }
			virtual int64 Seek(int64 position, Streaming::SeekOrigin origin) override { throw Exception(); }
			virtual uint64 Length(void) override { throw Exception(); }
			virtual void SetLength(uint64 length) override { throw Exception(); }
			virtual void Flush(void) override {}
			virtual handle GetIOHandle(void) noexcept override { return reinterpret_cast<handle>(sintptr(_socket)); }
		};
		class SystemConnectionListener : public IConnectionListener
		{
			string _name, _public_path;
			SafePointer< Array<char> > _path;
			int _socket;
		public:
			SystemConnectionListener(const string & listener_name, uint flags, uint * error)
			{
				try {
					_name = listener_name;
					_public_path = L"/tmp/pipe_" + _name;
					_path = _encode_utf8(_public_path);
				} catch (...) { if (error) *error = ErrorAllocation; throw; }
				sockaddr_un addr;
				ZeroMemory(&addr, sizeof(addr));
				addr.sun_len = sizeof(addr);
				addr.sun_family = PF_LOCAL;
				if (_path->Length() > sizeof(addr.sun_path)) { if (error) *error = ErrorBadFileName; throw InvalidArgumentException(); }
				MemoryCopy(&addr.sun_path, _path->GetBuffer(), _path->Length());
				_socket = socket(PF_LOCAL, SOCK_STREAM, 0);
				if (_socket < 0) { if (error) *error = ErrorAllocation; throw Exception(); }
				if (bind(_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
					if (error) {
						if (errno == EADDRINUSE || errno == EEXIST) *error = ErrorAlreadyExists;
						else if (errno == EADDRNOTAVAIL || errno == ENAMETOOLONG) *error = ErrorBadFileName;
						else *error = ErrorAllocation;
					}
					close(_socket);
					throw Exception();
				}
				if (listen(_socket, SOMAXCONN) < 0) {
					if (error) *error = ErrorAllocation;
					unlink(*_path); close(_socket);
					throw Exception();
				}
				*error = ErrorSuccess;
			}
			virtual ~SystemConnectionListener(void) override { unlink(*_path); shutdown(_socket, SHUT_RDWR); close(_socket); }
			virtual IConnection * Accept(void) noexcept override
			{
				int io;
				while (true) { if ((io = accept(_socket, 0, 0)) >= 0 || errno != EINTR) break; }
				if (io < 0) return 0;
				try { return new SystemConnection(io); } catch (...) { close(io); return 0; }
			}
			virtual string GetOSPath(void) override { return _public_path; }
			virtual string GetListenerName(void) override { return _name; }
		};
		class SystemSharedMemory : public ISharedMemory
		{
			string _name;
			SafePointer< Array<char> > _path;
			int _file;
			uint _length;
			bool _unlink;
			void * _pdata;
		public:
			SystemSharedMemory(const string & segment_name, uint length, uint flags, uint * error) : _pdata(0)
			{
				if (!length) { if (error) *error = ErrorInvalidArgument; throw InvalidArgumentException(); }
				try {
					_name = segment_name;
					_path = _encode_utf8(segment_name);
				} catch (...) { if (error) *error = ErrorAllocation; throw; }
				_unlink = (flags & SharedMemoryCreateNew) != 0;
				int flag = O_RDWR;
				if (_unlink) flag |= O_CREAT | O_EXCL;
				while (true) {
					_file = shm_open(*_path, flag, 0666);
					if (_file >= 0 || errno != EINTR) break;
				}
				if (_file < 0) {
					if (error) {
						if (errno == EEXIST) *error = ErrorAlreadyExists;
						else if (errno == ENOENT) *error = ErrorDoesNotExist;
						else if (errno == ENAMETOOLONG) *error = ErrorBadFileName;
						else *error = ErrorAllocation;
					}
					throw Exception();
				}
				if (_unlink) {
					_length = length;
					while (true) {
						if (ftruncate(_file, length) >= 0) break; else if (errno != EINTR) {
							close(_file); shm_unlink(*_path);
							if (error) *error = ErrorAllocation;
							throw Exception();
						}
					}
				} else {
					struct stat fs;
					if (fstat(_file, &fs) < 0) {
						close(_file);
						if (error) *error = ErrorAllocation;
						throw Exception();
					}
					if (fs.st_size > 0xFFFFFFFF) _length = 0xFFFFFFFF;
					else _length = fs.st_size;
				}
				if (error) *error = ErrorSuccess;
			}
			virtual ~SystemSharedMemory(void) override { Unmap(); close(_file); if (_unlink) shm_unlink(*_path); }
			virtual string GetSegmentName(void) override { return _name; }
			virtual uint GetLength(void) noexcept override { return _length; }
			virtual bool Map(void ** pdata, uint map_flags) noexcept override
			{
				if (_pdata) return false;
				int prot = PROT_NONE;
				if (map_flags & SharedMemoryMapRead) prot |= PROT_READ;
				if (map_flags & SharedMemoryMapWrite) prot |= PROT_WRITE;
				_pdata = mmap(0, _length, prot, MAP_SHARED, _file, 0);
				if (_pdata == MAP_FAILED) {
					_pdata = 0;
					return false;
				}
				if (pdata) *pdata = _pdata;
				return true;
			}
			virtual void Unmap(void) noexcept override { if (_pdata) { munmap(_pdata, _length); _pdata = 0; } }
		};
		class SystemSharedLock : public ISharedLock
		{
			string _name;
			SafePointer< Array<char> > _path;
			int _file;
			bool _locked;
		public:
			SystemSharedLock(const string & lock_name, uint * error) : _locked(false)
			{
				try {
					string path = L"/tmp/lock_" + lock_name;
					_name = lock_name;
					_path = _encode_utf8(path);
				} catch (...) { if (error) *error = ErrorAllocation; throw; }
				while (true) {
					_file = open(*_path, O_RDONLY | O_CREAT, 0666);
					if (_file >= 0 || errno != EINTR) break;
				}
				if (_file < 0) {
					if (error) {
						if (errno == EISDIR) *error = ErrorAlreadyExists;
						else if (errno == ENAMETOOLONG || errno == EILSEQ) *error = ErrorBadFileName;
						else *error = ErrorAllocation;
					}
					throw Exception();
				}
				if (error) *error = ErrorSuccess;
			}
			virtual ~SystemSharedLock(void) override { if (_locked) flock(_file, LOCK_UN); close(_file); }
			virtual string GetLockName(void) override { return _name; }
			virtual bool TryWait(void) noexcept override { if (_locked) return false; if (flock(_file, LOCK_EX | LOCK_NB) < 0) return false; _locked = true; return true; }
			virtual void Open(void) noexcept override { if (_locked) { flock(_file, LOCK_UN); _locked = false; } }
		};

		IConnection * Connect(const string & listener_name, uint flags, uint * error) noexcept { try { if (error) *error = ErrorAllocation; return new SystemConnection(listener_name, flags, error); } catch (...) { return 0; } }
		IConnectionListener * CreateConnectionListener(const string & listener_name, uint flags, uint * error) noexcept { try { if (error) *error = ErrorAllocation; return new SystemConnectionListener(listener_name, flags, error); } catch (...) { return 0; } }
		ISharedMemory * CreateSharedMemory(const string & segment_name, uint length, uint flags, uint * error) noexcept { try { if (error) *error = ErrorAllocation; return new SystemSharedMemory(segment_name, length, flags, error); } catch (...) { return 0; } }
		ISharedLock * CreateSharedLock(const string & lock_name, uint * error) noexcept { try { if (error) *error = ErrorAllocation; return new SystemSharedLock(lock_name, error); } catch (...) { return 0; } }

		void DestroyConnectionListener(const string & name) noexcept { try { SafePointer< Array<char> > utf = _encode_utf8(L"/tmp/pipe_" + name); unlink(*utf); } catch (...) {} }
		void DestroySharedMemory(const string & name) noexcept { try { SafePointer< Array<char> > utf = _encode_utf8(name); shm_unlink(*utf); } catch (...) {} }
		void DestroySharedLock(const string & name) noexcept { try { SafePointer< Array<char> > utf = _encode_utf8(L"/tmp/lock_" + name); unlink(*utf); } catch (...) {} }
	}
}