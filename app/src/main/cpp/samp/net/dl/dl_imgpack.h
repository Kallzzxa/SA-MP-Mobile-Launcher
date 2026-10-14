#pragma once

// IMG v2 archive packer for 0.3.DL custom models.
// Packs downloaded DFF/TXD files into a GTA SA "VER2" IMG archive so the
// game's streaming system (CStreaming::AddImageToList) can load them.
//
// IMG v2 layout (verified against real game IMG):
//   offset 0: "VER2" (4 bytes)
//   offset 4: entry count (uint32 LE)
//   each entry (32 bytes): offset_sectors (u32 LE), size_sectors (u32 LE),
//                          name[24] (null-padded)
//   file data follows, 2048-byte sector aligned, first entry at sector 2.

#include <cstdint>
#include <string>
#include <vector>

namespace DLImgPack {

// One file to pack: source path on disk + name inside the archive.
struct ImgEntry {
    std::string srcPath;   // absolute path to the file to pack
    std::string arcName;   // name inside IMG (max 23 chars + NUL)
};

static const size_t IMG_SECTOR_SIZE = 2048;
static const size_t IMG_MAX_NAME    = 24;   // incl. NUL
static const size_t IMG_MAX_ENTRIES = 4096; // sanity cap
static const uint64_t IMG_MAX_TOTAL = 512ULL * 1024ULL * 1024ULL; // 512 MB cap

// Packs files into a VER2 IMG at outPath.
// Returns true on success, false on any error (outError filled).
// Security: validates name length, file sizes (no overflow), total size cap,
// and rejects path traversal in arc names.
bool PackImgV2(const std::vector<ImgEntry>& entries,
               const std::string& outPath,
               std::string& outError);

} // namespace DLImgPack
