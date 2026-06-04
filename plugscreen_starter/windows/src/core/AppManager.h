#ifndef PLUGSCREEN_CORE_APP_MANAGER_H
#define PLUGSCREEN_CORE_APP_MANAGER_H

#include <string>
#include <atomic>
#include <mutex>
#include <memory>

#include "ConfigManager.h"
#include "Logger.h"
#include "usb/UsbManager.h"
#include "capture/ScreenCapture.h"
#include "encoder/H264Encoder.h"
#include "virtual_display.h"

namespace PlugScreen {

/**
 * @brief Principal controller coordinating Windows-side second screen rendering subsystems.
 * Unifies capture, encoding, transport threads, and simulates system mouse injections.
 */
class AppManager {
public:
    static AppManager& Instance() {
        static AppManager instance;
        return instance;
    }

    /**
     * @brief Initiates system memory parameters, config manager layers, and begins logging.
     * @param iniConfigPath Destination for loading / saving user setups.
     */
    bool Initialize(const std::string& iniConfigPath);

    /**
     * @brief Spawns virtual monitor adapters, binds USB endpoints,
     * structures FFmpeg encoders, and begins capturing high-frequency frames.
     */
    bool StartMirroring();

    /**
     * @brief Ceases acquisition threads and tears down WinUSB connection paths.
     */
    void StopMirroring();

    /**
     * @brief Full teardown of AppManager, committing final files to disk.
     */
    void Shutdown();

    // Context queries
    bool IsActive() const { return m_isActive.load(); }
    bool IsTransmissionReady() const { return m_usbManager.IsConnected(); }

private:
    AppManager();
    ~AppManager() { Shutdown(); }

    AppManager(const AppManager&) = delete;
    AppManager& operator=(const AppManager&) = delete;

    // Callbacks hooked to underlying subsystems
    void OnDeviceHandshakeReceived(const HandshakePayload& handshake);
    void OnDeviceInputReceived(const InputEventPayload& touchEvt);
    void OnDeviceDisconnectHooked();
    void OnDesktopFrameAcquired(ID3D11Texture2D* pTex, uint64_t timestampUs);

    // Coordinate conversion and Win32 Input injection logic
    void InjectVirtualTouchClick(uint8_t action, uint16_t touchId, uint16_t normX, uint16_t normY);

    std::atomic<bool>     m_initialized;
    std::atomic<bool>     m_isActive;
    std::mutex            m_stateMutex;
    std::string           m_iniPath;

    // Subsystem components
    UsbManager            m_usbManager;
    ScreenCapture         m_screenCapture;
    H264Encoder           m_h264Encoder;
    VirtualDisplay        m_virtualDisplay;

    // Tracking streams
    uint32_t              m_sequenceCounter = 0;
};

} // namespace PlugScreen

#endif // PLUGSCREEN_CORE_APP_MANAGER_H
