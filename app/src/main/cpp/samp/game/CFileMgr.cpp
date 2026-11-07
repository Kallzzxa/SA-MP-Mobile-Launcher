//
// Created by x1y2z on 14.04.2023.
//

#include "CFileMgr.h"
#include "main.h"
#include "../vendor/shadowhook/patch.h"
#include <android/asset_manager.h>
#include <stdio.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>

extern AAssetManager* g_pAssetManager;

static int asset_read(void* cookie, char* buf, int size) {
    return AAsset_read((AAsset*)cookie, buf, size);
}
static int asset_write(void* cookie, const char* buf, int size) {
    return -1; // Read only
}
static fpos_t asset_seek(void* cookie, fpos_t offset, int whence) {
    return AAsset_seek((AAsset*)cookie, offset, whence);
}
static int asset_close(void* cookie) {
    AAsset_close((AAsset*)cookie);
    return 0;
}

#include <ctype.h>
#include <string>
#include <algorithm>

static AAsset* OpenAssetCompat(const char* path) {
    // 1. Try exact match
    AAsset* asset = AAssetManager_open(g_pAssetManager, path, AASSET_MODE_UNKNOWN);
    if (asset) return asset;

    // 2. Try lowercase
    std::string lowerPath = path;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
    asset = AAssetManager_open(g_pAssetManager, lowerPath.c_str(), AASSET_MODE_UNKNOWN);
    if (asset) return asset;
    
    // 3. Try "Text" prefix normalization (common case "TEXT/..." -> "Text/...")
    if (strncasecmp(path, "TEXT/", 5) == 0) {
        std::string textPath = "Text/" + std::string(path + 5);
        // Try exact filename after Text/
        asset = AAssetManager_open(g_pAssetManager, textPath.c_str(), AASSET_MODE_UNKNOWN);
        if (asset) return asset;
        
        // Try lowercase filename after Text/
        std::string lowerFilename = path + 5;
        std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), ::tolower);
        textPath = "Text/" + lowerFilename;
        asset = AAssetManager_open(g_pAssetManager, textPath.c_str(), AASSET_MODE_UNKNOWN);
        if (asset) return asset;
    }

    return nullptr;
}

// Find only an unambiguous case variant of the required audio file. Keep this
// lookup consistent with GameDataVerifier; it never creates or changes data.
static bool AsciiNameEquals(const char* actual, const char* expected) {
    while (*actual && *expected) {
        unsigned char c = static_cast<unsigned char>(*actual++);
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != static_cast<unsigned char>(*expected++)) return false;
    }
    return *actual == 0 && *expected == 0;
}

static bool FindUniqueBankSlot(std::string& found) {
    std::string root(g_pszStorage);
    if (!root.empty() && root.back() != '/') root += '/';
    DIR* audioDirs = opendir(root.c_str());
    if (!audioDirs) return false;
    unsigned matches = 0;
    struct dirent* audio;
    while ((audio = readdir(audioDirs)) != nullptr) {
        if (!AsciiNameEquals(audio->d_name, "audio")) continue;
        std::string audioPath = root + audio->d_name + "/";
        DIR* configDirs = opendir(audioPath.c_str());
        if (!configDirs) continue;
        struct dirent* config;
        while ((config = readdir(configDirs)) != nullptr) {
            if (!AsciiNameEquals(config->d_name, "config")) continue;
            std::string configPath = audioPath + config->d_name + "/";
            DIR* files = opendir(configPath.c_str());
            if (!files) continue;
            struct dirent* file;
            while ((file = readdir(files)) != nullptr) {
                if (!AsciiNameEquals(file->d_name, "bankslot.dat")) continue;
                std::string candidate = configPath + file->d_name;
                struct stat info;
                if (stat(candidate.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) continue;
                if (++matches == 1) found = candidate;
            }
            closedir(files);
        }
        closedir(configDirs);
    }
    closedir(audioDirs);
    return matches == 1;
}

// Resolve a storage-relative path by matching each component
// case-insensitively. Android storage is case sensitive while game data packs
// are frequently shipped with mixed-case directory names, which used to turn
// an existing file into a failed open and then into a null handle crash.
static bool ResolvePathCaseInsensitive(const char* relativePath, std::string& resolved) {
    if (!g_pszStorage || !relativePath) return false;

    std::string out(g_pszStorage);
    if (!out.empty() && out.back() != '/') out += '/';

    const char* cursor = relativePath;
    while (*cursor == '/') cursor++;

    while (*cursor) {
        const char* slash = strchr(cursor, '/');
        size_t length = slash ? static_cast<size_t>(slash - cursor) : strlen(cursor);
        if (length == 0) break;

        std::string wanted(cursor, length);
        std::transform(wanted.begin(), wanted.end(), wanted.begin(), ::tolower);

        bool found = false;
        DIR* directory = opendir(out.c_str());
        if (!directory) return false;
        struct dirent* entry;
        while ((entry = readdir(directory)) != nullptr) {
            if (!AsciiNameEquals(entry->d_name, wanted.c_str())) continue;
            out += entry->d_name;
            found = true;
            break;
        }
        closedir(directory);
        if (!found) return false;

        if (!slash) break;
        out += '/';
        cursor = slash + 1;
        while (*cursor == '/') cursor++;
    }

    resolved = out;
    return true;
}

// Report what a failing directory actually contains once, so an incomplete or
// mismatched data pack can be diagnosed from the log alone.
static void LogDirectorySample(const char* relativePath) {
    static unsigned reported = 0;
    if (reported >= 8 || !g_pszStorage || !relativePath) return;

    std::string parent(relativePath);
    size_t slash = parent.find_last_of('/');
    if (slash == std::string::npos) return;
    parent.erase(slash);

    std::string directory(g_pszStorage);
    if (!directory.empty() && directory.back() != '/') directory += '/';
    directory += parent;

    DIR* dir = opendir(directory.c_str());
    if (!dir) {
        FLog("Directory missing: %s", directory.c_str());
        reported++;
        return;
    }

    std::string listing;
    struct dirent* entry;
    unsigned shown = 0;
    while ((entry = readdir(dir)) != nullptr && shown < 12) {
        if (entry->d_name[0] == '.' && entry->d_name[1] == 0) continue;
        if (entry->d_name[0] == '.' && entry->d_name[1] == '.' && entry->d_name[2] == 0) continue;
        if (!listing.empty()) listing += ", ";
        listing += entry->d_name;
        shown++;
    }
    closedir(dir);

    FLog("Directory %s contains: %s%s", directory.c_str(), listing.c_str(),
         shown >= 12 ? ", ..." : "");
    reported++;
}

void CFileMgr::SetDir(const char *path) {
    ( ( void(*)(const char *path) )(g_libGTASA + (VER_x32 ? 0x003F0C54 + 1 : 0x4D293C)) )(path);
}

FILE* CFileMgr::OpenFile(const char *path, const char *mode) {
    if (!path || !mode) return nullptr;
    // Check for Text or fonts in assets (Case insensitive)
    if (g_pAssetManager && (strcasestr(path, "Text/") || strcasestr(path, "fonts/") || strcasestr(path, "scache") || strcasestr(path, "stream.ini") || strcasestr(path, "version.txt"))) {
        AAsset* asset = OpenAssetCompat(path);
        if (asset) {
            FILE* assetFile = funopen(asset, asset_read, asset_write, asset_seek, asset_close);
            if (assetFile) {
                // LOGI("Opened asset: %s", path);
                return assetFile;
            }
            AAsset_close(asset);
        }
    }

    if (!g_pszStorage) return nullptr;
    int pathLength = snprintf(ms_path, sizeof(ms_path), "%s%s", g_pszStorage, path);
    if (pathLength < 0 || static_cast<size_t>(pathLength) >= sizeof(ms_path)) {
        FLog("File path too long");
        return nullptr;
    }

    auto file = fopen(ms_path, mode);

    // Some game data uses this mixed-case audio path on case-sensitive storage.
    if (!file && strcmp(path, "AUDIO/CONFIG/BANKSLOT.DAT") == 0 && strcmp(mode, "rb") == 0) {
        char alternate[sizeof(ms_path)];
        int length = snprintf(alternate, sizeof(alternate), "%s%s", g_pszStorage, "audio/CONFIG/BankSlot.dat");
        if (length >= 0 && static_cast<size_t>(length) < sizeof(alternate))
            file = fopen(alternate, mode);
        if (!file) {
            std::string found;
            if (FindUniqueBankSlot(found)) file = fopen(found.c_str(), mode);
        }
    }

    if (!file) {
        std::string resolved;
        if (ResolvePathCaseInsensitive(path, resolved))
            file = fopen(resolved.c_str(), mode);
    }

    if(!file) {
        LogDirectorySample(path);
    }

    if(!file) {
        if (!strstr(ms_path, ".idx") && !strstr(ms_path, "gta_sa.set") && !strstr(ms_path, "gtasatelem.set") && !strstr(ms_path, "gta3.ini") && !strstr(ms_path, "CINFO.BIN") && !strstr(ms_path, "MINFO.BIN") && !strstr(ms_path, "GTASAsf") && !strstr(ms_path, "Adjustable.cfg")) {
            FLog("Fail open file %s", ms_path);
        }
    }
    return file;
}

int32_t CFileMgr::CloseFile(FILE* file) {
    return fclose(file);
}

void CFileMgr::Initialise() {
    CHook::CallFunction<void>("_ZN8CFileMgr10InitialiseEv"); // ������� ����� �����
}
