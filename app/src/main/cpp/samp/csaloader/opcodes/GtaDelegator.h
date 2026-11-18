#pragma once

#include <cstdint>

namespace CSALoader {

class RunningScript;

class GtaDelegator {
public:
    static GtaDelegator& Instance();

    void Initialize(uintptr_t libGtaBase);
    bool Delegate(uint16_t opcode, RunningScript* pScript);
    bool IsAvailable() const { return m_pfnProcessOneCommand != nullptr; }

private:
    GtaDelegator();
    ~GtaDelegator() = default;

    uintptr_t m_libGtaBase;
    void*     m_pfnProcessOneCommand;
};

} // namespace CSALoader
