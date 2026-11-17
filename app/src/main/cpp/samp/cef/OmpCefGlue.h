// OmpCefGlue.h — JNI bridge native -> Java untuk omp-cef.
//
// Dipanggil dari CNetGame (Packet_ConnectionSucceeded saat game connect,
// destructor + ShutdownForGameModeRestart saat disconnect). Meneruskan ke
// CefJavaManager.startOmpCef/stopOmpCef lewat global ref yang didaftarkan
// SAMP.java (nativeSetOmpCefManager) saat CefJavaManager dibuat.
// CefMobile tidak tersentuh: jalurnya tetap Java -> native (CefMobileClient).

#pragma once

namespace cef
{
    void ompStart(const char* szHost, int iPort, int iPlayerId);
    void ompStop();
}
