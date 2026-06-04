#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

namespace PlugScreen {

void Logger::Initialize(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) {
        m_fileStream.open(logFilePath, std::ios::out | std::ios::app);
        m_initialized = m_fileStream.is_open();
        if (m_initialized) {
            m_fileStream << "\n=============================================\n";
            m_fileStream << "[LOG_INIT] PlugScreen Logging Subsystem Hooked\n";
            m_fileStream << "=============================================\n";
        }
    }
}

void Logger::Log(LogLevel level, const std::string& component, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string timestamp = GetCurrentTimestamp();
    std::string levelStr = GetLogLevelString(level);
    
    // Get calling thread ID
    std::stringstream tidStream;
    tidStream << "TID:" << std::this_thread::get_id();
    std::string tid = tidStream.str();

    std::stringstream logEntry;
    logEntry << "[" << timestamp << "] "
             << "[" << levelStr << "] "
             << "[" << tid << "] "
             << "[" << component << "] "
             << message << "\n";

    std::string logStr = logEntry.str();

    // Print to Console
    if (level == LogLevel::Error) {
        std::cerr << logStr;
    } else {
        std::cout << logStr;
    }

    // Print to Log File
    if (m_initialized && m_fileStream.is_open()) {
        m_fileStream << logStr;
        m_fileStream.flush();
    }
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) {
        if (m_fileStream.is_open()) {
            m_fileStream << "[LOG_SHUTDOWN] Logging system shutting down.\n";
            m_fileStream.close();
        }
        m_initialized = false;
    }
}

std::string Logger::GetLogLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:   return "ERROR";
        default:                return "UNKN ";
    }
}

std::string Logger::GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto timeTime = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;

    std::tm tmStruct;
#ifdef _MSC_VER
    localtime_s(&tmStruct, &timeTime);
#else
    localtime_r(&timeTime, &tmStruct);
#endif

    std::stringstream ss;
    ss << std::put_time(&tmStruct, "%Y-%m-%d %H:%M:%S") << "."
       << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

} // namespace PlugScreen
