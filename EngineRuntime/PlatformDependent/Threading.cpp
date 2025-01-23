#include "../Interfaces/Threading.h"

#include <Windows.h>

#undef CreateSemaphore

namespace Engine
{
	namespace WinapiThreading
	{
		class Thread : public Engine::Thread
		{
			HANDLE thread;
		public:
			Thread(void) : thread(INVALID_HANDLE_VALUE) {}
			virtual ~Thread(void) override { if (thread != INVALID_HANDLE_VALUE) CloseHandle(thread); }
			virtual bool Exited(void) noexcept override
			{
				DWORD code;
				auto result = GetExitCodeThread(thread, &code);
				return result && code != STILL_ACTIVE;
			}
			virtual int GetExitCode(void) noexcept override
			{
				DWORD code;
				auto result = GetExitCodeThread(thread, &code);
				return (result && code != STILL_ACTIVE) ? int(code) : -1;
			}
			virtual void Wait(void) noexcept override { WaitForSingleObject(thread, INFINITE); }
			void Init(HANDLE handle) noexcept { thread = handle; }
		};
		class Semaphore : public Engine::Semaphore
		{
			HANDLE semaphore;
		public:
			Semaphore(void) : semaphore(INVALID_HANDLE_VALUE) {}
			virtual ~Semaphore(void) override { if (semaphore != INVALID_HANDLE_VALUE) CloseHandle(semaphore); }
			virtual void Wait(void) noexcept override { WaitForSingleObject(semaphore, INFINITE); }
			virtual bool TryWait(void) noexcept override { return WaitForSingleObject(semaphore, 0) == WAIT_OBJECT_0; }
			virtual bool WaitFor(uint time) noexcept override { return WaitForSingleObject(semaphore, time) == WAIT_OBJECT_0; }
			virtual void Open(void) noexcept override { ReleaseSemaphore(semaphore, 1, 0); }
			void Init(HANDLE handle) noexcept { semaphore = handle; }
		};
		class Signal : public Engine::Signal
		{
			HANDLE event;
		public:
			Signal(void) : event(INVALID_HANDLE_VALUE) {}
			virtual ~Signal(void) override { if (event != INVALID_HANDLE_VALUE) CloseHandle(event); }
			virtual void Wait(void) noexcept override { WaitForSingleObject(event, INFINITE); }
			virtual bool TryWait(void) noexcept override { return WaitForSingleObject(event, 0) == WAIT_OBJECT_0; }
			virtual bool WaitFor(uint time) noexcept override { return WaitForSingleObject(event, time) == WAIT_OBJECT_0; }
			virtual void Raise(void) noexcept override { SetEvent(event); }
			virtual void Reset(void) noexcept override { ResetEvent(event); }
			void Init(HANDLE handle) noexcept { event = handle; }
		};
		struct NewThreadInfo
		{
			ThreadRoutine routine;
			void * argument;
		};
		DWORD WINAPI EngineThreadProc(LPVOID Argument) noexcept
		{
			auto info = reinterpret_cast<NewThreadInfo *>(Argument);
			CoInitializeEx(0, COINIT::COINIT_APARTMENTTHREADED);
			ThreadRoutine routine = info->routine;
			void * argument = info->argument;
			delete info;
			DWORD result;
			try { result = DWORD(routine(argument)); } catch (...) { abort(); }
			CoUninitialize();
			return result;
		}
	}
	Thread * CreateThread(ThreadRoutine routine, void * argument, uint32 stack_size) noexcept
	{
		if (!routine) return 0;
		SafePointer<WinapiThreading::Thread> result = new (std::nothrow) WinapiThreading::Thread;
		if (!result) return 0;
		auto arg = new (std::nothrow) WinapiThreading::NewThreadInfo;
		if (!arg) return 0;
		arg->argument = argument;
		arg->routine = routine;
		HANDLE handle = ::CreateThread(0, stack_size, WinapiThreading::EngineThreadProc, arg, 0, 0);
		if (!handle) { delete arg; return 0; }
		result->Init(handle);
		result->Retain();
		return result;
	}
	Semaphore * CreateSemaphore(uint32 initial) noexcept
	{
		SafePointer<WinapiThreading::Semaphore> result = new (std::nothrow) WinapiThreading::Semaphore;
		if (!result) return 0;
		HANDLE handle = CreateSemaphoreW(0, initial, 0x40000000, 0);
		if (!handle) return 0;
		result->Init(handle);
		result->Retain();
		return result;
	}
	Signal * CreateSignal(bool set) noexcept
	{
		SafePointer<WinapiThreading::Signal> result = new (std::nothrow) WinapiThreading::Signal;
		if (!result) return 0;
		HANDLE handle = CreateEventW(0, TRUE, set, 0);
		if (!handle) return 0;
		result->Init(handle);
		result->Retain();
		return result;
	}
	bool CreateThreadLocal(handle & index) noexcept
	{
		auto result = TlsAlloc();
		if (result != TLS_OUT_OF_INDEXES) {
			index = reinterpret_cast<handle>(uintptr(result));
			return true;
		} else return false;
	}
	void ReleaseThreadLocal(handle index) noexcept { TlsFree(DWORD(reinterpret_cast<uintptr>(index))); }
	void SetThreadLocal(handle index, handle value) noexcept { TlsSetValue(DWORD(reinterpret_cast<uintptr>(index)), value); }
	handle GetThreadLocal(handle index) noexcept { return TlsGetValue(DWORD(reinterpret_cast<uintptr>(index))); }
}