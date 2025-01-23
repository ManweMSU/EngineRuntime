#pragma once

#include <unistd.h>

namespace Engine
{
	namespace Cocoa
	{
		bool ActivateProcess(pid_t pid) noexcept;
		bool ProcessHasLaunched(pid_t pid) noexcept;
	}
}