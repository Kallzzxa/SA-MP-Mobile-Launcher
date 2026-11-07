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

// Read by the native crash handler, so these need external linkage.
volatile unsigned long long g_texVariantFallbacks = 0;
volatile unsigned long long g_texVariantLoadFailures = 0;

namespace
{

typedef bool (*LoadThumbsFn)(void* thiz, int format, bool setEntries);

LoadThumbsFn orig_LoadThumbs = nullptr;

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

// Forcing a different variant than the engine asked for is NOT safe: doing that
// for the "samp" database made the engine load unc data under the metadata of
// the requested variant and it died inside TextureDatabaseRuntime::SortEntries
// with a null entry (crash_native: fault 0x58, x1 = 0). The engine is only
// switched to another variant when the requested one is genuinely absent and
// the replacement carries a matching file set.
constexpr bool kForceVariant = false;

struct VariantFiles
{
    bool tmb = false;
    bool dat = false;
    bool toc = false;
    bool Any() const { return tmb || dat || toc; }
    bool Complete() const { return tmb; }
};

bool ProbeVariant(const char* database, int format, VariantFiles& files)
{
    files = VariantFiles{};
    const char* suffix = SuffixForFormat(format);
    if (!suffix || !database || !g_pszStorage) return false;

    const char* extensions[] = { "tmb", "dat", "toc" };
    bool* flags[] = { &files.tmb, &files.dat, &files.toc };

    for (int i = 0; i < 3; i++)
    {
        char path[512];
        const int written = snprintf(path, sizeof(path), "%s/texdb/%s/%s.%s.%s",
                                     g_pszStorage, database, database, suffix, extensions[i]);
        if (written <= 0 || static_cast<size_t>(written) >= sizeof(path)) continue;
        *flags[i] = access(path, R_OK) == 0;
    }

    return files.Any();
}

int PickExistingVariant(const char* database, int requested)
{
    if (kForceVariant && requested != DF_UNC && VariantExists(database, DF_UNC))
        return DF_UNC;

    VariantFiles requestedFiles;
    ProbeVariant(database, requested, requestedFiles);

    // Whatever form the requested variant exists in - complete or partial - it
    // belongs to the engine to judge. Only a variant that is completely absent
    // is replaced, because replacing a present variant is what produced the
    // empty entry array that crashed SortEntries.
    if (requestedFiles.Any()) return requested;

    for (int candidate : kVariantOrder)
    {
        if (candidate == requested) continue;

        VariantFiles candidateFiles;
        if (!ProbeVariant(database, candidate, candidateFiles)) continue;
        if (!candidateFiles.Complete()) continue;

        // Only accept a replacement that carries the same supporting files the
        // requested variant would have had; a lone thumbnail list with no data
        // file is what produces an empty entry array.
        if (requestedFiles.dat && !candidateFiles.dat) continue;
        if (requestedFiles.toc && !candidateFiles.toc) continue;

        FLog("texdb '%s': variant %d file set tmb=%d dat=%d toc=%d is incomplete, using %d",
             database, requested,
             (int)requestedFiles.tmb, (int)requestedFiles.dat, (int)requestedFiles.toc,
             candidate);
        return candidate;
    }

    return requested;   // nothing usable: leave the engine's own error path intact
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

// TextureDatabaseRuntime::Load(withName, fullyLoad, forcedFormat) runs before
// any database is touched, so it is the second chance to pick a variant that
// exists when the LoadThumbs hook is unavailable in this build.
typedef void* (*LoadFn)(const char* withName, bool fullyLoad, int forcedFormat);
LoadFn orig_Load = nullptr;

void* LoadHook(const char* withName, bool fullyLoad, int forcedFormat)
{
    if (!orig_Load) return nullptr;

    int chosen = forcedFormat;
    if (withName)
    {
        chosen = PickExistingVariant(withName, forcedFormat);
        if (chosen != forcedFormat)
        {
            g_texVariantFallbacks++;
            FLog("texdb '%s': load variant %d instead of %d", withName, chosen, forcedFormat);
        }
    }

    return orig_Load(withName, fullyLoad, chosen);
}

// ---------------------------------------------------------------------------
// TextureDatabaseRuntime::SortEntries(bool)
//
// crash_native.txt faults inside this function at address +0x58 with x1 = 0,
// i.e. while touching a null entry of the database it is sorting. The entry
// array comes from the database load, so a database that loaded with a
// mismatched variant (or not at all) hands the sort a container with null
// slots. The guard below inspects the container first and refuses to sort a
// container that would crash, which keeps the engine alive while leaving every
// healthy database sorted exactly as before.
// ---------------------------------------------------------------------------
typedef bool (*SortEntriesFn)(void* thiz, bool arg);

SortEntriesFn orig_SortEntries = nullptr;

// TextureDatabase layout: vtable, name, categories, entries, thumbs[6].
constexpr uintptr_t kEntryCountOffset64 = 36;
constexpr uintptr_t kEntryPtrOffset64 = 40;
constexpr uintptr_t kEntryCountOffset32 = 28;
constexpr uintptr_t kEntryPtrOffset32 = 32;

bool SortEntriesHook(void* thiz, bool arg)
{
    if (!orig_SortEntries) return false;
    if (!thiz)
    {
        FLog("SortEntries: null database, skipped");
        return false;
    }

    const uintptr_t base = reinterpret_cast<uintptr_t>(thiz);
    const char* name = DatabaseName(thiz);   // validated: never a wild read
    const uint32_t entryCount = *reinterpret_cast<uint32_t*>(base + (VER_x32 ? kEntryCountOffset32 : kEntryCountOffset64));
    void** entries = *reinterpret_cast<void***>(base + (VER_x32 ? kEntryPtrOffset32 : kEntryPtrOffset64));

    if (!entries)
    {
        FLog("SortEntries: '%s' has no entry array (count %u), skipped", name ? name : "?", entryCount);
        return false;
    }

    // A sane database never has more entries than this; anything above means
    // the container was built from mismatched metadata.
    if (entryCount > 500000u)
    {
        FLog("SortEntries: '%s' entry count %u looks corrupt, skipped", name ? name : "?", entryCount);
        return false;
    }

    uint32_t nullEntries = 0;
    for (uint32_t i = 0; i < entryCount; i++)
        if (!entries[i]) nullEntries++;

    if (nullEntries != 0)
    {
        FLog("SortEntries: '%s' has %u null entries of %u, sort skipped to avoid the crash",
             name ? name : "?", nullEntries, entryCount);
        return false;
    }

    return orig_SortEntries(thiz, arg);
}

} // namespace

void TextureVariantFallback::Initialise()
{
    CHook::InlineHook("_ZN15TextureDatabase10LoadThumbsE21TextureDatabaseFormatb",
                      LoadThumbsHook, &orig_LoadThumbs);

    CHook::InlineHook("_ZN22TextureDatabaseRuntime4LoadEPKcb21TextureDatabaseFormat",
                      LoadHook, &orig_Load);

    CHook::InlineHook("_ZN22TextureDatabaseRuntime11SortEntriesEb",
                      SortEntriesHook, &orig_SortEntries);

    FLog("TextureVariantFallback: LoadThumbs hook %s, Load hook %s, force variant=%d",
         orig_LoadThumbs ? "installed" : "not installed (symbol missing)",
         orig_Load ? "installed" : "not installed (symbol missing)",
         (int)kForceVariant);
    FLog("TextureVariantFallback: SortEntries guard %s",
         orig_SortEntries ? "installed" : "not installed (symbol missing)");
}
