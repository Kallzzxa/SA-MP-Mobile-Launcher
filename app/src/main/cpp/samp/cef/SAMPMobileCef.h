/*
    SA:MP Mobile CEF - C++ Client Header (Pro Upgraded Edition)
    Copyright © 2026 drgxbytezone & Community

    Open-Source Android WebView / Chromium Embedded Framework C++ NDK Client Header.
*/

#ifndef SAMP_MOBILE_CEF_H
#define SAMP_MOBILE_CEF_H

#include <stdint.h>
#include <stdbool.h>
#include <jni.h>
namespace cef
{
    // RPC Packet Identifiers
    enum CefRpcId
    {
        RPC_InitBrowser = 1,
        RPC_DestroyBrowser,
        RPC_ShowBrowser,
        RPC_HideBrowser,
        RPC_SetBrowserUrl,
        RPC_ChangeBrowserFocus,
        RPC_ServerEvent,

        RPC_LibraryInit = 8,
        RPC_BrowserInit = 9,
        RPC_ClientEvent = 10
    };

    /**
     * Sets the game storage cache directory path for client logs (SAMP/cef.log).
     * @param szPath Path to game storage directory (e.g. g_pszStorage).
     */
    void setGamePath(const char* szPath);

    /**
     * Initializes network layer for CEF custom packets.
     * @param pRakClient Pointer to RakClient instance.
     * @param packetId Custom network packet ID (e.g. ID_CUSTOM_CEF = 252).
     */
    void initNetwork(void* pRakClient, uint8_t packetId);

    /**
     * Handles incoming custom RakNet packets from SA:MP Server.
     * @param pPacket Pointer to RakNet Packet struct.
     */
    void handlePacket(void* pPacket);

    /**
     * Sends initial connection handshake to SA:MP Server.
     */
    void handleServerConnection();

    /**
     * Sends a client-side JavaScript event back to SA:MP Server via RakNet BitStream.
     * @param szEventName Name of client event.
     * @param szEventData JSON payload string.
     */
    void sendClientEvent(const char* szEventName, const char* szEventData);

    /**
     * Returns true if client CEF network is initialized.
     */
    bool isInitialized();

    /**
     * Reports WebView init result back to the SA:MP Server (RPC_BrowserInit).
     */
    void sendBrowserInit(bool success, int errorCode);

    /**
     * Utility function to append logs to SAMP/cef.log file.
     */
    void log(const char* fmt, ...);
    void setJavaManager(JNIEnv* env, jobject javaManager);
}

#endif // SAMP_MOBILE_CEF_H
