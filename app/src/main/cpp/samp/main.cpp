#include <jni.h>
#include <pthread.h>
#include <atomic>
#include <syscall.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "main.h"
#include "game/game.h"
#include "net/netgame.h"
#include "gui/gui.h"
#include "playertags.h"
#include "audiostream.h"
#include "java/jniutil.h"
#include "net/dl/dlmanager.h"
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "StackTrace.h"

// voice
#include "voice_new/Plugin.h"
#include "voice_new/PluginConfig.h"

#include "vendor/shadowhook/patch.h"
#include "vendor/str_obfuscator/str_obfuscator.hpp"

#include "settings.h"

#include "crashlytics.h"
#include "cef/SAMPMobileCef.h"
/*
Peerapol Unarak
*/

JavaVM* javaVM;

extern AAssetManager* g_pAssetManager; // Defined down below or use global
// Or just define it here if it was rejected before?
// Actually I replaced line 30 with just variable.
// I will just insert the function now.

std::vector<char> LoadAsset(const char* name) {
     if (!g_pAssetManager) return {};
     AAsset* asset = AAssetManager_open(g_pAssetManager, name, AASSET_MODE_BUFFER);
     if (!asset) return {};
     size_t size = AAsset_getLength(asset);
     std::vector<char> buffer(size);
     AAsset_read(asset, buffer.data(), size);
     AAsset_close(asset);
     return buffer;
 }

char* g_pszStorage = nullptr;

UI* pUI = nullptr;
CGame *pGame = nullptr;

CNetGame *pNetGame = nullptr;
CPlayerTags* pPlayerTags = nullptr;
CSnapShotHelper* pSnapShotHelper = nullptr;
CAudioStream* pAudioStream = nullptr;
CJavaWrapper* pJavaWrapper = nullptr;
static std::atomic<bool> g_javaReady{false};
CSettings* pSettings = nullptr;
//CVoice* pVoice = nullptr;

MaterialTextGenerator* pMaterialTextGenerator = nullptr;

bool bDebug = false;
bool bGameInited = false;
bool bNetworkInited = false;

uintptr_t g_libGTASA = 0x00;
uintptr_t g_libSAMP = 0x00;

void ApplyPatches();
void ApplyPatches_level0();
void ApplyMultiTouchPatches();
void InstallGlobalHooks();
void InstallSpecialHooks();
void InitRenderWareFunctions();
void FLog(const char* fmt, ...);
//void MyLog(const char* fmt, ...);

int work = 0;

void ReadSettingFile()
{
	pSettings = new CSettings();

	firebase::crashlytics::SetUserId(pSettings->Get().szNickName);
}

int hashing(const char* str) {
	int hashing = 5381;
	int c;
	while (c = *str++) {
		hashing = ((hashing << 5) + hashing) + c; /* hash * 33 + c */
		if (hashing < 0) hashing = 100;
	}
	if (hashing < 0) hashing = 100;
	return hashing;
}

void DoDebugLoop()
{
	// ...
}

void DoDebugStuff()
{
	// ...

	RwMatrix mat = pGame->FindPlayerPed()->m_pPed->GetMatrix().ToRwMatrix();
	
	for (int i = 0; i < 100; i++)
	{
		CPlayerPed* ped = pGame->NewPlayer(i, mat.pos.x + i, mat.pos.y, mat.pos.z, 0.0f, false, false);
		//ped->SetCollisionChecking(false);
		//ped->SetGravityProcessing(false);
	}
}
// ---------------------------------------------------------------------------
// Fatal signal reporting.
//
// FLog()/fprintf() are not async-signal-safe: if SIGSEGV lands on a thread that
// already holds the stdio or heap lock, the handler deadlocks and the crash is
// never recorded. The report below uses open()/write() only.
// ---------------------------------------------------------------------------
static int g_iCrashLogFd = -1;

static void CrashLogPut(const char* szText)
{
    if (g_iCrashLogFd < 0 || !szText) return;
    size_t uiLen = 0;
    while (szText[uiLen]) uiLen++;
    if (uiLen) (void)!write(g_iCrashLogFd, szText, uiLen);
}

static void CrashLogHex(uintptr_t uiValue)
{
    static const char szDigits[] = "0123456789abcdef";
    char szBuf[2 + sizeof(uintptr_t) * 2];
    int iLen = (int)(2 + sizeof(uintptr_t) * 2);
    szBuf[0] = '0';
    szBuf[1] = 'x';
    for (int i = iLen - 1; i >= 2; i--)
    {
        szBuf[i] = szDigits[uiValue & 0xF];
        uiValue >>= 4;
    }
    (void)!write(g_iCrashLogFd, szBuf, (size_t)iLen);
}

// Best effort: resolve an address to an exported symbol so the report names the
// engine function instead of only an offset. The raw offsets are already on
// disk before this runs, so a stalled dladdr() cannot lose the report.
static void CrashLogSymbol(uintptr_t uiAddress)
{
    if (!uiAddress) { CrashLogPut("(null)\n"); return; }

    Dl_info info;
    memset(&info, 0, sizeof(info));
    if (dladdr((void*)uiAddress, &info) && info.dli_sname)
        CrashLogPut(info.dli_sname);
    else
        CrashLogPut("(unknown)");
    CrashLogPut("\n");
}

static void CrashLogOpen()
{
    if (g_iCrashLogFd >= 0) return;

    const char* szRoot = g_pszStorage;
    if (!szRoot) return;

    const char szName[] = "crash_native.txt";
    char szPath[512];
    size_t i = 0;
    while (szRoot[i] && i + sizeof(szName) < sizeof(szPath))
    {
        szPath[i] = szRoot[i];
        i++;
    }
    for (size_t k = 0; k < sizeof(szName); k++)
    {
        szPath[i++] = szName[k];
    }

    g_iCrashLogFd = open(szPath, O_WRONLY | O_CREAT | O_APPEND, 0644);
}

// Live state of the RLEDecompress over-read guard (game/hooks.cpp), so each
// report shows which build produced it and whether the guard was active.
extern volatile int g_rleGuardHookResult;
extern volatile unsigned long long g_rleGuardCalls;
extern volatile unsigned long long g_rleGuardPads;

static void CrashLogReport(int iSignum, siginfo_t* pInfo, ucontext_t* pContext)
{
    CrashLogOpen();
    if (g_iCrashLogFd < 0) return;

    CrashLogPut("\n===== native crash =====\n");
    CrashLogPut("signal: ");
    CrashLogHex((uintptr_t)iSignum);
    CrashLogPut("\nfault:  ");
    CrashLogHex((uintptr_t)(pInfo ? pInfo->si_addr : nullptr));
    CrashLogPut("\n");

    if (pContext)
    {
#if defined(__aarch64__)
        uintptr_t uiPc = (uintptr_t)pContext->uc_mcontext.pc;
        uintptr_t uiLr = (uintptr_t)pContext->uc_mcontext.regs[30];
        uintptr_t uiFp = (uintptr_t)pContext->uc_mcontext.regs[29];
        uintptr_t uiSp = (uintptr_t)pContext->uc_mcontext.sp;
#elif defined(__arm__)
        uintptr_t uiPc = (uintptr_t)pContext->uc_mcontext.arm_pc;
        uintptr_t uiLr = (uintptr_t)pContext->uc_mcontext.arm_lr;
        uintptr_t uiFp = (uintptr_t)pContext->uc_mcontext.arm_fp;
        uintptr_t uiSp = (uintptr_t)pContext->uc_mcontext.arm_sp;
#else
        uintptr_t uiPc = 0, uiLr = 0, uiFp = 0, uiSp = 0;
#endif
        CrashLogPut("pc:     ");
        CrashLogHex(uiPc);
        CrashLogPut("\nlr:     ");
        CrashLogHex(uiLr);
        CrashLogPut("\nfp:     ");
        CrashLogHex(uiFp);
        CrashLogPut("\nsp:     ");
        CrashLogHex(uiSp);
        CrashLogPut("\n");

        // First argument registers: a null pointer here (file handle, entity)
        // is the usual cause of engine crashes after a failed file open.
#if defined(__aarch64__)
        CrashLogPut("x0:     ");
        CrashLogHex((uintptr_t)pContext->uc_mcontext.regs[0]);
        CrashLogPut("\nx1:     ");
        CrashLogHex((uintptr_t)pContext->uc_mcontext.regs[1]);
        CrashLogPut("\nx2:     ");
        CrashLogHex((uintptr_t)pContext->uc_mcontext.regs[2]);
        CrashLogPut("\n");
#elif defined(__arm__)
        CrashLogPut("r0:     ");
        CrashLogHex((uintptr_t)pContext->uc_mcontext.arm_r0);
        CrashLogPut("\nr1:     ");
        CrashLogHex((uintptr_t)pContext->uc_mcontext.arm_r1);
        CrashLogPut("\nr2:     ");
        CrashLogHex((uintptr_t)pContext->uc_mcontext.arm_r2);
        CrashLogPut("\n");
#endif

        CrashLogPut("base libGTASA: ");
        CrashLogHex(g_libGTASA);
        CrashLogPut("\nbase libsamp:  ");
        CrashLogHex(g_libSAMP);
        CrashLogPut("\n");

        if (g_libGTASA) { CrashLogPut("pc-libGTASA: "); CrashLogHex(uiPc - g_libGTASA); CrashLogPut("\n"); }
        if (g_libSAMP)  { CrashLogPut("pc-libsamp:  "); CrashLogHex(uiPc - g_libSAMP);  CrashLogPut("\n"); }
        if (g_libGTASA) { CrashLogPut("lr-libGTASA: "); CrashLogHex(uiLr - g_libGTASA); CrashLogPut("\n"); }
        if (g_libSAMP)  { CrashLogPut("lr-libsamp:  "); CrashLogHex(uiLr - g_libSAMP);  CrashLogPut("\n"); }

        // The caller symbol names the engine function that made the bad access.
        CrashLogPut("pc-symbol:  ");
        CrashLogSymbol(uiPc);
        CrashLogPut("lr-symbol:  ");
        CrashLogSymbol(uiLr);
    }

    CrashLogPut("build:      ");
    CrashLogPut(__DATE__);
    CrashLogPut(" ");
    CrashLogPut(__TIME__);
    CrashLogPut("\n");

    CrashLogPut("rle-guard:  hook=");
    CrashLogHex((uintptr_t)(long)g_rleGuardHookResult);
    CrashLogPut(" calls=");
    CrashLogHex((uintptr_t)g_rleGuardCalls);
    CrashLogPut(" padded=");
    CrashLogHex((uintptr_t)g_rleGuardPads);
    CrashLogPut("\n");

    CrashLogPut("========================\n");
}

// Hand the signal to whoever owned it before us (the AML crash logger on
// device), so its symbolised report is still produced. Handler chaining is
// skipped for SIG_DFL/SIG_IGN.
static void CrashChainPrevious(int iSignum, siginfo_t* pInfo, void* pContextPtr, struct sigaction* pPrevious)
{
    if (!pPrevious) return;

    if (pPrevious->sa_flags & SA_SIGINFO)
    {
        if (pPrevious->sa_sigaction &&
            pPrevious->sa_sigaction != (void*)SIG_DFL &&
            pPrevious->sa_sigaction != (void*)SIG_IGN)
        {
            pPrevious->sa_sigaction(iSignum, pInfo, pContextPtr);
        }
    }
    else if (pPrevious->sa_handler &&
             pPrevious->sa_handler != SIG_DFL &&
             pPrevious->sa_handler != SIG_IGN)
    {
        pPrevious->sa_handler(iSignum);
    }
}

// Restore the default disposition and return: the faulting instruction runs
// again and terminates the process instead of looping through this handler.
static void CrashRestoreDefault(int iSignum)
{
    struct sigaction dfl;
    memset(&dfl, 0, sizeof(dfl));
    dfl.sa_handler = SIG_DFL;
    sigemptyset(&dfl.sa_mask);
    sigaction(iSignum, &dfl, nullptr);
}

struct sigaction act_old;
struct sigaction act1_old;
struct sigaction act2_old;
struct sigaction act3_old;

extern int g_iLastProcessedSkinCollision, g_iLastProcessedEntityCollision, g_iLastRenderedObject;
extern uintptr_t g_dwLastRetAddrCrash;
void handler(int signum, siginfo_t *info, void* contextPtr)
{
    CrashLogReport(signum, info, (ucontext_t*)contextPtr);
    CrashChainPrevious(signum, info, contextPtr, &act_old);
    CrashRestoreDefault(signum);
}

void handler1(int signum, siginfo_t *info, void* contextPtr)
{
    CrashLogReport(signum, info, (ucontext_t*)contextPtr);
    CrashChainPrevious(signum, info, contextPtr, &act1_old);
    CrashRestoreDefault(signum);
}

void handler2(int signum, siginfo_t *info, void* contextPtr)
{
    CrashLogReport(signum, info, (ucontext_t*)contextPtr);
    CrashChainPrevious(signum, info, contextPtr, &act2_old);
    CrashRestoreDefault(signum);
}

void handler3(int signum, siginfo_t *info, void* contextPtr)
{
    CrashLogReport(signum, info, (ucontext_t*)contextPtr);
    CrashChainPrevious(signum, info, contextPtr, &act3_old);
    CrashRestoreDefault(signum);
}


void ClearModelCache()
{
	if(g_pszStorage)
	{
		char path[0xFF];
		sprintf(path, "%smodels/MINFO.BIN", g_pszStorage);
		std::remove(path);
		sprintf(path, "%sCINFO.BIN", g_pszStorage);
		std::remove(path);
	}
}

void DoInitStuff()
{
    // The GTA engine can start while Activity.onCreate is still configuring
    // the Java UI, storage and settings. Publish those before constructing CNetGame.
    if (!g_javaReady.load(std::memory_order_acquire) || !pGame || !pUI || !g_pszStorage) return;
	if (bGameInited == false)
	{
		ClearModelCache();

		pPlayerTags = new CPlayerTags();
		pSnapShotHelper = new CSnapShotHelper();
		pMaterialTextGenerator = new MaterialTextGenerator();
		pAudioStream = new CAudioStream();
		pAudioStream->Initialize();

		pUI->splashscreen()->setVisible(false);
		pUI->chat()->setVisible(true);
		pUI->buttonpanel()->setVisible(true);

		pGame->Initialize();
		pGame->SetMaxStats();
		pGame->ToggleThePassingOfTime(false);

		// voice
		LogVoice("[dbg:samp:load] : module loading...");

		for (const auto& loadCallback : Samp::loadCallbacks) {
			if (loadCallback != nullptr) {
				loadCallback();
			}
		}

		Samp::loadStatus = true;

		LogVoice("[dbg:samp:load] : module loaded");

		if (bDebug)
		{
            CCamera& TheCamera = *reinterpret_cast<CCamera*>(g_libGTASA + (VER_x32 ? 0x00951FA8 : 0xBBA8D0));
            //TheCamera.Restore();
            CCamera::SetBehindPlayer();
			pGame->DisplayHUD(true);
			pGame->EnableClock(false);

			DoDebugStuff();
		}

		bGameInited = true;
	}

	if (!bNetworkInited && !bDebug)
	{
		ReadSettingFile();

		pNetGame = new CNetGame(pSettings->Get().szHost, pSettings->Get().iPort, pSettings->Get().szNickName, pSettings->Get().szPassword/*g_pass*/);/*cryptor::create("H0ND4").decrypt()*//*""*/
		bNetworkInited = true;

        FLog("DoInitStuff end");
	}
}

extern "C" {
	JNIEXPORT void JNICALL Java_com_sampmobile_xyvern_game_SAMP_initializeSAMP(JNIEnv *pEnv, jobject thiz, jstring path)
	{
		const char *str = pEnv->GetStringUTFChars(path, NULL);
		if (str != NULL) {
			if (g_pszStorage == NULL) {
				g_pszStorage = strdup(str);
			}
			pEnv->ReleaseStringUTFChars(path, str);
		}

		pJavaWrapper = new CJavaWrapper(pEnv, thiz);
        g_javaReady.store(true, std::memory_order_release);
	}
	JNIEXPORT void JNICALL Java_com_sampmobile_xyvern_game_SAMP_onInputEnd(JNIEnv *pEnv, jobject thiz, jbyteArray str)
	{
		if(pUI)
		{
			pUI->keyboard()->sendForGB(pEnv, thiz, str);
		}
	}
	JNIEXPORT void JNICALL Java_com_sampmobile_xyvern_game_SAMP_onEventBackPressed(JNIEnv *pEnv, jobject thiz)
	{
		if(pSettings)
		{
			if(pSettings->Get().iAndroidKeyboard)
				pJavaWrapper->HideKeyboard();
		}
	}
	JNIEXPORT void JNICALL Java_com_sampmobile_xyvern_game_ui_dialog_DialogManager_sendDialogResponse(JNIEnv* pEnv, jobject thiz, jint i3, jint i, jint i2, jbyteArray str)
	{
		jboolean isCopy = true;

		jbyte* pMsg = pEnv->GetByteArrayElements(str, &isCopy);
		jsize length = pEnv->GetArrayLength(str);

		std::string szStr((char*)pMsg, length);
		
		if(pNetGame) {
			pNetGame->SendDialogResponse(i, i3, i2, (char*)szStr.c_str());
			//pGame->FindPlayerPed()->TogglePlayerControllableWithoutLock(true);
		}

		pEnv->ReleaseByteArrayElements(str, pMsg, JNI_ABORT);
	}
	JNIEXPORT void JNICALL
	Java_com_sampmobile_xyvern_cef_CefClientManager_nativeOnBrowserInit(
			JNIEnv* env,
			jobject thiz,
			jboolean isSuccess,
			jint errorCode)
	{
		cef::log(
			"nativeOnBrowserInit: success=%d errorCode=%d",
			isSuccess ? 1 : 0,
			(int)errorCode
		);
		cef::sendBrowserInit(isSuccess != JNI_FALSE, (int)errorCode);
	}

	JNIEXPORT void JNICALL
	Java_com_sampmobile_xyvern_cef_CefClientManager_nativeSendClientEvent(
			JNIEnv* env,
			jobject thiz,
			jstring eventName,
			jstring eventDataJson)
	{
		if (eventName == nullptr || eventDataJson == nullptr)
			return;

		const char* name = env->GetStringUTFChars(eventName, nullptr);
		const char* data = env->GetStringUTFChars(eventDataJson, nullptr);

		if (name && data)
		{
			cef::sendClientEvent(name, data);
		}

		if (name)
			env->ReleaseStringUTFChars(eventName, name);

		if (data)
			env->ReleaseStringUTFChars(eventDataJson, data);
	}
	JNIEXPORT void JNICALL
	Java_com_sampmobile_xyvern_cef_CefClientManager_nativeSetJavaManager(
			JNIEnv* env,
			jobject thiz,
			jobject javaManager)
	{
		cef::setJavaManager(env, javaManager);
	}
}

void MainLoop()
{
	if (!pGame || pGame->bIsGameExiting) return;

	DoInitStuff();

	if (bDebug) {
		DoDebugLoop();
	}

	if (pNetGame) {
		pNetGame->Process();
	}

	if (pAudioStream) {
		pAudioStream->Process();
	}

}

bool InitGui()
{
	// new voice
	if (pSettings)
	{
		PluginConfig::SetMicroEnable(pSettings->Get().bVoiceChatEnable);
		PluginConfig::SetSoundEnable(pSettings->Get().bVoiceChatEnable);
	}

	Plugin::OnPluginLoad();
	Plugin::OnSampLoad();

    std::string font_path = string_format("Textures/fonts/%s", FONT_NAME);
    std::vector<char> fontData = LoadAsset(font_path.c_str());

    if (!fontData.empty()) {
        pUI = new UI(ImVec2(RsGlobal->maximumWidth, RsGlobal->maximumHeight), fontData);
        LOGI("Loaded font from assets: %s (size: %zu)", font_path.c_str(), fontData.size());
    } else {
        // Preserve the installed font when present. Only fall back to a bundled
        // font when the optional downloaded font is missing.
        std::string file_path = g_pszStorage ? string_format("%sfonts/%s", g_pszStorage, FONT_NAME) : "";
        FILE* fontFile = file_path.empty() ? nullptr : fopen(file_path.c_str(), "rb");
        if (fontFile) {
            fclose(fontFile);
            pUI = new UI(ImVec2(RsGlobal->maximumWidth, RsGlobal->maximumHeight), file_path);
        } else {
            fontData = LoadAsset("Fonts/HELVETICANEUELTCYR-MDCOND.TTF");
            pUI = new UI(ImVec2(RsGlobal->maximumWidth, RsGlobal->maximumHeight), fontData);
            LOGI("Optional game UI font missing; using bundled or default font");
        }
    }
    if (!pUI->initialize()) {
        LOGE("SA-MP UI initialization failed");
        delete pUI;
        pUI = nullptr;
        return false;
    }
	pUI->performLayout();
    return true;
}

#include "game/multitouch.h"
#include "shadowhook/patch.h"
#include "util/CUtil.h"

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	javaVM = vm;
	LOGI("SA-MP library loaded! Build time: " __DATE__ " " __TIME__);

	// Cache JNI refs for the 0.3.DL HTTPS downloader while we still have the
	// app class loader context. Worker threads must never FindClass themselves.
	{
		JNIEnv* env = nullptr;
		if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK && env) {
			DLManager_InitJniCache(env);
		}
	}

	g_libGTASA = CUtil::FindLib("libGTASA.so");
	if (g_libGTASA == 0x00) {
		LOGE("libGTASA.so address was not found! ");
		return JNI_VERSION_1_6;
	}

	g_libSAMP = CUtil::FindLib("libsamp.so");
	if (g_libSAMP == 0x00) {
		LOGE("libsamp.so address was not found! ");
		return JNI_VERSION_1_6;
	}

	firebase::crashlytics::Initialize();

	uintptr_t libgtasa = CUtil::FindLib("libGTASA.so");
	uintptr_t libsamp = CUtil::FindLib("libsamp.so");
	uintptr_t libc = CUtil::FindLib("libc.so");

	FLog("libGTASA.so: 0x%x", libgtasa);
	FLog("libsamp.so: 0x%x", libsamp);
	FLog("libc.so: 0x%x", libc);

	char str[100];

	sprintf(str, "0x%x", libgtasa);
	firebase::crashlytics::SetCustomKey("libGTASA.so", str);
	
	sprintf(str, "0x%x", libsamp);
	firebase::crashlytics::SetCustomKey("libsamp.so", str);

	sprintf(str, "0x%x", libc);
	firebase::crashlytics::SetCustomKey("libc.so", str);

	CHook::InitHookStuff();
	InstallSpecialHooks();
	ApplyPatches_level0();
    InitRenderWareFunctions();
    MultiTouch::initialize();

	pGame = new CGame();

	//pVoice = new CVoice();
	//pVoice->Initialize(VOICE_FREQUENCY, CODEC_FREQUENCY, VOICE_SENDRRATE);

	//pthread_t thread;
	//pthread_create(&thread, 0, Init, 0);

	struct sigaction act;
	act.sa_sigaction = handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &act, &act_old);

	struct sigaction act1;
	act1.sa_sigaction = handler1;
	sigemptyset(&act1.sa_mask);
	act1.sa_flags = SA_SIGINFO;
	sigaction(SIGABRT, &act1, &act1_old);

	struct sigaction act2;
	act2.sa_sigaction = handler2;
	sigemptyset(&act2.sa_mask);
	act2.sa_flags = SA_SIGINFO;
	sigaction(SIGFPE, &act2, &act2_old);

	struct sigaction act3;
	act3.sa_sigaction = handler3;
	sigemptyset(&act3.sa_mask);
	act3.sa_flags = SA_SIGINFO;
	sigaction(SIGBUS, &act3, &act3_old);
		
	return JNI_VERSION_1_6;
}

uint32_t GetTickCount()
{
    return CTimer::m_snTimeInMillisecondsNonClipped;
}

void FLog(const char* fmt, ...)
{
	char buffer[1024];
	static FILE* flLog = nullptr;
	const char* pszStorage = g_pszStorage;

	if (flLog == nullptr && pszStorage != nullptr)
	{
		char path[512];
		snprintf(path, sizeof(path), "%s/samp_log.txt", pszStorage);
		flLog = fopen(path, "a");
	}

	va_list arg;
	va_start(arg, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, arg);
	va_end(arg);

	LOGI("%s", buffer);

	if (flLog != nullptr) {
		fprintf(flLog, "%s\n", buffer);
		fflush(flLog);
	}
}

void crashlyticsLog(const char* fmt, ...)
{
	char buffer[1024];
	static FILE* flLog = nullptr;
	const char* pszStorage = g_pszStorage;

	if (flLog == nullptr && pszStorage != nullptr)
	{
		char path[512];
		snprintf(path, sizeof(path), "%s/crashlytics_log.txt", pszStorage);
		flLog = fopen(path, "a");
	}

	va_list arg;
	va_start(arg, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, arg);
	va_end(arg);

	LOGI("%s", buffer);
	firebase::crashlytics::Log(buffer);

	if (flLog != nullptr) {
		fprintf(flLog, "%s\n", buffer);
		fflush(flLog);
	}
}

void ChatLog(const char* fmt, ...)
{
	char buffer[1024];
	static FILE* flLog = nullptr;
	const char* pszStorage = g_pszStorage;

	if (flLog == nullptr && pszStorage != nullptr)
	{
		char path[512];
		snprintf(path, sizeof(path), "%s/chat_log.txt", pszStorage);
		flLog = fopen(path, "a");
	}

	va_list arg;
	va_start(arg, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, arg);
	va_end(arg);

	if (flLog != nullptr) {
		fprintf(flLog, "%s\n", buffer);
		fflush(flLog);
	}
}

void MyLog(const char* fmt, ...)
{
	char buffer[0xFF];
	static FILE* flLog = nullptr;
	const char* pszStorage = g_pszStorage;


	if (flLog == nullptr && pszStorage != nullptr)
	{
		sprintf(buffer, "%s/samp_log.txt", pszStorage);
		//LOGI("buffer: %s", buffer);
		flLog = fopen(buffer, "a");
	}

	memset(buffer, 0, sizeof(buffer));

	va_list arg;
	va_start(arg, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, arg);
	va_end(arg);

	if (flLog == nullptr) return;
	fprintf(flLog, "%s\n", buffer);
	fflush(flLog);

	return;
}

void MyLog2(const char* fmt, ...)
{
	char buffer[0xFF];
	static FILE* flLog = nullptr;
	const char* pszStorage = g_pszStorage;


	if (flLog == nullptr && pszStorage != nullptr)
	{
		sprintf(buffer, "%s/samp_log.txt", pszStorage);
		//LOGI("buffer: %s", buffer);
		flLog = fopen(buffer, "a");
	}

	memset(buffer, 0, sizeof(buffer));

	va_list arg;
	va_start(arg, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, arg);
	va_end(arg);

	if (pUI) pUI->chat()->addDebugMessage(buffer);

	if (flLog == nullptr) return;
	fprintf(flLog, "%s\n", buffer);
	fflush(flLog);
	return;
}

void LogVoice(const char* fmt, ...)
{
	char buffer[0xFF];
	static FILE* flLog = nullptr;
	const char* pszStorage = g_pszStorage;

	if (flLog == nullptr && pszStorage != nullptr)
	{
		sprintf(buffer, "%sSAMP/%s", pszStorage, SV::kLogFileName);
		flLog = fopen(buffer, "w");
	}

	memset(buffer, 0, sizeof(buffer));

	va_list arg;
	va_start(arg, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, arg);
	va_end(arg);

	__android_log_write(ANDROID_LOG_INFO, "AXL", buffer);

	if (flLog == nullptr) return;
	fprintf(flLog, "%s\n", buffer);
	fflush(flLog);

	return;
}

AAssetManager* g_pAssetManager = nullptr;

extern "C"
JNIEXPORT void JNICALL
Java_com_sampmobile_xyvern_game_SAMP_initAssetManager(JNIEnv *env, jobject thiz, jobject assetManager) {
    g_pAssetManager = AAssetManager_fromJava(env, assetManager);
}