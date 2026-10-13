#include "dlmanager.h"
#include "../../main.h"
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

DLManager& DLManager::Instance() {
    static DLManager s_instance;
    return s_instance;
}

DLManager::DLManager()
    : m_serverPort(7777)
    , m_pRakClient(nullptr)
    , m_nextVirtualId(18631)
    , m_bDownloading(false)
    , m_bDownloadFinished(false)
    , m_totalModels(0)
    , m_downloadedModels(0)
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

    // Cache structure: <storagePath>/SAMP/cache/<serverIp>.<serverPort>/
    char cacheDir[512];
    snprintf(cacheDir, sizeof(cacheDir), "%sSAMP/cache/%s.%d/", storagePath.c_str(), serverIp.c_str(), serverPort);
    m_cachePath = cacheDir;

    // Ensure cache directories exist
    mkdir((storagePath + "SAMP").c_str(), 0777);
    mkdir((storagePath + "SAMP/cache").c_str(), 0777);
    mkdir(m_cachePath.c_str(), 0777);
}

void DLManager::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_models.clear();
    m_requestQueue.clear();
    m_networkToVirtual.clear();
    m_virtualToNetwork.clear();
    m_nextVirtualId = 18631;
    m_bDownloading = false;
    m_bDownloadFinished = false;
    m_totalModels = 0;
    m_downloadedModels = 0;
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
        // Wrap around safe reserve slot
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

void DLManager::OnModelRequest(RPCParameters* rpcParams) {
    unsigned char* data = reinterpret_cast<unsigned char*>(rpcParams->input);
    int bitLength = rpcParams->numberOfBitsOfData;
    int byteLength = bitLength / 8;
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

    if (info.poolID + 1 == static_cast<uint32_t>(info.count)) {
        CheckPendingDownloads();
    }
}

void DLManager::CheckPendingDownloads() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_requestQueue.empty()) {
        m_bDownloadFinished = true;
        SendFinishDownload();
    } else {
        RequestNextFile();
    }
}

void DLManager::RequestNextFile() {
    if (m_requestQueue.empty()) {
        m_bDownloadFinished = true;
        SendFinishDownload();
        return;
    }
    if (m_bDownloading.load()) {
        return;
    }

    PendingFile pf = m_requestQueue.front();
    if (!m_pRakClient) return;

    RakNet::BitStream bs;
    bs.Write(pf.crc);

    int rpcId = pf.isDff ? RPC_RequestDff : RPC_RequestTxd;
    m_pRakClient->RPC(&rpcId, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
}

void DLManager::OnModelFile(RPCParameters* rpcParams) {
    unsigned char* data = reinterpret_cast<unsigned char*>(rpcParams->input);
    int bitLength = rpcParams->numberOfBitsOfData;
    int byteLength = bitLength / 8;
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

    // Read response & separate headers from body
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
                }
            }
        } else {
            fwrite(recvBuf, 1, bytes, outFp);
        }
    }
    fclose(outFp);
    close(sock);

    // Verify CRC & Size
    uint32_t actualCrc = CalculateFileCRC32(tempPath);
    if (actualCrc == crc) {
        std::string finalPath = GetModelPath(crc, isDff);
        rename(tempPath, finalPath.c_str());
        m_downloadedModels++;
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
    RakNet::BitStream bs;
    uint8_t unused = 0x80;
    bs.Write(unused);
    m_pRakClient->RPC(&RPC_FinishDownload, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
}

void DLManager::OnDownloadCompleted(RPCParameters* rpcParams) {
    m_bDownloadFinished = true;
}
