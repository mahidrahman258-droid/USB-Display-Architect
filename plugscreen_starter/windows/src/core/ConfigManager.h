#ifndef PLUGSCREEN_CORE_CONFIG_MANAGER_H
#define PLUGSCREEN_CORE_CONFIG_MANAGER_H

#include <string>
#include <mutex>

namespace PlugScreen {

/**
 * @brief Enumerates the targeted hardware accelerated encoder hardware vendors.
 */
enum class EncoderHardwareBackend {
    SoftwareX264 = 0, // libx264 software threading fallback
    NvidiaNVENC  = 1, // h264_nvenc
    IntelQSV     = 2, // h264_qsv
    AmdAMF       = 3  // h264_amf
};

/**
 * @brief Structured configuration properties governing H264, capture, and USB threads.
 */
struct AppConfig {
    // Canvas Display settings
    uint16_t screenWidth  = 1920;
    uint16_t screenHeight = 1080;
    uint16_t frameRate    = 60;
    uint32_t targetBitrate = 6000000; // Default to 6 Mbps for clear 1080p60

    // USB Hardware identifiers
    uint16_t usbVendorId  = 0x18D1; // Standard Google Android VID
    uint16_t usbProductId = 0x2D01; // Accessory with ADB PID
    uint32_t packetBatchSize = 4;   // Bulk write batch sizing
    bool     enableFastPath  = true; // Zero-copy, RAW IO toggles

    // Video Acceleration settings
    EncoderHardwareBackend codecBackend = EncoderHardwareBackend::SoftwareX264;
    std::string encoderPreset = "ultrafast"; // ultrafast / superfast / high_performance
    std::string rateControlMode = "cbr";     // cbr (constant) / vbr (variable) / crf (factor)
    uint8_t     gopSize = 60;                // Intra-interval structure (e.g. 60 frames for 60fps)

    // Advanced Pipeline optimizations
    bool     enableFrameDropping = true;    // Drop delayed desktop updates to match queue backing
    uint16_t maxBacklogLatencyMs = 64;       // Permissible delay before frame-dropping starts
};

/**
 * @brief Thread-safe configuration state management.
 * Parses INI configuration entries with safe syntax checks and disk sync workflows.
 */
class ConfigManager {
public:
    static ConfigManager& Instance() {
        static ConfigManager instance;
        return instance;
    }

    /**
     * @brief Parses setting parameters from the specified INI text file.
     * Generates standard file presets if parsing fails or target does not exist.
     */
    bool LoadConfig(const std::string& path);

    /**
     * @brief Commit the current structural configuration variables back to the disk.
     */
    bool SaveConfig(const std::string& path);

    /**
     * @brief Retrieve thread-safe copy of operational fields.
     */
    AppConfig GetConfig();

    /**
     * @brief Atomically load an updated structured set of properties.
     */
    void SetConfig(const AppConfig& newConfig);

private:
    ConfigManager() = default;
    ~ConfigManager() = default;

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    std::mutex m_mutex;
    AppConfig  m_config;
};

} // namespace PlugScreen

#endif // PLUGSCREEN_CORE_CONFIG_MANAGER_H
