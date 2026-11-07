//
// Central localisation for the built in game interface.
//
// Everything the engine draws in its own menus goes through
// CFont::PrintString(float, float, GxtChar*), where GxtChar is UTF-16. That
// makes one hook the single place where interface text can be replaced, no
// matter which menu builds the string. Dynamic content (player names, chat,
// server messages, textdraws) is left untouched: it is never in the table.
//
// Unknown Cyrillic strings are logged once each so the table can be extended
// from a real log instead of guesswork.
//

#include "../main.h"
#include "Localization.h"
#include "../vendor/shadowhook/patch.h"

namespace
{

struct Entry
{
    const char16_t* from;   // engine text (Russian or English)
    const char16_t* to;     // Indonesian
};

// ---------------------------------------------------------------------------
// Interface dictionary.
// ---------------------------------------------------------------------------
const Entry kDictionary[] =
{
    // Pause menu / main menu, Russian build.
    { u"Возобновить",                      u"Lanjutkan" },
    { u"Назад",                            u"Kembali" },
    { u"Нажмите для просмотра карты",      u"Tekan untuk melihat peta" },
    { u"Новая игра",                       u"Permainan Baru" },
    { u"Начать новую игру",                u"Mulai Permainan Baru" },
    { u"Загрузить игру",                   u"Muat Permainan" },
    { u"Сохранить игру",                   u"Simpan Permainan" },
    { u"Удалить игру",                     u"Hapus Permainan" },
    { u"Настройки",                        u"Pengaturan" },
    { u"Возможности",                      u"Pengaturan" },
    { u"Экран",                            u"Tampilan" },
    { u"Настр. экрана",                    u"Atur Tampilan" },
    { u"Звук",                             u"Suara" },
    { u"Настр. звука",                     u"Atur Suara" },
    { u"Управление",                       u"Kontrol" },
    { u"Настр. управления",                u"Atur Kontrol" },
    { u"Игрок",                            u"Pemain" },
    { u"Язык",                             u"Bahasa" },
    { u"Статистика",                       u"Statistik" },
    { u"Карта",                            u"Peta" },
    { u"Краткое содержание",               u"Ringkasan" },
    { u"Выход",                            u"Keluar" },
    { u"Выход из игры",                    u"Keluar dari Permainan" },
    { u"Пауза",                            u"Jeda" },
    { u"Продолжить",                       u"Lanjutkan" },
    { u"Да",                               u"Ya" },
    { u"Нет",                              u"Tidak" },
    { u"Принять",                          u"Terima" },
    { u"Отмена",                           u"Batal" },
    { u"Готово",                           u"Selesai" },
    { u"Закрыть",                          u"Tutup" },
    { u"Нажмите, чтобы продолжить",        u"Tekan untuk melanjutkan" },
    { u"Нажмите для выбора",               u"Tekan untuk memilih" },
    { u"Нажмите, чтобы вернуться",         u"Tekan untuk kembali" },
    { u"Загрузка...",                      u"Memuat..." },

    // Multiplayer wording used by the interface.
    { u"Мультиплеер",                      u"Multipemain" },
    { u"Подключиться",                     u"Hubungkan" },
    { u"Отключиться",                      u"Putuskan Koneksi" },
    { u"Серверы",                          u"Server" },
    { u"Время игры",                       u"Waktu Bermain" },

    // Same screens on an English data set.
    { u"Resume",                           u"Lanjutkan" },
    { u"Back",                             u"Kembali" },
    { u"Press to view map",                u"Tekan untuk melihat peta" },
    { u"Press to view the map",            u"Tekan untuk melihat peta" },
    { u"New Game",                         u"Permainan Baru" },
    { u"Start New Game",                   u"Mulai Permainan Baru" },
    { u"Start Game",                       u"Mulai Permainan" },
    { u"Load Game",                        u"Muat Permainan" },
    { u"Save Game",                        u"Simpan Permainan" },
    { u"Delete Game",                      u"Hapus Permainan" },
    { u"Options",                          u"Pengaturan" },
    { u"Settings",                         u"Pengaturan" },
    { u"Display Settings",                 u"Pengaturan Tampilan" },
    { u"Audio Settings",                   u"Pengaturan Suara" },
    { u"Controller Setup",                 u"Pengaturan Kontrol" },
    { u"Player Setup",                     u"Pengaturan Pemain" },
    { u"Language",                         u"Bahasa" },
    { u"Statistics",                       u"Statistik" },
    { u"Map",                              u"Peta" },
    { u"Brief",                            u"Ringkasan" },
    { u"Quit Game",                        u"Keluar dari Permainan" },
    { u"Exit Game",                        u"Keluar dari Permainan" },
    { u"Quit",                             u"Keluar" },
    { u"Pause",                            u"Jeda" },
    { u"Paused",                           u"Jeda" },
    { u"Yes",                              u"Ya" },
    { u"No",                               u"Tidak" },
    { u"Accept",                           u"Terima" },
    { u"Cancel",                           u"Batal" },
    { u"Done",                             u"Selesai" },
    { u"Close",                            u"Tutup" },
    { u"Loading...",                       u"Memuat..." },
    { u"Press to continue",                u"Tekan untuk melanjutkan" },
    { u"Press to select",                  u"Tekan untuk memilih" },
    { u"Multiplayer",                      u"Multipemain" },
    { u"Connect",                          u"Hubungkan" },
    { u"Disconnect",                       u"Putuskan Koneksi" },
    { u"Play Time",                        u"Waktu Bermain" },
    { u"Waiting",                          u"Menunggu" },
    { u"Spawn",                            u"Muncul" },
    { u"Press to change game mode",        u"Tekan untuk mengganti mode permainan" },
};

constexpr size_t kDictionarySize = sizeof(kDictionary) / sizeof(kDictionary[0]);
constexpr uint32_t kMaxTextLength = 64;

uint32_t g_dictionaryHash[kDictionarySize] = {};
uint16_t g_dictionaryLength[kDictionarySize] = {};

typedef void (*PrintStringFn)(float posX, float posY, const uint16_t* text);

PrintStringFn orig_PrintString = nullptr;

volatile unsigned long long g_translatedCount = 0;
volatile unsigned long long g_unmatchedCyrillicCount = 0;

// Distinct unmatched strings already reported, so samp_log stays readable.
constexpr uint32_t kReportedMax = 96;
uint32_t g_reportedHash[kReportedMax] = {};
uint32_t g_reportedCount = 0;

uint32_t HashText(const uint16_t* text, uint32_t length)
{
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < length; i++)
    {
        hash ^= text[i];
        hash *= 16777619u;
    }
    return hash;
}

uint32_t TextLength(const uint16_t* text, uint32_t limit)
{
    uint32_t length = 0;
    while (length < limit && text[length] != 0) length++;
    return length;
}

bool HasCyrillic(const uint16_t* text, uint32_t length)
{
    for (uint32_t i = 0; i < length; i++)
        if (text[i] >= 0x0400 && text[i] <= 0x04FF) return true;
    return false;
}

// Only used for diagnostics: BMP to UTF-8 (Latin, Cyrillic, punctuation).
void AppendUtf8(char* out, size_t capacity, size_t& used, uint32_t code)
{
    if (code < 0x80)
    {
        if (used + 1 < capacity) out[used++] = static_cast<char>(code);
    }
    else if (code < 0x800)
    {
        if (used + 2 < capacity)
        {
            out[used++] = static_cast<char>(0xC0 | (code >> 6));
            out[used++] = static_cast<char>(0x80 | (code & 0x3F));
        }
    }
    else
    {
        if (used + 3 < capacity)
        {
            out[used++] = static_cast<char>(0xE0 | (code >> 12));
            out[used++] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out[used++] = static_cast<char>(0x80 | (code & 0x3F));
        }
    }
}

void ReportUnmatched(const uint16_t* text, uint32_t length, uint32_t hash)
{
    g_unmatchedCyrillicCount++;
    if (length == 0 || length > kMaxTextLength) return;

    for (uint32_t i = 0; i < g_reportedCount; i++)
        if (g_reportedHash[i] == hash) return;

    if (g_reportedCount >= kReportedMax) return;
    g_reportedHash[g_reportedCount++] = hash;

    char utf8[kMaxTextLength * 3 + 1] = {};
    size_t used = 0;
    for (uint32_t i = 0; i < length; i++) AppendUtf8(utf8, sizeof(utf8), used, text[i]);
    utf8[used] = 0;

    FLog("UI-UNTRANSLATED: %s", utf8);
}

const char16_t* Lookup(const uint16_t* text, uint32_t length, uint32_t hash)
{
    for (size_t i = 0; i < kDictionarySize; i++)
    {
        if (g_dictionaryLength[i] != length || g_dictionaryHash[i] != hash) continue;

        const char16_t* candidate = kDictionary[i].from;
        uint32_t j = 0;
        while (j < length && static_cast<uint16_t>(candidate[j]) == text[j]) j++;
        if (j == length) return kDictionary[i].to;
    }
    return nullptr;
}

void PrintStringHook(float posX, float posY, const uint16_t* text)
{
    if (!orig_PrintString) return;

    if (const uint16_t* translated = Localization::Translate(text))
    {
        g_translatedCount++;
        orig_PrintString(posX, posY, translated);
        return;
    }

    orig_PrintString(posX, posY, text);
}

} // namespace

void Localization::Initialise()
{
    for (size_t i = 0; i < kDictionarySize; i++)
    {
        const char16_t* from = kDictionary[i].from;
        uint32_t length = 0;
        while (length <= kMaxTextLength && from[length] != 0) length++;
        g_dictionaryLength[i] = static_cast<uint16_t>(length);
        g_dictionaryHash[i] = HashText(reinterpret_cast<const uint16_t*>(from), length);
    }

    const uintptr_t printString = g_libGTASA + (VER_x32 ? 0x005AA200 + 1 : 0x6CDEB0);
    CHook::InlineHook(printString, PrintStringHook, &orig_PrintString);

    FLog("Localization: Indonesian table with %u entries, font hook %s",
         (unsigned)kDictionarySize, orig_PrintString ? "installed" : "not installed");
}

const uint16_t* Localization::Translate(const uint16_t* gxt)
{
    if (!gxt) return nullptr;

    const uint32_t length = TextLength(gxt, kMaxTextLength + 1);
    if (length == 0 || length > kMaxTextLength) return nullptr;

    const uint32_t hash = HashText(gxt, length);
    if (const char16_t* translated = Lookup(gxt, length, hash))
        return reinterpret_cast<const uint16_t*>(translated);

    // Nothing in the table: it is dynamic content (names, chat, textdraws) and
    // stays as it is, but a Cyrillic interface string is worth reporting so the
    // table can be completed from a real log.
    if (HasCyrillic(gxt, length)) ReportUnmatched(gxt, length, hash);

    return nullptr;
}

unsigned long long Localization::TranslatedCount()
{
    return g_translatedCount;
}

unsigned long long Localization::UnmatchedCyrillicCount()
{
    return g_unmatchedCyrillicCount;
}
