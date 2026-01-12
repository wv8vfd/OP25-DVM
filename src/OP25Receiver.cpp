#include "OP25Receiver.h"
#include "Logger.h"

#include <sstream>
#include <cstring>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace op25gateway {

OP25Receiver::OP25Receiver(uint16_t port)
    : m_port(port)
    , m_socket(-1)
    , m_running(false)
    , m_packetsReceived(0)
    , m_packetsInvalid(0)
{
}

OP25Receiver::~OP25Receiver() {
    stop();
}

bool OP25Receiver::start() {
    if (m_running) return true;

    // Create UDP socket
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0) {
        LOG_ERROR("OP25: Failed to create socket");
        return false;
    }

    // Allow socket reuse
    int opt = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to port
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);

    if (bind(m_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("OP25: Failed to bind to port " + std::to_string(m_port));
        close(m_socket);
        m_socket = -1;
        return false;
    }

    m_running = true;
    m_receiveThread = std::thread(&OP25Receiver::receiveLoop, this);

    LOG_INFO("OP25: Listening on UDP port " + std::to_string(m_port));
    return true;
}

void OP25Receiver::stop() {
    if (!m_running) return;

    m_running = false;

    if (m_socket >= 0) {
        shutdown(m_socket, SHUT_RDWR);
        close(m_socket);
        m_socket = -1;
    }

    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }

    LOG_INFO("OP25: Receiver stopped");
}

void OP25Receiver::receiveLoop() {
    uint8_t buffer[256];
    struct sockaddr_in senderAddr;
    socklen_t senderLen = sizeof(senderAddr);

    while (m_running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(m_socket, &fds);

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int selectResult = select(m_socket + 1, &fds, nullptr, nullptr, &tv);
        if (selectResult < 0) {
            if (m_running) {
                LOG_ERROR("OP25: Select error");
            }
            break;
        }

        if (selectResult == 0) {
            continue;  // Timeout, check if still running
        }

        ssize_t len = recvfrom(m_socket, buffer, sizeof(buffer), 0,
                                (struct sockaddr*)&senderAddr, &senderLen);

        if (len <= 0) {
            if (m_running) {
                LOG_ERROR("OP25: Receive error");
            }
            continue;
        }

        // Parse the OP25 packet
        OP25Packet packet;
        if (!P25Utils::parseOP25Packet(buffer, len, packet)) {
            m_packetsInvalid++;

            if (m_packetsInvalid % 100 == 1) {
                std::stringstream ss;
                ss << "OP25: Invalid packet (len=" << len << ", total invalid=" << m_packetsInvalid << ")";
                LOG_WARN(ss.str());
            }
            continue;
        }

        m_packetsReceived++;

        // Debug logging for first few packets
        if (m_packetsReceived <= 5 || m_packetsReceived % 1000 == 0) {
            std::stringstream ss;
            ss << "OP25: Received packet #" << m_packetsReceived
               << " - NAC=0x" << std::hex << packet.nac << std::dec
               << " TG=" << packet.talkgroup
               << " SRC=" << packet.sourceId
               << " Type=" << (int)packet.frameType
               << " Index=" << (int)packet.voiceIndex;
            LOG_DEBUG(ss.str());
        }

        // Invoke callback
        if (m_frameCallback) {
            m_frameCallback(packet);
        }
    }
}

} // namespace op25gateway
