// IMG v2 archive packer for 0.3.DL custom models.
// See dl_imgpack.h for format documentation.

#include "dl_imgpack.h"

#include <cstdio>
#include <cstring>
#include <vector>
#include <sys/stat.h>

namespace DLImgPack {

namespace {

// Write a little-endian uint32 regardless of host endianness.
void WriteU32LE(FILE* f, uint32_t v) {
    uint8_t b[4];
    b[0] = static_cast<uint8_t>(v & 0xFF);
    b[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    b[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    b[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
    fwrite(b, 1, 4, f);
}

// Reject names that could escape the archive or break the 24-byte field.
bool IsSafeArcName(const std::string& name) {
    if (name.empty() || name.size() >= IMG_MAX_NAME) return false;
    if (name.find('/') != std::string::npos) return false;
    if (name.find('\\') != std::string::npos) return false;
    if (name.find("..") != std::string::npos) return false;
    for (char c : name) {
        if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E)
            return false;
    }
    return true;
}

} // namespace

bool PackImgV2(const std::vector<ImgEntry>& entries,
               const std::string& outPath,
               std::string& outError) {
    if (entries.empty()) {
        outError = "no entries to pack";
        return false;
    }
    if (entries.size() > IMG_MAX_ENTRIES) {
        outError = "too many entries";
        return false;
    }

    // Validate entries and compute sector layout first (no partial writes).
    struct Layout {
        std::string srcPath;
        std::string arcName;
        uint64_t fileSize;
        uint32_t sectorOffset;
        uint32_t sectorCount;
    };
    std::vector<Layout> layout;
    layout.reserve(entries.size());

    uint64_t totalBytes = 0;
    // Directory = 8-byte header + 32 bytes per entry; data starts at the
    // first sector after the directory sectors.
    uint64_t dirBytes = 8 + (uint64_t)entries.size() * 32;
    uint32_t dirSectors = static_cast<uint32_t>(
        (dirBytes + IMG_SECTOR_SIZE - 1) / IMG_SECTOR_SIZE);
    uint32_t nextSector = dirSectors;

    for (const auto& e : entries) {
        if (!IsSafeArcName(e.arcName)) {
            outError = "unsafe archive name: " + e.arcName;
            return false;
        }
        struct stat st;
        if (stat(e.srcPath.c_str(), &st) != 0 || st.st_size <= 0) {
            outError = "cannot stat source file: " + e.srcPath;
            return false;
        }
        uint64_t fsize = static_cast<uint64_t>(st.st_size);
        // Sector count with overflow guard.
        uint64_t sectors64 = (fsize + IMG_SECTOR_SIZE - 1) / IMG_SECTOR_SIZE;
        if (sectors64 > 0xFFFFFFFFu) {
            outError = "file too large: " + e.srcPath;
            return false;
        }
        uint32_t sectors = static_cast<uint32_t>(sectors64);
        // Check nextSector + sectors doesn't overflow uint32.
        if (nextSector > 0xFFFFFFFFu - sectors) {
            outError = "archive too large (sector overflow)";
            return false;
        }
        totalBytes += static_cast<uint64_t>(sectors) * IMG_SECTOR_SIZE;
        if (totalBytes > IMG_MAX_TOTAL) {
            outError = "archive exceeds size cap";
            return false;
        }
        layout.push_back({e.srcPath, e.arcName, fsize, nextSector,
                          static_cast<uint32_t>(sectors)});
        nextSector += sectors;
    }

    // Write to a temp file first, then atomic rename (never leave half an IMG).
    std::string tmpPath = outPath + ".tmp";
    FILE* out = fopen(tmpPath.c_str(), "wb");
    if (!out) {
        outError = "cannot open output file";
        return false;
    }

    bool ok = true;

    // Build the full directory blob in memory, then write sector by sector.
    // Blob = 8-byte header ("VER2" + count LE) followed by 32-byte entries.
    {
        size_t blobSize = 8 + layout.size() * 32;
        std::vector<uint8_t> blob(blobSize, 0);
        memcpy(blob.data(), "VER2", 4);
        uint32_t count = static_cast<uint32_t>(layout.size());
        blob[4] = static_cast<uint8_t>(count & 0xFF);
        blob[5] = static_cast<uint8_t>((count >> 8) & 0xFF);
        blob[6] = static_cast<uint8_t>((count >> 16) & 0xFF);
        blob[7] = static_cast<uint8_t>((count >> 24) & 0xFF);
        for (size_t i = 0; i < layout.size(); ++i) {
            const auto& l = layout[i];
            uint8_t* e = blob.data() + 8 + i * 32;
            e[0] = static_cast<uint8_t>(l.sectorOffset & 0xFF);
            e[1] = static_cast<uint8_t>((l.sectorOffset >> 8) & 0xFF);
            e[2] = static_cast<uint8_t>((l.sectorOffset >> 16) & 0xFF);
            e[3] = static_cast<uint8_t>((l.sectorOffset >> 24) & 0xFF);
            e[4] = static_cast<uint8_t>(l.sectorCount & 0xFF);
            e[5] = static_cast<uint8_t>((l.sectorCount >> 8) & 0xFF);
            e[6] = static_cast<uint8_t>((l.sectorCount >> 16) & 0xFF);
            e[7] = static_cast<uint8_t>((l.sectorCount >> 24) & 0xFF);
            memcpy(e + 8, l.arcName.c_str(), l.arcName.size());
        }
        // Write dirSectors sectors (blob zero-padded to sector boundary).
        uint8_t sector[IMG_SECTOR_SIZE];
        size_t blobPos = 0;
        for (uint32_t s = 0; ok && s < dirSectors; ++s) {
            memset(sector, 0, sizeof(sector));
            size_t copy = blobSize - blobPos;
            if (copy > IMG_SECTOR_SIZE) copy = IMG_SECTOR_SIZE;
            if (copy > 0) memcpy(sector, blob.data() + blobPos, copy);
            blobPos += copy;
            if (fwrite(sector, 1, IMG_SECTOR_SIZE, out) != IMG_SECTOR_SIZE) ok = false;
        }
    }

    // File data sectors.
    uint8_t buf[32768];
    for (const auto& l : layout) {
        if (!ok) break;
        FILE* in = fopen(l.srcPath.c_str(), "rb");
        if (!in) {
            outError = "cannot open source file: " + l.srcPath;
            ok = false;
            break;
        }
        uint64_t remaining = l.fileSize;
        while (remaining > 0) {
            size_t chunk = remaining > sizeof(buf) ? sizeof(buf) : (size_t)remaining;
            size_t got = fread(buf, 1, chunk, in);
            if (got == 0) {
                outError = "short read: " + l.srcPath;
                ok = false;
                break;
            }
            if (fwrite(buf, 1, got, out) != got) {
                outError = "write failed";
                ok = false;
                break;
            }
            remaining -= got;
        }
        fclose(in);
        if (!ok) break;
        // Pad to sector boundary.
        uint64_t written = l.fileSize;
        uint64_t padded = (uint64_t)l.sectorCount * IMG_SECTOR_SIZE;
        if (padded > written) {
            uint8_t zero[IMG_SECTOR_SIZE];
            memset(zero, 0, sizeof(zero));
            uint64_t padLeft = padded - written;
            while (padLeft > 0) {
                size_t chunk = padLeft > sizeof(zero) ? sizeof(zero) : (size_t)padLeft;
                if (fwrite(zero, 1, chunk, out) != chunk) {
                    outError = "padding write failed";
                    ok = false;
                    break;
                }
                padLeft -= chunk;
            }
        }
    }

    fclose(out);

    if (!ok) {
        remove(tmpPath.c_str());
        if (outError.empty()) outError = "pack failed";
        return false;
    }

    // Atomic replace.
    if (rename(tmpPath.c_str(), outPath.c_str()) != 0) {
        remove(tmpPath.c_str());
        outError = "rename failed";
        return false;
    }
    return true;
}

} // namespace DLImgPack
