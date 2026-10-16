/*
    SA:MP Mobile CEF - C++ Client Implementation (Pro Upgraded Edition)
    Copyright © 2026 drgxbytezone & Community

    Open-Source reference implementation of SA:MP Mobile CEF C++ NDK Layer.
*/

#include "SAMPMobileCef.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>

#include "../vendor/raknet/RakClientInterface.h"
#include "../vendor/raknet/BitStream.h"
#include "../vendor/raknet/PacketEnumerations.h"

#include <jni.h>

extern JavaVM* javaVM;
namespace cef
{
    static char g_szGamePath[512] = {0};
    static char g_szLogPath[512] = {0};
    static void* g_pRakClient = nullptr;
    static uint8_t g_nPacketId = 252;
    static bool g_bInitialized = false;
    static jobject g_pJavaManager = nullptr;
    void setJavaManager(JNIEnv* env, jobject javaManager)
    {
        if (!env) return;

        if (g_pJavaManager) {
            env->DeleteGlobalRef(g_pJavaManager);
            g_pJavaManager = nullptr;
        }

        if (javaManager) {
            g_pJavaManager = env->NewGlobalRef(javaManager);
        }

        log("Java CEF manager registered: %p", g_pJavaManager);
    }
    
    static JNIEnv* getJNIEnv(bool& attached)
    {
        attached = false;

        if (!javaVM)
            return nullptr;

        JNIEnv* env = nullptr;

        jint result = javaVM->GetEnv(
            reinterpret_cast<void**>(&env),
            JNI_VERSION_1_6
        );

        if (result == JNI_EDETACHED) {
            if (javaVM->AttachCurrentThread(&env, nullptr) != JNI_OK)
                return nullptr;

            attached = true;
        }

        return env;
    }

    static void callJavaVoidMethod(const char* methodName)
    {
        if (!g_pJavaManager || !methodName)
            return;

        bool attached = false;
        JNIEnv* env = getJNIEnv(attached);

        if (!env)
            return;

        jclass cls = env->GetObjectClass(g_pJavaManager);

        if (!cls) {
            if (attached)
                javaVM->DetachCurrentThread();
            return;
        }

        jmethodID method = env->GetMethodID(
            cls,
            methodName,
            "()V"
        );

        if (method)
            env->CallVoidMethod(g_pJavaManager, method);

        env->DeleteLocalRef(cls);

        if (attached)
            javaVM->DetachCurrentThread();
    }
    static void callJavaLoadUrl(const char* url)
    {
        if (!g_pJavaManager || !url)
            return;

        bool attached = false;
        JNIEnv* env = getJNIEnv(attached);

        if (!env)
            return;

        jclass cls = env->GetObjectClass(g_pJavaManager);

        if (!cls) {
            if (attached)
                javaVM->DetachCurrentThread();
            return;
        }

        jmethodID method = env->GetMethodID(
            cls,
            "loadUrl",
            "(Ljava/lang/String;)V"
        );

        if (method) {
            jstring jUrl = env->NewStringUTF(url);

            env->CallVoidMethod(
                g_pJavaManager,
                method,
                jUrl
            );

            env->DeleteLocalRef(jUrl);
        }

        env->DeleteLocalRef(cls);

        if (attached)
            javaVM->DetachCurrentThread();
    }
    static void callJavaServerEvent(const char* eventName, const char* eventDataJson)
    {
        if (!g_pJavaManager || !eventName)
            return;

        bool attached = false;
        JNIEnv* env = getJNIEnv(attached);

        if (!env)
            return;

        jclass cls = env->GetObjectClass(g_pJavaManager);

        if (!cls) {
            if (attached)
                javaVM->DetachCurrentThread();
            return;
        }

        jmethodID method = env->GetMethodID(
            cls,
            "dispatchServerEvent",
            "(Ljava/lang/String;Ljava/lang/String;)V"
        );

        if (method) {
            jstring jName = env->NewStringUTF(eventName);
            jstring jData = env->NewStringUTF(eventDataJson ? eventDataJson : "[]");

            env->CallVoidMethod(
                g_pJavaManager,
                method,
                jName,
                jData
            );

            env->DeleteLocalRef(jName);
            env->DeleteLocalRef(jData);
        }

        env->DeleteLocalRef(cls);

        if (attached)
            javaVM->DetachCurrentThread();
    }
    void sendBrowserInit(bool success, int errorCode)
    {
        if (!g_bInitialized || !g_pRakClient)
            return;

        RakClientInterface* rakClient =
            reinterpret_cast<RakClientInterface*>(g_pRakClient);

        RakNet::BitStream bs;

        bs.Write((uint16_t)g_nPacketId);
        bs.Write((uint32_t)RPC_BrowserInit);
        bs.Write(success);
        bs.Write((int16_t)errorCode);

        rakClient->Send(
            &bs,
            HIGH_PRIORITY,
            RELIABLE,
            0
        );

        log(
            "BrowserInit sent: success=%d error=%d",
            success ? 1 : 0,
            errorCode
        );
    }
    void setGamePath(const char* szPath)
    {
        if (!szPath) return;

        snprintf(g_szGamePath, sizeof(g_szGamePath), "%s", szPath);
        snprintf(g_szLogPath, sizeof(g_szLogPath), "%s/SAMP/cef.log", szPath);

        // Ensure SAMP directory exists
        char szDir[512];
        snprintf(szDir, sizeof(szDir), "%s/SAMP", szPath);
        mkdir(szDir, 0755);

        log("CEF Client initialized. Storage path: %s", szPath);
    }

    void initNetwork(void* pRakClient, uint8_t packetId)
    {
        g_pRakClient = pRakClient;
        g_nPacketId = packetId;
        g_bInitialized = true;

        log("CEF Network initialized. RakClient: %p, Packet ID: %d", pRakClient, packetId);
    }

    bool isInitialized()
    {
        return g_bInitialized;
    }
    void handleServerConnection()
    {
        if (!g_bInitialized || !g_pRakClient)
            return;

        RakClientInterface* rakClient =
            reinterpret_cast<RakClientInterface*>(g_pRakClient);

        RakNet::BitStream bs;

        // Server protocol:
        // uint16 packetId
        // uint32 rpcId

        bs.Write((uint16_t)g_nPacketId);
        bs.Write((uint32_t)RPC_LibraryInit);

        bool sent = rakClient->Send(
            &bs,
            HIGH_PRIORITY,
            RELIABLE,
            0
        );

        log(
            "CEF LibraryInit sent: %s",
            sent ? "SUCCESS" : "FAILED"
        );
    }
    void sendClientEvent(
        const char* szEventName,
        const char* szEventData
    )
    {
        if (!g_bInitialized ||
            !g_pRakClient ||
            !szEventName ||
            !szEventData)
            return;

        uint16_t nameLen =
            (uint16_t)strlen(szEventName);

        uint16_t dataLen =
            (uint16_t)strlen(szEventData);

        RakClientInterface* rakClient =
            reinterpret_cast<RakClientInterface*>(g_pRakClient);

        RakNet::BitStream bs;

        bs.Write((uint16_t)g_nPacketId);
        bs.Write((uint32_t)RPC_ClientEvent);

        bs.Write(nameLen);

        if (nameLen > 0)
            bs.Write(szEventName, nameLen);

        bs.Write(dataLen);

        if (dataLen > 0)
            bs.Write(szEventData, dataLen);

        bool sent = rakClient->Send(
            &bs,
            HIGH_PRIORITY,
            RELIABLE,
            0
        );

        log(
            "ClientEvent '%s' sent: %s",
            szEventName,
            sent ? "SUCCESS" : "FAILED"
        );
    }
    void handlePacket(void* pPacket)
    {
        if (!pPacket || !g_bInitialized)
            return;

        Packet* packet =
            reinterpret_cast<Packet*>(pPacket);

        if (!packet->data || packet->length < 6)
            return;

        RakNet::BitStream bs(
            (unsigned char*)packet->data,
            packet->length,
            false
        );

        uint16_t packetId = 0;
        uint32_t rpcId = 0;

        if (!bs.Read(packetId))
            return;

        if (packetId != g_nPacketId)
            return;

        if (!bs.Read(rpcId))
            return;

        log(
            "CEF packet received: packet=%u rpc=%u length=%u",
            packetId,
            rpcId,
            packet->length
        );

        switch (rpcId)
        {
            case RPC_InitBrowser:
            {
                uint16_t urlLen = 0;

                if (!bs.Read(urlLen))
                    return;

                if (urlLen == 0 || urlLen >= 1024)
                    return;

                char url[1024] = {0};

                if (!bs.Read(url, urlLen))
                    return;

                url[urlLen] = '\0';

                log(
                    "RPC_InitBrowser -> %s",
                    url
                );

                callJavaLoadUrl(url);
                break;
            }

            case RPC_DestroyBrowser:
            {
                log("RPC_DestroyBrowser");

                callJavaVoidMethod("hideBrowserView");
                break;
            }

            case RPC_ShowBrowser:
            {
                log("RPC_ShowBrowser");

                callJavaVoidMethod("showBrowserView");
                break;
            }

            case RPC_HideBrowser:
            {
                log("RPC_HideBrowser");

                callJavaVoidMethod("hideBrowserView");
                break;
            }

            case RPC_SetBrowserUrl:
            {
                uint16_t urlLen = 0;

                if (!bs.Read(urlLen))
                    return;

                if (urlLen == 0 || urlLen >= 1024)
                    return;

                char url[1024] = {0};

                if (!bs.Read(url, urlLen))
                    return;

                url[urlLen] = '\0';

                log(
                    "RPC_SetBrowserUrl -> %s",
                    url
                );

                callJavaLoadUrl(url);
                break;
            }

            case RPC_ChangeBrowserFocus:
            {
                bool focused = false;

                if (!bs.Read(focused))
                    return;

                log(
                    "RPC_ChangeBrowserFocus -> %d",
                    focused ? 1 : 0
                );

                break;
            }

            case RPC_ServerEvent:
            {
                uint16_t nameLen = 0;

                if (!bs.Read(nameLen))
                    return;

                if (nameLen == 0 || nameLen >= 256)
                    return;

                char eventName[256] = {0};

                if (!bs.Read(eventName, nameLen))
                    return;

                eventName[nameLen] = '\0';

                uint16_t dataLen = 0;

                if (!bs.Read(dataLen))
                    return;

                if (dataLen >= 4096)
                    return;

                char eventData[4096] = {0};

                if (dataLen > 0) {
                    if (!bs.Read(eventData, dataLen))
                        return;
                }

                eventData[dataLen] = '\0';

                log(
                    "RPC_ServerEvent -> %s / %s",
                    eventName,
                    eventData
                );

                // Forward to the WebView so window.cef.on(...) handlers fire,
                // exactly like omp-cef does on PC.
                callJavaServerEvent(eventName, eventData);

                break;
            }

            default:
            {
                log(
                    "Unknown CEF RPC: %u",
                    rpcId
                );
                break;
            }
        }
    }

    void log(const char* fmt, ...)
    {
        if (g_szLogPath[0] == '\0') return;

        FILE* f = fopen(g_szLogPath, "a");
        if (!f) return;

        time_t rawtime;
        struct tm* timeinfo;
        char timebuf[32];

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(timebuf, sizeof(timebuf), "[%Y-%m-%d %H:%M:%S]", timeinfo);

        fprintf(f, "%s ", timebuf);

        va_list args;
        va_start(args, fmt);
        vfprintf(f, fmt, args);
        va_end(args);

        fprintf(f, "\n");
        fclose(f);
    }
    
}
