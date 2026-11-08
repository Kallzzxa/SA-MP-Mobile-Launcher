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

// Mixing variants is what breaks the database: LoadThumbs would hand the engine
// the thumb list of variant B while the runtime object was created for variant
// A, and the resulting entries carry null fields - which is exactly the crash
// still reported (SortEntries, fault 0x58, x1 = 0) in a build that logged
// "variant-fallback=1". The engine's own choice is therefore always honoured;
// the hook only reports what is on disk so the data set can be judged.
constexpr bool kAllowVariantSwitch = false;

int PickExistingVariant(const char* database, int requested)
{
    if (requested >= DF_Default) return requested;

    VariantFiles requestedFiles;
    ProbeVariant(database, requested, requestedFiles);

    if (requestedFiles.Any()) return requested;

    for (int candidate : kVariantOrder)
    {
        if (candidate == requested) continue;

        VariantFiles candidateFiles;
        if (!ProbeVariant(database, candidate, candidateFiles)) continue;
        if (!candidateFiles.Complete()) continue;

        if (requestedFiles.dat && !candidateFiles.dat) continue;
        if (requestedFiles.toc && !candidateFiles.toc) continue;

        if (kAllowVariantSwitch)
        {
            g_texVariantFallbacks++;
            FLog("texdb '%s': variant %d is absent, using variant %d", database, requested, candidate);
            return candidate;
        }

        FLog("texdb '%s': variant %d is absent, variant %d would be available (switch disabled)",
             database, requested, candidate);
        return requested;
    }

    return requested;
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

// One shot dump of what the installed data set actually contains, database by
// database and variant by variant. This is the evidence needed to tell a
// missing variant apart from a corrupt one, without guessing.
void LogDatabaseInventory()
{
    if (!g_pszStorage)
    {
        FLog("texdb inventory: storage not resolved yet, skipped");
        return;
    }

    const char* databases[] = { "samp", "mobile", "txd", "gta3", "gta_int", "cutscene", "player", "menu" };

    for (const char* database : databases)
    {
        char line[512] = {};
        size_t used = 0;
        bool found = false;

        for (int format : kVariantOrder)
        {
            VariantFiles files;
            if (!ProbeVariant(database, format, files)) continue;
            if (!files.Any()) continue;

            found = true;
            const int written = snprintf(line + used, sizeof(line) - used, " %s(tmb=%d dat=%d toc=%d)",
                                         SuffixForFormat(format) ? SuffixForFormat(format) : "?",
                                         (int)files.tmb, (int)files.dat, (int)files.toc);
            if (written <= 0 || static_cast<size_t>(written) >= sizeof(line) - used) break;
            used += static_cast<size_t>(written);
        }

        if (found) FLog("texdb '%s' variants:%s", database, line);
        else       FLog("texdb '%s': no variant files on disk", database);
    }
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

    LogDatabaseInventory();

    FLog("TextureVariantFallback: LoadThumbs hook %s, Load hook %s, force variant=%d",
         orig_LoadThumbs ? "installed" : "not installed (symbol missing)",
         orig_Load ? "installed" : "not installed (symbol missing)",
         (int)kForceVariant);
    FLog("TextureVariantFallback: variant switching %s",
         kAllowVariantSwitch ? "enabled" : "disabled (log only)");
    FLog("TextureVariantFallback: SortEntries guard %s",
         orig_SortEntries ? "installed" : "not installed (symbol missing)");
}
