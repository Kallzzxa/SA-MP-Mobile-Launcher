//
// Texture database variant fallback. See TextureVariantFallback.h.
//

#include "TextureVariantFallback.h"
#include "../common.h"
#include "../../main.h"
#include "../../vendor/shadowhook/patch.h"
#include "TextureDatabase.h"

#include <unistd.h>
#include <cstdio>

namespace
{

typedef bool (*LoadThumbsFn)(void* thiz, int format, bool setEntries);

LoadThumbsFn orig_LoadThumbs = nullptr;

volatile unsigned long long g_texVariantFallbacks = 0;
volatile unsigned long long g_texVariantLoadFailures = 0;

// The engine asks for one variant only; these are tried in order of how widely
// the mobile renderer supports them when the requested one is not on disk.
const int kVariantOrder[] =
{
    DF_UNC,     // plain RGBA, always decodable, no RLE step
    DF_ETC,     // ETC1, present in most mobile data sets
    DF_DXT,     // S3TC / DXT
    DF_PVR,     // PowerVR
    DF_360,     // Xbox 360 packed
    DF_PS3,     // PS3 packed
};

const char* SuffixForFormat(int format)
{
    switch (format)
    {
        case DF_UNC: return "unc";
        case DF_DXT: return "dxt";
        case DF_360: return "360";
        case DF_PS3: return "ps3";
        case DF_PVR: return "pvr";
        case DF_ETC: return "etc";
        default:     return nullptr;
    }
}

// TextureDatabase lays out the vtable first, so the database name follows it.
const char* DatabaseName(void* thiz)
{
    if (!thiz) return nullptr;

    const uintptr_t offset = sizeof(uintptr_t);
    const char* name = *reinterpret_cast<const char**>(reinterpret_cast<uintptr_t>(thiz) + offset);
    if (!name) return nullptr;

    // Only accept something that looks like a plain directory name, so a wrong
    // layout cannot send us into a wild pointer.
    for (int i = 0; i < 64; i++)
    {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (c == 0) return (i > 0) ? name : nullptr;
        if (c < 0x20 || c > 0x7E || c == '/' || c == '\\') return nullptr;
    }
    return nullptr;
}

bool VariantExists(const char* database, int format)
{
    const char* suffix = SuffixForFormat(format);
    if (!suffix || !database || !g_pszStorage) return false;

    char path[512];
    const int written = snprintf(path, sizeof(path), "%s/texdb/%s/%s.%s.tmb",
                                 g_pszStorage, database, database, suffix);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(path)) return false;

    return access(path, R_OK) == 0;
}

int PickExistingVariant(const char* database, int requested)
{
    if (VariantExists(database, requested)) return requested;

    for (int candidate : kVariantOrder)
    {
        if (candidate == requested) continue;
        if (VariantExists(database, candidate)) return candidate;
    }

    return requested;   // nothing on disk: leave the engine's own error path intact
}

bool LoadThumbsHook(void* thiz, int format, bool setEntries)
{
    static thread_local bool inHook = false;
    if (!orig_LoadThumbs) return false;
    if (inHook) return orig_LoadThumbs(thiz, format, setEntries);

    inHook = true;

    const char* database = DatabaseName(thiz);
    int chosen = format;

    if (database)
    {
        chosen = PickExistingVariant(database, format);
        if (chosen != format)
        {
            g_texVariantFallbacks++;
            FLog("texdb '%s': variant %d (%s) not on disk, loading variant %d (%s) instead",
                 database, format, SuffixForFormat(format) ? SuffixForFormat(format) : "auto",
                 chosen, SuffixForFormat(chosen) ? SuffixForFormat(chosen) : "auto");
        }
        else
        {
            FLog("texdb '%s': variant %d (%s) available", database, format,
                 SuffixForFormat(format) ? SuffixForFormat(format) : "auto");
        }
    }

    const bool result = orig_LoadThumbs(thiz, chosen, setEntries);

    if (!result)
    {
        g_texVariantLoadFailures++;
        FLog("texdb '%s': variant %d failed to load", database ? database : "?", chosen);
    }

    inHook = false;
    return result;
}

} // namespace

void TextureVariantFallback::Initialise()
{
    CHook::InlineHook("_ZN15TextureDatabase10LoadThumbsE21TextureDatabaseFormatb",
                      LoadThumbsHook, &orig_LoadThumbs);

    FLog("TextureVariantFallback: LoadThumbs hook %s",
         orig_LoadThumbs ? "installed" : "not installed (symbol missing)");
}
