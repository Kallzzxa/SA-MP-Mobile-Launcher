#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>

namespace CSALoader {

// GTA SA SCM Parameter Data Types
enum ScmDataType : uint8_t {
    SCM_END_OF_ARG_LIST = 0x00,
    SCM_INT32           = 0x01,
    SCM_GLOBAL_VAR      = 0x02,
    SCM_LOCAL_VAR       = 0x03,
    SCM_INT8            = 0x04,
    SCM_INT16           = 0x05,
    SCM_FLOAT32         = 0x06,
    SCM_STRING8         = 0x09,
    SCM_GLOBAL_VAR_ARR  = 0x0A,
    SCM_LOCAL_VAR_ARR   = 0x0B,
    SCM_VAR_LEN_STRING  = 0x0E,
    SCM_STRING16        = 0x0F
};

// SCM Variable slot (can hold 32-bit int, float, or pointer)
union ScmVariable {
    int32_t   iValue;
    uint32_t  uValue;
    float     fValue;
    uintptr_t pValue;

    ScmVariable() : uValue(0) {}
    explicit ScmVariable(int32_t v) : iValue(v) {}
    explicit ScmVariable(uint32_t v) : uValue(v) {}
    explicit ScmVariable(float v) : fValue(v) {}
    explicit ScmVariable(uintptr_t v) : pValue(v) {}
};

// Script status
enum class ScriptState : uint8_t {
    Idle,
    Running,
    Sleeping,
    Terminated,
    Error
};

// Maximum number of local variables in a standard SCM thread
constexpr size_t SCM_MAX_LOCAL_VARS = 32;

// Maximum return stack depth for CALL / RETURN
constexpr size_t SCM_MAX_CALL_STACK = 16;

} // namespace CSALoader
