#include "ConfigManager.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace PlugScreen {

// Trimming helper routines
static inline std::string Trim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    return s;
}

bool ConfigManager::LoadConfig(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ifstream file(path);
    if (!file.is_open()) {
        PL_LOG_WARN("CONFIG_MGR", "Configuration file target \"" + path + "\" could not be opened. Creating standard template.");
        // We'll write out fresh presets in a fall-through logic later by returning true indicating fallback is loaded
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue; // Ignore commented or blank lines
        }

        size_t delimiterPos = line.find('=');
        if (delimiterPos == std::string::npos) continue;

        std::string key = Trim(line.substr(0, delimiterPos));
        std::string val = Trim(line.substr(delimiterPos + 1));

        if (key == "screenWidth") {
            m_config.screenWidth = static_cast<uint16_t>(std::stoul(val));
        } else if (key == "screenHeight") {
            m_config.screenHeight = static_cast<uint16_t>(std::stoul(val));
        } else if (key == "frameRate") {
            m_config.frameRate = static_cast<uint16_t>(std::stoul(val));
        } else if (key == "targetBitrate") {
            m_config.targetBitrate = std::stoul(val);
        } else if (key == "usbVendorId") {
            m_config.usbVendorId = static_cast<uint16_t>(std::stoul(val, nullptr, 16));
        } else if (key == "usbProductId") {
            m_config.usbProductId = static_cast<uint16_t>(std::stoul(val, nullptr, 16));
        } else if (key == "packetBatchSize") {
            m_config.packetBatchSize = std::stoul(val);
        } else if (key == "enableFastPath") {
            m_config.enableFastPath = (val == "true" || val == "1");
        } else if (key == "codecBackend") {
            m_config.codecBackend = static_cast<EncoderHardwareBackend>(std::stoi(val));
        } else if (key == "encoderPreset") {
            m_config.encoderPreset = val;
        } else if (key == "rateControlMode") {
            m_config.rateControlMode = val;
        } else if (key == "gopSize") {
            m_config.gopSize = static_cast<uint8_t>(std::stoul(val));
        } else if (key == "enableFrameDropping") {
            m_config.enableFrameDropping = (val == "true" || val == "1");
        } else if (key == "maxBacklogLatencyMs") {
            m_config.maxBacklogLatencyMs = static_cast<uint16_t>(std::stoul(val));
        }
    }

    PL_LOG_INFO("CONFIG_MGR", "Loaded configuration parameter set safely. Canvas: " +
                std::to_string(m_config.screenWidth) + "x" + std::to_string(m_config.screenHeight) +
                " @" + std::to_string(m_config.frameRate) + " FPS");
    return true;
}

bool ConfigManager::SaveConfig(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        PL_LOG_ERROR("CONFIG_MGR", "Failed to open configuration path for writing: " + path);
        return false;
    }

    file << "; ========================================================================\n"
         << "; PLUGSCREEN: Professional USB Second Screen Display Configuration\n"
         << "; ========================================================================\n\n"
         << "[Display]\n"
         << "screenWidth=" << m_config.screenWidth << "\n"
         << "screenHeight=" << m_config.screenHeight << "\n"
         << "frameRate=" << m_config.frameRate << "\n"
         << "targetBitrate=" << m_config.targetBitrate << "\n\n"
         << "[USB]\n"
         << "usbVendorId=0x" << std::hex << m_config.usbVendorId << std::dec << "\n"
         << "usbProductId=0x" << std::hex << m_config.usbProductId << std::dec << "\n"
         << "packetBatchSize=" << m_config.packetBatchSize << "\n"
         << "enableFastPath=" << (m_config.enableFastPath ? "true" : "false") << "\n\n"
         << "[Encoder]\n"
         << "codecBackend=" << static_cast<int>(m_config.codecBackend) << "\n"
         << "encoderPreset=" << m_config.encoderPreset << "\n"
         << "rateControlMode=" << m_config.rateControlMode << "\n"
         << "gopSize=" << static_cast<int>(m_config.gopSize) << "\n\n"
         << "[Pipeline]\n"
         << "enableFrameDropping=" << (m_config.enableFrameDropping ? "true" : "false") << "\n"
         << "maxBacklogLatencyMs=" << m_config.maxBacklogLatencyMs << "\n";

    PL_LOG_INFO("CONFIG_MGR", "Serialized AppConfig states permanently onto path: " + path);
    return true;
}

AppConfig ConfigManager::GetConfig() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void ConfigManager::SetConfig(const AppConfig& newConfig) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = newConfig;
}

} // namespace PlugScreen
