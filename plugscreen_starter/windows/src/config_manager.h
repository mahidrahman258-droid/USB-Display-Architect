#ifndef PLUGSCREEN_WINDOWS_CONFIG_MANAGER_H
#define PLUGSCREEN_WINDOWS_CONFIG_MANAGER_H

#include <string>

namespace PlugScreen {

struct ApplicationConfig {
    uint16_t screenWidth = 1920;
    uint16_t screenHeight = 1080;
    uint16_t frameRate = 60;
    uint32_t targetBitrate = 4500000; // 4.5 Mbps
    std::string deviceName = "PlugScreen Virtual Display";
    std::string usbDeviceGuid = "{88bae032-5a81-49f0-bc3d-a4ff138216d6}"; // Custom Guid interface
    uint16_t usbVendorId = 0x18D1; // Google (AOA mode Vendor)
    uint16_t usbProductId = 0x2D01; // Accessory with ADB model
};

class ConfigManager {
public:
    static ConfigManager& Instance() {
        static ConfigManager instance;
        return instance;
    }

    bool LoadConfig(const std::string& filePath);
    bool SaveConfig(const std::string& filePath);
    const ApplicationConfig& GetConfig() const { return m_config; }
    void UpdateConfig(const ApplicationConfig& newConfig) { m_config = newConfig; }

private:
    ConfigManager() = default;
    ~ConfigManager() = default;

    ApplicationConfig m_config;
};

} // namespace PlugScreen

#endif // PLUGSCREEN_WINDOWS_CONFIG_MANAGER_H
