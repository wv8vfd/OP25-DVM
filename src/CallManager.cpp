#include "CallManager.h"
#include "Logger.h"

#include <sstream>
#include <cstring>

namespace op25gateway {

CallManager::CallManager(FNEClient& fneClient)
    : m_fneClient(fneClient)
    , m_state(CallState::IDLE)
    , m_currentSrcId(0)
    , m_currentDstId(0)
    , m_firstLDU(true)
    , m_imbeCount(0)
    , m_expectingLDU2(false)
    , m_talkgroupOverride(0)
    , m_sourceIdOverride(0)
    , m_callTimeout(1000)
    , m_running(false)
    , m_callCount(0)
    , m_ldu1Count(0)
    , m_ldu2Count(0)
{
    std::memset(m_imbeBuffer, 0, sizeof(m_imbeBuffer));
}

CallManager::~CallManager() {
    stop();
}

void CallManager::start() {
    if (m_running) return;

    m_running = true;
    m_timeoutThread = std::thread(&CallManager::timeoutThread, this);

    LOG_INFO("CallManager: Started");
}

void CallManager::stop() {
    if (!m_running) return;

    m_running = false;

    if (m_timeoutThread.joinable()) {
        m_timeoutThread.join();
    }

    // End any active call
    if (m_state == CallState::ACTIVE) {
        endCall();
    }

    LOG_INFO("CallManager: Stopped");
}

void CallManager::timeoutThread() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_state == CallState::ACTIVE) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - m_lastPacketTime).count();

            if (elapsed > m_callTimeout) {
                LOG_INFO("CallManager: Call timeout, ending call");
                endCall();
            }
        }
    }
}

void CallManager::processIMBEFrame(const OP25Packet& packet) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Get source and destination IDs (with optional overrides)
    uint32_t srcId = m_sourceIdOverride > 0 ? m_sourceIdOverride : packet.sourceId;
    uint32_t dstId = m_talkgroupOverride > 0 ? m_talkgroupOverride : packet.talkgroup;

    // Check for call start (transition from IDLE to ACTIVE)
    if (m_state == CallState::IDLE) {
        startCall(srcId, dstId);
    }

    // Update last packet time
    m_lastPacketTime = std::chrono::steady_clock::now();

    // Check if source/dest changed (new call within existing)
    if (m_state == CallState::ACTIVE &&
        (srcId != m_currentSrcId || dstId != m_currentDstId)) {
        std::stringstream ss;
        ss << "CallManager: Call parameters changed (src=" << srcId
           << " dst=" << dstId << "), restarting";
        LOG_INFO(ss.str());

        endCall();
        startCall(srcId, dstId);
    }

    // Validate frame index
    if (packet.voiceIndex > 8) {
        LOG_WARN("CallManager: Invalid voice index " + std::to_string(packet.voiceIndex));
        return;
    }

    // Store IMBE frame in buffer
    std::memcpy(m_imbeBuffer[packet.voiceIndex], packet.imbe, IMBE_FRAME_SIZE);

    // Track which frames we've received
    m_imbeCount++;

    // Log frame reception
    {
        std::stringstream ss;
        ss << "CallManager: Frame " << (int)packet.voiceIndex
           << " (type=" << (int)packet.frameType << ")"
           << " count=" << m_imbeCount;
        LOG_DEBUG(ss.str());
    }

    // Check if we have a complete LDU (9 frames)
    // The voiceIndex goes 0-8 for each LDU
    if (packet.voiceIndex == 8) {
        sendLDU();
        m_imbeCount = 0;
    }
}

void CallManager::startCall(uint32_t srcId, uint32_t dstId) {
    m_state = CallState::ACTIVE;
    m_currentSrcId = srcId;
    m_currentDstId = dstId;
    m_firstLDU = true;
    m_imbeCount = 0;
    m_expectingLDU2 = false;
    m_lastPacketTime = std::chrono::steady_clock::now();
    m_callCount++;

    std::stringstream ss;
    ss << "CallManager: Call started - src=" << srcId << " dst=" << dstId
       << " (call #" << m_callCount << ")";
    LOG_INFO(ss.str());

    // Notify FNE of new stream
    m_fneClient.startStream(srcId, dstId);
}

void CallManager::endCall() {
    if (m_state == CallState::IDLE) return;

    std::stringstream ss;
    ss << "CallManager: Call ended - src=" << m_currentSrcId
       << " dst=" << m_currentDstId
       << " (LDU1=" << m_ldu1Count << " LDU2=" << m_ldu2Count << ")";
    LOG_INFO(ss.str());

    // Send TDU to FNE
    m_fneClient.endStream(m_currentSrcId, m_currentDstId);

    m_state = CallState::IDLE;
    m_currentSrcId = 0;
    m_currentDstId = 0;
    m_imbeCount = 0;
    m_expectingLDU2 = false;
    m_firstLDU = true;
}

void CallManager::sendLDU() {
    if (m_state != CallState::ACTIVE) return;

    // Alternate between LDU1 and LDU2
    if (!m_expectingLDU2) {
        // Send LDU1
        m_fneClient.sendLDU1(m_imbeBuffer, m_currentSrcId, m_currentDstId, m_firstLDU);
        m_ldu1Count++;
        m_firstLDU = false;
        m_expectingLDU2 = true;

        LOG_DEBUG("CallManager: Sent LDU1 #" + std::to_string(m_ldu1Count));
    } else {
        // Send LDU2
        m_fneClient.sendLDU2(m_imbeBuffer, m_currentSrcId, m_currentDstId);
        m_ldu2Count++;
        m_expectingLDU2 = false;

        LOG_DEBUG("CallManager: Sent LDU2 #" + std::to_string(m_ldu2Count));
    }

    // Clear buffer for next LDU
    std::memset(m_imbeBuffer, 0, sizeof(m_imbeBuffer));
}

} // namespace op25gateway
