#include "FNEClient.h"
#include "Logger.h"

#include <sstream>
#include <iomanip>
#include <cstring>
#include <vector>

#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <openssl/sha.h>

namespace op25gateway {

FNEClient::FNEClient(const std::string& host, uint16_t port,
                     uint32_t peerId, const std::string& password)
    : m_host(host)
    , m_port(port)
    , m_peerId(peerId)
    , m_password(password)
    , m_identity("OP25-Gateway")
    , m_wacn(0x92C19)
    , m_sysId(0x50E)
    , m_socket(-1)
    , m_connected(false)
    , m_running(false)
    , m_streamId(0)
    , m_seq(0)
    , m_timestamp(0)
    , m_reconnectEnabled(false)
    , m_reconnectInterval(10)
{
    std::memset(&m_fneAddr, 0, sizeof(m_fneAddr));
}

FNEClient::~FNEClient() {
    m_reconnectEnabled = false;
    if (m_reconnectThread.joinable()) {
        m_reconnectThread.join();
    }
    disconnect();
}

bool FNEClient::connect() {
    std::lock_guard<std::mutex> lock(m_reconnectMutex);

    if (m_connected) return true;

    // Cleanup existing state
    if (m_running) {
        m_running = false;
        if (m_pingThread.joinable()) m_pingThread.join();
        if (m_receiveThread.joinable()) m_receiveThread.join();
    }

    if (m_socket >= 0) {
        shutdown(m_socket, SHUT_RDWR);
        close(m_socket);
        m_socket = -1;
    }

    LOG_INFO("FNE: Connecting to " + m_host + ":" + std::to_string(m_port));

    // Create UDP socket
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0) {
        LOG_ERROR("FNE: Failed to create socket");
        return false;
    }

    // Resolve FNE address
    struct hostent* host = gethostbyname(m_host.c_str());
    if (!host) {
        LOG_ERROR("FNE: Failed to resolve address");
        close(m_socket);
        m_socket = -1;
        return false;
    }

    m_fneAddr.sin_family = AF_INET;
    m_fneAddr.sin_port = htons(m_port);
    std::memcpy(&m_fneAddr.sin_addr, host->h_addr, host->h_length);

    // Connect UDP socket
    if (::connect(m_socket, (struct sockaddr*)&m_fneAddr, sizeof(m_fneAddr)) < 0) {
        LOG_ERROR("FNE: Failed to connect socket");
        close(m_socket);
        m_socket = -1;
        return false;
    }

    // Authenticate
    if (!authenticate()) {
        LOG_ERROR("FNE: Authentication failed");
        close(m_socket);
        m_socket = -1;
        return false;
    }

    m_connected = true;
    m_running = true;

    // Start threads
    m_pingThread = std::thread(&FNEClient::pingThread, this);
    m_receiveThread = std::thread(&FNEClient::receiveThread, this);

    LOG_INFO("FNE: Connected successfully");

    if (m_connectionCallback) {
        m_connectionCallback(true);
    }

    return true;
}

void FNEClient::disconnect() {
    if (!m_connected && !m_running) return;

    m_reconnectEnabled = false;
    m_running = false;
    m_connected = false;

    if (m_socket >= 0) {
        shutdown(m_socket, SHUT_RDWR);
        close(m_socket);
        m_socket = -1;
    }

    if (m_pingThread.joinable()) m_pingThread.join();
    if (m_receiveThread.joinable()) m_receiveThread.join();
    if (m_reconnectThread.joinable()) m_reconnectThread.join();

    if (m_connectionCallback) {
        m_connectionCallback(false);
    }

    LOG_INFO("FNE: Disconnected");
}

void FNEClient::enableAutoReconnect(bool enable) {
    m_reconnectEnabled = enable;

    if (enable && !m_reconnectThread.joinable()) {
        m_reconnectThread = std::thread(&FNEClient::reconnectThread, this);
    }
}

void FNEClient::reconnectThread() {
    LOG_INFO("FNE: Reconnection thread started");

    while (m_reconnectEnabled) {
        if (!m_connected) {
            LOG_INFO("FNE: Attempting connection...");

            if (connect()) {
                LOG_INFO("FNE: Reconnection successful");
            } else {
                std::stringstream ss;
                ss << "FNE: Connection failed, retrying in " << m_reconnectInterval << " seconds...";
                LOG_WARN(ss.str());
            }
        }

        for (int i = 0; i < m_reconnectInterval && m_reconnectEnabled; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    LOG_INFO("FNE: Reconnection thread stopped");
}

bool FNEClient::authenticate() {
    uint32_t loginStreamId = rand();

    // Build RPTL (login request)
    uint8_t rptl[40];
    std::memset(rptl, 0, sizeof(rptl));
    P25Utils::buildDVMHeader(rptl, NET_FUNC_RPTL, NET_SUBFUNC_NOP, loginStreamId,
                              m_peerId, m_seq, m_timestamp, 8);

    rptl[32] = 'R';
    rptl[33] = 'P';
    rptl[34] = 'T';
    rptl[35] = 'L';
    rptl[36] = (m_peerId >> 24) & 0xFF;
    rptl[37] = (m_peerId >> 16) & 0xFF;
    rptl[38] = (m_peerId >> 8) & 0xFF;
    rptl[39] = m_peerId & 0xFF;

    P25Utils::insertDVMCrc(rptl, 40);

    if (!sendToFNE(rptl, 40)) return false;

    // Wait for challenge
    uint8_t response[256];
    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);
    FD_SET(m_socket, &fds);
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    if (select(m_socket + 1, &fds, nullptr, nullptr, &tv) <= 0) {
        LOG_ERROR("FNE: Timeout waiting for challenge");
        return false;
    }

    ssize_t respLen = recv(m_socket, response, sizeof(response), 0);
    if (respLen <= 0 || response[18] != NET_FUNC_ACK) {
        LOG_ERROR("FNE: Login rejected");
        return false;
    }

    // Extract salt from response
    uint32_t salt = ((uint32_t)response[38] << 24) | ((uint32_t)response[39] << 16) |
                    ((uint32_t)response[40] << 8) | (uint32_t)response[41];

    // Compute hash: SHA256(salt + password)
    std::vector<uint8_t> hashData;
    hashData.push_back((salt >> 24) & 0xFF);
    hashData.push_back((salt >> 16) & 0xFF);
    hashData.push_back((salt >> 8) & 0xFF);
    hashData.push_back(salt & 0xFF);
    hashData.insert(hashData.end(), m_password.begin(), m_password.end());

    uint8_t hash[32];
    SHA256(hashData.data(), hashData.size(), hash);

    // Build RPTK (auth response)
    uint8_t rptk[72];
    std::memset(rptk, 0, sizeof(rptk));
    P25Utils::buildDVMHeader(rptk, NET_FUNC_RPTK, NET_SUBFUNC_NOP, loginStreamId,
                              m_peerId, m_seq, m_timestamp, 40);

    rptk[32] = 'R';
    rptk[33] = 'P';
    rptk[34] = 'T';
    rptk[35] = 'K';
    rptk[36] = (m_peerId >> 24) & 0xFF;
    rptk[37] = (m_peerId >> 16) & 0xFF;
    rptk[38] = (m_peerId >> 8) & 0xFF;
    rptk[39] = m_peerId & 0xFF;
    std::memcpy(rptk + 40, hash, 32);

    P25Utils::insertDVMCrc(rptk, 72);

    if (!sendToFNE(rptk, 72)) return false;

    // Wait for ACK
    FD_ZERO(&fds);
    FD_SET(m_socket, &fds);
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    if (select(m_socket + 1, &fds, nullptr, nullptr, &tv) <= 0) {
        LOG_ERROR("FNE: Timeout waiting for auth ACK");
        return false;
    }

    respLen = recv(m_socket, response, sizeof(response), 0);
    if (respLen <= 0 || response[18] != NET_FUNC_ACK) {
        LOG_ERROR("FNE: Auth rejected");
        return false;
    }

    LOG_INFO("FNE: Auth successful, sending config");

    // Build RPTC (configuration)
    std::stringstream configJson;
    configJson << "{\"identity\":\"" << m_identity << "\","
               << "\"rxFrequency\":449000000,"
               << "\"txFrequency\":444000000,"
               << "\"info\":{\"latitude\":0.0,\"longitude\":0.0},"
               << "\"channel\":{\"txPower\":1},"
               << "\"software\":\"OP25-Gateway-1.0\"}";

    std::string config = configJson.str();
    size_t rptcLen = 32 + 8 + config.length();
    std::vector<uint8_t> rptc(rptcLen);

    P25Utils::buildDVMHeader(rptc.data(), NET_FUNC_RPTC, NET_SUBFUNC_NOP, loginStreamId,
                              m_peerId, m_seq, m_timestamp, 8 + config.length());

    rptc[32] = 'R';
    rptc[33] = 'P';
    rptc[34] = 'T';
    rptc[35] = 'C';
    rptc[36] = 0x00;
    rptc[37] = 0x00;
    rptc[38] = 0x00;
    rptc[39] = 0x00;
    std::memcpy(rptc.data() + 40, config.c_str(), config.length());

    P25Utils::insertDVMCrc(rptc.data(), rptcLen);

    if (!sendToFNE(rptc.data(), rptcLen)) return false;

    // Wait for final ACK
    FD_ZERO(&fds);
    FD_SET(m_socket, &fds);
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    if (select(m_socket + 1, &fds, nullptr, nullptr, &tv) <= 0) {
        LOG_ERROR("FNE: Timeout waiting for config ACK");
        return false;
    }

    respLen = recv(m_socket, response, sizeof(response), 0);
    if (respLen <= 0 || response[18] != NET_FUNC_ACK) {
        LOG_ERROR("FNE: Config rejected");
        return false;
    }

    return true;
}

void FNEClient::pingThread() {
    while (m_running) {
        if (m_connected) {
            uint8_t ping[43];
            std::memset(ping, 0, sizeof(ping));

            uint32_t pingStreamId = (rand() & 0x7FFFFFFF) | 0x00000001;
            P25Utils::buildDVMHeader(ping, NET_FUNC_PING, NET_SUBFUNC_NOP, pingStreamId,
                                      m_peerId, m_seq, m_timestamp, 11);

            ping[39] = (m_peerId >> 24) & 0xFF;
            ping[40] = (m_peerId >> 16) & 0xFF;
            ping[41] = (m_peerId >> 8) & 0xFF;
            ping[42] = m_peerId & 0xFF;

            P25Utils::insertDVMCrc(ping, 43);
            sendToFNE(ping, 43);
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void FNEClient::receiveThread() {
    uint8_t buffer[1024];

    while (m_running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(m_socket, &fds);

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int selectResult = select(m_socket + 1, &fds, nullptr, nullptr, &tv);
        if (selectResult < 0) {
            if (m_connected) {
                LOG_ERROR("FNE: Select error, connection lost");
                m_connected = false;
                m_running = false;

                if (m_connectionCallback) {
                    m_connectionCallback(false);
                }
            }
            break;
        }

        if (selectResult == 0) continue;

        ssize_t len = recv(m_socket, buffer, sizeof(buffer), 0);
        if (len <= 0) {
            if (m_connected && len < 0) {
                LOG_ERROR("FNE: Connection lost");
                m_connected = false;

                if (m_connectionCallback) {
                    m_connectionCallback(false);
                }
            }
            continue;
        }

        // Handle PONG responses
        if (len >= 32 && buffer[18] == NET_FUNC_PONG) {
            LOG_DEBUG("FNE: Received PONG");
            continue;
        }
    }
}

bool FNEClient::sendToFNE(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(m_sendMutex);
    if (m_socket < 0) return false;

    ssize_t sent = send(m_socket, data, len, 0);
    return sent == (ssize_t)len;
}

void FNEClient::startStream(uint32_t srcId, uint32_t dstId) {
    m_streamId = (rand() & 0x7FFFFFFF) | 0x00000001;

    std::stringstream ss;
    ss << "FNE: Starting voice stream - src=" << srcId << " dst=" << dstId
       << " streamId=0x" << std::hex << m_streamId;
    LOG_INFO(ss.str());

    // Send TDU with grant demand to trigger CC announcement
    sendTDU(srcId, dstId, true);
}

void FNEClient::endStream(uint32_t srcId, uint32_t dstId) {
    LOG_INFO("FNE: Ending voice stream");
    sendTDU(srcId, dstId, false);
}

void FNEClient::sendLDU1(const uint8_t imbe[9][IMBE_FRAME_SIZE],
                          uint32_t srcId, uint32_t dstId, bool firstLDU) {
    if (!m_connected) return;

    uint8_t ldu[P25_LDU1_LENGTH];
    P25Utils::buildLDU1(ldu, imbe, srcId, dstId, m_wacn, m_sysId, firstLDU);

    size_t totalLen = 32 + P25_LDU1_LENGTH;
    std::vector<uint8_t> packet(totalLen);

    P25Utils::buildDVMHeader(packet.data(), NET_FUNC_PROTOCOL, NET_SUBFUNC_P25,
                              m_streamId, m_peerId, m_seq, m_timestamp, P25_LDU1_LENGTH);
    std::memcpy(packet.data() + 32, ldu, P25_LDU1_LENGTH);
    P25Utils::insertDVMCrc(packet.data(), totalLen);

    sendToFNE(packet.data(), totalLen);

    LOG_DEBUG("FNE: Sent LDU1");
}

void FNEClient::sendLDU2(const uint8_t imbe[9][IMBE_FRAME_SIZE],
                          uint32_t srcId, uint32_t dstId) {
    if (!m_connected) return;

    uint8_t ldu[P25_LDU2_LENGTH];
    P25Utils::buildLDU2(ldu, imbe, srcId, dstId, m_wacn, m_sysId);

    size_t totalLen = 32 + P25_LDU2_LENGTH;
    std::vector<uint8_t> packet(totalLen);

    P25Utils::buildDVMHeader(packet.data(), NET_FUNC_PROTOCOL, NET_SUBFUNC_P25,
                              m_streamId, m_peerId, m_seq, m_timestamp, P25_LDU2_LENGTH);
    std::memcpy(packet.data() + 32, ldu, P25_LDU2_LENGTH);
    P25Utils::insertDVMCrc(packet.data(), totalLen);

    sendToFNE(packet.data(), totalLen);

    LOG_DEBUG("FNE: Sent LDU2");
}

void FNEClient::sendTDU(uint32_t srcId, uint32_t dstId, bool grantDemand) {
    if (!m_connected) return;

    uint8_t tdu[P25_TDU_LENGTH];
    P25Utils::buildTDU(tdu, srcId, dstId, m_wacn, m_sysId, grantDemand);

    size_t totalLen = 32 + P25_TDU_LENGTH;
    std::vector<uint8_t> packet(totalLen);

    bool endOfCall = !grantDemand;
    P25Utils::buildDVMHeader(packet.data(), NET_FUNC_PROTOCOL, NET_SUBFUNC_P25,
                              m_streamId, m_peerId, m_seq, m_timestamp, P25_TDU_LENGTH, endOfCall);
    std::memcpy(packet.data() + 32, tdu, P25_TDU_LENGTH);
    P25Utils::insertDVMCrc(packet.data(), totalLen);

    sendToFNE(packet.data(), totalLen);

    if (grantDemand) {
        LOG_DEBUG("FNE: Sent TDU with grant demand");
    } else {
        LOG_DEBUG("FNE: Sent TDU (call termination)");
    }
}

} // namespace op25gateway
