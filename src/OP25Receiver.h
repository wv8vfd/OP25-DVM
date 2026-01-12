#ifndef OP25RECEIVER_H
#define OP25RECEIVER_H

#include "P25Utils.h"

#include <cstdint>
#include <string>
#include <atomic>
#include <thread>
#include <functional>

namespace op25gateway {

// Callback for received IMBE frames
using OP25FrameCallback = std::function<void(const OP25Packet& packet)>;

class OP25Receiver {
public:
    OP25Receiver(uint16_t port);
    ~OP25Receiver();

    OP25Receiver(const OP25Receiver&) = delete;
    OP25Receiver& operator=(const OP25Receiver&) = delete;

    bool start();
    void stop();
    bool isRunning() const { return m_running; }

    void setFrameCallback(OP25FrameCallback callback) { m_frameCallback = callback; }

    // Statistics
    uint64_t getPacketsReceived() const { return m_packetsReceived; }
    uint64_t getPacketsInvalid() const { return m_packetsInvalid; }

private:
    void receiveLoop();

    uint16_t m_port;
    int m_socket;
    std::atomic<bool> m_running;
    std::thread m_receiveThread;

    OP25FrameCallback m_frameCallback;

    std::atomic<uint64_t> m_packetsReceived;
    std::atomic<uint64_t> m_packetsInvalid;
};

} // namespace op25gateway

#endif // OP25RECEIVER_H
