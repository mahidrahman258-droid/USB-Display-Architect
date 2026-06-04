#include "config_manager.h"
#include "logger.h"
#include <fstream>
#include <sstream>

namespace PlugScreen {

bool ConfigManager::LoadConfig(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        LOG_WARN("CONFIG", "Could not open configuration file: " + filePath + ". Generating standard defaults.");
        // Create an initial default config configuration file on the spot
        return SaveConfig(filePath);
    }

    std::string line;
    while (std::getline(file, line)) {
        // Simple ini-style or key-value file parsing
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        
        size_t delimiterPos = line.find('=');
        if (delimiterPos == std::string::npos) continue;

        std::string key = line.substr(0, delimiterPos);
        std::string val = line.substr(delimiterPos + 1);

        // Strip basic spaces
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        val.erase(0, val.find_first_not_of(" \t"));
        val.erase(val.find_last_not_of(" \t") + 1);

        if (key == "screenWidth") {
            m_config.screenWidth = static_cast<uint16_t>(std::stoi(val));
        } else if (key == "screenHeight") {
            m_config.screenHeight = static_cast<uint16_t>(std::stoi(val));
        } else if (key == "frameRate") {
            m_config.frameRate = static_cast<uint16_t>(std::stoi(val));
        } else if (key == "targetBitrate") {
            m_config.targetBitrate = static_cast<uint32_t>(std::stoul(val));
        } else if (key == "deviceName") {
            m_config.deviceName = val;
        } else if (key == "usbVendorId") {
            m_config.usbVendorId = static_cast<uint16_t>(std::stoul(val, nullptr, 16));
        } else if (key == "usbProductId") {
            m_config.usbProductId = static_cast<uint16_t>(std::stoul(val, nullptr, 16));
        }
    }

    LOG_INFO("CONFIG", "Config successfully fetched from file. Viewport: " + 
                       std::to_string(m_config.screenWidth) + "x" + 
                       std::to_string(m_config.screenHeight) + " @ " + 
                       std::to_string(m_config.frameRate) + " FPS");
    return true;
}

bool ConfigManager::SaveConfig(const std::string& filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        LOG_ERROR("CONFIG", "Could not open configuration file lock for writing: " + filePath);
        return false;
    }

    file << "# PlugScreen Configuration Parameters Manual\n";
    file << "screenWidth=" << m_config.screenWidth << "\n";
    file << "screenHeight=" << m_config.screenHeight << "\n";
    file << "frameRate=" << m_config.frameRate << "\n";
    file << "targetBitrate=" << m_config.targetBitrate << "\n";
    file << "deviceName=" << m_config.deviceName << "\n";
    file << "usbVendorId=0x" << std::hex << m_config.usbVendorId << "\n";
    file << "usbProductId=0x" << std::hex << m_config.usbProductId << "\n";

    LOG_INFO("CONFIG", "Saved hardware config profiles successfully back to disk in: " + filePath);
    return true;
}

} // namespace PlugScreen
