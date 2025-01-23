#pragma once

#include "../EngineBase.h"

namespace Engine
{
	typedef int (* ThreadRoutine) (void * argument);

	class Thread : public Object
	{
	public:
		virtual bool Exited(void) noexcept = 0;
		virtual int GetExitCode(void) noexcept = 0;
		virtual void Wait(void) noexcept = 0;
	};
	class Semaphore : public Object
	{
	public:
		virtual void Wait(void) noexcept = 0;
		virtual bool TryWait(void) noexcept = 0;
		virtual bool WaitFor(uint time) noexcept = 0;
		virtual void Open(void) noexcept = 0;
	};
	class Signal : public Object
	{
	public:
		virtual void Wait(void) noexcept = 0;
		virtual bool TryWait(void) noexcept = 0;
		virtual bool WaitFor(uint time) noexcept = 0;
		virtual void Raise(void) noexcept = 0;
		virtual void Reset(void) noexcept = 0;
	};

	Thread * CreateThread(ThreadRoutine routine, void * argument = 0, uint32 stack_size = 0x200000) noexcept;
	Semaphore * CreateSemaphore(uint32 initial) noexcept;
	Signal * CreateSignal(bool set) noexcept;

	bool CreateThreadLocal(handle & index) noexcept;
	void ReleaseThreadLocal(handle index) noexcept;
	void SetThreadLocal(handle index, handle value) noexcept;
	handle GetThreadLocal(handle index) noexcept;
}