// OmpCefGlue.cpp — JNI bridge native -> Java untuk omp-cef.
//
// SAMP.java memanggil nativeSetOmpCefManager(manager) sekali saat
// CefJavaManager dibuat; di sini disimpan sebagai global ref + JavaVM.
// ompStart/ompStop dipanggil dari thread game (CNetGame) — JNIEnv diambil
// dengan attach bila perlu, lalu dilepas lagi.

#include "OmpCefGlue.h"

#include <jni.h>
#include <android/log.h>

#define OMP_TAG "SAMP_OMP_CEF"

static JavaVM* g_ompJavaVM = nullptr;
static jobject g_ompCefManager = nullptr;

static JNIEnv* ompGetJNIEnv(bool& attached)
{
    attached = false;
    if (!g_ompJavaVM) return nullptr;
    JNIEnv* env = nullptr;
    if (g_ompJavaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK || !env)
    {
        if (g_ompJavaVM->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return nullptr;
        attached = true;
    }
    return env;
}

extern "C" JNIEXPORT void JNICALL
Java_com_sampmobile_xyvern_game_SAMP_nativeSetOmpCefManager(JNIEnv* env, jobject thiz, jobject manager)
{
    if (g_ompCefManager)
    {
        env->DeleteGlobalRef(g_ompCefManager);
        g_ompCefManager = nullptr;
    }
    env->GetJavaVM(&g_ompJavaVM);
    if (manager) g_ompCefManager = env->NewGlobalRef(manager);
    __android_log_print(ANDROID_LOG_INFO, OMP_TAG, "CefJavaManager registered: %p", (void*)g_ompCefManager);
}

namespace cef
{
    void ompStart(const char* szHost, int iPort, int iPlayerId)
    {
        if (!g_ompCefManager || !szHost) return;
        bool attached = false;
        JNIEnv* env = ompGetJNIEnv(attached);
        if (!env) return;
        jclass cls = env->GetObjectClass(g_ompCefManager);
        if (!cls) { if (attached) g_ompJavaVM->DetachCurrentThread(); return; }
        jmethodID method = env->GetMethodID(cls, "startOmpCef", "(Ljava/lang/String;II)V");
        if (method)
        {
            jstring jHost = env->NewStringUTF(szHost);
            env->CallVoidMethod(g_ompCefManager, method, jHost, (jint)iPort, (jint)iPlayerId);
            env->DeleteLocalRef(jHost);
            __android_log_print(ANDROID_LOG_INFO, OMP_TAG, "omp-cef start: %s:%d playerId=%d", szHost, iPort, iPlayerId);
        }
        env->DeleteLocalRef(cls);
        if (attached) g_ompJavaVM->DetachCurrentThread();
    }

    void ompStop()
    {
        if (!g_ompCefManager) return;
        bool attached = false;
        JNIEnv* env = ompGetJNIEnv(attached);
        if (!env) return;
        jclass cls = env->GetObjectClass(g_ompCefManager);
        if (!cls) { if (attached) g_ompJavaVM->DetachCurrentThread(); return; }
        jmethodID method = env->GetMethodID(cls, "stopOmpCef", "()V");
        if (method) env->CallVoidMethod(g_ompCefManager, method);
        env->DeleteLocalRef(cls);
        if (attached) g_ompJavaVM->DetachCurrentThread();
    }
}
