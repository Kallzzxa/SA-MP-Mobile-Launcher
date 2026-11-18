#pragma once

#include "ScmTypes.h"
#include <string>
#include <vector>
#include <memory>

namespace CSALoader {

class RunningScript {
public:
    RunningScript(std::string name, std::string path, std::vector<uint8_t> bytecode, bool isAutonomous);
    ~RunningScript() = default;

    // Execution
    void Process(uint32_t currentTick);
    void Reset();
    void Terminate();

    // Parameter extraction helpers
    bool ReadOpcode(uint16_t& outOpcode, bool& outNotFlag);
    bool ReadInt8(int8_t& outVal);
    bool ReadInt16(int16_t& outVal);
    bool ReadInt32(int32_t& outVal);
    bool ReadFloat(float& outVal);
    bool ReadString(char* outBuf, size_t maxLen);

    // Dynamic SCM Parameter evaluation
    bool ReadParameter(ScmVariable& outVal);
    bool ReadIntParam(int32_t& outVal);
    bool ReadFloatParam(float& outVal);
    bool ReadPointerParam(uintptr_t& outVal);
    bool ReadVarTarget(ScmVariable*& outTarget);

    // Stack operations
    bool PushStack(size_t returnIp);
    bool PopStack(size_t& outReturnIp);

    // Jump / Flow operations
    void Jump(int32_t offset);
    void JumpAbsolute(size_t targetIp);

    // State getters / setters
    const std::string& GetName() const { return m_name; }
    const std::string& GetPath() const { return m_path; }
    ScriptState GetState() const { return m_state; }
    bool IsAutonomous() const { return m_bAutonomous; }
    bool IsActive() const { return m_state == ScriptState::Running || m_state == ScriptState::Sleeping; }

    void SetConditionResult(bool res) { m_bConditionResult = res; }
    bool GetConditionResult() const { return m_bConditionResult; }

    void SetSleep(uint32_t durationMs, uint32_t currentTick);
    void WakeUp();

    size_t GetIP() const { return m_ip; }
    size_t GetSize() const { return m_bytecode.size(); }
    const uint8_t* GetBytecode() const { return m_bytecode.data(); }

    ScmVariable* GetLocalVars() { return m_aLocalVars; }
    ScmVariable& GetLocalVar(size_t index) { return m_aLocalVars[index % SCM_MAX_LOCAL_VARS]; }

private:
    std::string           m_name;
    std::string           m_path;
    std::vector<uint8_t>  m_bytecode;
    bool                  m_bAutonomous;

    size_t                m_ip;
    ScriptState           m_state;
    uint32_t              m_nWakeTime;

    bool                  m_bConditionResult;
    bool                  m_bNotFlag;

    ScmVariable           m_aLocalVars[SCM_MAX_LOCAL_VARS];
    size_t                m_aCallStack[SCM_MAX_CALL_STACK];
    size_t                m_nStackPointer;
};

} // namespace CSALoader
