#include "../Interfaces/Process.h"

#include "../Miscellaneous/DynamicString.h"
#include "../Interfaces/SystemIO.h"

#include <Windows.h>
#include <Psapi.h>

#undef CreateProcess
#undef GetCommandLine

namespace Engine
{
	namespace WinapiProcess
	{
		class Process : public Engine::Process
		{
			HANDLE process;
		private:
			struct _is_gui_data {
				DWORD pid;
				bool result;
			};
		private:
			static BOOL WINAPI _is_gui_callback(HWND wnd, LPARAM lparam) noexcept
			{
				DWORD wnd_pid;
				if (GetWindowThreadProcessId(wnd, &wnd_pid) && wnd_pid == reinterpret_cast<_is_gui_data *>(lparam)->pid) { reinterpret_cast<_is_gui_data *>(lparam)->result = true; return FALSE; }
				return TRUE;
			}
			static BOOL WINAPI _activate_gui_callback(HWND wnd, LPARAM lparam) noexcept
			{
				DWORD wnd_pid;
				if (GetWindowThreadProcessId(wnd, &wnd_pid) && wnd_pid == reinterpret_cast<_is_gui_data *>(lparam)->pid) {
					if (IsWindowVisible(wnd) && IsWindowEnabled(wnd) && SetForegroundWindow(wnd)) { reinterpret_cast<_is_gui_data *>(lparam)->result = true; return FALSE; }
				}
				return TRUE;
			}
		public:
			static bool IsGUI(DWORD pid) noexcept
			{
				_is_gui_data data;
				data.result = false;
				data.pid = pid;
				if (data.pid) EnumWindows(_is_gui_callback, reinterpret_cast<LPARAM>(&data));
				return data.result;
			}
			static bool Activate(DWORD pid) noexcept
			{
				_is_gui_data data;
				data.result = false;
				data.pid = pid;
				if (data.pid) EnumWindows(_activate_gui_callback, reinterpret_cast<LPARAM>(&data));
				return data.result;
			}
			Process(void) : process(INVALID_HANDLE_VALUE) {}
			virtual ~Process(void) override { if (process != INVALID_HANDLE_VALUE) CloseHandle(process); }
			virtual bool Exited(void) noexcept override
			{
				DWORD code;
				auto result = GetExitCodeProcess(process, &code);
				return result && code != STILL_ACTIVE;
			}
			virtual int GetExitCode(void) noexcept override
			{
				DWORD code;
				auto result = GetExitCodeProcess(process, &code);
				return (result && code != STILL_ACTIVE) ? int(code) : -1;
			}
			virtual int GetPID(void) noexcept override { return int(GetProcessId(process)); }
			virtual bool IsGUI(void) noexcept override { return IsGUI(GetProcessId(process)); }
			virtual bool Activate(void) noexcept override { return Activate(GetProcessId(process)); }
			virtual void Wait(void) noexcept override { WaitForSingleObject(process, INFINITE); }
			virtual void Terminate(void) noexcept override { TerminateProcess(process, -1); }
			void Init(HANDLE handle) noexcept { process = handle; }
		};
		class RunningProcess : public Engine::RunningProcess
		{
			HANDLE process;
		public:
			RunningProcess(void) : process(INVALID_HANDLE_VALUE) {}
			virtual ~RunningProcess(void) override { if (process != INVALID_HANDLE_VALUE) CloseHandle(process); }
			virtual bool Exited(void) noexcept override
			{
				DWORD code;
				auto result = GetExitCodeProcess(process, &code);
				return result && code != STILL_ACTIVE;
			}
			virtual int GetPID(void) noexcept override { return int(GetProcessId(process)); }
			virtual bool IsGUI(void) noexcept override { return Process::IsGUI(GetProcessId(process)); }
			virtual bool Activate(void) noexcept override { return Process::Activate(GetProcessId(process)); }
			virtual void Terminate(void) noexcept override { TerminateProcess(process, -1); }
			virtual Time GetCreationTime(void) noexcept override
			{
				uint64 tcr, tex, ckr, cus;
				if (GetProcessTimes(process, reinterpret_cast<LPFILETIME>(&tcr), reinterpret_cast<LPFILETIME>(&tex), reinterpret_cast<LPFILETIME>(&ckr), reinterpret_cast<LPFILETIME>(&cus))) return Time::FromWindowsTime(tcr);
				else return 0;
			}
			virtual string GetExecutablePath(void) override
			{
				DynamicString result;
				result.ReserveLength(0x100);
				while (true) {
					auto status = GetProcessImageFileNameW(process, result, result.ReservedLength());
					if (status) break;
					if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) { result.ReserveLength(result.ReservedLength() * 2); continue; }
					return L"";
				}
				return result.ToString();
			}
			virtual string GetBundlePath(void) override { return GetExecutablePath(); }
			virtual string GetBundleIdentifier(void) override { return L""; }
			void Init(HANDLE handle) noexcept { process = handle; }
		};
		void AppendCommandLine(DynamicString & cmd, const string & arg)
		{
			if (cmd.Length() != 0) cmd += L' ';
			cmd += L'\"';
			for (int i = 0; i < arg.Length(); i++) {
				if (arg[i] == L'\"') {
					int req = 0;
					for (int j = cmd.Length() - 1; j >= 0; j--) if (cmd[j] == L'\\') req++; else break;
					for (int j = 0; j < req; j++) cmd += L'\\';
					cmd += L'\\';
					cmd += arg[i];
				} else cmd += arg[i];
			}
			cmd += L'\"';
		}
	}
	Process * CreateProcess(const string & image, const Array<string> * command_line) noexcept
	{
		try {
			DynamicString cmd;
			WinapiProcess::AppendCommandLine(cmd, IO::ExpandPath(image));
			if (command_line) for (int i = 0; i < command_line->Length(); i++) WinapiProcess::AppendCommandLine(cmd, command_line->ElementAt(i));
			PROCESS_INFORMATION pi;
			STARTUPINFOW si;
			ZeroMemory(&si, sizeof(si));
			si.cb = sizeof(si);
			if (GetStdHandle(STD_ERROR_HANDLE) || GetStdHandle(STD_INPUT_HANDLE) || GetStdHandle(STD_OUTPUT_HANDLE)) {
				si.dwFlags = STARTF_USESTDHANDLES;
				si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
				si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
				si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
			}
			SafePointer<WinapiProcess::Process> result = new WinapiProcess::Process;
			DWORD Flags = 0;
			if (!CreateProcessW(0, cmd, 0, 0, TRUE, Flags, 0, 0, &si, &pi)) return 0;
			CloseHandle(pi.hThread);
			result->Init(pi.hProcess);
			result->Retain();
			return result;
		} catch (...) { return 0; }
	}
	Process * CreateCommandProcess(const string & command_image, const Array<string> * command_line) noexcept
	{
		try {
			DynamicString cmd;
			WinapiProcess::AppendCommandLine(cmd, command_image);
			if (command_line) for (int i = 0; i < command_line->Length(); i++) WinapiProcess::AppendCommandLine(cmd, command_line->ElementAt(i));
			PROCESS_INFORMATION pi;
			STARTUPINFOW si;
			ZeroMemory(&si, sizeof(si));
			si.cb = sizeof(si);
			if (GetStdHandle(STD_ERROR_HANDLE) || GetStdHandle(STD_INPUT_HANDLE) || GetStdHandle(STD_OUTPUT_HANDLE)) {
				si.dwFlags = STARTF_USESTDHANDLES;
				si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
				si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
				si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
			}
			SafePointer<WinapiProcess::Process> result = new WinapiProcess::Process;
			DWORD Flags = 0;	
			if (!CreateProcessW(0, cmd, 0, 0, TRUE, Flags, 0, 0, &si, &pi)) return 0;
			CloseHandle(pi.hThread);
			result->Init(pi.hProcess);
			result->Retain();
			return result;
		} catch (...) { return 0; }
	}
	bool CreateProcessElevated(const string & image, const Array<string>* command_line) noexcept
	{
		try {
			auto image_full = IO::ExpandPath(image);
			DynamicString cmd;
			if (command_line) for (int i = 0; i < command_line->Length(); i++) WinapiProcess::AppendCommandLine(cmd, command_line->ElementAt(i));
			SHELLEXECUTEINFOW info;
			ZeroMemory(&info, sizeof(info));
			info.cbSize = sizeof(info);
			info.fMask = SEE_MASK_UNICODE | SEE_MASK_NO_CONSOLE | SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
			info.lpVerb = L"runas";
			info.lpFile = image_full;
			info.lpParameters = cmd;
			info.nShow = SW_SHOW;
			if (!ShellExecuteExW(&info)) return false;
			return true;
		} catch (...) { return false; }
	}
	Array<string> * GetCommandLine(void)
	{
		int count;
		LPWSTR * cmd = CommandLineToArgvW(GetCommandLineW(), &count);
		if (!cmd) throw OutOfMemoryException();
		SafePointer< Array<string> > result;
		try {
			result = new (std::nothrow) Array<string>(0x10);
			if (!result) throw OutOfMemoryException();
			for (int i = 0; i < count; i++) result->Append(cmd[i]);
		} catch (...) { LocalFree(reinterpret_cast<HLOCAL>(cmd)); throw; }
		LocalFree(reinterpret_cast<HLOCAL>(cmd));
		result->Retain();
		return result;
	}
	void Sleep(uint32 time) noexcept { ::Sleep(time); }
	void ExitProcess(int exit_code) noexcept { ::ExitProcess(exit_code); }
	RunningProcess * OpenProcess(int pid) noexcept
	{
		try {
			SafePointer<WinapiProcess::RunningProcess> result = new WinapiProcess::RunningProcess;
			HANDLE process = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, pid);
			if (!process) process = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
			if (!process) process = ::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
			if (!process) process = ::OpenProcess(PROCESS_TERMINATE, FALSE, pid);
			if (!process) return 0;
			result->Init(process);
			result->Retain();
			return result;
		} catch (...) { return 0; }
	}
	int GetThisProcessPID(void) noexcept { return int(GetCurrentProcessId()); }
	Array<int> * EnumerateProcesses(void) noexcept
	{
		try {
			SafePointer< Array<int> > result = new Array<int>(0x100);
			result->SetLength(0x100);
			while (true) {
				DWORD length = sizeof(DWORD) * result->Length(), needed;
				if (!EnumProcesses(reinterpret_cast<LPDWORD>(result->GetBuffer()), length, &needed)) return 0;
				if (length == needed) {
					result->SetLength(result->Length() * 2);
					continue;
				} else {
					int count = needed / sizeof(DWORD);
					result->SetLength(count);
					break;
				}
			}
			result->Retain();
			return result;
		} catch (...) { return 0; }
	}
	Array<int> * EnumerateProcesses(const string & bundle) noexcept { return 0; }
}