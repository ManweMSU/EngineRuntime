#include "../Interfaces/Process.h"
#include "ProcessEx.h"
#include "CocoaInterop.h"

@import AppKit;

namespace Engine
{
	namespace Cocoa
	{
		bool ActivateProcess(pid_t pid) noexcept
		{
			@autoreleasepool {
				auto app = [NSRunningApplication runningApplicationWithProcessIdentifier: pid];
				return [app activateWithOptions: 0];
			}
		}
		bool ProcessHasLaunched(pid_t pid) noexcept
		{
			@autoreleasepool {
				auto app = [NSRunningApplication runningApplicationWithProcessIdentifier: pid];
				return [app isFinishedLaunching];
			}
		}
		class RunningProcess : public Engine::RunningProcess
		{
			NSRunningApplication * app;
		public:
			RunningProcess(NSRunningApplication * obj) : app(obj) { [app retain]; }
			virtual ~RunningProcess(void) override { [app release]; }
			virtual bool Exited(void) noexcept override { return [app isTerminated]; }
			virtual int GetPID(void) noexcept override { return [app processIdentifier]; }
			virtual bool IsGUI(void) noexcept override { return [app isFinishedLaunching]; }
			virtual bool Activate(void) noexcept override { return [app activateWithOptions: 0]; }
			virtual void Terminate(void) noexcept override { [app forceTerminate]; }
			virtual Time GetCreationTime(void) noexcept override
			{
				double interval;
				@autoreleasepool { interval = [[app launchDate] timeIntervalSince1970]; }
				if (!interval) return Time(0);
				return Time::FromUnixTime(uint64(interval));
			}
			virtual string GetExecutablePath(void) override { @autoreleasepool { return EngineString([[app executableURL] path]); } }
			virtual string GetBundlePath(void) override { @autoreleasepool { return EngineString([[app bundleURL] path]); } }
			virtual string GetBundleIdentifier(void) override { @autoreleasepool { return EngineString([app bundleIdentifier]); } }
		};
	}
	RunningProcess * OpenProcess(int pid) noexcept
	{
		@autoreleasepool {
			try {
				auto app = [NSRunningApplication runningApplicationWithProcessIdentifier: pid];
				if (!app) return 0;
				return new Cocoa::RunningProcess(app);
			} catch (...) { return 0; }
		}
	}
	int GetThisProcessPID(void) noexcept { return getpid(); }
	Array<int> * EnumerateProcesses(const string & bundle) noexcept
	{
		@autoreleasepool {
			try {
				auto bndl = Cocoa::CocoaString(bundle);
				auto apps = [NSRunningApplication runningApplicationsWithBundleIdentifier: bndl];
				[bndl release];
				SafePointer< Array<int> > result = new Array<int>(0x100);
				int count = [apps count];
				for (int i = 0; i < count; i++) {
					auto pid = [[apps objectAtIndex: i] processIdentifier];
					result->Append(pid);
				}
				result->Retain();
				return result;
			} catch (...) { return 0; }
		}
	}
}