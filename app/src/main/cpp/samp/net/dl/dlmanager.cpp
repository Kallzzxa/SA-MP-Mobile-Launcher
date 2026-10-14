#include "dlmanager.h"
#include "dl_imgpack.h"
#include "../../main.h"
#include "../../gui/gui.h"
#include "../../game/Streaming.h"
#include "../../game/Models/ModelInfo.h"
#include "../../vendor/imgui/imgui.h"
#include "../../vendor/raknet/SAMP/SAMPRPC.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <csignal>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <mutex>

extern UI* pUI;

// ---------------------------------------------------------------------------
// CRC32 (IEEE 802.3, reflected, init 0xFFFFFFFF, xorout 0xFFFFFFFF).
// Table init is once-only via call_once: the RakNet thread, the game thread and
// the worker pool all reach it (audit A9).
// ---------------------------------------------------------------------------
static uint32_t s_crcTable[256];
static std::once_flag s_crcOnce;

static void InitCRC32Table() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        s_crcTable[i] = c;
    }
}

static inline void EnsureCRC32Table() {
    std::call_once(s_crcOnce, InitCRC32Table);
}

uint32_t DLManager::CalculateCRC32(const void* data, size_t length) {
    EnsureCRC32Table();
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = s_crcTable[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

uint32_t DLManager::CalculateFileCRC32(const std::string& filepath) {
    FILE* fp = fopen(filepath.c_str(), "rb");
    if (!fp) return 0;
    EnsureCRC32Table();
    uint32_t crc = 0xFFFFFFFF;
    uint8_t buffer[16384];
    size_t bytesRead = 0;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        for (size_t i = 0; i < bytesRead; i++) {
            crc = s_crcTable[(crc ^ buffer[i]) & 0xFF] ^ (crc >> 8);
        }
    }
    fclose(fp);
    return crc ^ 0xFFFFFFFF;
}

static void MakeDirectoryTree(const std::string& path) {
    std::string current;
    for (size_t i = 0; i < path.length(); i++) {
        current += path[i];
        if (path[i] == '/' || i == path.length() - 1) {
            mkdir(current.c_str(), 0777); // EEXIST is fine; fopen reports hard failures
        }
    }
}

static uint32_t GetCurrentTick() {
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}

// DFF and TXD CRCs live in independent namespaces (audit A2): key tasks on both.
static inline uint64_t TaskKey(uint32_t crc, bool isDff) {
    return (static_cast<uint64_t>(crc) << 1) | (isDff ? 1u : 0u);
}

// ---------------------------------------------------------------------------
// URL parsing (audit B1/B2): recognise http/https + optional port. We cannot
// speak TLS in this tree, so https is rejected explicitly by the caller rather
// than silently connecting to a bogus host and burning retries.
// ---------------------------------------------------------------------------
struct ParsedUrl {
    bool        isHttps{false};
    std::string host;
    int         port{80};
    std::string path{"/"};
};

static bool ParseHttpUrl(const std::string& url, ParsedUrl& out) {
    std::string rest;
    if (url.rfind("http://", 0) == 0) {
        out.isHttps = false;
        out.port = 80;
        rest = url.substr(7);
    } else if (url.rfind("https://", 0) == 0) {
        out.isHttps = true;
        out.port = 443;
        rest = url.substr(8);
    } else {
        return false;
    }
    if (rest.empty()) return false;

    size_t slashPos = rest.find('/');
    if (slashPos == std::string::npos) {
        out.host = rest;
        out.path = "/";
    } else {
        out.path = rest.substr(slashPos);
        out.host = rest.substr(0, slashPos);
    }

    size_t colonPos = out.host.find(':');
    if (colonPos != std::string::npos) {
        std::string portStr = out.host.substr(colonPos + 1);
        out.host = out.host.substr(0, colonPos);
        char* end = nullptr;
        long p = strtol(portStr.c_str(), &end, 10);
        if (end && *end == '\0' && p > 0 && p <= 65535) out.port = static_cast<int>(p);
    }
    return !out.host.empty();
}

DLManager& DLManager::Instance() {
    static DLManager s_instance;
    return s_instance;
}

DLManager::DLManager()
    : m_serverPort(7777)
    , m_pRakClient(nullptr)
{
}

DLManager::~DLManager() {
    Reset();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void DLManager::Initialize(const std::string& serverIp, int serverPort,
                           const std::string& storagePath, RakClientInterface* pRakClient) {
    Reset();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_serverIp = serverIp;
    m_serverPort = serverPort;
    m_pRakClient = pRakClient;
    m_nextVirtualId = DL_VIRT_MIN;
    m_bVirtualIdExhausted = false;
    m_connectTime = GetCurrentTick();
    m_lastModelRequestTick = m_connectTime;
    m_bReportedNoManifest = false;
    m_bSawAnyModel = false;
    m_bFinishSent = false;

    std::string cleanStorage = storagePath;
    if (!cleanStorage.empty() && cleanStorage.back() != '/') {
        cleanStorage += "/";
    }

    char cacheDir[512];
    snprintf(cacheDir, sizeof(cacheDir), "%sSAMP/cache/%s.%d/",
             cleanStorage.c_str(), serverIp.c_str(), serverPort);
    {
        std::lock_guard<std::mutex> cl(m_cachePathMutex);
        m_cachePath = cacheDir;
    }
    MakeDirectoryTree(cacheDir);

    m_bShutdown = false;
    m_bInitialized = true;

    // Never let a TCP RST kill the process mid-download: without this, a
    // send() on a reset socket raises SIGPIPE and the OS terminates the app.
    // (FetchToFile's send() also uses MSG_NOSIGNAL; this is belt and braces.)
    // Idempotent across reconnects.
    signal(SIGPIPE, SIG_IGN);

    // Spawn the SA-MP PC style pool: DL_MAX_CONCURRENT parallel transfer slots.
    for (int i = 0; i < DL_MAX_CONCURRENT; ++i) {
        m_workers.emplace_back([this]() { WorkerLoop(); });
    }

    if (pUI && pUI->chat()) {
        pUI->chat()->addDebugMessage("Downloading custom models from %s:%d...", serverIp.c_str(), serverPort);
    }
}

void DLManager::Reset() {
    {
        std::lock_guard<std::mutex> lock(m_workMutex);
        m_bShutdown = true;
    }
    m_workCv.notify_all();

    for (auto& t : m_workers) {
        if (t.joinable()) t.join();
    }
    m_workers.clear();

    std::lock_guard<std::mutex> lock(m_mutex);
    // Null the RakClient (audit C2): the caller destroys it right after Reset(),
    // so a surviving outbox item must never dereference the old pointer.
    m_pRakClient = nullptr;
    m_serverIp.clear();
    m_serverPort = 7777;

    m_models.clear();
    m_tasks.clear();
    m_knownKey.clear();
    m_taskIndex.clear();
    m_rpcOutbox.clear();
    m_networkToVirtual.clear();
    m_virtualToNetwork.clear();
    m_nextVirtualId = DL_VIRT_MIN;
    m_bVirtualIdExhausted = false;
    m_registeredModels.clear();
    m_bModelsRegistered = false;
    m_bInitialized = false;
    m_bDownloadFinished = false;
    m_bFinishRequested = false;
    m_bFinishSent = false;
    m_bSawAnyModel = false;
    m_bShutdown = false;
    m_activeWorkers = 0;
    m_totalModels = 0;
    m_downloadedModels = 0;
    m_failedModels = 0;
    m_speedKBps = 0.0f;
    m_totalBytesRx = 0;
    m_lastSpeedSampleTick = 0;
    m_lastSpeedSampleBytes = 0;
    m_connectTime = 0;
    m_lastModelRequestTick = 0;
    m_finishTime = 0;
    m_bReportedNoManifest = false;
    m_rpc183DumpCount = 0;
    m_lastActivity.clear();

    {
        std::lock_guard<std::mutex> cl(m_cachePathMutex);
        m_cachePath.clear();
    }
    {
        std::lock_guard<std::mutex> cm(m_chatMutex);
        m_chatQueue.clear();
    }
}

// ---------------------------------------------------------------------------
// Paths & cache validation
// ---------------------------------------------------------------------------
std::string DLManager::GetModelPath(uint32_t crc, bool isDff) const {
    char filename[64];
    snprintf(filename, sizeof(filename), "0x%08X.%s", crc, isDff ? "dff" : "txd");
    std::string base;
    {
        std::lock_guard<std::mutex> lock(m_cachePathMutex);
        base = m_cachePath;
    }
    return base + filename;
}

bool DLManager::IsModelCached(uint32_t crc, uint32_t expectedSize, bool isDff) const {
    std::string path = GetModelPath(crc, isDff);
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    // CRC is the source of truth (audit A1). A size mismatch never deletes a
    // CRC-correct file and never forces a re-download; only a CRC mismatch does.
    if (CalculateFileCRC32(path) != crc) {
        unlink(path.c_str());
        return false;
    }
    return true;
}

bool DLManager::ValidateOrPurge(const std::string& path, uint32_t crc, uint32_t expectedSize) {
    (void)expectedSize;
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    if (CalculateFileCRC32(path) != crc) {
        unlink(path.c_str());
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Virtual model id remap (server ids -> safe internal CModelInfo slots)
// ---------------------------------------------------------------------------
int16_t DLManager::GetVirtualModelId(int32_t networkId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_networkToVirtual.find(networkId);
    if (it != m_networkToVirtual.end()) return it->second;

    // Do not wrap and alias a live slot (audit A8). On exhaustion, log once and
    // hand back -1 so the caller keeps the base id.
    if (m_nextVirtualId > DL_VIRT_MAX) {
        if (!m_bVirtualIdExhausted) {
            m_bVirtualIdExhausted = true;
            PushChat("Virtual model slot pool exhausted; extra models use their base id.");
        }
        return -1;
    }
    int16_t virtualId = m_nextVirtualId++;
    m_networkToVirtual[networkId] = virtualId;
    m_virtualToNetwork[virtualId] = networkId;
    return virtualId;
}

int32_t DLManager::GetNetworkModelId(int16_t virtualId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_virtualToNetwork.find(virtualId);
    if (it != m_virtualToNetwork.end()) return it->second;
    return -1; // sentinel: caller decides (audit A10)
}

bool DLManager::IsModelRegistered(int16_t virtualId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registeredModels.find(virtualId) != m_registeredModels.end();
}

// ---------------------------------------------------------------------------
// Custom model registration (0.3.DL PR #3).
// Packs downloaded DFF/TXD into a VER2 IMG, registers it with CStreaming,
// and allocates CModelInfo slots so the game can actually load the models.
// MUST run on the game thread (called from ProcessTick).
// ---------------------------------------------------------------------------
bool DLManager::RegisterCustomModels() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_bModelsRegistered) return !m_registeredModels.empty();
    }

    // Collect models that have both DFF and TXD files on disk.
    struct ModelFiles {
        int16_t virtualId;
        uint8_t type; // 1=Skin, 2=Object
        uint32_t dffCrc;
        uint32_t txdCrc;
        std::string dffPath;
        std::string txdPath;
        std::string dffArcName;
        std::string txdArcName;
    };
    std::vector<ModelFiles> toRegister;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& m : m_models) {
            if (m.virtualModelId < DL_VIRT_MIN || m.virtualModelId > DL_VIRT_MAX)
                continue;
            // Both files must exist (downloader CRC-validates on fetch).
            std::string dffPath = GetModelPath(m.dffChecksum, true);
            std::string txdPath = GetModelPath(m.txdChecksum, false);
            struct stat st;
            if (stat(dffPath.c_str(), &st) != 0) continue;
            if (stat(txdPath.c_str(), &st) != 0) continue;

            char dffName[32], txdName[32];
            snprintf(dffName, sizeof(dffName), "0x%08X.dff", m.dffChecksum);
            snprintf(txdName, sizeof(txdName), "0x%08X.txd", m.txdChecksum);
            toRegister.push_back({m.virtualModelId, m.type, m.dffChecksum,
                                  m.txdChecksum, dffPath, txdPath,
                                  dffName, txdName});
        }
    }

    if (toRegister.empty()) {
        PushChat("No custom models to register.");
        std::lock_guard<std::mutex> lock(m_mutex);
        m_bModelsRegistered = true;
        return false;
    }

    // Pack into IMG.
    std::string cacheBase;
    {
        std::lock_guard<std::mutex> lock(m_cachePathMutex);
        cacheBase = m_cachePath;
    }
    std::string imgPath = cacheBase + "custom_dl.img";

    std::vector<DLImgPack::ImgEntry> entries;
    entries.reserve(toRegister.size() * 2);
    for (const auto& mf : toRegister) {
        entries.push_back({mf.dffPath, mf.dffArcName});
        entries.push_back({mf.txdPath, mf.txdArcName});
    }

    std::string packError;
    if (!DLImgPack::PackImgV2(entries, imgPath, packError)) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to pack custom IMG: %s",
                 packError.c_str());
        PushChat(msg);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_bModelsRegistered = true;
        return false;
    }

    // Register IMG with the streaming system.
    // Note: path format follows the game's CdStreamOpen expectations.
    int imgIndex = CStreaming::AddImageToList(imgPath.c_str(), false);
    if (imgIndex < 0) {
        PushChat("Failed to register custom IMG with streaming (no free IMG slot).");
        std::lock_guard<std::mutex> lock(m_mutex);
        m_bModelsRegistered = true;
        return false;
    }
    // Verify the IMG was actually registered in the returned slot.
    // (Defense in depth: ensures ms_files[imgIndex] really holds our IMG,
    //  not a stale entry or an out-of-bounds index.)
    if (imgIndex < 0 || imgIndex >= (int)TOTAL_IMG_ARCHIVES ||
        strcmp(CStreaming::ms_files[imgIndex].m_szName, imgPath.c_str()) != 0) {
        PushChat("Custom IMG registration verification failed.");
        std::lock_guard<std::mutex> lock(m_mutex);
        m_bModelsRegistered = true;
        return false;
    }

    // Register each model slot.
    int registered = 0;
    int skippedFull = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& mf : toRegister) {
            CBaseModelInfo* pInfo = nullptr;
            if (mf.type == 1) { // Skin -> ped model
                pInfo = CModelInfo::AddPedModel(mf.virtualId);
            } else { // Object -> atomic model
                pInfo = CModelInfo::AddAtomicModel(mf.virtualId);
            }
            if (!pInfo) {
                // Store is full (shouldn't happen with enlarged capacities,
                // but never crash: skip this model, it will use fallback).
                ++skippedFull;
                continue;
            }

            // DFF name (without extension) for streaming lookup.
            // Matches the arc name "0x<CRC>.dff" so the IMG directory hash aligns.
            char dffBase[32], txdBase[32];
            snprintf(dffBase, sizeof(dffBase), "0x%08X", mf.dffCrc);
            snprintf(txdBase, sizeof(txdBase), "0x%08X", mf.txdCrc);
            pInfo->SetModelName(dffBase);
            pInfo->SetTexDictionary(txdBase, "custom_dl");

            m_registeredModels.insert(mf.virtualId);
            ++registered;
        }
        m_bModelsRegistered = true;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "Registered %d custom models.", registered);
    PushChat(msg);
    if (skippedFull > 0) {
        char warn[128];
        snprintf(warn, sizeof(warn),
                 "Warning: %d models skipped (model store full).",
                 skippedFull);
        PushChat(warn);
    }
    return registered > 0;
}

// ---------------------------------------------------------------------------
// Chat marshalling: safe from any thread, drained on the game thread in
// ProcessTick. Prevents interleaved chat writes from the 5 worker threads and
// from the RakNet callback thread (audit B14).
// ---------------------------------------------------------------------------
void DLManager::PushChat(const std::string& line) const {
    std::lock_guard<std::mutex> lock(m_chatMutex);
    if (m_chatQueue.size() < 256) m_chatQueue.push_back(line);
}

void DLManager::FlushChatQueue() {
    std::deque<std::string> local;
    {
        std::lock_guard<std::mutex> lock(m_chatMutex);
        if (m_chatQueue.empty()) return;
        local.swap(m_chatQueue);
    }
    if (!(pUI && pUI->chat())) return;
    for (const auto& line : local) {
        pUI->chat()->addDebugMessage("%s", line.c_str());
    }
}

int DLManager::Next183DumpIndex() {
    int c = m_rpc183DumpCount.load();
    if (c < 3) {
        return m_rpc183DumpCount.fetch_add(1);
    }
    return -1;
}

// ---------------------------------------------------------------------------
// RPC inbound: 179 ModelRequest (S->C) - one packet per registered model
// ---------------------------------------------------------------------------
void DLManager::OnModelRequest(RPCParameters* rpcParams) {
    if (!rpcParams || !rpcParams->input) return;

    unsigned char* data = reinterpret_cast<unsigned char*>(rpcParams->input);
    int bitLength = rpcParams->numberOfBitsOfData;
    // Exact 39-byte fixed payload (audit A3): no "+1" fudge, no truncated parse.
    if (bitLength < static_cast<int>(DL_RPC179_WIRE_BYTES * 8)) return;
    int byteLength = bitLength / 8;
    if (byteLength < static_cast<int>(DL_RPC179_WIRE_BYTES)) return;

    RakNet::BitStream bs(data, byteLength, false);
    DLModelInfo info{};
    bs.Read(info.poolID);
    bs.Read(info.count);
    bs.Read(info.type);
    bs.Read(info.virtualWorld);
    bs.Read(info.baseId);
    bs.Read(info.newId);
    bs.Read(info.dffChecksum);
    bs.Read(info.txdChecksum);
    bs.Read(info.dffSize);
    bs.Read(info.txdSize);
    bs.Read(info.timeOn);
    bs.Read(info.timeOff);

    if (info.type != 1 && info.type != 2) return; // 0 = undefined type: drop

    info.virtualModelId = GetVirtualModelId(info.newId);
    info.dffCached = IsModelCached(info.dffChecksum, info.dffSize, true);
    info.txdCached = IsModelCached(info.txdChecksum, info.txdSize, false);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_totalModels = info.count;
        m_models.push_back(info);
        m_bSawAnyModel = true;
        m_lastModelRequestTick = GetCurrentTick();

        // A manifest arriving after we already sent 184 (audit A6): re-arm.
        if (m_bFinishSent) {
            m_bFinishSent = false;
            m_bFinishRequested = false;
            m_bDownloadFinished = false;
        }

        auto enqueue = [&](uint32_t crc, uint32_t size, bool isDff) {
            if (crc == 0) return;
            uint64_t key = TaskKey(crc, isDff);   // audit A2: DFF/TXD namespaced
            if (m_knownKey.count(key)) return;
            m_knownKey.insert(key);
            DLFileTask t;
            t.crc = crc;
            t.size = size;
            t.isDff = isDff;
            char hex[48];
            snprintf(hex, sizeof(hex), "0x%08X.%s", crc, isDff ? "dff" : "txd");
            t.hexName = hex;
            m_tasks.push_back(t);
        };
        if (!info.dffCached) enqueue(info.dffChecksum, info.dffSize, true);
        if (!info.txdCached) enqueue(info.txdChecksum, info.txdSize, false);
    }

    // Wake the pool immediately: idle slots pick this up without any batching delay.
    m_workCv.notify_all();
}

// ---------------------------------------------------------------------------
// RPC inbound: 183 ModelFile (S->C) - download URL for one checksum
// ---------------------------------------------------------------------------
void DLManager::OnModelFile(RPCParameters* rpcParams) {
    if (!rpcParams || !rpcParams->input) return;

    unsigned char* data = reinterpret_cast<unsigned char*>(rpcParams->input);
    int bitLength = rpcParams->numberOfBitsOfData;
    if (bitLength < static_cast<int>(DL_RPC183_MIN_BYTES * 8)) return;
    int byteLength = bitLength / 8;

    RakNet::BitStream bs(data, byteLength, false);
    uint8_t  arg = 0;        // 6 = HTTP transport
    uint8_t  fileType = 0;   // 1 = DFF, 2 = TXD
    uint32_t fileChecksum = 0;
    uint8_t  urlLen = 0;

    bs.Read(arg);
    bs.Read(fileType);
    bs.Read(fileChecksum);
    bs.Read(urlLen);

    // Only the HTTP-URL prefix is handled here; in-band RakNet chunking (arg=1)
    // is a different transport and is out of scope.
    if (arg != 6) return;
    if (fileChecksum == 0) return;

    const bool isDff = (fileType == 1);

    std::string url;
    if (urlLen > 0) {
        // Bound the read by what actually arrived (audit A4): 4 (arg/type/crc) +
        // 1 (urlLen) bytes of prefix precede the URL body.
        const size_t available = (byteLength > 5) ? static_cast<size_t>(byteLength - 5) : 0u;
        size_t toRead = std::min<size_t>({ urlLen, available, DL_MAX_URL_BYTES });
        if (toRead > 0) {
            char urlBuffer[512] = {0};
            bs.Read(urlBuffer, toRead);
            urlBuffer[toRead] = '\0';
            url = urlBuffer;
        }
    }

    if (url.empty()) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& t : m_tasks) {
            if (t.crc == fileChecksum && t.isDff == isDff) {
                t.url = url;
                break;
            }
        }
    }
    m_workCv.notify_all();
}

// ---------------------------------------------------------------------------
// RPC inbound: 185 DownloadCompleted (S->C)
// ---------------------------------------------------------------------------
void DLManager::OnDownloadCompleted(RPCParameters* /*rpcParams*/) {
    m_bDownloadFinished = true;
    m_finishTime = GetCurrentTick();
}

// RAII guard: keeps m_activeWorkers paired even if the fetch throws.
// Without this, an exception would leak the counter and the UI would show
// phantom active workers forever.
struct ActiveWorkerGuard {
    explicit ActiveWorkerGuard(std::atomic<int>& counter) : m_counter(counter) {
        m_counter.fetch_add(1, std::memory_order_relaxed);
    }
    ~ActiveWorkerGuard() { m_counter.fetch_sub(1, std::memory_order_relaxed); }
    ActiveWorkerGuard(const ActiveWorkerGuard&) = delete;
    ActiveWorkerGuard& operator=(const ActiveWorkerGuard&) = delete;
private:
    std::atomic<int>& m_counter;
};

// ---------------------------------------------------------------------------
// Worker pool: each slot claims a file, asks the server for its URL through the
// RPC outbox, waits for RPC 183, fetches it over HTTP, validates CRC32 and
// commits it into the cache. Mirrors SA-MP PC's concurrent artwork transfers.
// ---------------------------------------------------------------------------
void DLManager::WorkerLoop() {
    while (!m_bShutdown.load()) {
        // 1. Claim the next unstarted file.
        uint32_t pickedCrc = 0;
        bool pickedIsDff = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& t : m_tasks) {
                if (t.done || t.started || t.failed) continue;
                t.started = true;
                pickedCrc = t.crc;
                pickedIsDff = t.isDff;
                break;
            }
        }

        if (pickedCrc == 0) {
            bool anyPending = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (const auto& t : m_tasks) {
                    if (!t.done && !t.failed) { anyPending = true; break; }
                }
            }
            // Predicate-ish wait with a bounded backstop (audit B12): no 50 Hz spin.
            std::unique_lock<std::mutex> lock(m_workMutex);
            m_workCv.wait_for(lock, std::chrono::milliseconds(anyPending ? 250 : 500));
            continue;
        }

        // 2-5. Per-package error boundary. NOTHING in here may escape the
        // worker thread as an exception: the build uses -fexceptions, so an
        // uncaught throw (e.g. std::bad_alloc under memory pressure while
        // pulling 325 files) calls std::terminate and the whole app
        // force-closes mid-download. A package that fails — even after
        // DL_MAX_ATTEMPTS — is marked failed/skipped: it NEVER blocks the
        // loop and NEVER kills the app.
        bool fetchOk = false;
        std::string fetchError;
        uint64_t bytes = 0;
        try {
            // 2. Request the URL from the server (RPC 181/182) via the outbox —
            // but skip the round-trip when a previous attempt already cached it.
            bool needRpc = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (const auto& t : m_tasks) {
                    if (t.crc != pickedCrc || t.isDff != pickedIsDff) continue;
                    needRpc = t.url.empty();
                    break;
                }
            }
            if (needRpc) {
                RequestFileFromServer(pickedCrc, pickedIsDff);
            }

            // 3. Wait (bounded) for RPC 183 to deliver the URL.
            std::string url;
            uint32_t expectedSize = 0;
            bool isDff = pickedIsDff;
            for (uint32_t waited = 0; waited < DL_URL_WAIT_MS && !m_bShutdown.load(); waited += 25) {
                bool gotUrl = false;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    for (auto& t : m_tasks) {
                        if (t.crc != pickedCrc || t.isDff != pickedIsDff) continue;
                        if (!t.url.empty()) {
                            url = t.url;
                            expectedSize = t.size;
                            isDff = t.isDff;
                            gotUrl = true;
                        }
                        break;
                    }
                }
                if (gotUrl) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }

            if (url.empty()) {
                fetchError = "no url (RPC 183 timeout)";
            } else {
                // 4. Fetch over HTTP(S). The guard keeps m_activeWorkers paired
                // even if FetchToFile throws.
                DLFileTask local;
                local.crc = pickedCrc;
                local.size = expectedSize;
                local.isDff = isDff;
                local.url = url;

                ActiveWorkerGuard activeGuard(m_activeWorkers);
                fetchOk = FetchToFile(local, bytes, fetchError);
                m_totalBytesRx += bytes;
            }
        } catch (const std::exception& e) {
            fetchOk = false;
            fetchError = std::string("c++ exception: ") + e.what();
        } catch (...) {
            fetchOk = false;
            fetchError = "unknown c++ exception";
        }

        // 5. Commit result. Failure (after DL_MAX_ATTEMPTS) marks the task
        // failed = skipped with its reason logged; the worker ALWAYS moves on
        // to the next package.
        std::string skipLog;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& t : m_tasks) {
                if (t.crc != pickedCrc || t.isDff != pickedIsDff) continue;
                if (fetchOk) {
                    t.done = true;
                    t.url.clear();
                } else if (++t.attempts >= DL_MAX_ATTEMPTS) {
                    t.failed = true;
                    t.lastError = fetchError;
                    m_failedModels++;
                    char msg[192];
                    snprintf(msg, sizeof(msg), "Skipped %s (%s)",
                             t.hexName.c_str(), fetchError.c_str());
                    skipLog = msg;
                } else {
                    t.started = false;
                    // t.url is never cleared on failure, so a retry keeps it
                    // and skips the RPC round-trip.
                    t.lastError = fetchError;
                }
                break;
            }
        }
        if (!skipLog.empty()) PushChat(skipLog);

        RecomputeProgress();
    }
}

// ---------------------------------------------------------------------------
// Cached JNI refs for DLHttpClient. Populated once by DLManager_InitJniCache,
// which MUST run on a thread with the app class loader context (JNI_OnLoad).
// Worker threads only read these — never FindClass for app classes there.
// ---------------------------------------------------------------------------
namespace {
jclass    g_dlHttpClientCls = nullptr;  // global ref
jmethodID g_dlHttpClientMid = nullptr; // downloadFile(Ljava/lang/String;Ljava/lang/String;J)I
}

void DLManager_InitJniCache(JNIEnv* env) {
    if (!env || g_dlHttpClientCls) return;  // already cached
    jclass local = env->FindClass("com/sampmobile/xyvern/game/DLHttpClient");
    if (!local) {
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
        return;
    }
    g_dlHttpClientCls = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    if (!g_dlHttpClientCls) return;
    g_dlHttpClientMid = env->GetStaticMethodID(g_dlHttpClientCls, "downloadFile",
        "(Ljava/lang/String;Ljava/lang/String;J)I");
    if (!g_dlHttpClientMid && env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}

// ---------------------------------------------------------------------------
// JNI HTTPS download: delegates https:// URLs to Android's HttpURLConnection
// via com.sampmobile.xyvern.game.DLHttpClient. The JVM's TLS stack (system
// trust store, full cert validation) is used — cert validation is NEVER
// disabled. Worker threads attach/detach around the call.
//
// NOTE: the jclass/jmethodID are cached as global refs by DLManager_InitJniCache
// (called from JNI_OnLoad). FindClass must NOT be called here: worker threads
// attached via AttachCurrentThread have no app class loader context, so
// FindClass returns NULL with a pending exception, and DetachCurrentThread
// with a pending exception aborts the VM — that was the download crash.
// Returns true and leaves a complete file at destPath on success.
// ---------------------------------------------------------------------------
static bool FetchViaJniHttps(const std::string& url, const std::string& destPath,
                             uint64_t maxBytes, std::string& outError) {
    if (!javaVM) {
        outError = "jni: no JVM";
        return false;
    }
    if (!g_dlHttpClientCls || !g_dlHttpClientMid) {
        outError = "jni: DLHttpClient refs not cached (JNI_OnLoad init missing)";
        return false;
    }

    JNIEnv* env = nullptr;
    bool attached = false;
    if (javaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (javaVM->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) {
            outError = "jni: attach failed";
            return false;
        }
        attached = true;
    }

    bool ok = false;
    jstring jUrl = env->NewStringUTF(url.c_str());
    jstring jPath = env->NewStringUTF(destPath.c_str());
    if (!jUrl || !jPath) {
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
        outError = "jni: NewStringUTF failed";
    } else {
        jint result = env->CallStaticIntMethod(g_dlHttpClientCls, g_dlHttpClientMid,
                                               jUrl, jPath,
                                               static_cast<jlong>(maxBytes));
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            outError = "jni: exception in downloadFile";
        } else if (result == 0) {
            ok = true;
        } else {
            char e[64];
            snprintf(e, sizeof(e), "jni: download failed (%d)", static_cast<int>(result));
            outError = e;
        }
    }
    if (jUrl) env->DeleteLocalRef(jUrl);
    if (jPath) env->DeleteLocalRef(jPath);

    if (attached) {
        // DetachCurrentThread with a pending exception aborts the VM —
        // make sure the slate is clean before detaching.
        if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
        javaVM->DetachCurrentThread();
    }
    return ok;
}

// ---------------------------------------------------------------------------
// HTTP GET hardening: full request write, header cap, redirect following,
// 2xx-only, chunked/encoding rejection, stall-based timeout, size cap,
// fsync-then-atomic-rename. Plain-HTTP only: https:// URLs are delegated to
// Android's HttpURLConnection via JNI (FetchViaJniHttps) at the top of
// FetchToFile, which uses the system TLS stack with full cert validation.
// ---------------------------------------------------------------------------
bool DLManager::FetchToFile(const DLFileTask& task, uint64_t& outBytes, std::string& outError) {
    outBytes = 0;

    ParsedUrl pu;
    if (!ParseHttpUrl(task.url, pu)) {
        outError = "bad url";
        return false;
    }

    const std::string finalPath = GetModelPath(task.crc, task.isDff);
    const uint64_t hardCap = (task.size > 0)
        ? (static_cast<uint64_t>(task.size) + (static_cast<uint64_t>(task.size) / 2) + 65536ull)
        : (64ull * 1024ull * 1024ull); // 64 MiB ceiling when the server gave no size

    // HTTPS: the native raw-socket client cannot speak TLS, so delegate to
    // Android's HttpURLConnection via JNI (system trust store, full cert
    // validation). The downloaded temp file goes through the same CRC32 /
    // size validation and atomic rename as the plain-HTTP path below.
    if (pu.isHttps) {
        char tempPath[600];
        snprintf(tempPath, sizeof(tempPath), "%s%08X.%d.temp",
                 GetCachePath().c_str(), task.crc, static_cast<int>(getpid()));

        if (!FetchViaJniHttps(task.url, tempPath, hardCap, outError)) {
            unlink(tempPath);
            return false;
        }
        if (CalculateFileCRC32(tempPath) != task.crc) {
            unlink(tempPath);
            outError = "crc mismatch";
            return false;
        }
        if (task.size > 0) {
            struct stat st;
            if (stat(tempPath, &st) == 0 &&
                static_cast<uint32_t>(st.st_size) != task.size) {
                unlink(tempPath);
                outError = "size mismatch";
                return false;
            }
        }
        // Durability before the atomic rename.
        {
            FILE* fp = fopen(tempPath, "rb");
            if (fp) { int fd = fileno(fp); if (fd >= 0) fsync(fd); fclose(fp); }
        }
        if (rename(tempPath, finalPath.c_str()) != 0) {
            unlink(tempPath);
            outError = "rename";
            return false;
        }
        struct stat st;
        outBytes = (stat(finalPath.c_str(), &st) == 0)
            ? static_cast<uint64_t>(st.st_size) : 0ull;
        return true;
    }

    std::string host = pu.host;
    int         port = pu.port;
    std::string path = pu.path;

    bool ok = false;
    uint64_t totalWritten = 0;

    for (size_t redirect = 0; redirect <= DL_MAX_REDIRECTS && !ok; ++redirect) {
        struct addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo* res = nullptr;
        char portStr[16];
        snprintf(portStr, sizeof(portStr), "%d", port);

        if (getaddrinfo(host.c_str(), portStr, &hints, &res) != 0 || !res) {
            outError = "dns: " + host;
            return false;
        }

        int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock < 0) { freeaddrinfo(res); outError = "socket"; return false; }

        int one = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

        // Non-blocking connect with an explicit deadline (audit B11): shutdown and
        // Reset stay responsive instead of blocking the join on connect().
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        int rc = connect(sock, res->ai_addr, res->ai_addrlen);
        if (rc < 0 && errno != EINPROGRESS) {
            close(sock); freeaddrinfo(res);
            outError = "connect";
            return false;
        }
        freeaddrinfo(res);

        if (rc < 0) {
            struct pollfd pfd{ sock, POLLOUT, 0 };
            int pr = poll(&pfd, 1, static_cast<int>(DL_CONNECT_TIMEOUT_MS));
            if (pr <= 0 || !(pfd.revents & POLLOUT)) {
                close(sock); outError = "connect timeout"; return false;
            }
            int soErr = 0; socklen_t slen = sizeof(soErr);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &soErr, &slen) != 0 || soErr != 0) {
                close(sock); outError = "connect refused"; return false;
            }
        }
        fcntl(sock, F_SETFL, flags);

        char req[1200];
        int reqLen = snprintf(req, sizeof(req),
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: SAMP/0.3\r\n"
            "Accept: */*\r\n"
            "Connection: close\r\n\r\n",
            path.c_str(), host.c_str());

        // Full request write (audit B8): loop until the whole request is sent.
        // MSG_NOSIGNAL: a RST from the server must surface as EPIPE (handled
        // below), never as a SIGPIPE that terminates the whole app.
        size_t sent = 0;
        bool sendOk = true;
        while (sent < static_cast<size_t>(reqLen)) {
            ssize_t n = send(sock, req + sent, reqLen - sent, MSG_NOSIGNAL);
            if (n <= 0) { if (errno == EINTR) continue; sendOk = false; break; }
            sent += static_cast<size_t>(n);
        }
        if (!sendOk) { close(sock); outError = "send"; return false; }

        // Unique temp path per pid (audit B9).
        char tempPath[600];
        snprintf(tempPath, sizeof(tempPath), "%s%08X.%d.temp",
                 GetCachePath().c_str(), task.crc, static_cast<int>(getpid()));

        FILE* outFp = fopen(tempPath, "wb");
        if (!outFp) { close(sock); outError = "open temp"; return false; }

        std::string headerBuf;
        bool inBody = false;
        bool httpOk = false;
        int  statusCode = 0;
        std::string redirectTo;
        bool fatal = false;
        uint32_t lastProgress = GetCurrentTick();

        char buf[16384];
        for (;;) {
            if (m_bShutdown.load()) { fatal = true; outError = "shutdown"; break; }

            struct pollfd pfd{ sock, POLLIN, 0 };
            int pr = poll(&pfd, 1, 500);   // 500 ms slices keep shutdown responsive
            if (pr < 0) {
                if (errno == EINTR) continue;
                outError = "poll"; fatal = true; break;
            }
            if (pr == 0) {
                // Stall-based timeout (audit B7): only abort if NO bytes for a while.
                if (GetCurrentTick() - lastProgress > DL_HTTP_STALL_MS) {
                    outError = "stalled"; fatal = true; break;
                }
                continue;
            }

            ssize_t n = recv(sock, buf, sizeof(buf), 0);
            if (n == 0) break;            // clean EOF
            if (n < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
                outError = "recv"; fatal = true; break;
            }
            lastProgress = GetCurrentTick();

            if (!inBody) {
                if (headerBuf.size() + static_cast<size_t>(n) > DL_MAX_HTTP_HEADER) {
                    outError = "header too large"; fatal = true; break; // audit B4
                }
                headerBuf.append(buf, static_cast<size_t>(n));
                size_t headerEnd = headerBuf.find("\r\n\r\n");
                if (headerEnd == std::string::npos) continue;

                size_t sp = headerBuf.find(' ');
                if (sp != std::string::npos) statusCode = atoi(headerBuf.c_str() + sp + 1);

                std::string lower = headerBuf;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

                // Redirects (audit B6): follow Location, or fail with a reason.
                if (statusCode >= 300 && statusCode < 400) {
                    size_t lp = lower.find("\nlocation:");
                    if (lp != std::string::npos) {
                        size_t vs = lp + strlen("\nlocation:");
                        while (vs < headerBuf.size() && (headerBuf[vs] == ' ' || headerBuf[vs] == '\t')) vs++;
                        size_t ve = headerBuf.find("\r\n", vs);
                        if (ve != std::string::npos) redirectTo = headerBuf.substr(vs, ve - vs);
                    }
                    if (redirectTo.empty()) outError = "redirect without location";
                    break;
                }

                if (statusCode < 200 || statusCode >= 300) {
                    char e[64];
                    snprintf(e, sizeof(e), "http %d", statusCode);
                    outError = e; fatal = true; break;
                }
                httpOk = true;

                // Reject transfer encodings we cannot decode (audit B5).
                if (lower.find("transfer-encoding: chunked") != std::string::npos) {
                    outError = "chunked encoding"; fatal = true; break;
                }

                inBody = true;
                const char* bodyStart = headerBuf.data() + headerEnd + 4;
                size_t bodyLen = headerBuf.size() - (headerEnd + 4);
                if (bodyLen > 0) {
                    if (fwrite(bodyStart, 1, bodyLen, outFp) != bodyLen) {
                        outError = "disk write"; fatal = true; break;
                    }
                    totalWritten += bodyLen;
                    if (totalWritten > hardCap) { outError = "size cap"; fatal = true; break; } // B3
                }
            } else {
                if (fwrite(buf, 1, static_cast<size_t>(n), outFp) != static_cast<size_t>(n)) {
                    outError = "disk write"; fatal = true; break;
                }
                totalWritten += static_cast<uint64_t>(n);
                if (totalWritten > hardCap) { outError = "size cap"; fatal = true; break; }
            }
        }

        // Durability before the atomic rename (audit B10).
        if (outFp) {
            fflush(outFp);
            int fd = fileno(outFp);
            if (fd >= 0) fsync(fd);
            fclose(outFp);
        }
        close(sock);

        if (fatal) { unlink(tempPath); return false; }

        if (httpOk && inBody) {
            // Content-addressed cache: the file name IS its CRC32, so verify it.
            if (CalculateFileCRC32(tempPath) != task.crc) {
                unlink(tempPath);
                outError = "crc mismatch";
                return false;
            }
            if (task.size > 0) {
                struct stat st;
                if (stat(tempPath, &st) == 0 &&
                    static_cast<uint32_t>(st.st_size) != task.size) {
                    unlink(tempPath);
                    outError = "size mismatch";
                    return false;
                }
            }
            if (rename(tempPath, finalPath.c_str()) != 0) {
                unlink(tempPath);
                outError = "rename";
                return false;
            }
            outBytes = totalWritten;
            ok = true;
            break;
        }

        unlink(tempPath);

        if (!redirectTo.empty()) {
            ParsedUrl np;
            if (!ParseHttpUrl(redirectTo, np)) {
                outError = "redirect target unsupported";
                return false;
            }
            if (np.isHttps) {
                // Redirect target is HTTPS: delegate the remainder to the JNI
                // downloader (same validation path as a direct HTTPS URL).
                DLFileTask httpsTask = task;
                httpsTask.url = redirectTo;
                return FetchToFile(httpsTask, outBytes, outError);
            }
            host = np.host; port = np.port; path = np.path;
            totalWritten = 0;
            continue;
        }
        return false;
    }

    return ok;
}

// ---------------------------------------------------------------------------
// Game-thread side
// ---------------------------------------------------------------------------
void DLManager::RequestFileFromServer(uint32_t crc, bool isDff) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rpcOutbox.emplace_back(crc, isDff);
}

void DLManager::ProcessRpcOutbox() {
    RakClientInterface* client;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        client = m_pRakClient;   // may have been nulled by Reset (audit C2)
    }
    if (!client) return;

    for (int guard = 0; guard < 32; ++guard) {
        std::pair<uint32_t, bool> item{0, false};
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_rpcOutbox.empty()) break;
            item = m_rpcOutbox.front();
            m_rpcOutbox.pop_front();
        }

        RakNet::BitStream bs;
        bs.Write(item.first);
        int rpcId = item.second ? RPC_RequestDff : RPC_RequestTxd;
        client->RPC(&rpcId, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false,
                    UNASSIGNED_NETWORK_ID, nullptr);

        // Diagnostic per-RPC chat log removed (was commit b978e39): with 325+
        // models it spams hundreds of "-> 181/182" lines that look like a retry
        // loop but are normal initial traffic. The skip logic in WorkerLoop
        // (needRpc = url.empty()) already prevents redundant re-sends.
    }
}

void DLManager::ProcessTick() {
    if (!m_bInitialized.load()) return;

    FlushChatQueue();
    ProcessRpcOutbox();

    const uint32_t now = GetCurrentTick();

    // Speed meter (KB/s), one sample per second.
    if (m_lastSpeedSampleTick == 0) {
        m_lastSpeedSampleTick = now;
        m_lastSpeedSampleBytes = m_totalBytesRx.load();
    } else if (now - m_lastSpeedSampleTick >= 1000) {
        uint64_t cur = m_totalBytesRx.load();
        m_speedKBps = static_cast<float>(cur - m_lastSpeedSampleBytes) / 1024.0f;
        m_lastSpeedSampleTick = now;
        m_lastSpeedSampleBytes = cur;
    }

    // Tell the player, once per connection, if a server never offered models.
    // Per-instance + reset on reconnect (audit D5) instead of a process-global.
    if (!m_bReportedNoManifest && !m_bSawAnyModel &&
        m_connectTime > 0 && (now - m_connectTime > 9000) && m_totalModels.load() == 0) {
        m_bReportedNoManifest = true;
        PushChat("This server has no custom models. Joining...");
    }

    if (m_bDownloadFinished.load()) return;

    size_t queued = 0, active = 0, done = 0, failed = 0;
    GetProgressStats(queued, active, done, failed);

    const bool allSettled = (queued == 0 && active == 0);
    const bool haveManifest = (m_totalModels.load() > 0);

    if (haveManifest && allSettled && !m_bFinishRequested.load()) {
        // Every task settled: downloaded or skipped-after-failures. A failed
        // package NEVER holds the player on the download screen — it is
        // skipped (the game falls back to base models) and /dlr stays
        // available for a manual retry pass.
        m_bFinishRequested = true;
        m_finishTime = now;
        m_bDownloadFinished = true;
        if (failed > 0) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "Model download finished: %zu ok, %zu skipped (use /dlr to retry)",
                     done, failed);
            PushChat(msg);
        }
        // Register downloaded models with the game engine BEFORE telling the
        // server we're ready. Must run on the game thread (ProcessTick is
        // called from CNetGame::Process).
        RegisterCustomModels();
        SendFinishDownload();
    } else if (!haveManifest && !m_bSawAnyModel &&
               (now - m_connectTime > 6000) && !m_bFinishRequested.load()) {
        // No manifest at all: do not hold the player in class selection. Gated on
        // "no RPC 179 ever" so a slow manifest cannot race us (audit A6).
        m_bFinishRequested = true;
        m_finishTime = now;
        m_bDownloadFinished = true;
        SendFinishDownload();
    }
}

void DLManager::GetProgressStats(size_t& queued, size_t& active, size_t& done, size_t& failed) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    queued = active = done = failed = 0;
    for (const auto& t : m_tasks) {
        if (t.done) { ++done; continue; }
        if (t.failed) { ++failed; continue; }
        if (t.started) { ++active; continue; }   // claimed: awaiting URL or fetching
        ++queued;
    }
}

void DLManager::RecomputeProgress() {
    size_t queued = 0, active = 0, done = 0, failed = 0;
    GetProgressStats(queued, active, done, failed);
    if (m_totalModels.load() > 0) {
        m_downloadedModels = static_cast<int>(done);
    }
}

void DLManager::RequestRetryFailed() {
    int rearmed = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& t : m_tasks) {
            if (!t.failed) continue;
            t.failed = false;
            t.started = false;
            t.attempts = 0;
            t.lastError.clear();
            ++rearmed;
        }
        if (rearmed > 0) {
            m_failedModels = 0;
            m_bFinishRequested = false;
            m_bFinishSent = false;
            m_bDownloadFinished = false;
            m_finishTime = 0;
        }
    }
    if (rearmed > 0) {
        m_workCv.notify_all();
        PushChat("Retrying failed model downloads...");
    }
}

void DLManager::SendFinishDownload() {
    RakClientInterface* client;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        client = m_pRakClient;
        m_bFinishSent = true;
    }
    if (!client) return;
    // RPC 184 must carry a zero-length bitstream (no dummy bytes).
    RakNet::BitStream bs;
    client->RPC(&RPC_FinishDownload, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false,
                UNASSIGNED_NETWORK_ID, nullptr);
}

void DLManager::ToggleOverlay() {
    m_bForceShowOverlay = !m_bForceShowOverlay;
}

// ---------------------------------------------------------------------------
// UI - a quiet HUD line, not a panel. Drawn on the same draw list the FPS label
// uses (classic ImGui windows fight the game for frame ordering).
//
// House style (kept from the minimal-UI pass): bottom-left, one short lowercase
// sentence, the stock outlined HUD font, a hairline progress bar, no card, no
// border, no brand colour, no counters the player cannot act on.
//
// Hardening: sizes measured via calculateTextSize (not a 0.58 guess), laid out
// bottom-left against the real screen size, and failures are actionable.
// ---------------------------------------------------------------------------
void DLManager::RenderImGui() {
    if (!m_bInitialized.load()) return;
    if (!pUI) return;

    ImGuiRenderer* r = pUI->renderer();
    if (!r) return;

    size_t queued = 0, active = 0, done = 0, failed = 0;
    GetProgressStats(queued, active, done, failed);

    const int      total    = m_totalModels.load();
    const bool     finished = m_bDownloadFinished.load();
    const uint32_t now      = GetCurrentTick();

    const bool waiting  = (total == 0) && !finished && (now - m_connectTime) < 6000;
    const bool showDone = (m_finishTime > 0) && (now - m_finishTime) < 4000;
    const bool busy     = !finished && (active > 0 || queued > 0 || (total > 0 && done < (size_t)total));

    // Visible while there is real work, while the manifest is still expected,
    // or for a beat after finishing. The /dl command pins it open.
    if (!m_bForceShowOverlay && !busy && !waiting && !(finished && showDone)) return;

    // ---- Compose the one line the player needs.
    char label[96];
    bool showBar = false;
    float fraction = 0.0f;

    // Progress counts settled work: downloaded + skipped. The bar must always
    // be able to reach 100% — failures are skipped, never stuck.
    const size_t settled = done + failed;

    if (failed > 0 && !busy && !finished) {
        snprintf(label, sizeof(label), "Model download: %zu failed (/dlr to retry)", failed);
    } else if (finished) {
        if (failed > 0) {
            snprintf(label, sizeof(label), "Models ready (%zu skipped)", failed);
        } else {
            snprintf(label, sizeof(label), "Models ready");
        }
        fraction = 1.0f;
        showBar = total > 0;
    } else if (total > 0) {
        fraction = std::clamp(static_cast<float>(settled) / static_cast<float>(total), 0.0f, 1.0f);
        if (failed > 0) {
            snprintf(label, sizeof(label), "Downloading models %zu/%d (%zu skipped)",
                     settled, total, failed);
        } else {
            snprintf(label, sizeof(label), "Downloading models %zu/%d", done, total);
        }
        showBar = true;
    } else {
        snprintf(label, sizeof(label), "Downloading models");
    }

    // ---- Layout: bottom-left, measured, scaled by screen size.
    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    const float  fs     = std::max(UISettings::fontSize() * 0.85f, 11.0f);
    const float  margin = std::max(screen.y * 0.03f, 16.0f);
    const float  barH   = std::max(fs * 0.14f, 3.0f);
    const float  gap    = fs * 0.35f;

    const ImVec2 labelSize = r->calculateTextSize(label, fs);
    const float  barW      = std::min(std::max(screen.x * 0.28f, labelSize.x), fs * 22.0f);
    const float  totalH    = labelSize.y + (showBar ? gap + barH : 0.0f);

    const float x = margin;
    const float y = screen.y - margin - totalH;

    // Stock HUD colours: white pair with the black outline, flat grey bar.
    const ImColor kText (245, 245, 245, 255);
    const ImColor kTrack(0, 0, 0, 110);
    const ImColor kFill (235, 235, 235, 235);

    r->drawText(ImVec2(x, y), kText, label, true, fs);

    if (showBar) {
        const float by0 = y + labelSize.y + gap;
        r->drawRect(ImVec2(x, by0), ImVec2(x + barW, by0 + barH), kTrack, true);
        if (fraction > 0.0f) {
            r->drawRect(ImVec2(x, by0), ImVec2(x + barW * fraction, by0 + barH), kFill, true);
        }
    }
}
