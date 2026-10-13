#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include "../../vendor/raknet/PacketPriority.h"
#include "../../vendor/raknet/BitStream.h"
#include "../../vendor/raknet/RakClientInterface.h"
#include "../../vendor/raknet/NetworkTypes.h"

// Model types in SA-MP 0.3.DL wire format
enum class DLModelType : uint8_t {
    Skin = 1,
    Object = 2
};

// Number of parallel HTTP workers, matching SA-MP PC's artwork downloader pool.
#define DL_MAX_CONCURRENT 5

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

// One HTTP asset waiting to be fetched. Created when RPC 179 registers a model
// whose file is missing from cache.
struct DLFileTask {
    uint32_t crc{0};
    uint32_t size{0};
    bool     isDff{true};
    std::string url;         // filled by RPC 183 (ModelUrl)
    std::string hexName;     // 0x<CRC>.dff for logging / UI
    int      attempts{0};
    bool     started{false};  // RPC 181/182 sent for this file
    bool     done{false};
    bool     failed{false};
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
    int GetActiveWorkers() const { return m_activeWorkers.load(); }
    float GetCurrentSpeedKBps() const { return m_speedKBps.load(); }
    void GetProgressStats(size_t& queued, size_t& active, size_t& done, size_t& failed) const;

    void SendFinishDownload();
    void ToggleOverlay();
    bool IsOverlayForced() const { return m_bForceShowOverlay; }

private:
    DLManager();
    ~DLManager();

    // --- RPC handshake side (runs on the RakNet update thread, never blocks) ---
    void RequestFileFromServer(const DLFileTask& task);
    void ProcessRpcOutbox();
    void RecomputeProgress();

    // --- HTTP worker side (runs on DL_MAX_CONCURRENT detached threads) ---
    void WorkerLoop();
    bool FetchToFile(const DLFileTask& task, uint64_t& outBytes);

    std::string m_serverIp;
    int m_serverPort;
    std::string m_cachePath;
    RakClientInterface* m_pRakClient;

    mutable std::mutex m_mutex;

    // Model registry
    std::vector<DLModelInfo> m_models;
    std::unordered_map<int32_t, int16_t> m_networkToVirtual;
    std::unordered_map<int16_t, int32_t> m_virtualToNetwork;
    int16_t m_nextVirtualId{18631};

    // Download queue. Keyed by crc so a duplicate RPC 179 cannot enqueue twice.
    std::deque<DLFileTask> m_tasks;
    std::unordered_set<uint32_t> m_knownCrc;
    std::unordered_set<uint32_t> m_rpcRequested;

    // RPC outbox: workers/callbacks must not touch RakClient, only push here.
    // ProcessTick() drains it on the game thread.
    std::deque<std::pair<uint32_t, bool>> m_rpcOutbox; // (crc, isDff)
    std::atomic<bool> m_bFinishRequested{false};

    // Worker pool
    std::mutex m_workMutex;
    std::condition_variable m_workCv;
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_bShutdown{false};
    std::atomic<int> m_activeWorkers{0};

    std::atomic<bool> m_bInitialized{false};
    std::atomic<bool> m_bDownloadFinished{false};
    std::atomic<int>  m_totalModels{0};
    std::atomic<int>  m_downloadedModels{0};
    std::atomic<int>  m_failedModels{0};
    std::atomic<float> m_speedKBps{0.0f};

    uint32_t m_lastSpeedSampleTick{0};
    uint64_t m_lastSpeedSampleBytes{0};
    std::atomic<uint64_t> m_totalBytesRx{0};

    uint32_t m_connectTime{0};
    uint32_t m_finishTime{0};
    bool m_bForceShowOverlay{false};
    std::string m_lastActivity;
};
