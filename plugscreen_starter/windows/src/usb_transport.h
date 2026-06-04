#ifndef PLUGSCREEN_WINDOWS_USB_TRANSPORT_H
#define PLUGSCREEN_WINDOWS_USB_TRANSPORT_H

#include <windows.h>
#include <winusb.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include "protocol.h"

namespace PlugScreen {

class UsbTransport {
public:
    using HandshakeCallback = std::function<void(const HandshakePayload& handshake)>;
    using InputCallback     = std::function<void(const InputEventPayload& touchEvt)>;

    UsbTransport();
    ~UsbTransport();

    bool Initialize(HandshakeCallback hCallback, InputCallback iCallback);
    
    /**
     * @brief Initiates SetupAPI scanners to find the device matching the USB Vendor/Product IDs.
     * Starts bulk IO transaction pipelines.
     */
    bool AutoDetectDevice(uint16_t vid, uint16_t pid);

    /**
     * @brief Compiles a complete PlugScreen binary packet (Header + payload)
     * and streams it over the active WinUSB bulk write endpoint.
     */
    bool StreamDataPacket(PacketType type, const uint8_t* pPayload, uint32_t payloadLength, uint32_t sequence, uint8_t flags);

    void Shutdown();

    bool IsConnected() const { return m_isConnected && m_handshakeCompleted; }

private:
    void BackgroundListener();
    void UsbReadLoop();
    void UsbHeartbeatLoop();
    bool EstablishWinUsbInterface(std::wstring devicePath);
    void CloseUsbResources();
    bool StreamDataPacketInternal(PacketType type, const uint8_t* pPayload, uint32_t payloadLength, uint32_t sequence, uint8_t flags);

    std::thread             m_listenerThread;
    std::thread             m_readThread;
    std::thread             m_heartbeatThread;
    std::atomic<bool>       m_isRunning;
    std::atomic<bool>       m_isConnected;
    std::atomic<bool>       m_handshakeCompleted;

    std::chrono::steady_clock::time_point m_lastActivityTime;

    // Callbacks
    HandshakeCallback       m_handshakeCallback;
    InputCallback           m_inputCallback;

    // WinUSB Interface elements
    HANDLE                  m_deviceHandle = INVALID_HANDLE_VALUE;
    WINUSB_INTERFACE_HANDLE m_winusbHandle = INVALID_HANDLE_VALUE;
    
    // USB bulk pipelines addresses
    UCHAR                   m_bulkInPipeId = 0x81;  // Standard Endpoint 1 IN
    UCHAR                   m_bulkOutPipeId = 0x02; // Standard Endpoint 2 OUT

    std::mutex              m_writeMutex;
    uint16_t                m_vid = 0x18D1;
    uint16_t                m_pid = 0x2D01;

    static const uint32_t   MAX_SLICE_SIZE = 32768; // 32 KB slice limit
};

} // namespace PlugScreen

#endif // PLUGSCREEN_WINDOWS_USB_TRANSPORT_H
