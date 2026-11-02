#pragma once
#include "GakuPlugin.h"
#include "hooks/HookManager.hpp"

namespace GakumasVR::Hooks {
// Bind once during native activation. Tables and trampolines remain resident.
bool BindHost(const GakuHookService& hooks, const GakuLogService& log);
GakuStatus LastHostStatus() noexcept;
GakuStatus FirstHostFailure() noexcept;
}
