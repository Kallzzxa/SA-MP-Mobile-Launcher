#include "ScriptLoader.h"
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <algorithm>
#include <android/log.h>

#define LOG_TAG "CSALoader_Loader"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace CSALoader {

ScriptLoader& ScriptLoader::Instance() {
    static ScriptLoader s_instance;
    return s_instance;
}

static bool EndsWithCaseInsensitive(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    size_t start = str.length() - suffix.length();
    for (size_t i = 0; i < suffix.length(); ++i) {
        if (std::tolower(str[start + i]) != std::tolower(suffix[i])) return false;
    }
    return true;
}

static std::string GetFilename(const std::string& path) {
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return path.substr(lastSlash + 1);
    }
    return path;
}

int ScriptLoader::ScanDirectory(const std::string& folderPath) {
    DIR* dir = opendir(folderPath.c_str());
    if (!dir) {
        LOGD("Folder does not exist or unreadable: %s", folderPath.c_str());
        return 0;
    }

    int loadedCount = 0;
    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;

        std::string fullPath = folderPath + "/" + entry->d_name;
        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            // Recursive scan for subdirectories (e.g. scripts/sa/)
            loadedCount += ScanDirectory(fullPath);
        } else if (S_ISREG(st.st_mode)) {
            std::string name(entry->d_name);
            if (EndsWithCaseInsensitive(name, ".csa")) {
                if (LoadScript(fullPath, true)) {
                    loadedCount++;
                }
            } else if (EndsWithCaseInsensitive(name, ".csi")) {
                if (LoadScript(fullPath, false)) {
                    loadedCount++;
                }
            }
        }
    }
    closedir(dir);
    return loadedCount;
}

bool ScriptLoader::LoadScript(const std::string& filePath, bool isAutonomous) {
    FILE* f = fopen(filePath.c_str(), "rb");
    if (!f) {
        LOGE("Failed to open script: %s", filePath.c_str());
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 10 * 1024 * 1024) {
        LOGE("Invalid script size (%ld bytes): %s", size, filePath.c_str());
        fclose(f);
        return false;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    size_t bytesRead = fread(buffer.data(), 1, static_cast<size_t>(size), f);
    fclose(f);

    if (bytesRead != static_cast<size_t>(size)) {
        LOGE("Short read on script: %s", filePath.c_str());
        return false;
    }

    std::string name = GetFilename(filePath);

    std::lock_guard<std::mutex> lock(m_mutex);
    // Avoid duplicate scripts
    for (const auto& s : m_scripts) {
        if (s->GetPath() == filePath) {
            LOGD("Script already loaded: %s", filePath.c_str());
            return true;
        }
    }

    auto script = std::make_shared<RunningScript>(name, filePath, std::move(buffer), isAutonomous);
    m_scripts.push_back(script);

    LOGI("Loaded %s script [%s] (%ld bytes)",
         isAutonomous ? "autonomous (.csa)" : "interactive (.csi)",
         name.c_str(), size);
    return true;
}

void ScriptLoader::ProcessAll(uint32_t currentTick) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& script : m_scripts) {
        if (script->IsAutonomous() && script->IsActive()) {
            script->Process(currentTick);
        } else if (!script->IsAutonomous() && script->IsActive()) {
            script->Process(currentTick);
        }
    }
}

void ScriptLoader::TriggerInteractive(size_t index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t csiIndex = 0;
    for (auto& script : m_scripts) {
        if (!script->IsAutonomous()) {
            if (csiIndex == index) {
                script->Reset();
                LOGI("Triggered interactive script: %s", script->GetName().c_str());
                break;
            }
            csiIndex++;
        }
    }
}

void ScriptLoader::TriggerInteractiveByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& script : m_scripts) {
        if (!script->IsAutonomous() && script->GetName() == name) {
            script->Reset();
            LOGI("Triggered interactive script by name: %s", name.c_str());
            break;
        }
    }
}

std::vector<std::shared_ptr<RunningScript>> ScriptLoader::GetLoadedScripts() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_scripts;
}

size_t ScriptLoader::GetActiveCount() {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& s : m_scripts) {
        if (s->IsActive()) count++;
    }
    return count;
}

void ScriptLoader::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_scripts.clear();
}

} // namespace CSALoader
