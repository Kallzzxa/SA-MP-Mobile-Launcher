#pragma once

#include <cstdint>
#include <functional>

namespace CSALoader {

class GameHook {
public:
    static GameHook& Instance();

    bool InstallHooks();
    void RemoveHooks();

    uintptr_t GetLibGtaBase() const { return m_libGtaBase; }
    bool IsInstalled() const { return m_bInstalled; }

    // Touch event pass-through from game to detect gestures (e.g. downward swipe)
    void OnTouchEvent(int type, int num, int posX, int posY);

private:
    GameHook();
    ~GameHook();

    uintptr_t FindLibraryBase(const char* libName);

    uintptr_t m_libGtaBase;
    bool      m_bInstalled;

    // Gesture tracking for interactive .csi scripts
    float     m_fTouchDownY;
    bool      m_bTouchDown;
};

} // namespace CSALoader
