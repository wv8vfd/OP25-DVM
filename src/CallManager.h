#ifndef CALLMANAGER_H
#define CALLMANAGER_H

#include "P25Utils.h"
#include "FNEClient.h"

#include <cstdint>
#include <chrono>
#include <mutex>
#include <atomic>
#include <thread>

namespace op25gateway {

enum class CallState {
    IDLE,
    ACTIVE
};

class CallManager {
public:
    CallManager(FNEClient& fneClient);
    ~CallManager();

    CallManager(const CallManager&) = delete;
    CallManager& operator=(const CallManager&) = delete;

    void start();
    void stop();

    // Process incoming IMBE frame from OP25
    void processIMBEFrame(const OP25Packet& packet);

    // Configuration
    void setTalkgroupOverride(uint32_t tg) { m_talkgroupOverride = tg; }
    void setSourceIdOverride(uint32_t srcId) { m_sourceIdOverride = srcId; }
    void setCallTimeout(uint32_t timeoutMs) { m_callTimeout = timeoutMs; }

    // Statistics
    uint64_t getCallCount() const { return m_callCount; }
    uint64_t getLDU1Count() const { return m_ldu1Count; }
    uint64_t getLDU2Count() const { return m_ldu2Count; }

private:
    void timeoutThread();
    void startCall(uint32_t srcId, uint32_t dstId);
    void endCall();
    void sendLDU();

    FNEClient& m_fneClient;

    // Call state
    CallState m_state;
    uint32_t m_currentSrcId;
    uint32_t m_currentDstId;
    std::chrono::steady_clock::time_point m_lastPacketTime;
    bool m_firstLDU;

    // IMBE frame buffer (accumulate 9 frames for each LDU)
    uint8_t m_imbeBuffer[9][IMBE_FRAME_SIZE];
    int m_imbeCount;
    bool m_expectingLDU2;  // true = next 9 frames are LDU2, false = LDU1

    // Configuration
    uint32_t m_talkgroupOverride;
    uint32_t m_sourceIdOverride;
    uint32_t m_callTimeout;

    // Threading
    std::mutex m_mutex;
    std::atomic<bool> m_running;
    std::thread m_timeoutThread;

    // Statistics
    std::atomic<uint64_t> m_callCount;
    std::atomic<uint64_t> m_ldu1Count;
    std::atomic<uint64_t> m_ldu2Count;
};

} // namespace op25gateway

#endif // CALLMANAGER_H
