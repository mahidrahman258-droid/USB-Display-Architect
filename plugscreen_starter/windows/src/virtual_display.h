#ifndef PLUGSCREEN_WINDOWS_VIRTUAL_DISPLAY_H
#define PLUGSCREEN_WINDOWS_VIRTUAL_DISPLAY_H

#include <windows.h>
#include <string>

namespace PlugScreen {

class VirtualDisplay {
public:
    VirtualDisplay();
    ~VirtualDisplay();

    /**
     * @brief Instantiates the software device interface, triggering IddCx loading.
     * Hooks up the device node through Win32 SetupAPI channels.
     */
    bool Initialize(uint16_t width, uint16_t height, uint16_t fps);

    /**
     * @brief Informs Windows display manager to plug the monitor in virtual driver.
     */
    bool EnableMonitor();

    /**
     * @brief Informs Windows display manager to unplug/disable the virtual display, releasing RAM bounds.
     */
    bool DisableMonitor();

    /**
     * @brief Tears down SetupAPI handles and deletes display instances.
     */
    void Shutdown();

    bool IsActive() const { return m_isActive; }

private:
    bool SetupVirtualDisplayCard();

    std::wstring m_driverGuid;
    HANDLE       m_deviceHandle = INVALID_HANDLE_VALUE;
    bool         m_initialized = false;
    bool         m_isActive = false;
    uint16_t     m_width = 1920;
    uint16_t     m_height = 1080;
    uint16_t     m_fps = 60;
};

} // namespace PlugScreen

#endif // PLUGSCREEN_WINDOWS_VIRTUAL_DISPLAY_H
