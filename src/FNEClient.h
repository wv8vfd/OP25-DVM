#ifndef FNECLIENT_H
#define FNECLIENT_H

#include "P25Utils.h"

#include <cstdint>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <netinet/in.h>

namespace op25gateway {

// Connection state callback
using FNEConnectionCallback = std::function<void(bool connected)>;

class FNEClient {
public:
    FNEClient(const std::string& host, uint16_t port,
              uint32_t peerId, const std::string& password);
    ~FNEClient();

    FNEClient(const FNEClient&) = delete;
    FNEClient& operator=(const FNEClient&) = delete;

    bool connect();
    void disconnect();
    bool isConnected() const { return m_connected; }

    void enableAutoReconnect(bool enable = true);
    void setReconnectInterval(int seconds) { m_reconnectInterval = seconds; }

    void setConnectionCallback(FNEConnectionCallback callback) { m_connectionCallback = callback; }

    // Configuration
    void setIdentity(const std::string& identity) { m_identity = identity; }
    void setWACN(uint32_t wacn) { m_wacn = wacn; }
    void setSystemId(uint16_t sysId) { m_sysId = sysId; }

    // Send LDU1 (9 IMBE frames)
    void sendLDU1(const uint8_t imbe[9][IMBE_FRAME_SIZE],
                  uint32_t srcId, uint32_t dstId, bool firstLDU);

    // Send LDU2 (9 IMBE frames)
    void sendLDU2(const uint8_t imbe[9][IMBE_FRAME_SIZE],
                  uint32_t srcId, uint32_t dstId);

    // Send TDU (terminator)
    void sendTDU(uint32_t srcId, uint32_t dstId, bool grantDemand = false);

    // Start new voice stream
    void startStream(uint32_t srcId, uint32_t dstId);

    // End voice stream
    void endStream(uint32_t srcId, uint32_t dstId);

private:
    bool authenticate();
    void pingThread();
    void receiveThread();
    void reconnectThread();

    bool sendToFNE(const uint8_t* data, size_t len);

    // Configuration
    std::string m_host;
    uint16_t m_port;
    uint32_t m_peerId;
    std::string m_password;
    std::string m_identity;
    uint32_t m_wacn;
    uint16_t m_sysId;

    // Socket
    int m_socket;
    struct sockaddr_in m_fneAddr;

    // State
    std::atomic<bool> m_connected;
    std::atomic<bool> m_running;

    // Stream state
    uint32_t m_streamId;
    uint16_t m_seq;
    uint32_t m_timestamp;

    // Threads
    std::thread m_pingThread;
    std::thread m_receiveThread;
    std::thread m_reconnectThread;
    std::mutex m_sendMutex;
    std::mutex m_reconnectMutex;

    // Reconnection
    std::atomic<bool> m_reconnectEnabled;
    int m_reconnectInterval;

    // Callback
    FNEConnectionCallback m_connectionCallback;
};

} // namespace op25gateway

#endif // FNECLIENT_H
