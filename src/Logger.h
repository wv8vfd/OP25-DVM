#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>

namespace op25gateway {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class Logger {
public:
    static Logger& instance();

    void setLevel(LogLevel level);
    void setLogFile(const std::string& path);

    void debug(const std::string& msg);
    void info(const std::string& msg);
    void warn(const std::string& msg);
    void error(const std::string& msg);

    void hexDump(const std::string& label, const uint8_t* data, size_t len);

private:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(LogLevel level, const std::string& msg);
    void writeToFile(const std::string& msg);
    void writeToConsole(const std::string& msg, LogLevel level);
    std::string levelToString(LogLevel level);
    std::string timestamp();

    LogLevel m_level;
    std::string m_logFile;
    std::ofstream m_fileStream;
    std::mutex m_mutex;
};

#define LOG_DEBUG(msg) op25gateway::Logger::instance().debug(msg)
#define LOG_INFO(msg) op25gateway::Logger::instance().info(msg)
#define LOG_WARN(msg) op25gateway::Logger::instance().warn(msg)
#define LOG_ERROR(msg) op25gateway::Logger::instance().error(msg)
#define LOG_HEXDUMP(label, data, len) op25gateway::Logger::instance().hexDump(label, data, len)

} // namespace op25gateway

#endif // LOGGER_H
