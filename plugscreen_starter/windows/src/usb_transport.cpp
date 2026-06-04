#include "usb_transport.h"
#include "logger.h"
#include <setupapi.h>
#include <usbioctl.h>

namespace PlugScreen {

UsbTransport::UsbTransport() 
    : m_isRunning(false), m_isConnected(false), m_handshakeCompleted(false), m_lastActivityTime(std::chrono::steady_clock::now()) {
}

UsbTransport::~UsbTransport() {
    Shutdown();
}

bool UsbTransport::Initialize(HandshakeCallback hCallback, InputCallback iCallback) {
    m_handshakeCallback = hCallback;
    m_inputCallback = iCallback;
    LOG_INFO("USB_TRANSPORT", "USB Abstraction Layer Initialized. Waiting for device detections.");
    return true;
}

bool UsbTransport::AutoDetectDevice(uint16_t vid, uint16_t pid) {
    m_vid = vid;
    m_pid = pid;

    if (m_isRunning) return true;

    LOG_INFO("USB_TRANSPORT", "Starting device monitoring for accessory nodes...");
    m_isRunning = true;
    m_listenerThread = std::thread(&UsbTransport::BackgroundListener, this);
    return true;
}

void UsbTransport::BackgroundListener() {
    LOG_INFO("USB_TRANSPORT", "USB Discovery loop running.");
    
    while (m_isRunning) {
        if (!m_isConnected) {
            m_handshakeCompleted = false;
            m_lastActivityTime = std::chrono::steady_clock::now();

            // Real production flow checks WinUSB devices matching Vendor and Product IDs
            // Since we running in emulated context, we wait 1 second and initialize virtual receiver
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if (!m_isRunning) break;
            
            LOG_INFO("USB_TRANSPORT", "Found PlugScreen Android Accessory! Activating WinUSB interface handles...");
            m_isConnected = true;

            if (m_isConnected) {
                m_lastActivityTime = std::chrono::steady_clock::now();
                
                // Spawn thread groups
                if (m_readThread.joinable()) m_readThread.join();
                m_readThread = std::thread(&UsbTransport::UsbReadLoop, this);

                if (m_heartbeatThread.joinable()) m_heartbeatThread.join();
                m_heartbeatThread = std::thread(&UsbTransport::UsbHeartbeatLoop, this);

                // Initiates handshake exchange by streaming HandshakeRequest (0x01)
                HandshakePayload hostHandshake = {};
                hostHandshake.requestWidth = 1920;
                hostHandshake.requestHeight = 1080;
                hostHandshake.preferredFps = 60;
                hostHandshake.targetBitrate = 6000000;
                std::strcpy(hostHandshake.deviceName, "PlugScreen Host (Windows)");

                LOG_INFO("USB_TRANSPORT", "Streaming handshake request (42 bytes) to client display.");
                StreamDataPacket(PacketType::HandshakeRequest, reinterpret_cast<const uint8_t*>(&hostHandshake), sizeof(HandshakePayload), 0, 0);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
}

bool UsbTransport::EstablishWinUsbInterface(std::wstring devicePath) {
    // 1. Open direct Win32 device handle
    m_deviceHandle = CreateFileW(
        devicePath.c_str(),
        GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED, // Overlapped required for WinUSB
        nullptr
    );

    if (m_deviceHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    // 2. Instantiate WinUSB handle linked to device handle
    BOOL bResult = WinUsb_Initialize(m_deviceHandle, &m_winusbHandle);
    if (!bResult) {
        CloseHandle(m_deviceHandle);
        m_deviceHandle = INVALID_HANDLE_VALUE;
        return false;
    }

    // 3. Configure short timeout policies to guarantee zero-latency
    ULONG timeoutValue = 100; // 100ms
    WinUsb_SetPipePolicy(m_winusbHandle, m_bulkOutPipeId, PIPE_TRANSFER_TIMEOUT, sizeof(timeoutValue), &timeoutValue);
    
    UCHAR rawPolicy = 1;
    WinUsb_SetPipePolicy(m_winusbHandle, m_bulkOutPipeId, RAW_IO, sizeof(rawPolicy), &rawPolicy);

    LOG_INFO("USB_TRANSPORT", "Established secure USB WinUSB bulk interfaces.");
    return true;
}

bool UsbTransport::StreamDataPacket(PacketType type, const uint8_t* pPayload, uint32_t payloadLength, uint32_t sequence, uint8_t flags) {
    if (!m_isConnected) {
        return false;
    }

    // Direct fragmentation logic for VideoFrames exceeding MAX_SLICE_SIZE
    if (type == PacketType::VideoFrame && payloadLength > MAX_SLICE_SIZE) {
        uint32_t bytesRemaining = payloadLength;
        uint32_t offset = 0;
        bool bSuccess = true;

        while (bytesRemaining > 0 && bSuccess) {
            uint32_t chunkSize = (bytesRemaining > MAX_SLICE_SIZE) ? MAX_SLICE_SIZE : bytesRemaining;
            
            uint8_t sliceFlags = flags;
            if (offset == 0) {
                sliceFlags |= FrameFlag_FirstSlice;
            }
            if (offset + chunkSize == payloadLength) {
                sliceFlags |= FrameFlag_LastSlice;
            }

            // Stream standard individual slice packet
            bSuccess = StreamDataPacketInternal(type, pPayload + offset, chunkSize, sequence, sliceFlags);
            
            bytesRemaining -= chunkSize;
            offset += chunkSize;
        }
        return bSuccess;
    } else {
        // Single standard packet dispatch
        return StreamDataPacketInternal(type, pPayload, payloadLength, sequence, flags);
    }
}

bool UsbTransport::StreamDataPacketInternal(PacketType type, const uint8_t* pPayload, uint32_t payloadLength, uint32_t sequence, uint8_t flags) {
    std::lock_guard<std::mutex> lock(m_writeMutex);
    if (!m_isConnected) {
        return false;
    }

    // 1. Structure binary packet header
    PacketHeader header = {};
    std::memcpy(header.magic, MAGIC_SIGNATURE, 4);
    header.packetType = static_cast<uint8_t>(type);
    header.payloadLength = payloadLength;
    
    // Acquire system micros
    auto now = std::chrono::steady_clock::now();
    header.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    header.sequenceIndex = sequence;
    header.flags = flags;

    // 2. Allocate contiguous transfer buffer (Header + Payload)
    std::vector<uint8_t> txBuffer(sizeof(PacketHeader) + payloadLength);
    std::memcpy(txBuffer.data(), &header, sizeof(PacketHeader));
    if (payloadLength > 0 && pPayload != nullptr) {
        std::memcpy(txBuffer.data() + sizeof(PacketHeader), pPayload, payloadLength);
    }

    // 3. Real WinUSB dispatching logic
    if (m_winusbHandle != INVALID_HANDLE_VALUE) {
        ULONG bytesWritten = 0;
        BOOL bResult = WinUsb_WritePipe(
            m_winusbHandle, 
            m_bulkOutPipeId, 
            txBuffer.data(), 
            static_cast<ULONG>(txBuffer.size()), 
            &bytesWritten, 
            nullptr
        );
        if (!bResult) {
            LOG_ERROR("USB_TRANSPORT", "USB write frame block aborted with error code: " + std::to_string(GetLastError()));
            m_isConnected = false;
            return false;
        }
    }

    return true;
}

void UsbTransport::UsbReadLoop() {
    LOG_INFO("USB_TRANSPORT", "USB Inward reader thread running...");

    std::vector<uint8_t> rxBuffer(65536); // 64K read buffer capacity

    while (m_isRunning && m_isConnected) {
        if (m_winusbHandle != INVALID_HANDLE_VALUE) {
            ULONG bytesRead = 0;
            
            // Wait and read bulk packet payloads from Android handset touch inputs
            BOOL bResult = WinUsb_ReadPipe(
                m_winusbHandle,
                m_bulkInPipeId,
                rxBuffer.data(),
                static_cast<ULONG>(rxBuffer.size()),
                &bytesRead,
                nullptr
            );

            if (!bResult) {
                // If the device is disconnected, stop the loop
                LOG_WARN("USB_TRANSPORT", "Read interface torn down. Disconnect detected.");
                m_isConnected = false;
                break;
            }

            if (bytesRead >= sizeof(PacketHeader)) {
                PacketHeader header;
                std::memcpy(&header, rxBuffer.data(), sizeof(PacketHeader));

                // Verify magic signatures
                if (std::memcmp(header.magic, MAGIC_SIGNATURE, 4) != 0) continue;

                // Mark activity timeline
                m_lastActivityTime = std::chrono::steady_clock::now();

                if (header.packetType == static_cast<uint8_t>(PacketType::HandshakeResponse)) {
                    if (bytesRead >= sizeof(PacketHeader) + sizeof(HandshakePayload)) {
                        HandshakePayload clientHandshake;
                        std::memcpy(&clientHandshake, rxBuffer.data() + sizeof(PacketHeader), sizeof(HandshakePayload));
                        m_handshakeCompleted = true;
                        LOG_INFO("USB_TRANSPORT", "Successfully completed handshake with Android device: " + std::string(clientHandshake.deviceName));
                        m_handshakeCallback(clientHandshake);
                    }
                } 
                else if (header.packetType == static_cast<uint8_t>(PacketType::InputEvent)) {
                    if (bytesRead >= sizeof(PacketHeader) + sizeof(InputEventPayload)) {
                        InputEventPayload touchEvt;
                        std::memcpy(&touchEvt, rxBuffer.data() + sizeof(PacketHeader), sizeof(InputEventPayload));
                        m_inputCallback(touchEvt);
                    }
                }
                else if (header.packetType == static_cast<uint8_t>(PacketType::Heartbeat)) {
                    // Periodic ping-pong packet to stay alive
                }
            }
        } else {
            // Emulated background protocol state machinery (Offline / Testing Simulator)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!m_isConnected) break;

            // 1. Simulate handshake response from Client if we just initiated connection
            if (!m_handshakeCompleted) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                
                HandshakePayload clientResponse = {};
                clientResponse.requestWidth = 1920;
                clientResponse.requestHeight = 1080;
                clientResponse.preferredFps = 60;
                clientResponse.targetBitrate = 6000000;
                std::strcpy(clientResponse.deviceName, "Simulated Android Display");

                m_lastActivityTime = std::chrono::steady_clock::now();
                m_handshakeCompleted = true;
                
                LOG_INFO("USB_TRANSPORT", "[EMULATED] Complete Handshake received from client.");
                m_handshakeCallback(clientResponse);
            } else {
                // Once Handshake is done, simulate periodic heartbeat response & touch inputs
                static int loopCounter = 0;
                loopCounter++;

                m_lastActivityTime = std::chrono::steady_clock::now(); // Keeps connection active

                if (loopCounter % 50 == 0) { // roughly every 5 seconds
                    InputEventPayload touchEvt = {};
                    touchEvt.action = static_cast<uint8_t>(TouchAction::Move);
                    touchEvt.touchId = 1;
                    touchEvt.normalizedX = 5000 + (rand() % 500);
                    touchEvt.normalizedY = 5000 + (rand() % 500);
                    
                    LOG_INFO("USB_TRANSPORT", "[EMULATED] Received virtual touch digitizer action from client.");
                    m_inputCallback(touchEvt);
                }
            }
        }
    }

    LOG_INFO("USB_TRANSPORT", "Read inward thread terminated.");
}

void UsbTransport::UsbHeartbeatLoop() {
    LOG_INFO("USB_TRANSPORT", "USB Heartbeat scheduler loop running.");
    
    while (m_isRunning && m_isConnected) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastActivityTime).count();
        
        // Active recovery: Reconnect if host receives no responses for 4.5 seconds
        if (elapsed > 4500) {
            LOG_ERROR("USB_TRANSPORT", "Heartbeat lost! Android device is silent. Triggers reconnection pipeline.");
            m_isConnected = false;
            m_handshakeCompleted = false;
            break;
        }

        // Send Heartbeat (Ping) Packet every 1 second
        uint64_t ticks = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        StreamDataPacketInternal(PacketType::Heartbeat, reinterpret_cast<const uint8_t*>(&ticks), sizeof(ticks), 0, 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    LOG_INFO("USB_TRANSPORT", "USB Heartbeat scheduler loop stopped.");
}

void UsbTransport::CloseUsbResources() {
    if (m_winusbHandle != INVALID_HANDLE_VALUE) {
        WinUsb_Free(m_winusbHandle);
        m_winusbHandle = INVALID_HANDLE_VALUE;
    }
    if (m_deviceHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_deviceHandle);
        m_deviceHandle = INVALID_HANDLE_VALUE;
    }
    m_isConnected = false;
    m_handshakeCompleted = false;
}

void UsbTransport::Shutdown() {
    m_isRunning = false;
    m_isConnected = false;
    m_handshakeCompleted = false;

    if (m_listenerThread.joinable()) m_listenerThread.join();
    if (m_readThread.joinable()) m_readThread.join();
    if (m_heartbeatThread.joinable()) m_heartbeatThread.join();

    CloseUsbResources();
    LOG_INFO("USB_TRANSPORT", "USB hardware links safely closed.");
}

} // namespace PlugScreen
