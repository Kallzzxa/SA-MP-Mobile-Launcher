#include "jniutil.h"
#include "jni_env_scope.h"
#include "game/game.h"
extern CGame *pGame;

CJavaWrapper::CJavaWrapper(JNIEnv *env, jobject activity)
{
    this->activity = env->NewGlobalRef(activity);

    jclass clas = env->GetObjectClass(activity);
    if(!clas)
    {
        FLog("no clas");
        return;
    }

    s_showTab = env->GetMethodID(clas, "showTab", "()V");
    s_hideTab = env->GetMethodID(clas, "hideTab", "()V");
    s_clearTab = env->GetMethodID(clas, "clearTab", "()V");
    s_setTab = env->GetMethodID(clas, "setTab", "(ILjava/lang/String;II)V");

    s_showLoadingScreen = env->GetMethodID(clas, "showLoadingScreen", "()V");
    s_hideLoadingScreen = env->GetMethodID(clas, "hideLoadingScreen", "()V");

    s_setPauseState = env->GetMethodID(clas, "setPauseState", "(Z)V");
    
    s_ShowDialog = env->GetMethodID(clas, "showDialog", "(II[B[B[B[B)V");
    s_HideDialog = env->GetMethodID(clas, "hideDialog", "()V");

	s_showInputLayout = env->GetMethodID(clas, "showKeyboard", "()V");
    s_hideInputLayout = env->GetMethodID(clas, "hideKeyboard", "()V");

    s_exitGame = env->GetMethodID(clas, "exitGame", "()V");

    s_showEditObject = env->GetMethodID(clas, "showEditObject", "()V");
    s_hideEditObject = env->GetMethodID(clas, "hideEditObject", "()V");

    env->DeleteLocalRef(clas);
}

void CJavaWrapper::ShowKeyboard()
{
    ScopedJniEnv<JavaVM, JNIEnv> scope(javaVM);
    JNIEnv* p = scope.get();
    if (!p || !activity || !s_showInputLayout) return;
    p->CallVoidMethod(activity, s_showInputLayout);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::HideKeyboard()
{
    ScopedJniEnv<JavaVM, JNIEnv> scope(javaVM);
    JNIEnv* p = scope.get();
    if (!p || !activity || !s_hideInputLayout) return;
    p->CallVoidMethod(activity, s_hideInputLayout);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::ShowLoadingScreen()
{
    ScopedJniEnv<JavaVM, JNIEnv> scope(javaVM);
    JNIEnv* p = scope.get();
    if (!p || !activity || !s_showLoadingScreen) return;
    p->CallVoidMethod(activity, s_showLoadingScreen);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::HideLoadingScreen()
{
    ScopedJniEnv<JavaVM, JNIEnv> scope(javaVM);
    JNIEnv* p = scope.get();
    if (!p || !activity || !s_hideLoadingScreen) return;
    p->CallVoidMethod(activity, s_hideLoadingScreen);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::SetPauseState(bool pause)
{
    ScopedJniEnv<JavaVM, JNIEnv> scope(javaVM);
    JNIEnv* p = scope.get();
    if (!p || !activity || !s_setPauseState) return;
    p->CallVoidMethod(activity, s_setPauseState, pause);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::SetTab(int id, char* names, int score, int pings)
{
    ScopedJniEnv<JavaVM, JNIEnv> scope(javaVM);
    JNIEnv* global_env = scope.get();
    if (!global_env || !activity || !s_setTab || !names) return;

	jclass strClass = global_env->FindClass("java/lang/String"); 
    EXCEPTION_CHECK(global_env);
    if (!strClass) return;
	jmethodID ctorID = global_env->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V");
    EXCEPTION_CHECK(global_env);
    if (!ctorID) { global_env->DeleteLocalRef(strClass); return; }
	jstring encoding = global_env->NewStringUTF("UTF-8");
    EXCEPTION_CHECK(global_env);
    if (!encoding) { global_env->DeleteLocalRef(strClass); return; }

	jbyteArray bytes = global_env->NewByteArray(strlen(names));
    EXCEPTION_CHECK(global_env);
    if (!bytes) { global_env->DeleteLocalRef(encoding); global_env->DeleteLocalRef(strClass); return; }
	global_env->SetByteArrayRegion(bytes, 0, strlen(names), (jbyte*)names);
    EXCEPTION_CHECK(global_env);
	jstring str1 = (jstring)global_env->NewObject(strClass, ctorID, bytes, encoding);
    EXCEPTION_CHECK(global_env);
    if (!str1) { global_env->DeleteLocalRef(bytes); global_env->DeleteLocalRef(encoding); global_env->DeleteLocalRef(strClass); return; }

    global_env->CallVoidMethod(activity, s_setTab, id, str1, score, pings);
    global_env->DeleteLocalRef(str1);
    global_env->DeleteLocalRef(bytes);
    global_env->DeleteLocalRef(encoding);
    global_env->DeleteLocalRef(strClass);
    EXCEPTION_CHECK(global_env);
}

void CJavaWrapper::ShowTab()
{
    ScopedJniEnv<JavaVM, JNIEnv> scope(javaVM);
    JNIEnv* p = scope.get();
    if (!p || !activity || !s_showTab) return;
    p->CallVoidMethod(activity, s_showTab);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::HideTab()
{
    ScopedJniEnv<JavaVM, JNIEnv> scope(javaVM);
    JNIEnv* p = scope.get();
    if (!p || !activity || !s_hideTab) return;
    p->CallVoidMethod(activity, s_hideTab);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::ClearTab()
{
    ScopedJniEnv<JavaVM, JNIEnv> scope(javaVM);
    JNIEnv* p = scope.get();
    if (!p || !activity || !s_clearTab) return;
    p->CallVoidMethod(activity, s_clearTab);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::ShowDialog(int dialogStyle, int dialogID, char* title, char* text, char* button1, char* button2)
{
    ScopedJniEnv<JavaVM, JNIEnv> scope(javaVM);
    JNIEnv* env = scope.get();
    if (!env || !activity || !s_ShowDialog || !title || !text || !button1 || !button2) return;

	std::string sTitle(title);
	std::string sText(text);
	std::string sButton1(button1);
	std::string sButton2(button2);

	jbyteArray jstrTitle = as_byte_array(env, (unsigned char*)sTitle.c_str(), sTitle.length());
    EXCEPTION_CHECK(env);
    if (!jstrTitle) return;
	jbyteArray jstrText = as_byte_array(env, (unsigned char*)sText.c_str(), sText.length());
    EXCEPTION_CHECK(env);
    if (!jstrText) return;
	jbyteArray jstrButton1 = as_byte_array(env, (unsigned char*)sButton1.c_str(), sButton1.length());
    EXCEPTION_CHECK(env);
    if (!jstrButton1) return;
	jbyteArray jstrButton2 = as_byte_array(env, (unsigned char*)sButton2.c_str(), sButton2.length());
    EXCEPTION_CHECK(env);
    if (!jstrButton2) return;

	env->CallVoidMethod(activity, s_ShowDialog, dialogID, dialogStyle, jstrTitle, jstrText, jstrButton1, jstrButton2);
	env->DeleteLocalRef(jstrTitle);
	env->DeleteLocalRef(jstrText);
	env->DeleteLocalRef(jstrButton1);
	env->DeleteLocalRef(jstrButton2);

	EXCEPTION_CHECK(env);
}

void CJavaWrapper::HideDialog()
{
    ScopedJniEnv<JavaVM, JNIEnv> scope(javaVM);
    JNIEnv* env = scope.get();
    if (!env || !activity || !s_HideDialog) return;
    env->CallVoidMethod(activity, s_HideDialog);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::exitGame() {
    ScopedJniEnv<JavaVM, JNIEnv> scope(javaVM);
    JNIEnv* env = scope.get();
    if (!env || !activity || !s_exitGame) return;

    env->CallVoidMethod(this->activity, this->s_exitGame);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::ShowEditObject() {
    ScopedJniEnv<JavaVM, JNIEnv> scope(javaVM);
    JNIEnv* env = scope.get();
    if (!env || !activity || !s_showEditObject) return;

    env->CallVoidMethod(this->activity, this->s_showEditObject);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::HideEditObject() {
    ScopedJniEnv<JavaVM, JNIEnv> scope(javaVM);
    JNIEnv* env = scope.get();
    if (!env || !activity || !s_hideEditObject) return;

    env->CallVoidMethod(this->activity, this->s_hideEditObject);
    EXCEPTION_CHECK(env);
}
