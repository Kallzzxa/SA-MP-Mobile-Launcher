#pragma once

#include <cstdint>
#include <unordered_map>
#include <functional>

namespace CSALoader {

class RunningScript;

using OpcodeHandler = bool (*)(RunningScript* pScript);

class OpcodeRegistry {
public:
    static OpcodeRegistry& Instance();

    void Register(uint16_t opcode, OpcodeHandler handler);
    void Unregister(uint16_t opcode);

    bool Execute(uint16_t opcode, RunningScript* pScript);
    bool HasHandler(uint16_t opcode) const;

    void Clear();

private:
    OpcodeRegistry();
    ~OpcodeRegistry() = default;

    std::unordered_map<uint16_t, OpcodeHandler> m_handlers;
};

} // namespace CSALoader
