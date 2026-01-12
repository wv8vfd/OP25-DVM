#include "Logger.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <unistd.h>

namespace op25gateway {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger()
    : m_level(LogLevel::INFO)
    , m_logFile("")
{
}

Logger::~Logger() {
    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_level = level;
}

void Logger::setLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }
    m_logFile = path;
    if (!path.empty()) {
        m_fileStream.open(path, std::ios::app);
        if (!m_fileStream.is_open()) {
            std::cerr << "Failed to open log file: " << path << std::endl;
        }
    }
}

std::string Logger::timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm;
    localtime_r(&time, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        default: return "?????";
    }
}

void Logger::writeToFile(const std::string& msg) {
    if (!m_fileStream.is_open()) {
        return;
    }
    m_fileStream << msg << std::endl;
    m_fileStream.flush();
}

void Logger::writeToConsole(const std::string& msg, LogLevel level) {
    if (!isatty(STDOUT_FILENO)) {
        return;
    }

    const char* color = "";
    const char* reset = "\033[0m";

    switch (level) {
        case LogLevel::DEBUG:
            color = "\033[36m";  // Cyan
            break;
        case LogLevel::INFO:
            color = "\033[32m";  // Green
            break;
        case LogLevel::WARN:
            color = "\033[33m";  // Yellow
            break;
        case LogLevel::ERROR:
            color = "\033[31m";  // Red
            break;
        default:
            break;
    }

    std::cout << color << msg << reset << std::endl;
}

void Logger::log(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (static_cast<int>(level) < static_cast<int>(m_level)) {
        return;
    }

    std::string formattedMsg = "[" + timestamp() + "] [" + levelToString(level) + "] " + msg;

    writeToConsole(formattedMsg, level);
    writeToFile(formattedMsg);
}

void Logger::debug(const std::string& msg) {
    log(LogLevel::DEBUG, msg);
}

void Logger::info(const std::string& msg) {
    log(LogLevel::INFO, msg);
}

void Logger::warn(const std::string& msg) {
    log(LogLevel::WARN, msg);
}

void Logger::error(const std::string& msg) {
    log(LogLevel::ERROR, msg);
}

void Logger::hexDump(const std::string& label, const uint8_t* data, size_t len) {
    if (static_cast<int>(LogLevel::DEBUG) < static_cast<int>(m_level)) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    std::ostringstream oss;
    oss << label << " (" << len << " bytes): ";

    for (size_t i = 0; i < len && i < 64; i++) {
        oss << std::hex << std::setfill('0') << std::setw(2) << (int)data[i] << " ";
    }
    if (len > 64) {
        oss << "...";
    }

    std::string formattedMsg = "[" + timestamp() + "] [DEBUG] " + oss.str();

    writeToConsole(formattedMsg, LogLevel::DEBUG);
    writeToFile(formattedMsg);
}

} // namespace op25gateway
