#include "GtaDelegator.h"
#include "../core/RunningScript.h"
#include <dlfcn.h>
#include <android/log.h>

#define LOG_TAG "CSALoader_Delegator"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace CSALoader {

GtaDelegator& GtaDelegator::Instance() {
    static GtaDelegator s_instance;
    return s_instance;
}

GtaDelegator::GtaDelegator()
    : m_libGtaBase(0),
      m_pfnProcessOneCommand(nullptr)
{
}

void GtaDelegator::Initialize(uintptr_t libGtaBase) {
    m_libGtaBase = libGtaBase;

    // Lookup CRunningScript::ProcessOneCommand symbol or offset
    void* handle = dlopen("libGTASA.so", RTLD_NOLOAD);
    if (!handle) {
        handle = dlopen("libGTASA.so", RTLD_NOW);
    }

    if (handle) {
        // Mangled name for CRunningScript::ProcessOneCommand()
        m_pfnProcessOneCommand = dlsym(handle, "_ZN14CRunningScript17ProcessOneCommandEv");
        if (m_pfnProcessOneCommand) {
            LOGD("Resolved CRunningScript::ProcessOneCommand at %p", m_pfnProcessOneCommand);
        } else {
            LOGW("Could not resolve CRunningScript::ProcessOneCommand by symbol");
        }
    }
}

bool GtaDelegator::Delegate(uint16_t opcode, RunningScript* pScript) {
    if (!m_pfnProcessOneCommand || !pScript) {
        return false;
    }

    // In advanced setups, a fake or wrapper CRunningScript GTA struct is passed
    // to GTA's native opcode dispatcher.
    return false;
}

} // namespace CSALoader
