#pragma once

#include "../core/RunningScript.h"
#include <vector>
#include <memory>
#include <mutex>
#include <string>

namespace CSALoader {

class ScriptLoader {
public:
    static ScriptLoader& Instance();

    int  ScanDirectory(const std::string& folderPath);
    bool LoadScript(const std::string& filePath, bool isAutonomous);

    void ProcessAll(uint32_t currentTick);

    void TriggerInteractive(size_t index);
    void TriggerInteractiveByName(const std::string& name);

    std::vector<std::shared_ptr<RunningScript>> GetLoadedScripts();
    size_t GetActiveCount();

    void Clear();

private:
    ScriptLoader() = default;
    ~ScriptLoader() = default;

    std::vector<std::shared_ptr<RunningScript>> m_scripts;
    std::mutex m_mutex;
};

} // namespace CSALoader
