#include "Config.h"
#include "Logger.h"
#include "OP25Receiver.h"
#include "FNEClient.h"
#include "CallManager.h"

#include <iostream>
#include <sstream>
#include <csignal>
#include <atomic>

using namespace op25gateway;

std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutdown requested..." << std::endl;
        g_running = false;
    }
}

void printBanner() {
    std::cout << "========================================" << std::endl;
    std::cout << "  OP25-to-DVM Gateway" << std::endl;
    std::cout << "  Version 1.0" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
}

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -c <file>  Configuration file (default: config.yml)" << std::endl;
    std::cout << "  -h         Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    printBanner();

    // Parse command line arguments
    std::string configFile = "config.yml";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-c" && i + 1 < argc) {
            configFile = argv[++i];
        }
    }

    // Load configuration
    Config config;
    config.load(configFile);

    // Setup logging
    Logger::instance().setLevel(static_cast<LogLevel>(config.getLogLevel()));
    if (!config.getLogFile().empty()) {
        Logger::instance().setLogFile(config.getLogFile());
    }

    LOG_INFO("Configuration loaded");

    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Create FNE client
    FNEClient fneClient(
        config.getFneHost(),
        config.getFnePort(),
        config.getFnePeerId(),
        config.getFnePassword()
    );

    fneClient.setIdentity("OP25-Gateway");

    // Set connection callback
    fneClient.setConnectionCallback([](bool connected) {
        if (connected) {
            LOG_INFO("FNE connection established");
        } else {
            LOG_WARN("FNE connection lost");
        }
    });

    // Create call manager
    CallManager callManager(fneClient);
    callManager.setTalkgroupOverride(config.getGatewayTalkgroup());
    callManager.setSourceIdOverride(config.getGatewaySourceId());
    callManager.setCallTimeout(config.getCallTimeout());

    // Create OP25 receiver
    OP25Receiver op25Receiver(config.getOP25ListenPort());

    // Set frame callback
    op25Receiver.setFrameCallback([&callManager](const OP25Packet& packet) {
        callManager.processIMBEFrame(packet);
    });

    // Connect to FNE with auto-reconnect
    fneClient.enableAutoReconnect(true);
    fneClient.setReconnectInterval(10);

    LOG_INFO("Waiting for FNE connection...");

    // Wait for initial connection (or timeout after 30 seconds)
    for (int i = 0; i < 30 && g_running && !fneClient.isConnected(); i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!fneClient.isConnected()) {
        LOG_WARN("Could not connect to FNE, continuing anyway (will auto-reconnect)");
    }

    // Start call manager
    callManager.start();

    // Start OP25 receiver
    if (!op25Receiver.start()) {
        LOG_ERROR("Failed to start OP25 receiver");
        return 1;
    }

    LOG_INFO("Gateway running - Press Ctrl+C to stop");

    // Main loop
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Periodic stats logging
        static int statCounter = 0;
        if (++statCounter >= 60) {
            statCounter = 0;

            std::stringstream ss;
            ss << "Stats: OP25 packets=" << op25Receiver.getPacketsReceived()
               << " calls=" << callManager.getCallCount()
               << " LDU1=" << callManager.getLDU1Count()
               << " LDU2=" << callManager.getLDU2Count()
               << " FNE=" << (fneClient.isConnected() ? "connected" : "disconnected");
            LOG_INFO(ss.str());
        }
    }

    // Shutdown
    LOG_INFO("Shutting down...");

    op25Receiver.stop();
    callManager.stop();
    fneClient.disconnect();

    LOG_INFO("Shutdown complete");

    return 0;
}
