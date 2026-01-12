#include "Config.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <fstream>

namespace op25gateway {

Config::Config()
    : m_op25ListenPort(9999)
    , m_fneHost("127.0.0.1")
    , m_fnePort(62031)
    , m_fnePassword("PASSWORD")
    , m_fnePeerId(9000999)
    , m_gatewayTalkgroup(0)
    , m_gatewaySourceId(9000999)
    , m_callTimeout(1000)
    , m_logLevel(1)
    , m_logFile("gateway.log")
{
}

bool Config::load(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.good()) {
            std::cerr << "Config file not found: " << filename << std::endl;
            std::cerr << "Using default values" << std::endl;
            return false;
        }

        YAML::Node config = YAML::LoadFile(filename);

        // OP25 settings
        if (config["op25"]) {
            if (config["op25"]["listenPort"]) {
                m_op25ListenPort = config["op25"]["listenPort"].as<uint16_t>();
            }
        }

        // FNE settings
        if (config["fne"]) {
            if (config["fne"]["host"]) {
                m_fneHost = config["fne"]["host"].as<std::string>();
            }
            if (config["fne"]["port"]) {
                m_fnePort = config["fne"]["port"].as<uint16_t>();
            }
            if (config["fne"]["password"]) {
                m_fnePassword = config["fne"]["password"].as<std::string>();
            }
            if (config["fne"]["peerId"]) {
                m_fnePeerId = config["fne"]["peerId"].as<uint32_t>();
            }
        }

        // Gateway settings
        if (config["gateway"]) {
            if (config["gateway"]["talkgroup"]) {
                m_gatewayTalkgroup = config["gateway"]["talkgroup"].as<uint32_t>();
            }
            if (config["gateway"]["sourceId"]) {
                m_gatewaySourceId = config["gateway"]["sourceId"].as<uint32_t>();
            }
            if (config["gateway"]["callTimeout"]) {
                m_callTimeout = config["gateway"]["callTimeout"].as<uint32_t>();
            }
        }

        // Logging settings
        if (config["logging"]) {
            if (config["logging"]["level"]) {
                std::string levelStr = config["logging"]["level"].as<std::string>();
                if (levelStr == "DEBUG") m_logLevel = 0;
                else if (levelStr == "INFO") m_logLevel = 1;
                else if (levelStr == "WARN") m_logLevel = 2;
                else if (levelStr == "ERROR") m_logLevel = 3;
            }
            if (config["logging"]["file"]) {
                m_logFile = config["logging"]["file"].as<std::string>();
            }
        }

        std::cout << "Configuration loaded from " << filename << std::endl;
        return true;

    } catch (const YAML::Exception& e) {
        std::cerr << "Error parsing config file: " << e.what() << std::endl;
        return false;
    }
}

} // namespace op25gateway
