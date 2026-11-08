// Host test stub for the shadowhook wrapper. Nothing is hooked on the host, the
// hooks simply never install, which is exactly the path the hook code has to
// survive anyway (symbol missing on a different build).
#pragma once

#include <cstdint>

class CHook
{
public:
    template <typename Ret, typename Ptr, typename... Args>
    static Ret CallFunction(Ptr, Args...) { return Ret{}; }

    template <typename Ptr, typename Func, typename Orig>
    static void InlineHook(Ptr, Func, Orig) {}

    template <typename Ptr, typename Func, typename Orig>
    static void InlineHook(const char*, Func, Orig) {}

    template <typename Ptr, typename Data>
    static void Write(Ptr, Data) {}

    template <typename Ptr>
    static void WriteMemory(Ptr, uintptr_t, size_t) {}
};

static inline int shadowhook_hook_sym_name(const char*, const char*, void*, void**) { return 0; }
