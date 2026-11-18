#include "RunningScript.h"
#include "OpcodeRegistry.h"
#include <android/log.h>
#include <cstring>

#define LOG_TAG "CSALoader"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace CSALoader {

// Max instructions per script per frame to prevent freezing on while(true) without wait
constexpr size_t MAX_COMMANDS_PER_TICK = 1000;

RunningScript::RunningScript(std::string name, std::string path, std::vector<uint8_t> bytecode, bool isAutonomous)
    : m_name(std::move(name)),
      m_path(std::move(path)),
      m_bytecode(std::move(bytecode)),
      m_bAutonomous(isAutonomous),
      m_ip(0),
      m_state(ScriptState::Running),
      m_nWakeTime(0),
      m_bConditionResult(false),
      m_bNotFlag(false),
      m_nStackPointer(0)
{
    std::memset(m_aLocalVars, 0, sizeof(m_aLocalVars));
    std::memset(m_aCallStack, 0, sizeof(m_aCallStack));
}

void RunningScript::Reset() {
    m_ip = 0;
    m_state = ScriptState::Running;
    m_nWakeTime = 0;
    m_bConditionResult = false;
    m_bNotFlag = false;
    m_nStackPointer = 0;
    std::memset(m_aLocalVars, 0, sizeof(m_aLocalVars));
}

void RunningScript::Terminate() {
    m_state = ScriptState::Terminated;
}

void RunningScript::SetSleep(uint32_t durationMs, uint32_t currentTick) {
    m_state = ScriptState::Sleeping;
    m_nWakeTime = currentTick + durationMs;
}

void RunningScript::WakeUp() {
    m_state = ScriptState::Running;
    m_nWakeTime = 0;
}

void RunningScript::Process(uint32_t currentTick) {
    if (m_state == ScriptState::Terminated || m_state == ScriptState::Error) {
        return;
    }

    if (m_state == ScriptState::Sleeping) {
        if (currentTick >= m_nWakeTime) {
            WakeUp();
        } else {
            return;
        }
    }

    size_t commandsExecuted = 0;
    while (m_state == ScriptState::Running && m_ip < m_bytecode.size()) {
        if (++commandsExecuted > MAX_COMMANDS_PER_TICK) {
            // Guard against infinite loops without WAIT
            SetSleep(0, currentTick);
            break;
        }

        uint16_t opcode = 0;
        bool notFlag = false;
        if (!ReadOpcode(opcode, notFlag)) {
            LOGE("[%s] Unexpected EOF reading opcode at IP %zu", m_name.c_str(), m_ip);
            m_state = ScriptState::Error;
            break;
        }

        m_bNotFlag = notFlag;

        if (!OpcodeRegistry::Instance().Execute(opcode, this)) {
            LOGE("[%s] Unhandled or failed opcode 0x%04X (NOT: %d) at IP %zu",
                 m_name.c_str(), opcode, notFlag ? 1 : 0, m_ip);
            // Default behavior: stop script to avoid cascade corruptions
            m_state = ScriptState::Error;
            break;
        }
    }

    if (m_ip >= m_bytecode.size() && m_state == ScriptState::Running) {
        LOGD("[%s] Script completed normally (reached end of bytecode).", m_name.c_str());
        m_state = ScriptState::Terminated;
    }
}

bool RunningScript::ReadOpcode(uint16_t& outOpcode, bool& outNotFlag) {
    if (m_ip + 2 > m_bytecode.size()) return false;
    uint16_t raw = *reinterpret_cast<const uint16_t*>(&m_bytecode[m_ip]);
    m_ip += 2;

    outNotFlag = (raw & 0x8000) != 0;
    outOpcode = raw & 0x7FFF;
    return true;
}

bool RunningScript::ReadInt8(int8_t& outVal) {
    if (m_ip + 1 > m_bytecode.size()) return false;
    outVal = static_cast<int8_t>(m_bytecode[m_ip++]);
    return true;
}

bool RunningScript::ReadInt16(int16_t& outVal) {
    if (m_ip + 2 > m_bytecode.size()) return false;
    outVal = *reinterpret_cast<const int16_t*>(&m_bytecode[m_ip]);
    m_ip += 2;
    return true;
}

bool RunningScript::ReadInt32(int32_t& outVal) {
    if (m_ip + 4 > m_bytecode.size()) return false;
    outVal = *reinterpret_cast<const int32_t*>(&m_bytecode[m_ip]);
    m_ip += 4;
    return true;
}

bool RunningScript::ReadFloat(float& outVal) {
    if (m_ip + 4 > m_bytecode.size()) return false;
    outVal = *reinterpret_cast<const float*>(&m_bytecode[m_ip]);
    m_ip += 4;
    return true;
}

bool RunningScript::ReadString(char* outBuf, size_t maxLen) {
    if (!outBuf || maxLen == 0 || m_ip >= m_bytecode.size()) return false;
    uint8_t type = m_bytecode[m_ip++];
    size_t len = 0;

    if (type == SCM_STRING8) {
        len = 8;
    } else if (type == SCM_STRING16) {
        len = 16;
    } else if (type == SCM_VAR_LEN_STRING) {
        if (m_ip >= m_bytecode.size()) return false;
        len = m_bytecode[m_ip++];
    } else {
        return false;
    }

    if (m_ip + len > m_bytecode.size()) return false;
    size_t copyLen = (len < maxLen - 1) ? len : maxLen - 1;
    std::memcpy(outBuf, &m_bytecode[m_ip], copyLen);
    outBuf[copyLen] = '\0';
    m_ip += len;
    return true;
}

bool RunningScript::ReadParameter(ScmVariable& outVal) {
    if (m_ip >= m_bytecode.size()) return false;
    uint8_t type = m_bytecode[m_ip++];

    switch (type) {
        case SCM_INT8: {
            int8_t v = 0;
            if (!ReadInt8(v)) return false;
            outVal.iValue = v;
            return true;
        }
        case SCM_INT16: {
            int16_t v = 0;
            if (!ReadInt16(v)) return false;
            outVal.iValue = v;
            return true;
        }
        case SCM_INT32: {
            int32_t v = 0;
            if (!ReadInt32(v)) return false;
            outVal.iValue = v;
            return true;
        }
        case SCM_FLOAT32: {
            float v = 0.0f;
            if (!ReadFloat(v)) return false;
            outVal.fValue = v;
            return true;
        }
        case SCM_LOCAL_VAR: {
            uint16_t idx = 0;
            if (m_ip + 2 > m_bytecode.size()) return false;
            idx = *reinterpret_cast<const uint16_t*>(&m_bytecode[m_ip]);
            m_ip += 2;
            outVal = GetLocalVar(idx);
            return true;
        }
        case SCM_GLOBAL_VAR: {
            uint16_t idx = 0;
            if (m_ip + 2 > m_bytecode.size()) return false;
            idx = *reinterpret_cast<const uint16_t*>(&m_bytecode[m_ip]);
            m_ip += 2;
            // GTA SA script space global variable offset (approx)
            outVal.uValue = idx;
            return true;
        }
        default:
            return false;
    }
}

bool RunningScript::ReadIntParam(int32_t& outVal) {
    ScmVariable v;
    if (!ReadParameter(v)) return false;
    outVal = v.iValue;
    return true;
}

bool RunningScript::ReadFloatParam(float& outVal) {
    ScmVariable v;
    if (!ReadParameter(v)) return false;
    outVal = v.fValue;
    return true;
}

bool RunningScript::ReadPointerParam(uintptr_t& outVal) {
    ScmVariable v;
    if (!ReadParameter(v)) return false;
    outVal = v.pValue;
    return true;
}

bool RunningScript::ReadVarTarget(ScmVariable*& outTarget) {
    if (m_ip >= m_bytecode.size()) return false;
    uint8_t type = m_bytecode[m_ip++];

    if (type == SCM_LOCAL_VAR) {
        if (m_ip + 2 > m_bytecode.size()) return false;
        uint16_t idx = *reinterpret_cast<const uint16_t*>(&m_bytecode[m_ip]);
        m_ip += 2;
        outTarget = &GetLocalVar(idx);
        return true;
    }
    return false;
}

bool RunningScript::PushStack(size_t returnIp) {
    if (m_nStackPointer >= SCM_MAX_CALL_STACK) return false;
    m_aCallStack[m_nStackPointer++] = returnIp;
    return true;
}

bool RunningScript::PopStack(size_t& outReturnIp) {
    if (m_nStackPointer == 0) return false;
    outReturnIp = m_aCallStack[--m_nStackPointer];
    return true;
}

void RunningScript::Jump(int32_t offset) {
    int64_t target = static_cast<int64_t>(m_ip) + offset;
    if (target >= 0 && static_cast<size_t>(target) <= m_bytecode.size()) {
        m_ip = static_cast<size_t>(target);
    }
}

void RunningScript::JumpAbsolute(size_t targetIp) {
    if (targetIp <= m_bytecode.size()) {
        m_ip = targetIp;
    }
}

} // namespace CSALoader
