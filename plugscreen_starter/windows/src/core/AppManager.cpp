#include "AppManager.h"
#include <windows.h>

namespace PlugScreen {

AppManager::AppManager() 
    : m_initialized(false), m_isActive(false) {
}

bool AppManager::Initialize(const std::string& iniConfigPath) {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    if (m_initialized.load()) return true;

    m_iniPath = iniConfigPath;

    // 1. Fire up Asynchronous disk Logging threads
    Logger::Instance().Initialize("plugscreen_host.log", true);
    PL_LOG_INFO("APP_MGR", "--------------------------------------------------------");
    PL_LOG_INFO("APP_MGR", "PlugScreen Commercial Second Display Host Booting System");
    PL_LOG_INFO("APP_MGR", "--------------------------------------------------------");

    // 2. Load configurations
    ConfigManager::Instance().LoadConfig(m_iniPath);

    m_initialized.store(true);
    return true;
}

bool AppManager::StartMirroring() {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    if (!m_initialized.load()) {
        std::cerr << "[APP_MGR] Cannot start mirroring prior to initialization." << std::endl;
        return false;
    }

    if (m_isActive.load()) return true;

    PL_LOG_INFO("APP_MGR", "Launching high performance video stream duplication pipelines...");
    m_sequenceCounter = 0;

    AppConfig config = ConfigManager::Instance().GetConfig();

    // 1. Initialize Virtual Monitor system
    // (Simulates native extended custom resolutions)
    if (!m_virtualDisplay.Initialize(config.screenWidth, config.screenHeight, config.frameRate)) {
        PL_LOG_ERROR("APP_MGR", "Failed to load Virtual Monitor drivers adapters.");
        return false;
    }
    m_virtualDisplay.EnableMonitor();

    // 2. Setup low-latency USB transport pipelines
    m_usbManager.Initialize(
        [this](const HandshakePayload& handshake) { OnDeviceHandshakeReceived(handshake); },
        [this](const InputEventPayload& touchEvt) { OnDeviceInputReceived(touchEvt); },
        [this]() { OnDeviceDisconnectHooked(); }
    );
    m_usbManager.AutoDetectDevice(config.usbVendorId, config.usbProductId);

    // 3. Setup hardware FFmpeg encoders, binding output targets to USB packages
    m_h264Encoder.Initialize(
        config.screenWidth,
        config.screenHeight,
        config.frameRate,
        config.targetBitrate,
        [this](const uint8_t* pData, size_t size, uint64_t timestampUs, bool isKeyframe) {
            uint8_t flags = 0;
            if (isKeyframe) flags |= FrameFlag_KeyFrame;
            
            m_sequenceCounter++;
            
            // Push elementary H.264 video slices directly onto WinUSB transmitters
            m_usbManager.StreamPackage(
                PacketType::VideoFrame,
                pData,
                static_cast<uint32_t>(size),
                m_sequenceCounter,
                flags
            );
        }
    );

    // 4. Hook Screen capture loop back onto encoding pipeline
    m_screenCapture.Initialize(0, [this](ID3D11Texture2D* pTex, uint64_t timestampUs) {
        OnDesktopFrameAcquired(pTex, timestampUs);
    });
    
    // Begin capturing display
    m_screenCapture.Start();

    m_isActive.store(true);
    PL_LOG_INFO("APP_MGR", "Second display capture thread mirror active.");
    return true;
}

void AppManager::OnDesktopFrameAcquired(ID3D11Texture2D* pTex, uint64_t timestampUs) {
    if (!m_isActive.load()) return;

    if (pTex) {
        // Retrieve DirectX graphics device context mapping pointers
        ID3D11Device* device = nullptr;
        pTex->GetDevice(&device);
        if (device) {
            ID3D11DeviceContext* context = nullptr;
            device->GetImmediateContext(&context);
            if (context) {
                // Encode and stream current GPU frame slice
                m_h264Encoder.EncodeFrame(pTex, context, timestampUs);
                context->Release();
            }
            device->Release();
        }
    } else {
        // Fallback procedural encoding (software rendering clock updates)
        m_h264Encoder.EncodeFrame(nullptr, nullptr, timestampUs);
    }
}

void AppManager::OnDeviceHandshakeReceived(const HandshakePayload& handshake) {
    PL_LOG_INFO("APP_MGR", "Accessory handshakes matched!");
    PL_LOG_INFO("APP_MGR", "Device Name: " + std::string(handshake.deviceName));
    PL_LOG_INFO("APP_MGR", "Requested Grid: " + std::to_string(handshake.requestWidth) + "x" + 
                std::to_string(handshake.requestHeight) + " @" + std::to_string(handshake.preferredFps) + " Hz");

    // Dynamic configuration adjustments can happen here matching phone screen aspect ratios
}

void AppManager::OnDeviceInputReceived(const InputEventPayload& touchEvt) {
    // Interactive multi-touch feedback from tables/phones coordinates
    InjectVirtualTouchClick(touchEvt.action, touchEvt.touchId, touchEvt.normalizedX, touchEvt.normalizedY);
}

void AppManager::OnDeviceDisconnectHooked() {
    PL_LOG_WARN("APP_MGR", "Android Client disconnected. Awaiting hotplug reattachment...");
}

void AppManager::InjectVirtualTouchClick(uint8_t action, uint16_t touchId, uint16_t normX, uint16_t normY) {
    (void)touchId; // Suppress unused parameter warn

    // 1. Calculate relative screen workspace positions.
    // MOUSEEVENTF_ABSOLUTE works over a standardized 0 - 65535 coordinate bounds mapped across the active monitors.
    // We map our normalized [0-10000] payload from client onto DXGI system ratios.
    double xRatio = normX / 10000.0;
    double yRatio = normY / 10000.0;

    DWORD absoluteX = static_cast<DWORD>(xRatio * 65535.0);
    DWORD absoluteY = static_cast<DWORD>(yRatio * 65535.0);

    // 2. Synthesize Win32 input commands
    INPUT inputElement = {};
    inputElement.type = INPUT_MOUSE;
    inputElement.mi.dx = absoluteX;
    inputElement.mi.dy = absoluteY;
    inputElement.mi.mouseData = 0;
    inputElement.mi.time = 0;
    inputElement.mi.dwExtraInfo = 0;

    // Standard flag layout: MOUSEEVENTF_VIRTUALDESK forces mappings onto our secondary extended monitor
    DWORD commonFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK;

    switch (action) {
        case 0x00: // Touch Down
            inputElement.mi.dwFlags = commonFlags | MOUSEEVENTF_LEFTDOWN;
            break;
        case 0x01: // Touch Move / Drag
            inputElement.mi.dwFlags = commonFlags;
            break;
        case 0x02: // Touch Up / Release
            inputElement.mi.dwFlags = commonFlags | MOUSEEVENTF_LEFTUP;
            break;
        default:
            return; // Unknown mapping action
    }

    // 3. Inject pointer click event directly into Windows scheduling kernel
    ::SendInput(1, &inputElement, sizeof(INPUT));
}

void AppManager::StopMirroring() {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    if (!m_isActive.load()) return;

    PL_LOG_INFO("APP_MGR", "Stopping low latency screen mirror pipelines.");
    
    m_screenCapture.Shutdown();
    m_h264Encoder.Shutdown();
    m_virtualDisplay.Shutdown();
    m_usbManager.Shutdown();

    m_isActive.store(false);
    PL_LOG_INFO("APP_MGR", "Pipelines closed cleanly. Idle State.");
}

void AppManager::Shutdown() {
    StopMirroring();

    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_initialized.load()) {
        PL_LOG_INFO("APP_MGR", "AppManager shut down complete. Saving configurations...");
        ConfigManager::Instance().SaveConfig(m_iniPath);
        Logger::Instance().Shutdown();
        m_initialized.store(false);
    }
}

} // namespace PlugScreen
