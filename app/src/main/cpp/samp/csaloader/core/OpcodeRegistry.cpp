#include "OpcodeRegistry.h"
#include "RunningScript.h"

namespace CSALoader {

OpcodeRegistry& OpcodeRegistry::Instance() {
    static OpcodeRegistry s_instance;
    return s_instance;
}

OpcodeRegistry::OpcodeRegistry() {
    // Opcode table initialization happens in BaseOpcodes and CleoOpcodes init
}

void OpcodeRegistry::Register(uint16_t opcode, OpcodeHandler handler) {
    m_handlers[opcode] = handler;
}

void OpcodeRegistry::Unregister(uint16_t opcode) {
    m_handlers.erase(opcode);
}

bool OpcodeRegistry::HasHandler(uint16_t opcode) const {
    return m_handlers.find(opcode) != m_handlers.end();
}

bool OpcodeRegistry::Execute(uint16_t opcode, RunningScript* pScript) {
    auto it = m_handlers.find(opcode);
    if (it != m_handlers.end() && it->second != nullptr) {
        return it->second(pScript);
    }
    return false;
}

void OpcodeRegistry::Clear() {
    m_handlers.clear();
}

} // namespace CSALoader
