#include "dlmanager.h"
#include "../../main.h"
#include "../../gui/gui.h"
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>

extern UI* pUI;

// ---------------------------------------------------------------------------
// CRC32 (IEEE 802.3, reflected, init 0xFFFFFFFF, xorout 0xFFFFFFFF)
// ---------------------------------------------------------------------------
static uint32_t s_crcTable[256];
static bool s_crcTableInitialized = false;

static void InitCRC32Table() {
    if (s_crcTableInitialized) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        s_crcTable[i] = c;
    }
    s_crcTableInitialized = true;
}

uint32_t DLManager::CalculateCRC32(const void* data, size_t length) {
    InitCRC32Table();
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
    InitCRC32Table();
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
            mkdir(current.c_str(), 0777);
        }
    }
}

static uint32_t GetCurrentTick() {
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}

// Parse http://host[:port]/path into its components.
static bool ParseHttpUrl(const std::string& url, std::string& host, int& port, std::string& path) {
    const std::string prefix = "http://";
    if (url.rfind(prefix, 0) != 0) return false;

    std::string rest = url.substr(prefix.length());
    size_t slashPos = rest.find('/');
    if (slashPos == std::string::npos) {
        path = "/";
    } else {
        path = rest.substr(slashPos);
        rest = rest.substr(0, slashPos);
    }

    size_t colonPos = rest.find(':');
    if (colonPos != std::string::npos) {
        host = rest.substr(0, colonPos);
        port = atoi(rest.substr(colonPos + 1).c_str());
        if (port <= 0) port = 80;
    } else {
        host = rest;
        port = 80;
    }
    return !host.empty();
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
    m_nextVirtualId = 18631;
    m_connectTime = GetCurrentTick();

    std::string cleanStorage = storagePath;
    if (!cleanStorage.empty() && cleanStorage.back() != '/') {
        cleanStorage += "/";
    }

    char cacheDir[512];
    snprintf(cacheDir, sizeof(cacheDir), "%sSAMP/cache/%s.%d/",
             cleanStorage.c_str(), serverIp.c_str(), serverPort);
    m_cachePath = cacheDir;
    MakeDirectoryTree(m_cachePath);

    m_bShutdown = false;
    m_bInitialized = true;

    // Spawn the SA-MP PC style pool: DL_MAX_CONCURRENT parallel transfer slots.
    for (int i = 0; i < DL_MAX_CONCURRENT; ++i) {
        m_workers.emplace_back([this]() { WorkerLoop(); });
    }

    if (pUI && pUI->chat()) {
        pUI->chat()->addDebugMessage("{38BDF8}[0.3.DL]{FFFFFF} Model sync ready (%d parallel slots). Cache: %s.%d",
                                     DL_MAX_CONCURRENT, serverIp.c_str(), serverPort);
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
    m_models.clear();
    m_tasks.clear();
    m_knownCrc.clear();
    m_rpcOutbox.clear();
    m_networkToVirtual.clear();
    m_virtualToNetwork.clear();
    m_nextVirtualId = 18631;
    m_bInitialized = false;
    m_bDownloadFinished = false;
    m_bFinishRequested = false;
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
    m_finishTime = 0;
    m_lastActivity.clear();
}

// ---------------------------------------------------------------------------
// Paths & cache validation
// ---------------------------------------------------------------------------
std::string DLManager::GetModelPath(uint32_t crc, bool isDff) const {
    char filename[64];
    snprintf(filename, sizeof(filename), "0x%08X.%s", crc, isDff ? "dff" : "txd");
    return m_cachePath + filename;
}

bool DLManager::IsModelCached(uint32_t crc, uint32_t expectedSize, bool isDff) const {
    std::string path = GetModelPath(crc, isDff);
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    if (expectedSize > 0 && static_cast<uint32_t>(st.st_size) != expectedSize) {
        unlink(path.c_str());
        return false;
    }
    return CalculateFileCRC32(path) == crc;
}

// ---------------------------------------------------------------------------
// Virtual model id remap (server ids -> safe internal CModelInfo slots)
// ---------------------------------------------------------------------------
int16_t DLManager::GetVirtualModelId(int32_t networkId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_networkToVirtual.find(networkId);
    if (it != m_networkToVirtual.end()) return it->second;

    if (m_nextVirtualId >= 19999) m_nextVirtualId = 18631;
    int16_t virtualId = m_nextVirtualId++;
    m_networkToVirtual[networkId] = virtualId;
    m_virtualToNetwork[virtualId] = networkId;
    return virtualId;
}

int32_t DLManager::GetNetworkModelId(int16_t virtualId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_virtualToNetwork.find(virtualId);
    if (it != m_virtualToNetwork.end()) return it->second;
    return virtualId;
}

// ---------------------------------------------------------------------------
// RPC inbound: 179 ModelRequest (S->C) - one packet per registered model
// ---------------------------------------------------------------------------
void DLManager::OnModelRequest(RPCParameters* rpcParams) {
    if (!rpcParams || !rpcParams->input) return;

    unsigned char* data = reinterpret_cast<unsigned char*>(rpcParams->input);
    int bitLength = rpcParams->numberOfBitsOfData;
    int byteLength = (bitLength / 8) + 1;
    if (byteLength < 39) return;

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

    info.virtualModelId = GetVirtualModelId(info.newId);
    info.dffCached = IsModelCached(info.dffChecksum, info.dffSize, true);
    info.txdCached = IsModelCached(info.txdChecksum, info.txdSize, false);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_totalModels = info.count;
        m_models.push_back(info);

        auto enqueue = [&](uint32_t crc, uint32_t size, bool isDff) {
            if (crc == 0 || m_knownCrc.count(crc)) return;
            m_knownCrc.insert(crc);
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
    int byteLength = (bitLength / 8) + 1;
    if (byteLength < 6) return;

    RakNet::BitStream bs(data, byteLength, false);
    uint8_t  arg = 0;        // 6 = HTTP transport
    uint8_t  fileType = 0;   // 1 = DFF, 2 = TXD
    uint32_t fileChecksum = 0;
    uint8_t  urlLen = 0;

    bs.Read(arg);
    bs.Read(fileType);
    bs.Read(fileChecksum);
    bs.Read(urlLen);

    std::string url;
    if (urlLen > 0) {
        char urlBuffer[512] = {0};
        size_t toRead = (urlLen < sizeof(urlBuffer) - 1) ? urlLen : sizeof(urlBuffer) - 1;
        bs.Read(urlBuffer, toRead);
        urlBuffer[toRead] = '\0';
        url = urlBuffer;
    }

    if (url.empty() || fileChecksum == 0) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& t : m_tasks) {
            if (t.crc == fileChecksum) {
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
    if (pUI && pUI->chat()) {
        pUI->chat()->addDebugMessage("{38BDF8}[0.3.DL]{FFFFFF} Server confirmed artwork sync complete (RPC 185).");
    }
}

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
            if (!anyPending) {
                std::unique_lock<std::mutex> lock(m_workMutex);
                m_workCv.wait_for(lock, std::chrono::milliseconds(200));
            } else {
                // Slots are busy waiting for their RPC 183 URL.
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            continue;
        }

        // 2. Request the URL from the server (RPC 181/182) via the outbox.
        RequestFileFromServer(pickedCrc, pickedIsDff);

        // 3. Wait (bounded, ~10s) for RPC 183 to deliver the URL.
        std::string url;
        uint32_t expectedSize = 0;
        bool isDff = pickedIsDff;
        bool retry = false;
        for (int waited = 0; waited < 400 && !m_bShutdown.load(); ++waited) {
            bool gotUrl = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (auto& t : m_tasks) {
                    if (t.crc != pickedCrc) continue;
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
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& t : m_tasks) {
                if (t.crc != pickedCrc) continue;
                if (++t.attempts >= 3) {
                    t.failed = true;
                    m_failedModels++;
                    m_lastActivity = t.hexName + " no url";
                } else {
                    t.started = false;
                    m_lastActivity = t.hexName + " retry";
                }
                break;
            }
            retry = true;
        }
        if (retry) continue;

        // 4. Fetch over HTTP.
        DLFileTask local;
        local.crc = pickedCrc;
        local.size = expectedSize;
        local.isDff = isDff;
        local.url = url;
        local.hexName = GetModelPath(pickedCrc, isDff);

        m_activeWorkers++;
        uint64_t bytes = 0;
        bool ok = FetchToFile(local, bytes);
        m_activeWorkers--;
        m_totalBytesRx += bytes;

        // 5. Commit result.
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& t : m_tasks) {
                if (t.crc != pickedCrc) continue;
                if (ok) {
                    t.done = true;
                    t.url.clear();
                    m_lastActivity = t.hexName + " done";
                } else if (++t.attempts >= 3) {
                    t.failed = true;
                    m_failedModels++;
                    m_lastActivity = t.hexName + " failed";
                } else {
                    t.started = false;
                    t.url = url;   // keep the URL so a retry skips the RPC round-trip
                    m_lastActivity = t.hexName + " error, retry";
                }
                break;
            }
        }

        RecomputeProgress();
    }
}

bool DLManager::FetchToFile(const DLFileTask& task, uint64_t& outBytes) {
    outBytes = 0;
    std::string host, path;
    int port = 80;
    if (!ParseHttpUrl(task.url, host, port, path)) return false;

    struct addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    char portStr[16];
    snprintf(portStr, sizeof(portStr), "%d", port);

    if (getaddrinfo(host.c_str(), portStr, &hints, &res) != 0 || !res) return false;

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) { freeaddrinfo(res); return false; }

    int one = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    struct timeval tv{ 8, 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        close(sock);
        freeaddrinfo(res);
        return false;
    }
    freeaddrinfo(res);

    char req[1024];
    snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: SAMP/0.3\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n\r\n",
        path.c_str(), host.c_str(), port);

    if (send(sock, req, strlen(req), 0) <= 0) { close(sock); return false; }

    char tempPath[512];
    snprintf(tempPath, sizeof(tempPath), "%s%08X.temp", m_cachePath.c_str(), task.crc);
    FILE* outFp = fopen(tempPath, "wb");
    if (!outFp) { close(sock); return false; }

    char recvBuf[16384];
    std::string headerBuf;
    bool inBody = false;
    bool httpOk = true;
    ssize_t bytes = 0;

    while ((bytes = recv(sock, recvBuf, sizeof(recvBuf), 0)) > 0) {
        if (!inBody) {
            headerBuf.append(recvBuf, static_cast<size_t>(bytes));
            size_t headerEnd = headerBuf.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                // open.mp answers 401 when User-Agent is wrong - treat as failure.
                if (headerBuf.rfind("HTTP/1.1 200", 0) != 0 &&
                    headerBuf.rfind("HTTP/1.0 200", 0) != 0) {
                    httpOk = false;
                    break;
                }
                inBody = true;
                const char* bodyStart = headerBuf.data() + headerEnd + 4;
                size_t bodyLen = headerBuf.size() - (headerEnd + 4);
                if (bodyLen > 0) {
                    fwrite(bodyStart, 1, bodyLen, outFp);
                    outBytes += bodyLen;
                }
            }
        } else {
            fwrite(recvBuf, 1, static_cast<size_t>(bytes), outFp);
            outBytes += static_cast<uint64_t>(bytes);
        }
    }

    fclose(outFp);
    close(sock);

    if (!httpOk || !inBody) {
        unlink(tempPath);
        return false;
    }

    // Content-addressed cache: the file name IS its CRC32, so verify it.
    if (CalculateFileCRC32(tempPath) != task.crc) {
        unlink(tempPath);
        return false;
    }

    if (task.size > 0) {
        struct stat st;
        if (stat(tempPath, &st) == 0 && static_cast<uint32_t>(st.st_size) != task.size) {
            unlink(tempPath);
            return false;
        }
    }

    std::string finalPath = GetModelPath(task.crc, task.isDff);
    if (rename(tempPath, finalPath.c_str()) != 0) {
        unlink(tempPath);
        return false;
    }

    if (pUI && pUI->chat()) {
        pUI->chat()->addDebugMessage("{38BDF8}[0.3.DL]{FFFFFF} %s cached (%u KB)",
                                     task.hexName.c_str(),
                                     static_cast<unsigned>(outBytes / 1024));
    }
    return true;
}

// ---------------------------------------------------------------------------
// Game-thread side
// ---------------------------------------------------------------------------
void DLManager::RequestFileFromServer(uint32_t crc, bool isDff) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rpcOutbox.emplace_back(crc, isDff);
}

void DLManager::ProcessRpcOutbox() {
    if (!m_pRakClient) return;

    for (int guard = 0; guard < 16; ++guard) {
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
        m_pRakClient->RPC(&rpcId, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false,
                          UNASSIGNED_NETWORK_ID, nullptr);
    }
}

void DLManager::ProcessTick() {
    if (!m_bInitialized.load()) return;

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

    if (m_bDownloadFinished.load()) return;

    size_t queued = 0, active = 0, done = 0, failed = 0;
    GetProgressStats(queued, active, done, failed);

    const bool allSettled = (queued == 0 && active == 0);
    const bool haveManifest = (m_totalModels.load() > 0);

    if (haveManifest && allSettled && !m_bFinishRequested.load()) {
        m_bFinishRequested = true;
        m_finishTime = now;
        m_bDownloadFinished = true;
        SendFinishDownload();
    } else if (!haveManifest && (now - m_connectTime > 6000) && !m_bFinishRequested.load()) {
        // Server sent no model manifest at all: do not hold the player in class selection.
        m_bFinishRequested = true;
        m_finishTime = now;
        m_bDownloadFinished = true;
        SendFinishDownload();
        if (pUI && pUI->chat()) {
            pUI->chat()->addDebugMessage("{38BDF8}[0.3.DL]{FFFFFF} No custom models announced. Spawn unblocked.");
        }
    }
}

void DLManager::GetProgressStats(size_t& queued, size_t& active, size_t& done, size_t& failed) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    queued = active = done = failed = 0;
    for (const auto& t : m_tasks) {
        if (t.done) { ++done; continue; }
        if (t.failed) { ++failed; continue; }
        if (t.started && !t.url.empty()) { ++active; continue; }
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

void DLManager::SendFinishDownload() {
    if (!m_pRakClient) return;
    // RPC 184 must carry a zero-length bitstream (no dummy bytes).
    RakNet::BitStream bs;
    m_pRakClient->RPC(&RPC_FinishDownload, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false,
                      UNASSIGNED_NETWORK_ID, nullptr);
    if (pUI && pUI->chat()) {
        pUI->chat()->addDebugMessage("{38BDF8}[0.3.DL]{FFFFFF} RPC 184 FinishDownload sent. Spawn enabled.");
    }
}

void DLManager::ToggleOverlay() {
    m_bForceShowOverlay = !m_bForceShowOverlay;
    if (pUI && pUI->chat()) {
        pUI->chat()->addDebugMessage("{38BDF8}[0.3.DL]{FFFFFF} Download overlay: %s",
                                     m_bForceShowOverlay ? "pinned" : "auto-hide");
    }
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------
void DLManager::RenderImGui() {
    if (!m_bInitialized.load()) return;

    size_t queued = 0, active = 0, done = 0, failed = 0;
    GetProgressStats(queued, active, done, failed);

    const int      total    = m_totalModels.load();
    const bool     finished = m_bDownloadFinished.load();
    const uint32_t now      = GetCurrentTick();
    const size_t   pending  = queued + active;

    const bool shouldShow = m_bForceShowOverlay ||
                            active > 0 ||
                            (total > 0 && !finished) ||
                            (total == 0 && (now - m_connectTime) < 6000) ||
                            (m_finishTime > 0 && (now - m_finishTime) < 6000);
    if (!shouldShow) return;

    ImGuiIO& io = ImGui::GetIO();
    const float cardWidth  = std::clamp(io.DisplaySize.x * 0.52f, 420.0f, 560.0f);
    const float cardHeight = 96.0f;
    const float posX = (io.DisplaySize.x - cardWidth) * 0.5f;
    const float posY = 20.0f;

    ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(cardWidth, cardHeight), ImGuiCond_Always);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;

    const ImVec4 kCyberBlue(0.008f, 0.518f, 0.780f, 1.0f);   // #0284C7
    const ImVec4 kSky      (0.220f, 0.741f, 0.973f, 1.0f);   // #38BDF8
    const ImVec4 kDark     (0.035f, 0.051f, 0.086f, 0.96f);  // #090D16
    const ImVec4 kTrack    (0.055f, 0.078f, 0.126f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, kDark);
    ImGui::PushStyleColor(ImGuiCol_Border, kCyberBlue);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));

    if (ImGui::Begin("##dl_card", nullptr, flags)) {
        ImGui::TextColored(kSky, "CUSTOM MODELS");
        ImGui::SameLine();

        if (failed > 0) {
            ImGui::TextColored(ImVec4(0.94f, 0.35f, 0.35f, 1.0f), "// %zu FAILED", failed);
        } else if (finished) {
            ImGui::TextColored(ImVec4(0.133f, 0.773f, 0.369f, 1.0f), "// READY");
        } else if (active > 0) {
            ImGui::TextColored(ImVec4(0.961f, 0.620f, 0.043f, 1.0f),
                               "// %zu/%d SLOTS ACTIVE", active, DL_MAX_CONCURRENT);
        } else if (total > 0) {
            ImGui::TextColored(kSky, "// VERIFYING CACHE");
        } else {
            ImGui::TextColored(ImVec4(0.65f, 0.68f, 0.72f, 1.0f), "// HANDSHAKE");
        }

        float fraction = 0.0f;
        if (total > 0) {
            fraction = std::clamp(static_cast<float>(done) / static_cast<float>(total), 0.0f, 1.0f);
        } else if (finished) {
            fraction = 1.0f;
        }

        char overlay[96];
        if (total > 0) {
            snprintf(overlay, sizeof(overlay), "%zu / %d models  (%.0f%%)",
                     done, total, fraction * 100.0f);
        } else if (finished) {
            snprintf(overlay, sizeof(overlay), "No custom models");
        } else {
            snprintf(overlay, sizeof(overlay), "Querying server model list...");
        }

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, kCyberBlue);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, kTrack);
        ImGui::ProgressBar(fraction, ImVec2(-1.0f, 18.0f), overlay);
        ImGui::PopStyleColor(2);

        if (total > 0 && !finished) {
            ImGui::TextDisabled("pending %zu  |  slots %zu/%d  |  %.0f KB/s  |  cached %zu",
                                pending, active, DL_MAX_CONCURRENT, m_speedKBps.load(), done);
        } else if (finished) {
            ImGui::TextColored(kSky, "%s",
                               failed > 0 ? "Finished with errors - rejoin to retry."
                                          : "All assets verified. Spawn enabled.");
        } else {
            ImGui::TextDisabled("Connected. Waiting for artwork manifest...");
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}
