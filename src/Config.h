#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <cstdint>

namespace op25gateway {

class Config {
public:
    Config();

    bool load(const std::string& filename = "config.yml");

    // OP25 receiver settings
    uint16_t getOP25ListenPort() const { return m_op25ListenPort; }

    // FNE settings
    std::string getFneHost() const { return m_fneHost; }
    uint16_t getFnePort() const { return m_fnePort; }
    std::string getFnePassword() const { return m_fnePassword; }
    uint32_t getFnePeerId() const { return m_fnePeerId; }

    // Gateway settings
    uint32_t getGatewayTalkgroup() const { return m_gatewayTalkgroup; }
    uint32_t getGatewaySourceId() const { return m_gatewaySourceId; }
    uint32_t getCallTimeout() const { return m_callTimeout; }

    // Logging settings
    int getLogLevel() const { return m_logLevel; }
    std::string getLogFile() const { return m_logFile; }

private:
    // OP25
    uint16_t m_op25ListenPort;

    // FNE
    std::string m_fneHost;
    uint16_t m_fnePort;
    std::string m_fnePassword;
    uint32_t m_fnePeerId;

    // Gateway
    uint32_t m_gatewayTalkgroup;
    uint32_t m_gatewaySourceId;
    uint32_t m_callTimeout;

    // Logging
    int m_logLevel;
    std::string m_logFile;
};

} // namespace op25gateway

#endif // CONFIG_H
