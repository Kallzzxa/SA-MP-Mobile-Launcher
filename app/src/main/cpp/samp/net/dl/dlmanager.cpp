#include "dlmanager.h"
#include "../../main.h"
#include "../../gui/gui.h"
#include "../../vendor/imgui/imgui.h"
#include "../../vendor/raknet/SAMP/SAMPRPC.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <chrono>

extern UI* pUI;

// Standard CRC32 IEEE 802.3 table
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
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}

DLManager& DLManager::Instance() {
    static DLManager s_instance;
    return s_instance;
}

DLManager::DLManager()
    : m_serverPort(7777)
    , m_pRakClient(nullptr)
    , m_nextVirtualId(18631)
    , m_bInitialized(false)
    , m_bDownloading(false)
    , m_bDownloadFinished(false)
    , m_totalModels(0)
    , m_downloadedModels(0)
    , m_connectTime(0)
    , m_finishTime(0)
    , m_currentFileProgress(0.0f)
{
}

DLManager::~DLManager() {
    Reset();
}

void DLManager::Initialize(const std::string& serverIp, int serverPort, const std::string& storagePath, RakClientInterface* pRakClient) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_serverIp = serverIp;
    m_serverPort = serverPort;
    m_pRakClient = pRakClient;
    m_bDownloadFinished = false;
    m_bDownloading = false;
    m_totalModels = 0;
    m_downloadedModels = 0;
    m_nextVirtualId = 18631;
    m_connectTime = GetCurrentTick();
    m_finishTime = 0;

    std::string cleanStorage = storagePath;
    if (!cleanStorage.empty() && cleanStorage.back() != '/') {
        cleanStorage += "/";
    }

    char cacheDir[512];
    snprintf(cacheDir, sizeof(cacheDir), "%sSAMP/cache/%s.%d/", cleanStorage.c_str(), serverIp.c_str(), serverPort);
    m_cachePath = cacheDir;

    MakeDirectoryTree(m_cachePath);
    m_bInitialized = true;

    if (pUI && pUI->chat()) {
        pUI->chat()->addDebugMessage("{38BDF8}[0.3.DL]{FFFFFF} Model sync ready. Cache: %s.%d", serverIp.c_str(), serverPort);
    }
}

void DLManager::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_models.clear();
    m_requestQueue.clear();
    m_networkToVirtual.clear();
    m_virtualToNetwork.clear();
    m_nextVirtualId = 18631;
    m_bInitialized = false;
    m_bDownloading = false;
    m_bDownloadFinished = false;
    m_totalModels = 0;
    m_downloadedModels = 0;
    m_connectTime = 0;
    m_finishTime = 0;
    m_currentFileName.clear();
    m_currentFileProgress = 0.0f;
}

std::string DLManager::GetModelPath(uint32_t crc, bool isDff) const {
    char filename[64];
    snprintf(filename, sizeof(filename), "0x%08X.%s", crc, isDff ? "dff" : "txd");
    return m_cachePath + filename;
}

bool DLManager::IsModelCached(uint32_t crc, uint32_t expectedSize, bool isDff) const {
    std::string path = GetModelPath(crc, isDff);
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    if (expectedSize > 0 && static_cast<uint32_t>(st.st_size) != expectedSize) {
        return false;
    }
    uint32_t actualCrc = CalculateFileCRC32(path);
    return (actualCrc == crc);
}

int16_t DLManager::GetVirtualModelId(int32_t networkId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_networkToVirtual.find(networkId);
    if (it != m_networkToVirtual.end()) {
        return it->second;
    }
    if (m_nextVirtualId >= 19999) {
        m_nextVirtualId = 18631;
    }
    int16_t virtualId = m_nextVirtualId++;
    m_networkToVirtual[networkId] = virtualId;
    m_virtualToNetwork[virtualId] = networkId;
    return virtualId;
}

int32_t DLManager::GetNetworkModelId(int16_t virtualId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_virtualToNetwork.find(virtualId);
    if (it != m_virtualToNetwork.end()) {
        return it->second;
    }
    return virtualId;
}

void DLManager::ProcessTick() {
    if (!m_bInitialized.load() || m_bDownloadFinished.load()) {
        return;
    }

    uint32_t now = GetCurrentTick();
    if (m_totalModels.load() == 0 && (now - m_connectTime > 2500)) {
        // Server has no custom models -> auto-ack so spawn is not blocked
        m_bDownloadFinished = true;
        m_finishTime = now;
        SendFinishDownload();
        if (pUI && pUI->chat()) {
            pUI->chat()->addDebugMessage("{38BDF8}[0.3.DL]{FFFFFF} Server has no custom models. Spawn unblocked.");
        }
    }
}

void DLManager::OnModelRequest(RPCParameters* rpcParams) {
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

        if (!info.dffCached) {
            PendingFile pf{ info.dffChecksum, info.dffSize, true, "" };
            m_requestQueue.push_back(pf);
        }
        if (!info.txdCached) {
            PendingFile pf{ info.txdChecksum, info.txdSize, false, "" };
            m_requestQueue.push_back(pf);
        }
    }

    if (pUI && pUI->chat()) {
        pUI->chat()->addDebugMessage("{38BDF8}[0.3.DL]{FFFFFF} Model [%d/%d] ID %d (%s) - %s",
            info.poolID + 1, info.count, info.newId,
            (info.type == 1) ? "Skin" : "Object",
            (info.dffCached && info.txdCached) ? "Cached" : "Downloading");
    }

    if (info.poolID + 1 == static_cast<uint32_t>(info.count)) {
        CheckPendingDownloads();
    }
}

void DLManager::CheckPendingDownloads() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_requestQueue.empty()) {
        m_bDownloadFinished = true;
        m_finishTime = GetCurrentTick();
        SendFinishDownload();
        if (pUI && pUI->chat()) {
            pUI->chat()->addDebugMessage("{38BDF8}[0.3.DL]{FFFFFF} All %d models synced from cache. Ready to play!", m_totalModels.load());
        }
    } else {
        RequestNextFile();
    }
}

void DLManager::RequestNextFile() {
    if (m_requestQueue.empty()) {
        m_bDownloadFinished = true;
        m_finishTime = GetCurrentTick();
        SendFinishDownload();
        return;
    }
    if (m_bDownloading.load()) {
        return;
    }

    PendingFile pf = m_requestQueue.front();
    if (!m_pRakClient) return;

    char hexName[64];
    snprintf(hexName, sizeof(hexName), "0x%08X.%s", pf.crc, pf.isDff ? "dff" : "txd");
    m_currentFileName = hexName;
    m_currentFileProgress = 0.0f;

    RakNet::BitStream bs;
    bs.Write(pf.crc);

    int rpcId = pf.isDff ? RPC_RequestDff : RPC_RequestTxd;
    m_pRakClient->RPC(&rpcId, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
}

void DLManager::OnModelFile(RPCParameters* rpcParams) {
    unsigned char* data = reinterpret_cast<unsigned char*>(rpcParams->input);
    int bitLength = rpcParams->numberOfBitsOfData;
    int byteLength = (bitLength / 8) + 1;
    if (byteLength < 6) return;

    RakNet::BitStream bs(data, byteLength, false);
    uint8_t arg = 0;
    uint8_t fileType = 0; // 1 = DFF, 2 = TXD
    uint32_t fileChecksum = 0;

    bs.Read(arg);
    bs.Read(fileType);
    bs.Read(fileChecksum);

    uint8_t urlLen = 0;
    bs.Read(urlLen);
    std::string downloadUrl;
    if (urlLen > 0) {
        char urlBuffer[256];
        bs.Read(urlBuffer, urlLen);
        urlBuffer[urlLen] = '\0';
        downloadUrl = urlBuffer;
    }

    uint32_t expectedSize = 0;
    bool isDff = (fileType == 1);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& m : m_models) {
            if (isDff && m.dffChecksum == fileChecksum) {
                expectedSize = m.dffSize;
                break;
            } else if (!isDff && m.txdChecksum == fileChecksum) {
                expectedSize = m.txdSize;
                break;
            }
        }
    }

    if (!downloadUrl.empty()) {
        m_bDownloading = true;
        std::thread([this, downloadUrl, fileChecksum, expectedSize, isDff]() {
            this->DownloadWorker(downloadUrl, fileChecksum, expectedSize, isDff);
        }).detach();
    }
}

static bool ParseHttpUrl(const std::string& url, std::string& host, int& port, std::string& path) {
    std::string prefix = "http://";
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
    } else {
        host = rest;
        port = 80;
    }
    return !host.empty();
}

void DLManager::DownloadWorker(const std::string& url, uint32_t crc, uint32_t expectedSize, bool isDff) {
    std::string host, path;
    int port = 80;
    if (!ParseHttpUrl(url, host, port, path)) {
        m_bDownloading = false;
        return;
    }

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char portStr[16];
    snprintf(portStr, sizeof(portStr), "%d", port);

    if (getaddrinfo(host.c_str(), portStr, &hints, &res) != 0 || !res) {
        m_bDownloading = false;
        return;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        m_bDownloading = false;
        return;
    }

    struct timeval tv{ 10, 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        close(sock);
        freeaddrinfo(res);
        m_bDownloading = false;
        return;
    }
    freeaddrinfo(res);

    // open.mp requires User-Agent: SAMP/0.3
    char req[1024];
    snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: SAMP/0.3\r\n"
        "Connection: close\r\n\r\n",
        path.c_str(), host.c_str(), port);

    send(sock, req, strlen(req), 0);

    char tempPath[512];
    snprintf(tempPath, sizeof(tempPath), "%s00000_%08X.temp", m_cachePath.c_str(), crc);
    FILE* outFp = fopen(tempPath, "wb");
    if (!outFp) {
        close(sock);
        m_bDownloading = false;
        return;
    }

    char recvBuf[8192];
    bool inBody = false;
    std::string headerBuf;
    ssize_t bytes = 0;
    size_t totalReceived = 0;

    while ((bytes = recv(sock, recvBuf, sizeof(recvBuf), 0)) > 0) {
        if (!inBody) {
            headerBuf.append(recvBuf, bytes);
            size_t headerEnd = headerBuf.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                inBody = true;
                const char* bodyStart = headerBuf.data() + headerEnd + 4;
                size_t bodyLen = headerBuf.size() - (headerEnd + 4);
                if (bodyLen > 0) {
                    fwrite(bodyStart, 1, bodyLen, outFp);
                    totalReceived += bodyLen;
                }
            }
        } else {
            fwrite(recvBuf, 1, bytes, outFp);
            totalReceived += bytes;
        }
        if (expectedSize > 0) {
            m_currentFileProgress = static_cast<float>(totalReceived) / static_cast<float>(expectedSize);
        }
    }
    fclose(outFp);
    close(sock);

    uint32_t actualCrc = CalculateFileCRC32(tempPath);
    if (actualCrc == crc) {
        std::string finalPath = GetModelPath(crc, isDff);
        rename(tempPath, finalPath.c_str());
        m_downloadedModels++;
        if (pUI && pUI->chat()) {
            pUI->chat()->addDebugMessage("{38BDF8}[0.3.DL]{FFFFFF} Downloaded %s (0x%08X) [%d/%d]",
                isDff ? "DFF" : "TXD", crc, m_downloadedModels.load(), m_totalModels.load());
        }
    } else {
        unlink(tempPath);
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_requestQueue.empty() && m_requestQueue.front().crc == crc) {
            m_requestQueue.erase(m_requestQueue.begin());
        }
    }

    m_bDownloading = false;
    RequestNextFile();
}

void DLManager::SendFinishDownload() {
    if (!m_pRakClient) return;
    // Empty bitstream for 184 FinishDownload per protocol spec
    RakNet::BitStream bs;
    m_pRakClient->RPC(&RPC_FinishDownload, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
    if (pUI && pUI->chat()) {
        pUI->chat()->addDebugMessage("{38BDF8}[0.3.DL]{FFFFFF} Sent RPC 184 FinishDownload to server.");
    }
}

void DLManager::OnDownloadCompleted(RPCParameters* rpcParams) {
    m_bDownloadFinished = true;
    m_finishTime = GetCurrentTick();
    if (pUI && pUI->chat()) {
        pUI->chat()->addDebugMessage("{38BDF8}[0.3.DL]{FFFFFF} Server confirmed: Artwork sync complete (RPC 185).");
    }
}

void DLManager::RenderImGui() {
    if (!m_bInitialized.load()) return;

    uint32_t now = GetCurrentTick();
    if (m_bDownloadFinished.load()) {
        // Hide after 3.5 seconds once finished
        if (m_finishTime > 0 && (now - m_finishTime > 3500)) {
            return;
        }
    }

    if (m_totalModels.load() == 0 && m_bDownloadFinished.load()) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    float cardWidth = 380.0f;
    float cardHeight = 82.0f;
    float posX = (io.DisplaySize.x - cardWidth) * 0.5f;
    float posY = 24.0f;

    ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(cardWidth, cardHeight), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.043f, 0.059f, 0.098f, 0.94f)); // #0B0F19
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.008f, 0.518f, 0.780f, 0.90f));   // #0284C7
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));

    if (ImGui::Begin("##03DL_DownloadCard", nullptr, flags)) {
        ImGui::TextColored(ImVec4(0.220f, 0.741f, 0.973f, 1.0f), "0.3.DL SERVER ARTWORK"); // #38BDF8
        ImGui::SameLine();
        if (m_bDownloadFinished.load()) {
            ImGui::TextColored(ImVec4(0.133f, 0.773f, 0.369f, 1.0f), "[Synchronized]");
        } else if (m_bDownloading.load()) {
            ImGui::TextColored(ImVec4(0.961f, 0.620f, 0.043f, 1.0f), "[Downloading]");
        } else {
            ImGui::TextColored(ImVec4(0.70f, 0.70f, 0.70f, 1.0f), "[Checking Cache]");
        }

        int total = m_totalModels.load();
        int downloaded = m_downloadedModels.load();
        float fraction = (total > 0) ? (static_cast<float>(downloaded) / static_cast<float>(total)) : 1.0f;
        fraction = std::clamp(fraction, 0.0f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.008f, 0.518f, 0.780f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.11f, 0.17f, 1.0f));
        
        char progressOverlay[64];
        if (total > 0) {
            snprintf(progressOverlay, sizeof(progressOverlay), "%d / %d models (%.0f%%)", downloaded, total, fraction * 100.0f);
        } else {
            snprintf(progressOverlay, sizeof(progressOverlay), "No custom models");
        }

        ImGui::ProgressBar(fraction, ImVec2(-1.0f, 18.0f), progressOverlay);
        ImGui::PopStyleColor(2);

        if (!m_currentFileName.empty() && m_bDownloading.load()) {
            ImGui::TextDisabled("File: %s", m_currentFileName.c_str());
        } else if (m_bDownloadFinished.load()) {
            ImGui::TextColored(ImVec4(0.220f, 0.741f, 0.973f, 1.0f), "All models loaded. Spawn enabled.");
        } else {
            ImGui::TextDisabled("Validating local asset integrity...");
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}
