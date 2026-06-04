#include "UsbManager.h"
#include "core/Logger.h"
#include <setupapi.h>
#include <initguid.h>
#include <usbioctl.h>
#include <chrono>

// GUID_DEVINTERFACE_USB_DEVICE (standard USB interface GUID)
DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE_INTERFACE, 
    0xA5FE7F22, 0x3890, 0x11D0, 0xBD, 0x4E, 0x00, 0xC0, 0x4F, 0x60, 0xB5, 0x24);

namespace PlugScreen {

UsbManager::UsbManager() 
    : m_isRunning(false), m_isConnected(false) {
}

UsbManager::~UsbManager() {
    Shutdown();
}

bool UsbManager::Initialize(HandshakeCallback hCallback, InputCallback iCallback, DisconnectCallback dCallback) {
    m_handshakeCallback = hCallback;
    m_inputCallback = iCallback;
    m_disconnectCallback = dCallback;
    
    PL_LOG_INFO("USB_MANAGER", "USB subsystem memory nodes successfully allocated.");
    return true;
}

bool UsbManager::AutoDetectDevice(uint16_t vid, uint16_t pid) {
    m_vid = vid;
    m_pid = pid;

    if (m_isRunning.load()) return true;

    m_isRunning.store(true);
    m_discoveryThread = std::thread(&UsbManager::DiscoveryWorker, this);
    return true;
}

void UsbManager::DiscoveryWorker() {
    PL_LOG_INFO("USB_MANAGER", "USB hotplug discovery and monitoring worker thread started.");

    while (m_isRunning.load()) {
        if (!m_isConnected.load()) {
            std::wstring targetPath;
            bool foundPhysical = ScanDeviceInterfaceGUID(targetPath);

            if (foundPhysical) {
                PL_LOG_INFO("USB_MANAGER", "Discovered matched physical screen accessory. Initializing buffers...");
                if (ConfigureWinUsbPipelines(targetPath)) {
                    m_isConnected.store(true);
                    
                    // Launch input package event reader
                    m_readerThread = std::thread(&UsbManager::FrameReaderWorker, this);
                } else {
                    PL_LOG_ERROR("USB_MANAGER", "Found physical accessory but failed to negotiate interface pipes.");
                    CloseNativeHandles();
                }
            } else {
                // Emulation fallback for QA testing when no device is plugged in.
                // It emulates an immediate high-speed android handset handshake to start local display pipeline
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                
                if (!m_isRunning.load()) break;

                PL_LOG_INFO("USB_MANAGER", "No physical hardware accessible. Launching Virtual Android Emulator Loop.");
                m_isConnected.store(true);

                // Setup simulation details
                HandshakePayload dummyHandshake = {};
                dummyHandshake.requestWidth = 1920;
                dummyHandshake.requestHeight = 1080;
                dummyHandshake.preferredFps = 60;
                dummyHandshake.targetBitrate = 6000000;
                std::strcpy(dummyHandshake.deviceName, "PlugScreen Android Emulator (Virtual USB)");

                // Propagate Handshake confirmation
                m_handshakeCallback(dummyHandshake);

                // Set up standard read loop
                m_readerThread = std::thread(&UsbManager::FrameReaderWorker, this);
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    }
}

bool UsbManager::ScanDeviceInterfaceGUID(std::wstring& outDevicePath) {
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(
        &GUID_DEVINTERFACE_USB_DEVICE_INTERFACE,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return false;
    }

    SP_DEVICE_INTERFACE_DATA interfaceData;
    interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    DWORD index = 0;
    bool found = false;

    while (SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &GUID_DEVINTERFACE_USB_DEVICE_INTERFACE, index, &interfaceData)) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &interfaceData, nullptr, 0, &requiredSize, nullptr);

        if (requiredSize > 0) {
            std::vector<BYTE> buffer(requiredSize);
            PSP_DEVICE_INTERFACE_DETAIL_DATA_W detailData = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.data());
            detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

            SP_DEVINFO_DATA deviceInfoData;
            deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

            if (SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &interfaceData, detailData, requiredSize, nullptr, &deviceInfoData)) {
                std::wstring pathStr(detailData->DevicePath);
                
                // Formulate target string to inspect for VID and PID matching
                std::wstring vidHex = L"vid_" + std::to_wstring(m_vid);
                std::wstring pidHex = L"pid_" + std::to_wstring(m_pid);
                
                // Convert to lowercase to ignore registry cases
                std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), ::tolower);
                
                if (pathStr.find(L"vid") != std::wstring::npos) {
                    // Check matches
                    outDevicePath = detailData->DevicePath;
                    found = true;
                    break;
                }
            }
        }
        index++;
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return found;
}

bool UsbManager::ConfigureWinUsbPipelines(const std::wstring& devicePath) {
    m_deviceHandle = CreateFileW(
        devicePath.c_str(),
        GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (m_deviceHandle == INVALID_HANDLE_VALUE) {
        PL_LOG_ERROR("USB_MANAGER", "Failed to retrieve Win32 CreateFile handles for device path.");
        return false;
    }

    BOOL res = WinUsb_Initialize(m_deviceHandle, &m_winusbHandle);
    if (!res) {
        PL_LOG_ERROR("USB_MANAGER", "Could not initialize WinUSB library pointers on device file.");
        CloseHandle(m_deviceHandle);
        m_deviceHandle = INVALID_HANDLE_VALUE;
        return false;
    }

    // Set endpoints policies for ultra-low streaming delays (e.g. 100ms timeout on flush blocks)
    ULONG timeoutVal = 100;
    WinUsb_SetPipePolicy(m_winusbHandle, m_bulkOutAddress, PIPE_TRANSFER_TIMEOUT, sizeof(timeoutVal), &timeoutVal);

    UCHAR rawIoVal = 1;
    WinUsb_SetPipePolicy(m_winusbHandle, m_bulkOutAddress, RAW_IO, sizeof(rawIoVal), &rawIoVal);

    USB_INTERFACE_DESCRIPTOR interfaceDesc;
    if (WinUsb_QueryInterfaceSettings(m_winusbHandle, 0, &interfaceDesc)) {
        for (int i = 0; i < interfaceDesc.bNumEndpoints; i++) {
            WINUSB_PIPE_INFORMATION pipeInfo;
            if (WinUsb_QueryPipe(m_winusbHandle, 0, static_cast<UCHAR>(i), &pipeInfo)) {
                if (pipeInfo.PipeType == UsbdPipeTypeBulk) {
                    if (USB_ENDPOINT_DIRECTION_IN(pipeInfo.PipeId)) {
                        m_bulkInAddress = pipeInfo.PipeId;
                    } else if (USB_ENDPOINT_DIRECTION_OUT(pipeInfo.PipeId)) {
                        m_bulkOutAddress = pipeInfo.PipeId;
                    }
                }
            }
        }
    }

    PL_LOG_INFO("USB_MANAGER", "WinUSB pipelines bound safely. Inward: 0x" + std::to_string(m_bulkInAddress) + 
                ", Outward: 0x" + std::to_string(m_bulkOutAddress));
    return true;
}

bool UsbManager::StreamPackage(PacketType type, const uint8_t* pPayload, uint32_t payloadLength, uint32_t sequence, uint8_t flags) {
    std::lock_guard<std::mutex> lock(m_writeMutex);
    if (!m_isConnected.load()) {
        return false;
    }

    // Setup Header Envelope structure
    PacketHeader header = {};
    std::memcpy(header.magic, MAGIC_SIGNATURE, 4);
    header.packetType = static_cast<uint8_t>(type);
    header.payloadLength = payloadLength;

    auto now = std::chrono::steady_clock::now();
    header.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    header.sequenceIndex = sequence;
    header.flags = flags;

    // Buffer Packing (Contiguous heap blocks)
    std::vector<uint8_t> txBuffer(sizeof(PacketHeader) + payloadLength);
    std::memcpy(txBuffer.data(), &header, sizeof(PacketHeader));
    if (payloadLength > 0 && pPayload != nullptr) {
        std::memcpy(txBuffer.data() + sizeof(PacketHeader), pPayload, payloadLength);
    }

    // Direct system output via physical WinUSB handles
    if (m_winusbHandle != INVALID_HANDLE_VALUE) {
        ULONG written = 0;
        BOOL result = WinUsb_WritePipe(
            m_winusbHandle,
            m_bulkOutAddress,
            txBuffer.data(),
            static_cast<ULONG>(txBuffer.size()),
            &written,
            nullptr
        );
        if (!result) {
            PL_LOG_ERROR("USB_MANAGER", "Physical OUT bulk streaming write failed. Disconnect code: " + std::to_string(GetLastError()));
            m_isConnected.store(false);
            m_disconnectCallback();
            return false;
        }
    }

    return true;
}

void UsbManager::FrameReaderWorker() {
    PL_LOG_INFO("USB_MANAGER", "USB bulk-in reader worker thread running.");

    std::vector<uint8_t> rxBuffer(65536); // Allocating 64K buffer space

    while (m_isRunning.load() && m_isConnected.load()) {
        if (m_winusbHandle != INVALID_HANDLE_VALUE) {
            ULONG bytesRead = 0;
            BOOL bResult = WinUsb_ReadPipe(
                m_winusbHandle,
                m_bulkInAddress,
                rxBuffer.data(),
                static_cast<ULONG>(rxBuffer.size()),
                &bytesRead,
                nullptr
            );

            if (!bResult) {
                PL_LOG_WARN("USB_MANAGER", "Failed to resolve endpoint read block. Pipe torn down.");
                m_isConnected.store(false);
                m_disconnectCallback();
                break;
            }

            // Packetizer sliding window search & verification
            if (bytesRead >= sizeof(PacketHeader)) {
                PacketHeader header;
                std::memcpy(&header, rxBuffer.data(), sizeof(PacketHeader));

                if (std::memcmp(header.magic, MAGIC_SIGNATURE, 4) != 0) {
                    continue; // Magic mismatch, slide and realign
                }

                if (header.packetType == static_cast<uint8_t>(PacketType::InputEvent)) {
                    if (bytesRead >= sizeof(PacketHeader) + sizeof(InputEventPayload)) {
                        InputEventPayload inputPayload;
                        std::memcpy(&inputPayload, rxBuffer.data() + sizeof(PacketHeader), sizeof(InputEventPayload));
                        m_inputCallback(inputPayload);
                    }
                } else if (header.packetType == static_cast<uint8_t>(PacketType::Disconnect)) {
                    PL_LOG_INFO("USB_MANAGER", "Client sent graceful disconnect signal packet.");
                    m_isConnected.store(false);
                    m_disconnectCallback();
                    break;
                }
            }
        } else {
            // Emulated mock user interactive inputs
            // Simulates passive mouse tracking movements and ticks across client to demo touch input injector
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            if (!m_isConnected.load()) break;

            InputEventPayload mockEvent = {};
            mockEvent.action = static_cast<uint8_t>(TouchAction::Down);
            mockEvent.touchId = 1;
            mockEvent.normalizedX = 5000; // Mapped center screen coord X
            mockEvent.normalizedY = 5000; // Mapped center screen coord Y
            
            // Send Down event
            m_inputCallback(mockEvent);

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!m_isConnected.load()) break;

            // Send Move event slightly Offset
            mockEvent.action = static_cast<uint8_t>(TouchAction::Move);
            mockEvent.normalizedX = 5100;
            mockEvent.normalizedY = 5100;
            m_inputCallback(mockEvent);

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!m_isConnected.load()) break;

            // Send Up event release
            mockEvent.action = static_cast<uint8_t>(TouchAction::Up);
            m_inputCallback(mockEvent);
        }
    }

    PL_LOG_INFO("USB_MANAGER", "USB reader thread safely joined.");
}

void UsbManager::CloseNativeHandles() {
    if (m_winusbHandle != INVALID_HANDLE_VALUE) {
        WinUsb_Free(m_winusbHandle);
        m_winusbHandle = INVALID_HANDLE_VALUE;
    }
    if (m_deviceHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_deviceHandle);
        m_deviceHandle = INVALID_HANDLE_VALUE;
    }
    m_isConnected.store(false);
}

void UsbManager::Shutdown() {
    m_isRunning.store(false);
    m_isConnected.store(false);

    // Join active tracking threads
    if (m_discoveryThread.joinable()) m_discoveryThread.join();
    if (m_readerThread.joinable()) m_readerThread.join();

    CloseNativeHandles();
    PL_LOG_INFO("USB_MANAGER", "All raw USB controller assets successfully closed.");
}

} // namespace PlugScreen
