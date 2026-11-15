#pragma once

#include "main.h"

#define EXCEPTION_CHECK(env) \
	if ((env)->ExceptionCheck()) \ 
	{ \
		(env)->ExceptionDescribe(); \
		(env)->ExceptionClear(); \
		return; \
	}

class CJavaWrapper
{
public:
    CJavaWrapper(JNIEnv *env, jobject activity);
    ~CJavaWrapper() {};
    
    const char* GetClipboardString();
    void SetPauseState(bool pause);
    void ShowLoadingScreen();
    void HideLoadingScreen();
    void ShowTab();
    void HideTab();
    void SetTab(int id, char* names, int score, int pings);
    void ClearTab();
    
    void ShowKeyboard();
    void HideKeyboard();

	void exitGame();

    jbyteArray as_byte_array(JNIEnv* p, unsigned char* buf, int len) {
		jbyteArray array = p->NewByteArray (len);
        if (!array) return nullptr;
		p->SetByteArrayRegion (array, 0, len, reinterpret_cast<jbyte*>(buf));
		return array;
	}

    void ShowDialog(int dialogStyle, int dialogID, char* title, char* text, char* button1, char* button2);
    void HideDialog();

	void ShowEditObject();
	void HideEditObject();

    jobject activity = nullptr;
    jmethodID s_setPauseState = nullptr;
    jmethodID s_showLoadingScreen = nullptr;
    jmethodID s_hideLoadingScreen = nullptr;
    jmethodID s_ShowDialog = nullptr;
    jmethodID s_HideDialog = nullptr;
    jmethodID s_showTab = nullptr;
    jmethodID s_setTab = nullptr;
    jmethodID s_clearTab = nullptr;
    jmethodID s_hideTab = nullptr;
    jmethodID s_showInputLayout = nullptr;
    jmethodID s_hideInputLayout = nullptr;
	jmethodID s_exitGame = nullptr;
	jmethodID s_showEditObject = nullptr;
	jmethodID s_hideEditObject = nullptr;
};
