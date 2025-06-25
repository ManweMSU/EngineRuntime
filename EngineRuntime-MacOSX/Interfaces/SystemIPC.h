#pragma once

#include "../Streaming.h"

namespace Engine
{
	namespace IPC
	{
		enum CreateConnectionFlags {
			CreateConnectionSingleThreaded	= 0x00,
			CreateConnectionMultiThreaded	= 0x01,
		};
		enum CreateSharedMemoryFlags {
			SharedMemoryOpenExisting	= 0x00,
			SharedMemoryCreateNew		= 0x80,
		};
		enum SharedMemoryMapFlags {
			SharedMemoryMapNone			= 0x00,
			SharedMemoryMapRead			= 0x01,
			SharedMemoryMapWrite		= 0x02,
			SharedMemoryMapReadWrite	= SharedMemoryMapRead | SharedMemoryMapWrite
		};
		enum IPCErrors {
			ErrorSuccess			= 0x00,
			ErrorAlreadyExists		= 0x01,
			ErrorDoesNotExist		= 0x02,
			ErrorBadFileName		= 0x03,
			ErrorAllocation			= 0x04,
			ErrorInvalidArgument	= 0x05,
		};

		class IConnection : public Streaming::Stream
		{
		public:
			virtual handle GetIOHandle(void) noexcept = 0;
		};
		class IConnectionListener : public Object
		{
		public:
			virtual IConnection * Accept(void) noexcept = 0;
			virtual string GetOSPath(void) = 0;
			virtual string GetListenerName(void) = 0;
		};
		class ISharedMemory : public Object
		{
		public:
			virtual string GetSegmentName(void) = 0;
			virtual uint GetLength(void) noexcept = 0;
			virtual bool Map(void ** pdata, uint map_flags) noexcept = 0;
			virtual void Unmap(void) noexcept = 0;
		};
		class ISharedLock : public Object
		{
		public:
			virtual string GetLockName(void) = 0;
			virtual bool TryWait(void) noexcept = 0;
			virtual void Open(void) noexcept = 0;
		};

		IConnection * Connect(const string & listener_name, uint flags, uint * error = 0) noexcept;
		IConnectionListener * CreateConnectionListener(const string & listener_name, uint flags, uint * error = 0) noexcept;
		ISharedMemory * CreateSharedMemory(const string & segment_name, uint length, uint flags, uint * error = 0) noexcept;
		ISharedLock * CreateSharedLock(const string & lock_name, uint * error = 0) noexcept;

		void DestroyConnectionListener(const string & name) noexcept;
		void DestroySharedMemory(const string & name) noexcept;
		void DestroySharedLock(const string & name) noexcept;
	}
}