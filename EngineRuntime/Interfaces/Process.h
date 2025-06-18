#pragma once

#include "../EngineBase.h"
#include "../Miscellaneous/Time.h"

namespace Engine
{
	class Process : public Object
	{
	public:
		virtual bool Exited(void) noexcept = 0;
		virtual int GetExitCode(void) noexcept = 0;
		virtual int GetPID(void) noexcept = 0;
		virtual bool IsGUI(void) noexcept = 0;
		virtual bool Activate(void) noexcept = 0;
		virtual void Wait(void) noexcept = 0;
		virtual void Terminate(void) noexcept = 0;
	};
	class RunningProcess : public Object
	{
	public:
		virtual bool Exited(void) noexcept = 0;
		virtual int GetPID(void) noexcept = 0;
		virtual bool IsGUI(void) noexcept = 0;
		virtual bool Activate(void) noexcept = 0;
		virtual void Terminate(void) noexcept = 0;
		virtual Time GetCreationTime(void) noexcept = 0;
		virtual string GetExecutablePath(void) = 0;
		virtual string GetBundlePath(void) = 0;
		virtual string GetBundleIdentifier(void) = 0;
	};

	Process * CreateProcess(const string & image, const Array<string> * command_line = 0) noexcept;
	Process * CreateCommandProcess(const string & command_image, const Array<string> * command_line = 0) noexcept;
	bool CreateProcessElevated(const string & image, const Array<string> * command_line = 0) noexcept;
	Array<string> * GetCommandLine(void);
	void Sleep(uint32 time) noexcept;
	void ExitProcess(int exit_code) noexcept;
	bool IsProcessElevated(void) noexcept;

	RunningProcess * OpenProcess(int pid) noexcept;
	int GetThisProcessPID(void) noexcept;
	Array<int> * EnumerateProcesses(void) noexcept;
	Array<int> * EnumerateProcesses(const string & bundle) noexcept;
}