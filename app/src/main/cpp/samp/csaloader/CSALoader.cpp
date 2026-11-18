#include "CSALoader.h"
#include "core/RunningScript.h"
#include "core/OpcodeRegistry.h"
#include "opcodes/BaseOpcodes.h"
#include "opcodes/CleoOpcodes.h"
#include "opcodes/GtaDelegator.h"
#include "loader/ScriptLoader.h"
#include "hook/GameHook.h"

#include <jni.h>
#include <android/log.h>
#include <string>

#define LOG_TAG "CSALoader_Main"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace CSALoader {

static bool s_bInitialized = false;
static std::string s_storagePath;

bool Initialize(const char* storagePath) {
    if (s_bInitialized) return true;

    LOGI("=== Initializing CSALoader (Custom Mobile SCM Engine) ===");

    if (storagePath && *storagePath) {
        s_storagePath = storagePath;
    }

    // 1. Register Opcode Handlers
    RegisterBaseOpcodes();
    RegisterCleoOpcodes();

    // 2. Install Game Hooks into libGTASA
    GameHook::Instance().InstallHooks();

    // 3. Initialize GTA Delegator
    GtaDelegator::Instance().Initialize(GameHook::Instance().GetLibGtaBase());

    // 4. Auto-scan default script directories if storage path provided
    if (!s_storagePath.empty()) {
        std::string scriptsDir = s_storagePath + "/scripts";
        std::string csaDir = s_storagePath + "/csa";
        std::string cleoDir = s_storagePath + "/cleo";

        int count = 0;
        count += ScriptLoader::Instance().ScanDirectory(scriptsDir);
        count += ScriptLoader::Instance().ScanDirectory(csaDir);
        count += ScriptLoader::Instance().ScanDirectory(cleoDir);
        LOGI("Total scripts auto-loaded from storage: %d", count);
    }

    s_bInitialized = true;
    LOGI("CSALoader initialized successfully.");
    return true;
}

void Shutdown() {
    if (!s_bInitialized) return;

    LOGI("Shutting down CSALoader...");
    ScriptLoader::Instance().Clear();
    OpcodeRegistry::Instance().Clear();
    GameHook::Instance().RemoveHooks();

    s_bInitialized = false;
    LOGI("CSALoader shutdown complete.");
}

void Process(uint32_t currentTick) {
    if (!s_bInitialized) return;
    ScriptLoader::Instance().ProcessAll(currentTick);
}

int ScanAndLoadScripts(const char* directoryPath) {
    if (!directoryPath || !*directoryPath) return 0;
    return ScriptLoader::Instance().ScanDirectory(directoryPath);
}

bool LoadScriptFromFile(const char* filePath, ScriptType type) {
    if (!filePath || !*filePath) return false;
    return ScriptLoader::Instance().LoadScript(filePath, type == ScriptType::Autonomous);
}

bool StartScript(const char* scriptName) {
    if (!scriptName) return false;
    ScriptLoader::Instance().TriggerInteractiveByName(scriptName);
    return true;
}

bool StopScript(const char* scriptName) {
    if (!scriptName) return false;
    auto scripts = ScriptLoader::Instance().GetLoadedScripts();
    for (auto& s : scripts) {
        if (s->GetName() == scriptName) {
            s->Terminate();
            return true;
        }
    }
    return false;
}

bool RestartScript(const char* scriptName) {
    if (!scriptName) return false;
    auto scripts = ScriptLoader::Instance().GetLoadedScripts();
    for (auto& s : scripts) {
        if (s->GetName() == scriptName) {
            s->Reset();
            return true;
        }
    }
    return false;
}

std::vector<ScriptInfo> GetLoadedScripts() {
    std::vector<ScriptInfo> result;
    auto scripts = ScriptLoader::Instance().GetLoadedScripts();
    result.reserve(scripts.size());

    for (const auto& s : scripts) {
        ScriptInfo info;
        info.name = s->GetName();
        info.path = s->GetPath();
        info.type = s->IsAutonomous() ? ScriptType::Autonomous : ScriptType::Interactive;
        info.size = static_cast<uint32_t>(s->GetSize());
        info.isActive = s->IsActive();
        result.push_back(std::move(info));
    }
    return result;
}

size_t GetActiveScriptCount() {
    return ScriptLoader::Instance().GetActiveCount();
}

void OnTriggerInteractiveMenu() {
    ScriptLoader::Instance().TriggerInteractive(0);
}

} // namespace CSALoader

// ---------------------------------------------------------------------------
// JNI Interface for Java Integration
// ---------------------------------------------------------------------------

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_sampmobile_xyvern_loader_CSALoaderBridge_nativeInit(JNIEnv* env, jclass clazz, jstring storagePath) {
    const char* pathStr = storagePath ? env->GetStringUTFChars(storagePath, nullptr) : "";
    bool ok = CSALoader::Initialize(pathStr);
    if (storagePath) env->ReleaseStringUTFChars(storagePath, pathStr);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_sampmobile_xyvern_loader_CSALoaderBridge_nativeScanScripts(JNIEnv* env, jclass clazz, jstring dirPath) {
    if (!dirPath) return 0;
    const char* dirStr = env->GetStringUTFChars(dirPath, nullptr);
    int count = CSALoader::ScanAndLoadScripts(dirStr);
    env->ReleaseStringUTFChars(dirPath, dirStr);
    return count;
}

JNIEXPORT void JNICALL
Java_com_sampmobile_xyvern_loader_CSALoaderBridge_nativeProcess(JNIEnv* env, jclass clazz, jint tick) {
    CSALoader::Process(static_cast<uint32_t>(tick));
}

JNIEXPORT void JNICALL
Java_com_sampmobile_xyvern_loader_CSALoaderBridge_nativeTriggerInteractive(JNIEnv* env, jclass clazz, jint index) {
    CSALoader::ScriptLoader::Instance().TriggerInteractive(static_cast<size_t>(index));
}

JNIEXPORT jint JNICALL
Java_com_sampmobile_xyvern_loader_CSALoaderBridge_nativeGetScriptCount(JNIEnv* env, jclass clazz) {
    return static_cast<jint>(CSALoader::GetActiveScriptCount());
}

} // extern "C"
