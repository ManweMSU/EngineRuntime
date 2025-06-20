#include "../Interfaces/Process.h"

#include "../Interfaces/SystemIO.h"
#include "../Interfaces/Threading.h"
#include "../Miscellaneous/Volumes.h"
#include "ProcessEx.h"

#include <time.h>
#include <errno.h>
#include <crt_externs.h>
#include <atomic>
#include <sys/sysctl.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

extern char **environ;

namespace Engine
{
	class PosixProcess : public Process
	{
		typedef Volumes::ObjectDictionary<pid_t, PosixProcess> ProcessList;
		static SafePointer<ProcessList> _proc_list;
		static std::atomic_flag _global_lock;
	private:
		SafePointer<Semaphore> _local_lock, _wait_lock;
		pid_t _pid;
		int _exit_code;
		bool _exited;
	private:
		static int _process_waiter_thread(void * plist)
		{
			SafePointer<ProcessList> list = reinterpret_cast<ProcessList *>(plist);
			while (true) {
				int status;
				auto pid = wait(&status);
				if (pid < 0) {
					if (errno == EINTR) continue; else {
						while (_global_lock.test_and_set(std::memory_order_acquire));
						for (auto & proc : *list) {
							proc.value->_local_lock->Wait();
							proc.value->_exit_code = -1;
							proc.value->_exited = true;
							proc.value->_pid = -1;
							proc.value->_wait_lock->Open();
							proc.value->_local_lock->Open();
						}
						list->Clear();
						_global_lock.clear(std::memory_order_release);
					}
				} else {
					int set_exit_code = -1;
					if (WIFEXITED(status)) set_exit_code = WEXITSTATUS(status);
					while (_global_lock.test_and_set(std::memory_order_acquire));
					auto proc = (*list)[pid];
					if (proc) {
						proc->_local_lock->Wait();
						proc->_exit_code = set_exit_code;
						proc->_exited = true;
						proc->_pid = -1;
						proc->_wait_lock->Open();
						proc->_local_lock->Open();
						list->Remove(pid);
					}
					auto exit_thread = list->IsEmpty();
					_global_lock.clear(std::memory_order_release);
					if (exit_thread) return 0;
				}
			}
			return 0;
		}
		PosixProcess(pid_t pid) : _pid(pid), _exit_code(-1), _exited(false)
		{
			_local_lock = CreateSemaphore(1);
			_wait_lock = CreateSemaphore(0);
			if (!_local_lock || !_wait_lock) throw OutOfMemoryException();
		}
	public:
		virtual ~PosixProcess(void) override {}
		virtual bool Exited(void) noexcept override
		{
			_local_lock->Wait();
			auto result = _exited;
			_local_lock->Open();
			return result;
		}
		virtual int GetExitCode(void) noexcept override
		{
			_local_lock->Wait();
			auto result = _exit_code;
			_local_lock->Open();
			return result;
		}
		virtual int GetPID(void) noexcept override
		{
			_local_lock->Wait();
			auto result = _pid;
			_local_lock->Open();
			return result;
		}
		virtual bool IsGUI(void) noexcept override
		{
			_local_lock->Wait();
			auto result = _pid;
			_local_lock->Open();
			return result >= 0 ? Cocoa::ProcessHasLaunched(result) : false;
		}
		virtual bool Activate(void) noexcept override
		{
			_local_lock->Wait();
			auto result = _pid;
			_local_lock->Open();
			return result >= 0 ? Cocoa::ActivateProcess(result) : false;
		}
		virtual void Wait(void) noexcept override { _wait_lock->Wait(); _wait_lock->Open(); }
		virtual void Terminate(void) noexcept override
		{
			_local_lock->Wait();
			if (_pid >= 0) kill(_pid, SIGKILL);
			_local_lock->Open();
		}
		static PosixProcess * Create(const string & image, const Array<string> * command_line, bool use_path) noexcept
		{
			try {
				int argc = command_line ? command_line->Length() + 1 : 1;
				Array<char *> argv(1);
				ObjectArray<Object> retain(argc);
				Array<string> try_list(0x10);
				argv.SetLength(argc + 1);
				if (command_line) {
					for (int i = 0; i < command_line->Length(); i++) {
						SafePointer<DataBlock> data = command_line->ElementAt(i).EncodeSequence(Encoding::UTF8, true);
						retain.Append(data);
						argv[i + 1] = reinterpret_cast<char *>(data->GetBuffer());
					}
				}
				argv[argc] = 0;
				if (image[0] == L'/') use_path = false;
				if (use_path) {
					try_list << IO::GetCurrentDirectory() + L"/" + image;
					try_list << IO::Path::GetDirectory(IO::GetExecutablePath()) + L"/" + image;
					int envind = 0;
					while (environ[envind]) {
						if (environ[envind][0] == 'P' && environ[envind][1] == 'A' && environ[envind][2] == 'T' && environ[envind][3] == 'H' && environ[envind][4] == '=') {
							auto paths = string(environ[envind] + 5, -1, Encoding::UTF8).Split(L':');
							for (auto & p : paths) try_list << p + L"/" + image;
							break;
						}
						envind++;
					}
				} else try_list << image;
				int inter_io[2];
				if (pipe(inter_io) < 0) return 0;
				if (fcntl(inter_io[0], F_SETFD, FD_CLOEXEC) < 0) { close(inter_io[0]); close(inter_io[1]); return 0; }
				if (fcntl(inter_io[1], F_SETFD, FD_CLOEXEC) < 0) { close(inter_io[0]); close(inter_io[1]); return 0; }
				while (_global_lock.test_and_set(std::memory_order_acquire));
				auto pid = fork();
				if (pid < 0) { _global_lock.clear(std::memory_order_release); close(inter_io[0]); close(inter_io[1]); return 0; }
				if (pid > 0) {
					close(inter_io[1]);
					char exec_failed = 0;
					while (true) {
						int io = read(inter_io[0], &exec_failed, 1);
						if (io < 0) {
							if (errno == EINTR) continue;
							else { exec_failed = 1; break; }
						} else if (io == 0) { exec_failed = 0; break; }
						else break;
					}
					close(inter_io[0]);
					if (exec_failed) {
						while (true) {
							auto io = waitpid(pid, 0, 0);
							if (io >= 0 || errno != EINTR) break;
						}
						_global_lock.clear(std::memory_order_release);
						return 0;
					}
					SafePointer<PosixProcess> result;
					try {
						result = new PosixProcess(pid);
						if (!_proc_list) _proc_list = new ProcessList;
					} catch (...) { _global_lock.clear(std::memory_order_release); return 0; }
					bool was_empty = _proc_list->IsEmpty();
					try { _proc_list->Append(pid, result); } catch (...) { _global_lock.clear(std::memory_order_release); return 0; }
					if (was_empty) {
						_proc_list->Retain();
						SafePointer<Thread> waiter = CreateThread(_process_waiter_thread, _proc_list.Inner(), 0x10000);
						if (!waiter) {
							_proc_list->Remove(pid);
							_proc_list->Release();
							_global_lock.clear(std::memory_order_release);
							return 0;
						}
					}
					_global_lock.clear(std::memory_order_release);
					result->Retain();
					return result;
				} else {
					close(inter_io[0]);
					uint64 limit = 0;
					struct rlimit rl;
					if (getrlimit(RLIMIT_NOFILE, &rl) == 0) limit = min(rl.rlim_max, rl.rlim_cur);
					auto conf_limit = sysconf(_SC_OPEN_MAX);
					if (conf_limit >= 0 && uint64(conf_limit) < limit) limit = conf_limit;
					while (limit) {
						limit--;
						if (limit != inter_io[1] && limit != STDIN_FILENO && limit != STDOUT_FILENO && limit != STDERR_FILENO) close(limit);
					}
					try {
						for (auto & t : try_list) {
							SafePointer<DataBlock> url = t.EncodeSequence(Encoding::UTF8, true);
							argv[0] = reinterpret_cast<char *>(url->GetBuffer());
							execve(argv[0], argv, environ);
						}
					} catch (...) {}
					int failed = 1;
					write(inter_io[1], &failed, 1);
					_exit(0);
				}
			} catch (...) { return 0; }
		}
	};
	SafePointer<PosixProcess::ProcessList> PosixProcess::_proc_list;
	std::atomic_flag PosixProcess::_global_lock = ATOMIC_FLAG_INIT;

	Process * CreateProcess(const string & image, const Array<string> * command_line) noexcept { return PosixProcess::Create(image, command_line, false); }
	Process * CreateCommandProcess(const string & command_image, const Array<string> * command_line) noexcept { return PosixProcess::Create(command_image, command_line, true); }
	bool CreateProcessElevated(const string & image, const Array<string> * command_line) noexcept
	{
		int argc = 1 + (command_line ? command_line->Length() : 0);
		char ** argv = new (std::nothrow) char * [argc + 1];
		if (!argv) return false;
		ZeroMemory(argv, (argc + 1) * sizeof(char *));
		string image_full;
		try { image_full = IO::ExpandPath(image); } catch (...) { return false; }
		argv[0] = new (std::nothrow) char[image_full.GetEncodedLength(Encoding::UTF8) + 1];
		if (!argv[0]) { delete[] argv; return false; }
		image_full.Encode(argv[0], Encoding::UTF8, true);
		if (command_line) for (int i = 0; i < command_line->Length(); i++) {
			argv[i + 1] = new (std::nothrow) char[command_line->ElementAt(i).GetEncodedLength(Encoding::UTF8) + 1];
			if (!argv[i + 1]) {
				for (int j = 0; j < i; j++) delete[] argv[j];
				delete[] argv;
				return false;
			}
			command_line->ElementAt(i).Encode(argv[i + 1], Encoding::UTF8, true);
		}
		AuthorizationRef auth;
		if (AuthorizationCreate(0, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &auth) != errAuthorizationSuccess) {
			for (int j = 0; j < argc; j++) delete[] argv[j];
			delete[] argv;
			return false;
		}
		if (AuthorizationExecuteWithPrivileges(auth, argv[0], kAuthorizationFlagDefaults, argv + 1, 0) != errAuthorizationSuccess) {
			AuthorizationFree(auth, kAuthorizationFlagDefaults);
			for (int j = 0; j < argc; j++) delete[] argv[j];
			delete[] argv;
			return false;
		}
		AuthorizationFree(auth, kAuthorizationFlagDefaults);
		for (int j = 0; j < argc; j++) delete[] argv[j];
		delete[] argv;
		return true;
	}
	Array<string> * GetCommandLine(void)
	{
		int argc = *_NSGetArgc();
		char ** argv = *_NSGetArgv();
		SafePointer< Array<string> > result = new Array<string>(0x10);
		for (int i = 0; i < argc; i++) result->Append(string(argv[i], -1, Encoding::UTF8));
		result->Retain();
		return result;
	}
	void Sleep(uint32 time) noexcept
	{
		struct timespec req, elasped;
		req.tv_nsec = (time % 1000) * 1000000;
		req.tv_sec = time / 1000;
		do {
			int result = nanosleep(&req, &elasped);
			if (result == -1) {
				if (errno == EINTR) req = elasped;
				else return;
			} else return;
		} while (true);
	}
	void ExitProcess(int exit_code) noexcept { _exit(exit_code); }
	bool IsProcessElevated(void) noexcept { return geteuid() == 0; }
	Array<int> * EnumerateProcesses(void) noexcept
	{
		try {
			SafePointer< Array<int> > result = new Array<int>(0x100);
			kinfo_proc * procdata = 0;
			size_t length;
			try {
				while (true) {
					int name[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
					auto io = sysctl(name, 3, 0, &length, 0, 0);
					if (io < 0) throw Exception();
					free(procdata);
					procdata = reinterpret_cast<kinfo_proc *>(malloc(length));
					if (!procdata) throw OutOfMemoryException();
					io = sysctl(name, 3, procdata, &length, 0, 0);
					if (io < 0) {
						if (errno == ENOMEM) continue;
						else throw Exception();
					} else break;
				}
				int count = length / sizeof(kinfo_proc);
				for (int i = 0; i < count; i++) result->Append(procdata[i].kp_proc.p_pid);
				free(procdata);
				procdata = 0;
			} catch (...) { free(procdata); throw; }
			result->Retain();
			return result;
		} catch (...) { return 0; }
	}
}