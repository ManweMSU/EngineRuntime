#include "../Interfaces/Threading.h"

#include <pthread.h>
#include <new>

@import Foundation;

@class ERTThreadStartupInfo;
@interface ERTThreadStartupInfo : NSObject
{
@public
	Engine::ThreadRoutine routine;
	Engine::Semaphore * wait_sem;
	void * argument;
	int exit_code;
}
- (void) doJob: (id) arg;
- (void) dealloc;
@end
@implementation ERTThreadStartupInfo
- (void) doJob: (id) arg
{
	try { exit_code = routine(argument); } catch (...) { abort(); }
	wait_sem->Open();
}
- (void) dealloc
{
	if (wait_sem) wait_sem->Release();
	[super dealloc];
}
@end

namespace Engine
{
	namespace Cocoa
	{
		class Semaphore : public Engine::Semaphore
		{
			NSConditionLock * lock;
			uint32 semaphore;
		public:
			Semaphore(uint32 value) : semaphore(value)
			{
				lock = [[NSConditionLock alloc] initWithCondition: ((semaphore == 0) ? 0 : 1) ];
				if (!lock) throw OutOfMemoryException();
			}
			virtual ~Semaphore(void) override { [lock release]; }
			virtual void Wait(void) noexcept override
			{
				[lock lockWhenCondition: 1];
				if (semaphore == 0) abort();
				semaphore--;
				[lock unlockWithCondition: ((semaphore == 0) ? 0 : 1) ];
			}
			virtual bool TryWait(void) noexcept override
			{
				while (true) {
					if ([lock tryLockWhenCondition: 1]) {
						if (semaphore == 0) abort();
						semaphore--;
						[lock unlockWithCondition: ((semaphore == 0) ? 0 : 1) ];
						return true;
					}
					if ([lock tryLockWhenCondition: 0]) {
						[lock unlockWithCondition: 0];
						return false;
					}
				}
			}
			virtual bool WaitFor(uint time) noexcept override
			{
				@autoreleasepool {
					auto date = [NSDate dateWithTimeIntervalSinceNow: double(time) / 1000.0];
					if (!date) return false;
					auto result = [lock lockWhenCondition: 1 beforeDate: date];
					if (!result) return false;
				}
				if (semaphore == 0) abort();
				semaphore--;
				[lock unlockWithCondition: ((semaphore == 0) ? 0 : 1) ];
				return true;
			}
			virtual void Open(void) noexcept override
			{
				while (![lock tryLock]);
				semaphore++;
				[lock unlockWithCondition: ((semaphore == 0) ? 0 : 1) ];
			}
		};
		class Signal : public Engine::Signal
		{
			NSConditionLock * lock;
		public:
			Signal(bool set)
			{
				lock = [[NSConditionLock alloc] initWithCondition: set ? 1 : 0];
				if (!lock) throw OutOfMemoryException();
			}
			virtual ~Signal(void) override { [lock release]; }
			virtual void Wait(void) noexcept override { [lock lockWhenCondition: 1]; [lock unlockWithCondition: 1]; }
			virtual bool TryWait(void) noexcept override
			{
				while (true) {
					if ([lock tryLockWhenCondition: 1]) { [lock unlockWithCondition: 1]; return true; }
					if ([lock tryLockWhenCondition: 0]) { [lock unlockWithCondition: 0]; return false; }
				}
			}
			virtual bool WaitFor(uint time) noexcept override
			{
				@autoreleasepool {
					auto date = [NSDate dateWithTimeIntervalSinceNow: double(time) / 1000.0];
					if (!date) return false;
					if ([lock lockWhenCondition: 1 beforeDate: date]) {
						[lock unlockWithCondition: 1];
						return true;
					} else return false;
				}
			}
			virtual void Raise(void) noexcept override { [lock lock]; [lock unlockWithCondition: 1]; }
			virtual void Reset(void) noexcept override { [lock lock]; [lock unlockWithCondition: 0]; }
		};
		class Thread : public Engine::Thread
		{
			NSThread * thread;
			ERTThreadStartupInfo * thread_info;
		public:
			Thread(void) : thread(0), thread_info(0) {}
			virtual ~Thread(void) override { [thread release]; [thread_info release]; }
			virtual bool Exited(void) noexcept override { return [thread isFinished]; }
			virtual int GetExitCode(void) noexcept override { return thread_info ? thread_info->exit_code : -1; }
			virtual void Wait(void) noexcept override
			{
				if (!thread_info) abort();
				thread_info->wait_sem->Wait();
				thread_info->wait_sem->Open();
			}
			void Init(NSThread * handle, ERTThreadStartupInfo * info) noexcept { thread = handle; thread_info = info; }
		};
	}
	Thread * CreateThread(ThreadRoutine routine, void * argument, uint32 stack_size) noexcept
	{
		if (!routine) return 0;
		SafePointer<Cocoa::Thread> result = new (std::nothrow) Cocoa::Thread;
		if (!result) return 0;
		ERTThreadStartupInfo * info = [[ERTThreadStartupInfo alloc] init];
		if (!info) return 0;
		info->routine = routine;
		info->argument = argument;
		info->wait_sem = CreateSemaphore(0);
		info->exit_code = -1;
		if (!info->wait_sem) { [info release]; return 0; }
		NSThread * thread = [[NSThread alloc] initWithTarget: info selector: @selector(doJob:) object: NULL];
		if (!thread) { [info release]; return 0; }
		[thread setStackSize: uint32((uint64(stack_size) * 0x1000 + 0x0FFF) / 0x1000)];
		[thread start];
		result->Init(thread, info);
		result->Retain();
		return result;
	}
	Semaphore * CreateSemaphore(uint32 initial) noexcept { try { return new Cocoa::Semaphore(initial); } catch (...) { return 0; } }
	Signal * CreateSignal(bool set) noexcept { try { return new Cocoa::Signal(set); } catch (...) { return 0; } }
	bool CreateThreadLocal(handle & index) noexcept
	{
		pthread_key_t result;
		if (pthread_key_create(&result, 0) == 0) {
			index = reinterpret_cast<handle>(result);
			return true;
		} else return false;
	}
	void ReleaseThreadLocal(handle index) noexcept { pthread_key_delete(reinterpret_cast<pthread_key_t>(index)); }
	void SetThreadLocal(handle index, handle value) noexcept { pthread_setspecific(reinterpret_cast<pthread_key_t>(index), value); }
	handle GetThreadLocal(handle index) noexcept { return pthread_getspecific(reinterpret_cast<pthread_key_t>(index)); }
}