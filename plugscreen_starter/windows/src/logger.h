#ifndef PLUGSCREEN_WINDOWS_LOGGER_H
#define PLUGSCREEN_WINDOWS_LOGGER_H

#include <string>
#include <mutex>
#include <fstream>
#include <iostream>

namespace PlugScreen {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    void Initialize(const std::string& logFilePath);
    void Log(LogLevel level, const std::string& component, const std::string& message);
    void Shutdown();

private:
    Logger() = default;
    ~Logger() { Shutdown(); }
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string GetLogLevelString(LogLevel level);
    std::string GetCurrentTimestamp();

    std::mutex m_mutex;
    std::ofstream m_fileStream;
    bool m_initialized = false;
};

#define LOG_DEBUG(comp, msg) PlugScreen::Logger::Instance().Log(PlugScreen::LogLevel::Debug, comp, msg)
#define LOG_INFO(comp, msg)  PlugScreen::Logger::Instance().Log(PlugScreen::LogLevel::Info, comp, msg)
#define LOG_WARN(comp, msg)  PlugScreen::Logger::Instance().Log(PlugScreen::LogLevel::Warning, comp, msg)
#define LOG_ERROR(comp, msg) PlugScreen::Logger::Instance().Log(PlugScreen::LogLevel::Error, comp, msg)

} // namespace PlugScreen

#endif // PLUGSCREEN_WINDOWS_LOGGER_H
