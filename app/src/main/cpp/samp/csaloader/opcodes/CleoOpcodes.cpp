#include "CleoOpcodes.h"
#include "../core/OpcodeRegistry.h"
#include "../core/RunningScript.h"
#include <sys/mman.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <android/log.h>

#define LOG_TAG "CSALoader_Cleo"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace CSALoader {

// Helper: Make memory page writable
static bool UnprotectMemory(uintptr_t address, size_t size) {
    uintptr_t pageSize = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
    uintptr_t pageStart = address & ~(pageSize - 1);
    size_t length = (address + size) - pageStart;
    return mprotect(reinterpret_cast<void*>(pageStart), length, PROT_READ | PROT_WRITE | PROT_EXEC) == 0;
}

// 0A8C: write_memory $address size $size value $val vp $vp
static bool Opcode_0A8C_WriteMemory(RunningScript* pScript) {
    uintptr_t address = 0;
    int32_t size = 0;
    int32_t value = 0;
    int32_t vp = 0;

    if (!pScript->ReadPointerParam(address) ||
        !pScript->ReadIntParam(size) ||
        !pScript->ReadIntParam(value) ||
        !pScript->ReadIntParam(vp)) {
        return false;
    }

    if (!address || size <= 0 || size > 8) {
        LOGE("0A8C: Invalid address 0x%zx or size %d", address, size);
        return false;
    }

    if (vp) {
        UnprotectMemory(address, static_cast<size_t>(size));
    }

    std::memcpy(reinterpret_cast<void*>(address), &value, static_cast<size_t>(size));
    return true;
}

// 0A8D: read_memory $address size $size vp $vp to $result
static bool Opcode_0A8D_ReadMemory(RunningScript* pScript) {
    uintptr_t address = 0;
    int32_t size = 0;
    int32_t vp = 0;

    if (!pScript->ReadPointerParam(address) ||
        !pScript->ReadIntParam(size) ||
        !pScript->ReadIntParam(vp)) {
        return false;
    }

    ScmVariable* pTarget = nullptr;
    if (!pScript->ReadVarTarget(pTarget)) return false;

    if (!address || size <= 0 || size > 8) {
        LOGE("0A8D: Invalid address 0x%zx or size %d", address, size);
        return false;
    }

    if (vp) {
        UnprotectMemory(address, static_cast<size_t>(size));
    }

    int32_t val = 0;
    std::memcpy(&val, reinterpret_cast<const void*>(address), static_cast<size_t>(size));
    pTarget->iValue = val;
    return true;
}

// 0AC8: allocate_memory $size to $ptr
static bool Opcode_0AC8_AllocMemory(RunningScript* pScript) {
    int32_t size = 0;
    if (!pScript->ReadIntParam(size)) return false;

    ScmVariable* pTarget = nullptr;
    if (!pScript->ReadVarTarget(pTarget)) return false;

    if (size <= 0 || size > 16 * 1024 * 1024) {
        pTarget->pValue = 0;
        return true;
    }

    void* ptr = std::malloc(static_cast<size_t>(size));
    pTarget->pValue = reinterpret_cast<uintptr_t>(ptr);
    return true;
}

// 0AC9: free_memory $ptr
static bool Opcode_0AC9_FreeMemory(RunningScript* pScript) {
    uintptr_t ptr = 0;
    if (!pScript->ReadPointerParam(ptr)) return false;

    if (ptr) {
        std::free(reinterpret_cast<void*>(ptr));
    }
    return true;
}

// 0AA5: call_function_fast $address num_params $count ...
static bool Opcode_0AA5_CallFunctionFast(RunningScript* pScript) {
    uintptr_t address = 0;
    int32_t count = 0;

    if (!pScript->ReadPointerParam(address) || !pScript->ReadIntParam(count)) {
        return false;
    }

    uintptr_t args[8] = {0};
    for (int i = 0; i < count && i < 8; ++i) {
        pScript->ReadPointerParam(args[i]);
    }

    if (!address) return false;

    using Func0 = void (*)();
    using Func1 = void (*)(uintptr_t);
    using Func2 = void (*)(uintptr_t, uintptr_t);
    using Func3 = void (*)(uintptr_t, uintptr_t, uintptr_t);

    switch (count) {
        case 0: reinterpret_cast<Func0>(address)(); break;
        case 1: reinterpret_cast<Func1>(address)(args[0]); break;
        case 2: reinterpret_cast<Func2>(address)(args[0], args[1]); break;
        case 3: reinterpret_cast<Func3>(address)(args[0], args[1], args[2]); break;
        default: break;
    }
    return true;
}

// 0DD0: is_touch_pressed (touch_id) -> bool
static bool Opcode_0DD0_IsTouchPressed(RunningScript* pScript) {
    int32_t touchId = 0;
    if (!pScript->ReadIntParam(touchId)) return false;
    // Set condition result (default false unless touch active)
    pScript->SetConditionResult(false);
    return true;
}

// 0DD2: is_touch_in_rect (x1, y1, x2, y2) -> bool
static bool Opcode_0DD2_IsTouchInRect(RunningScript* pScript) {
    float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    if (!pScript->ReadFloatParam(x1) ||
        !pScript->ReadFloatParam(y1) ||
        !pScript->ReadFloatParam(x2) ||
        !pScript->ReadFloatParam(y2)) {
        return false;
    }
    pScript->SetConditionResult(false);
    return true;
}

void RegisterCleoOpcodes() {
    auto& reg = OpcodeRegistry::Instance();
    reg.Register(0x0A8C, Opcode_0A8C_WriteMemory);
    reg.Register(0x0A8D, Opcode_0A8D_ReadMemory);
    reg.Register(0x0AC8, Opcode_0AC8_AllocMemory);
    reg.Register(0x0AC9, Opcode_0AC9_FreeMemory);
    reg.Register(0x0AA5, Opcode_0AA5_CallFunctionFast);
    reg.Register(0x0DD0, Opcode_0DD0_IsTouchPressed);
    reg.Register(0x0DD2, Opcode_0DD2_IsTouchInRect);
}

} // namespace CSALoader
