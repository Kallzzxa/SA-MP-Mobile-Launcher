// Host tests for the parts of HopePride that do not need the engine: the
// Indonesian interface table and the safe mode marker probe.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <cstdint>
#include <string>

uintptr_t g_libGTASA = 0;
char* g_pszStorage = nullptr;

void FLog(const char*, ...) {}

#include "../../app/src/main/cpp/samp/game/Localization.h"
#include "../../app/src/main/cpp/samp/game/SafeMode.h"

static int g_failures = 0;

static void Check(bool condition, const char* what)
{
    printf("%s  %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) g_failures++;
}

// The engine passes UTF-16 as uint16_t; u"" literals are char16_t.
static const uint16_t* U(const char16_t* text)
{
    return reinterpret_cast<const uint16_t*>(text);
}

static bool Equals(const uint16_t* translated, const char16_t* expected)
{
    if (!translated) return false;
    return std::u16string(reinterpret_cast<const char16_t*>(translated)) == std::u16string(expected);
}

int main()
{
    Localization::Initialise();

    // --- Indonesian interface table -------------------------------------
    Check(Localization::Translate(U(u"Возобновить")) != nullptr, "russian menu label is translated");
    Check(Equals(Localization::Translate(U(u"Возобновить")), u"Lanjutkan"), "Возобновить -> Lanjutkan");
    Check(Equals(Localization::Translate(U(u"возобновить")), u"Lanjutkan"), "matching ignores case");
    Check(Equals(Localization::Translate(U(u"ВОЗОБНОВИТЬ")), u"Lanjutkan"), "matching ignores case (upper)");
    Check(Equals(Localization::Translate(U(u"Назад")), u"Kembali"), "Назад -> Kembali");
    Check(Localization::Translate(U(u"Ivan_Petrov: privet vsem")) == nullptr, "chat line is never rewritten");
    Check(Localization::Translate(U(u"[SERVER] Welcome to HopePride")) == nullptr, "server message is never rewritten");
    Check(Localization::Translate(U(u"")) == nullptr, "empty string is left alone");

    const unsigned long long before = Localization::UnmatchedCyrillicCount();
    Localization::Translate(U(u"Совершенно новое слово"));
    const bool logged = Localization::UnmatchedCyrillicCount() == before + 1;
    Check(logged, "unknown russian string is counted once for the log");

    Localization::Translate(U(u"Совершенно новое слово"));
    Check(Localization::UnmatchedCyrillicCount() == before + 1, "same unknown string is counted only once");

    Check(Localization::TranslatedCount() >= 4, "translation counter advances");

    // --- Safe mode marker ------------------------------------------------
    char path[] = "/tmp/hopepride_safe_mode_XXXXXX";
    const int fd = mkstemp(path);
    Check(fd >= 0, "temporary marker file created");
    if (fd >= 0) close(fd);

    setenv("HOPEPRIDE_SAFE_MODE_MARKER", path, 1);
    Check(SafeMode::IsEnabled(), "safe mode engages when the marker exists");
    Check(std::strcmp(SafeMode::ActiveMarker(), path) == 0, "reported marker is the one that was found");

    unlink(path);
    printf("\n%s (%d failure(s))\n", g_failures == 0 ? "ALL PASS" : "FAILURES", g_failures);
    return g_failures == 0 ? 0 : 1;
}
