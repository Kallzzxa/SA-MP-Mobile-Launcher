#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include "../../vendor/raknet/PacketPriority.h"
#include "../../vendor/raknet/BitStream.h"
#include "../../vendor/raknet/RakClientInterface.h"
#include "../../vendor/raknet/NetworkTypes.h"

// Model types in SA-MP 0.3.DL wire format
enum class DLModelType : uint8_t {
    Skin = 1,
    Object = 2
};

struct DLModelInfo {
    uint32_t poolID;
    int32_t  count;
    uint8_t  type;          // 1 = Skin, 2 = Object
    int32_t  virtualWorld;
    int32_t  baseId;
    int32_t  newId;
    uint32_t dffChecksum;
    uint32_t txdChecksum;
    uint32_t dffSize;
    uint32_t txdSize;
    uint8_t  timeOn;
    uint8_t  timeOff;
    bool     dffCached;
    bool     txdCached;
    int16_t  virtualModelId; // Safe internal range: 18631..19999
};

class DLManager {
public:
    static DLManager& Instance();

    void Initialize(const std::string& serverIp, int serverPort, const std::string& storagePath, RakClientInterface* pRakClient);
    void Reset();
    void ProcessTick();
    void RenderImGui();

    // RPC Handlers
    void OnModelRequest(RPCParameters* rpcParams);
    void OnModelFile(RPCParameters* rpcParams);
    void OnDownloadCompleted(RPCParameters* rpcParams);

    // Cache & Integrity Helpers
    std::string GetCachePath() const { return m_cachePath; }
    std::string GetModelPath(uint32_t crc, bool isDff) const;
    bool IsModelCached(uint32_t crc, uint32_t expectedSize, bool isDff) const;
    static uint32_t CalculateCRC32(const void* data, size_t length);
    static uint32_t CalculateFileCRC32(const std::string& filepath);

    // Virtual Model Mapping (Part 4 solution)
    int16_t GetVirtualModelId(int32_t networkId);
    int32_t GetNetworkModelId(int16_t virtualId);

    bool IsInitialized() const { return m_bInitialized.load(); }
    bool IsDownloadFinished() const { return m_bDownloadFinished.load(); }
    int GetTotalModels() const { return m_totalModels.load(); }
    int GetDownloadedModels() const { return m_downloadedModels.load(); }

    void CheckPendingDownloads();
    void SendFinishDownload();

private:
    DLManager();
    ~DLManager();

    void RequestNextFile();
    void DownloadWorker(const std::string& url, uint32_t crc, uint32_t expectedSize, bool isDff);

    std::string m_serverIp;
    int m_serverPort;
    std::string m_cachePath;
    RakClientInterface* m_pRakClient;

    mutable std::mutex m_mutex;
    std::vector<DLModelInfo> m_models;
    std::unordered_map<int32_t, int16_t> m_networkToVirtual;
    std::unordered_map<int16_t, int32_t> m_virtualToNetwork;
    int16_t m_nextVirtualId;

    // Download queue
    struct PendingFile {
        uint32_t crc;
        uint32_t size;
        bool isDff;
        std::string url;
    };
    std::vector<PendingFile> m_requestQueue;
    std::atomic<bool> m_bInitialized{false};
    std::atomic<bool> m_bDownloading{false};
    std::atomic<bool> m_bDownloadFinished{false};
    std::atomic<int> m_totalModels{0};
    std::atomic<int> m_downloadedModels{0};

    uint32_t m_connectTime{0};
    uint32_t m_finishTime{0};
    std::string m_currentFileName;
    float m_currentFileProgress{0.0f};
};
