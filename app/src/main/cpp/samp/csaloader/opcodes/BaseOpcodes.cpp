#include "BaseOpcodes.h"
#include "../core/OpcodeRegistry.h"
#include "../core/RunningScript.h"
#include <chrono>

namespace CSALoader {

static uint32_t GetCurrentTickMs() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count()
    );
}

// 0001: wait $val ms
static bool Opcode_0001_Wait(RunningScript* pScript) {
    int32_t duration = 0;
    if (!pScript->ReadIntParam(duration)) return false;
    pScript->SetSleep(duration >= 0 ? static_cast<uint32_t>(duration) : 0, GetCurrentTickMs());
    return true;
}

// 0002: jump @label
static bool Opcode_0002_Goto(RunningScript* pScript) {
    int32_t offset = 0;
    if (!pScript->ReadIntParam(offset)) return false;
    // In SCM bytecode, offsets in 0002 are either negative relative or absolute
    if (offset < 0) {
        pScript->JumpAbsolute(static_cast<size_t>(-offset));
    } else {
        pScript->JumpAbsolute(static_cast<size_t>(offset));
    }
    return true;
}

// 0004: $var = $val (int)
static bool Opcode_0004_SetVarInt(RunningScript* pScript) {
    ScmVariable* pTarget = nullptr;
    if (!pScript->ReadVarTarget(pTarget)) return false;
    int32_t val = 0;
    if (!pScript->ReadIntParam(val)) return false;
    pTarget->iValue = val;
    return true;
}

// 0005: $var = $val (float)
static bool Opcode_0005_SetVarFloat(RunningScript* pScript) {
    ScmVariable* pTarget = nullptr;
    if (!pScript->ReadVarTarget(pTarget)) return false;
    float val = 0.0f;
    if (!pScript->ReadFloatParam(val)) return false;
    pTarget->fValue = val;
    return true;
}

// 0006: $var += $val (int)
static bool Opcode_0006_AddVarInt(RunningScript* pScript) {
    ScmVariable* pTarget = nullptr;
    if (!pScript->ReadVarTarget(pTarget)) return false;
    int32_t val = 0;
    if (!pScript->ReadIntParam(val)) return false;
    pTarget->iValue += val;
    return true;
}

// 0007: $var += $val (float)
static bool Opcode_0007_AddVarFloat(RunningScript* pScript) {
    ScmVariable* pTarget = nullptr;
    if (!pScript->ReadVarTarget(pTarget)) return false;
    float val = 0.0f;
    if (!pScript->ReadFloatParam(val)) return false;
    pTarget->fValue += val;
    return true;
}

// 0008: $var -= $val (int)
static bool Opcode_0008_SubVarInt(RunningScript* pScript) {
    ScmVariable* pTarget = nullptr;
    if (!pScript->ReadVarTarget(pTarget)) return false;
    int32_t val = 0;
    if (!pScript->ReadIntParam(val)) return false;
    pTarget->iValue -= val;
    return true;
}

// 0009: $var -= $val (float)
static bool Opcode_0009_SubVarFloat(RunningScript* pScript) {
    ScmVariable* pTarget = nullptr;
    if (!pScript->ReadVarTarget(pTarget)) return false;
    float val = 0.0f;
    if (!pScript->ReadFloatParam(val)) return false;
    pTarget->fValue -= val;
    return true;
}

// 000A: $var *= $val (int)
static bool Opcode_000A_MultVarInt(RunningScript* pScript) {
    ScmVariable* pTarget = nullptr;
    if (!pScript->ReadVarTarget(pTarget)) return false;
    int32_t val = 0;
    if (!pScript->ReadIntParam(val)) return false;
    pTarget->iValue *= val;
    return true;
}

// 000B: $var *= $val (float)
static bool Opcode_000B_MultVarFloat(RunningScript* pScript) {
    ScmVariable* pTarget = nullptr;
    if (!pScript->ReadVarTarget(pTarget)) return false;
    float val = 0.0f;
    if (!pScript->ReadFloatParam(val)) return false;
    pTarget->fValue *= val;
    return true;
}

// 000C: $var /= $val (int)
static bool Opcode_000C_DivVarInt(RunningScript* pScript) {
    ScmVariable* pTarget = nullptr;
    if (!pScript->ReadVarTarget(pTarget)) return false;
    int32_t val = 0;
    if (!pScript->ReadIntParam(val)) return false;
    if (val != 0) pTarget->iValue /= val;
    return true;
}

// 000D: $var /= $val (float)
static bool Opcode_000D_DivVarFloat(RunningScript* pScript) {
    ScmVariable* pTarget = nullptr;
    if (!pScript->ReadVarTarget(pTarget)) return false;
    float val = 0.0f;
    if (!pScript->ReadFloatParam(val)) return false;
    if (val != 0.0f) pTarget->fValue /= val;
    return true;
}

// 0038: $val1 > $val2 (int)
static bool Opcode_0038_IsIntGreater(RunningScript* pScript) {
    int32_t v1 = 0, v2 = 0;
    if (!pScript->ReadIntParam(v1) || !pScript->ReadIntParam(v2)) return false;
    pScript->SetConditionResult(v1 > v2);
    return true;
}

// 0039: $val1 >= $val2 (int)
static bool Opcode_0039_IsIntGreaterOrEqual(RunningScript* pScript) {
    int32_t v1 = 0, v2 = 0;
    if (!pScript->ReadIntParam(v1) || !pScript->ReadIntParam(v2)) return false;
    pScript->SetConditionResult(v1 >= v2);
    return true;
}

// 004D: jf @label (jump if condition is false)
static bool Opcode_004D_JumpIfFalse(RunningScript* pScript) {
    int32_t offset = 0;
    if (!pScript->ReadIntParam(offset)) return false;
    if (!pScript->GetConditionResult()) {
        if (offset < 0) {
            pScript->JumpAbsolute(static_cast<size_t>(-offset));
        } else {
            pScript->JumpAbsolute(static_cast<size_t>(offset));
        }
    }
    return true;
}

// 004E: terminate_this_custom_script
static bool Opcode_004E_EndThread(RunningScript* pScript) {
    pScript->Terminate();
    return true;
}

// 0050: return
static bool Opcode_0050_Return(RunningScript* pScript) {
    size_t returnIp = 0;
    if (!pScript->PopStack(returnIp)) {
        pScript->Terminate();
        return false;
    }
    pScript->JumpAbsolute(returnIp);
    return true;
}

// 0051: call @label
static bool Opcode_0051_Call(RunningScript* pScript) {
    int32_t offset = 0;
    if (!pScript->ReadIntParam(offset)) return false;
    size_t returnIp = pScript->GetIP();
    if (!pScript->PushStack(returnIp)) return false;
    if (offset < 0) {
        pScript->JumpAbsolute(static_cast<size_t>(-offset));
    } else {
        pScript->JumpAbsolute(static_cast<size_t>(offset));
    }
    return true;
}

// 00D6: if count
static bool Opcode_00D6_If(RunningScript* pScript) {
    int32_t count = 0;
    if (!pScript->ReadIntParam(count)) return false;
    // Standard SCM if evaluation reset
    pScript->SetConditionResult(true);
    return true;
}

void RegisterBaseOpcodes() {
    auto& reg = OpcodeRegistry::Instance();
    reg.Register(0x0001, Opcode_0001_Wait);
    reg.Register(0x0002, Opcode_0002_Goto);
    reg.Register(0x0004, Opcode_0004_SetVarInt);
    reg.Register(0x0005, Opcode_0005_SetVarFloat);
    reg.Register(0x0006, Opcode_0006_AddVarInt);
    reg.Register(0x0007, Opcode_0007_AddVarFloat);
    reg.Register(0x0008, Opcode_0008_SubVarInt);
    reg.Register(0x0009, Opcode_0009_SubVarFloat);
    reg.Register(0x000A, Opcode_000A_MultVarInt);
    reg.Register(0x000B, Opcode_000B_MultVarFloat);
    reg.Register(0x000C, Opcode_000C_DivVarInt);
    reg.Register(0x000D, Opcode_000D_DivVarFloat);
    reg.Register(0x0038, Opcode_0038_IsIntGreater);
    reg.Register(0x0039, Opcode_0039_IsIntGreaterOrEqual);
    reg.Register(0x004D, Opcode_004D_JumpIfFalse);
    reg.Register(0x004E, Opcode_004E_EndThread);
    reg.Register(0x0050, Opcode_0050_Return);
    reg.Register(0x0051, Opcode_0051_Call);
    reg.Register(0x00D6, Opcode_00D6_If);
}

} // namespace CSALoader
