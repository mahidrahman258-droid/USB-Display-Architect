#include "Logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace PlugScreen {

void Logger::Initialize(const std::string& logFilePath, bool enableConsole) {
    if (m_initialized.load()) return;

    m_logFilePath = logFilePath;
    m_enableConsole = enableConsole;

    // Open stream in clean append mode
    m_fileStream.open(m_logFilePath, std::ios::out | std::ios::app);
    if (!m_fileStream.is_open()) {
        std::cerr << "[LOGGER_ERROR] Failed to open log file index at: " << logFilePath << std::endl;
        return;
    }

    // Append standard startup separators
    m_fileStream << "\n========================================================================\n"
                 << "[LOG_INIT] Core Diagnostics System Online. Running in Async Mode.\n"
                 << "========================================================================\n";
    m_fileStream.flush();

    m_initialized.store(true);
    m_running.store(true);

    // Spawn async IO commit worker thread
    m_workerThread = std::thread(&Logger::ProcessQueue, this);
}

void Logger::Log(LogLevel level, const std::string& component, const std::string& message) {
    if (!m_running.load()) return;

    // Instantly map execution environments
    LogEntry entry;
    entry.timestamp = GetCurrentTimestamp();
    entry.level = level;
    entry.component = component;
    entry.message = message;

    std::stringstream tidStream;
    tidStream << std::this_thread::get_id();
    entry.threadId = tidStream.str();

    // Lock and queue for commit processing
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_queue.push(std::move(entry));
    }
    m_cv.notify_one();
}

void Logger::ProcessQueue() {
    std::vector<LogEntry> localBuffer;
    localBuffer.reserve(100);

    while (m_running.load() || !m_queue.empty()) {
        // Drain incoming queues blockingly
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(200), [this] {
                return !m_queue.empty() || !m_running.load();
            });

            while (!m_queue.empty()) {
                localBuffer.push_back(std::move(m_queue.front()));
                m_queue.pop();
            }
        }

        // Commit drain buffer directly to active targets
        if (!localBuffer.empty()) {
            for (const auto& entry : localBuffer) {
                std::stringstream entryStream;
                entryStream << "[" << entry.timestamp << "] "
                            << "[" << GetLogLevelString(entry.level) << "] "
                            << "[Thread:" << entry.threadId << "] "
                            << "[" << entry.component << "] "
                            << entry.message << "\n";
                
                std::string line = entryStream.str();

                // 1. Output to Console STDOUT / STDERR
                if (m_enableConsole) {
                    if (entry.level == LogLevel::Error || entry.level == LogLevel::Fatal) {
                        std::cerr << line;
                    } else {
                        std::cout << line;
                    }
                }

                // 2. Output to Disk
                if (m_initialized.load() && m_fileStream.is_open()) {
                    m_fileStream << line;
                }
            }

            // Flush the transaction updates blockingly
            if (m_initialized.load() && m_fileStream.is_open()) {
                m_fileStream.flush();
            }

            localBuffer.clear();
        }
    }
}

void Logger::Shutdown() {
    bool expectedVal = true;
    if (!m_running.compare_exchange_strong(expectedVal, false)) {
        return; // Already stopped or not initialized
    }

    // Wake and join thread loops
    m_cv.notify_all();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    // Safely write shutdowns index
    if (m_fileStream.is_open()) {
        m_fileStream << "[LOG_SHUTDOWN] Core diagnostics safely saved to disk.\n"
                     << "========================================================================\n";
        m_fileStream.close();
    }

    m_initialized.store(false);
}

std::string Logger::GetLogLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Fatal:   return "FATAL";
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
