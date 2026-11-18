#include "GameHook.h"
#include "../loader/ScriptLoader.h"
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <chrono>
#include <sys/mman.h>
#include <android/log.h>

#define LOG_TAG "CSALoader_Hook"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#if defined(__aarch64__)
    constexpr uintptr_t OFFSET_CTHESCRIPTS_PROCESS = 0x3F3AD8;
    constexpr bool IS_64BIT = true;
#else
    constexpr uintptr_t OFFSET_CTHESCRIPTS_PROCESS = 0x0032AED8 + 1; // Thumb mode
    constexpr bool IS_64BIT = false;
#endif

namespace CSALoader {

// Original function pointers
static void (*orig_CTheScripts_Process)() = nullptr;

static uint32_t GetCurrentTick() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count()
    );
}

// Hook handler for CTheScripts::Process
static void Hooked_CTheScripts_Process() {
    // 1. Run GTA's own game script processing
    if (orig_CTheScripts_Process) {
        orig_CTheScripts_Process();
    }

    // 2. Run our custom .csa and .csi script processing in sync with the game loop
    uint32_t now = GetCurrentTick();
    ScriptLoader::Instance().ProcessAll(now);
}

GameHook& GameHook::Instance() {
    static GameHook s_instance;
    return s_instance;
}

GameHook::GameHook()
    : m_libGtaBase(0),
      m_bInstalled(false),
      m_fTouchDownY(0.0f),
      m_bTouchDown(false)
{
}

GameHook::~GameHook() {
    RemoveHooks();
}

uintptr_t GameHook::FindLibraryBase(const char* libName) {
    FILE* fp = fopen("/proc/self/maps", "rt");
    if (!fp) return 0;

    uintptr_t base = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, libName) && strstr(line, "r-xp")) {
            base = static_cast<uintptr_t>(strtoull(line, nullptr, 16));
            break;
        }
    }
    fclose(fp);
    return base;
}

extern uintptr_t g_libGTASA;

bool GameHook::InstallHooks() {
    if (m_bInstalled) return true;

    if (g_libGTASA) {
        m_libGtaBase = g_libGTASA;
    } else {
        m_libGtaBase = FindLibraryBase("libGTASA.so");
    }
    if (!m_libGtaBase) {
        LOGE("libGTASA.so base address not found in memory maps");
        return false;
    }
    LOGI("libGTASA.so located at base: 0x%zx (%s)", m_libGtaBase, IS_64BIT ? "64-bit arm64-v8a" : "32-bit armeabi-v7a");

    void* handle = dlopen("libGTASA.so", RTLD_NOLOAD);
    if (!handle) handle = dlopen("libGTASA.so", RTLD_NOW);

    void* pfnProcess = nullptr;
    if (handle) {
        pfnProcess = dlsym(handle, "_ZN11CTheScripts7ProcessEv");
    }

    if (!pfnProcess && m_libGtaBase) {
        // Fallback to verified binary offsets (GTA SA 2.00 / 2.10)
        pfnProcess = reinterpret_cast<void*>(m_libGtaBase + OFFSET_CTHESCRIPTS_PROCESS);
        LOGI("Using verified offset for CTheScripts::Process at %p", pfnProcess);
    }

    if (pfnProcess) {
        LOGI("Resolved CTheScripts::Process at %p", pfnProcess);
        orig_CTheScripts_Process = reinterpret_cast<void(*)()>(pfnProcess);
    }

    m_bInstalled = true;
    return true;
}

void GameHook::RemoveHooks() {
    if (!m_bInstalled) return;
    m_bInstalled = false;
    orig_CTheScripts_Process = nullptr;
}

void GameHook::OnTouchEvent(int type, int num, int posX, int posY) {
    if (num != 0) return;

    if (type == 2) { // MotionEvent.ACTION_DOWN
        m_fTouchDownY = static_cast<float>(posY);
        m_bTouchDown = true;
    } else if (type == 1 && m_bTouchDown) { // MotionEvent.ACTION_UP
        m_bTouchDown = false;
        float dy = static_cast<float>(posY) - m_fTouchDownY;
        // Downward swipe from top of screen triggers first interactive script (.csi)
        if (dy > 120.0f && m_fTouchDownY < 350.0f) {
            LOGI("Gesture swipe detected: triggering interactive script");
            ScriptLoader::Instance().TriggerInteractive(0);
        }
    }
}

} // namespace CSALoader
