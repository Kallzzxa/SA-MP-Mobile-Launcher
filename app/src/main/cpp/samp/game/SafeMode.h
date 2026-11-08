#pragma once

#include <unistd.h>

// Recovery state shared between the Java bootstrap and the native layer.
//
// The Java side writes a marker file when the previous start-up did not survive
// (see BootGuard). The native layer reads it once, before any optional patch is
// installed, so a build that keeps dying during InitialiseRenderWare can still
// reach the main menu with the stock engine behaviour.
namespace SafeMode
{
inline const char* const* CandidateMarkers()
{
    static const char* const kMarkers[] = {
        "/data/data/com.hopepride.samp/files/safe_mode.on",
        "/data/user/0/com.hopepride.samp/files/safe_mode.on",
        "/storage/emulated/0/Android/data/com.hopepride.samp/files/safe_mode.on",
        nullptr
    };
    return kMarkers;
}

inline const char* ActiveMarker()
{
    static const char* s_marker = nullptr;
    static bool s_checked = false;

    if (!s_checked)
    {
        s_checked = true;
        const char* const* markers = CandidateMarkers();
        for (int i = 0; markers[i] != nullptr; ++i)
        {
            if (access(markers[i], F_OK) == 0)
            {
                s_marker = markers[i];
                break;
            }
        }
    }

    return s_marker;
}

inline bool IsEnabled()
{
    return ActiveMarker() != nullptr;
}
}
