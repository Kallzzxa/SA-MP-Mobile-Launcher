#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef CSALOADER_EXPORTS
    #define CSALOADER_API __attribute__((visibility("default")))
#else
    #define CSALOADER_API
#endif

namespace CSALoader {

enum class ScriptType {
    Autonomous,  // .csa: Runs automatically in background
    Interactive  // .csi: Triggered by user gesture / menu
};

struct ScriptInfo {
    std::string name;
    std::string path;
    ScriptType  type;
    uint32_t    size;
    bool        isActive;
};

// Lifecycle functions
CSALOADER_API bool Initialize(const char* storagePath);
CSALOADER_API void Shutdown();

// Frame processing - called once per game tick from CTheScripts::Process hook
CSALOADER_API void Process(uint32_t currentTick);

// Script management
CSALOADER_API int  ScanAndLoadScripts(const char* directoryPath);
CSALOADER_API bool LoadScriptFromFile(const char* filePath, ScriptType type);
CSALOADER_API bool StartScript(const char* scriptName);
CSALOADER_API bool StopScript(const char* scriptName);
CSALOADER_API bool RestartScript(const char* scriptName);

// Inspection & status
CSALOADER_API std::vector<ScriptInfo> GetLoadedScripts();
CSALOADER_API size_t GetActiveScriptCount();

// Input / Interactive script trigger
CSALOADER_API void OnTriggerInteractiveMenu();

} // namespace CSALoader
