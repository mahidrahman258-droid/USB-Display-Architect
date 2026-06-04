#ifndef PLUGSCREEN_USB_USB_MANAGER_H
#define PLUGSCREEN_USB_USB_MANAGER_H

#include <windows.h>
#include <winusb.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <functional>
#include "protocol.h"

namespace PlugScreen {

/**
 * @brief Coordinates underlying bulk data flows to and from Android USB accessory nodes.
 * Implements WinUSB overlapped asynchronous I/O architectures under high-pressure streaming conditions.
 */
class UsbManager {
public:
    using HandshakeCallback = std::function<void(const HandshakePayload& handshake)>;
    using InputCallback     = std::function<void(const InputEventPayload& touchEvt)>;
    using DisconnectCallback = std::function<void()>;

    UsbManager();
    ~UsbManager();

    /**
     * @brief Hooks event listeners and initializes structures. Must be called prior to AutoDetect calls.
     */
    bool Initialize(HandshakeCallback hCallback, InputCallback iCallback, DisconnectCallback dCallback);

    /**
     * @brief Launches background SetupAPI scan lists and active monitoring loops matching IDs.
     */
    bool AutoDetectDevice(uint16_t vid, uint16_t pid);

    /**
     * @brief Formulate binary envelopes with sliding window magic structures, sequence tokens,
     * type descriptors and write them asynchronously to bulk pipelines.
     */
    bool StreamPackage(PacketType type, const uint8_t* pPayload, uint32_t payloadLength, uint32_t sequence, uint8_t flags);

    /**
     * @brief Forceful teardown of links, joining threads, and releasing handles.
     */
    void Shutdown();

    /**
     * @brief Direct queries about linkage connectivity state.
     */
    bool IsConnected() const { return m_isConnected.load(); }

    /**
     * @brief Direct queries targeting hardware discovery.
     */
    bool IsSearching() const { return m_isRunning.load(); }

private:
    void DiscoveryWorker();
    void FrameReaderWorker();

    bool ScanDeviceInterfaceGUID(std::wstring& outDevicePath);
    bool ConfigureWinUsbPipelines(const std::wstring& devicePath);
    void CloseNativeHandles();

    std::atomic<bool> m_isRunning;
    std::atomic<bool> m_isConnected;

    // Trigger delegates
    HandshakeCallback  m_handshakeCallback;
    InputCallback      m_inputCallback;
    DisconnectCallback m_disconnectCallback;

    // Operation handles
    HANDLE                  m_deviceHandle = INVALID_HANDLE_VALUE;
    WINUSB_INTERFACE_HANDLE m_winusbHandle = INVALID_HANDLE_VALUE;

    // Thread workers
    std::thread m_discoveryThread;
    std::thread m_readerThread;

    // Pipe layout targets (aligned to standard AOA targets)
    UCHAR m_bulkInAddress  = 0x81;  // IN Bulk Pipe Endpoint 1
    UCHAR m_bulkOutAddress = 0x02; // OUT Bulk Pipe Endpoint 2

    // Safety and serialization controls
    std::mutex m_writeMutex;
    uint16_t   m_vid = 0x18D1;
    uint16_t   m_pid = 0x2D01;
};

} // namespace PlugScreen

#endif // PLUGSCREEN_USB_USB_MANAGER_H
