#ifndef PLUGSCREEN_CORE_LOGGER_H
#define PLUGSCREEN_CORE_LOGGER_H

#include <string>
#include <mutex>
#include <fstream>
#include <iostream>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>

namespace PlugScreen {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

/**
 * @brief Thread-safe high-speed asynchronous logging subsystem.
 * Employs a dedicated disk-commit worker thread and lock-free/mutex-guarded ring buffer
 * to prevent logging from stalling high-frequency capture or encoder pipelines.
 */
class Logger {
public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    /**
     * @brief Instantiates disk logs and begins the background commit thread.
     * @param logFilePath Path of the file where logs must be flushed permanently.
     * @param enableConsole Print to standard out/err streams.
     */
    void Initialize(const std::string& logFilePath, bool enableConsole = true);

    /**
     * @brief Safely queue log statements for background indexing and printing.
     * @param level Severity level of the log entry.
     * @param component Originating subsystem context (e.g., ENCODER, WINUSB).
     * @param message Text payload descriptive of current operations.
     */
    void Log(LogLevel level, const std::string& component, const std::string& message);

    /**
     * @brief Safely shuts down the logger, flushing any queued lines to disk.
     */
    void Shutdown();

    /**
     * @brief Checks if logger holds active operational thread contexts.
     */
    bool IsInitialized() const { return m_initialized.load(); }

private:
    Logger() : m_initialized(false), m_running(false), m_enableConsole(true) {}
    ~Logger() { Shutdown(); }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Worker loop running on a background thread to write files
    void ProcessQueue();

    std::string GetLogLevelString(LogLevel level);
    std::string GetCurrentTimestamp();

    // Log message container representation
    struct LogEntry {
        std::string timestamp;
        LogLevel level;
        std::string threadId;
        std::string component;
        std::string message;
    };

    std::atomic<bool>     m_initialized;
    std::atomic<bool>     m_running;
    bool                  m_enableConsole;
    std::string           m_logFilePath;
    std::ofstream         m_fileStream;

    // Asynchronous communication containers
    std::queue<LogEntry>  m_queue;
    std::mutex            m_queueMutex;
    std::condition_variable m_cv;
    std::thread           m_workerThread;
};

// Global inline macros for easy subsystem calls
#define PL_LOG_DEBUG(comp, msg) PlugScreen::Logger::Instance().Log(PlugScreen::LogLevel::Debug, comp, msg)
#define PL_LOG_INFO(comp, msg)  PlugScreen::Logger::Instance().Log(PlugScreen::LogLevel::Info, comp, msg)
#define PL_LOG_WARN(comp, msg)  PlugScreen::Logger::Instance().Log(PlugScreen::LogLevel::Warning, comp, msg)
#define PL_LOG_ERROR(comp, msg) PlugScreen::Logger::Instance().Log(PlugScreen::LogLevel::Error, comp, msg)
#define PL_LOG_FATAL(comp, msg) PlugScreen::Logger::Instance().Log(PlugScreen::LogLevel::Fatal, comp, msg)

} // namespace PlugScreen

#endif // PLUGSCREEN_CORE_LOGGER_H
