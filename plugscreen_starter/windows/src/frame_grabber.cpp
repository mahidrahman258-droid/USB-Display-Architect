#include "frame_grabber.h"
#include "logger.h"

namespace PlugScreen {

FrameGrabber::FrameGrabber() : m_isRunning(false) {}

FrameGrabber::~FrameGrabber() {
    Shutdown();
}

bool FrameGrabber::Initialize(int monitorIndex, FrameCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_monitorIndex = monitorIndex;
    m_callback = callback;

    LOG_INFO("FRAME_GRABBER", "Initializing DXGI Desktop Duplication engine for monitor: " + std::to_string(monitorIndex));

    HRESULT hr = SetupDirect3D();
    if (FAILED(hr)) {
        LOG_ERROR("FRAME_GRABBER", "Direct3D 11 Initialization failed. HRESULT: " + std::to_string(hr));
        return false;
    }

    hr = SetupDuplication();
    if (FAILED(hr)) {
        LOG_ERROR("FRAME_GRABBER", "DXGI Duplication acquisition failed. HRESULT: " + std::to_string(hr));
        FreeResources();
        return false;
    }

    LOG_INFO("FRAME_GRABBER", "Desktop Duplication registered successfully. Ready for stream extraction.");
    return true;
}

HRESULT FrameGrabber::SetupDirect3D() {
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1
    };
    D3D_FEATURE_LEVEL activeFeatureLevel;

    // Use D3D11_CREATE_DEVICE_BGRA_SUPPORT for high-compatibility GDI interoperability
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, featureLevels, ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION, &m_pd3dDevice, &activeFeatureLevel, &m_pImmediateContext
    );

    return hr;
}

HRESULT FrameGrabber::SetupDuplication() {
    if (!m_pd3dDevice) return E_POINTER;

    // Fetch the DXGI Device
    IDXGIDevice* pDxgiDevice = nullptr;
    HRESULT hr = m_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDxgiDevice);
    if (FAILED(hr)) return hr;

    // Fetch Adapter
    IDXGIAdapter* pDxgiAdapter = nullptr;
    hr = pDxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&pDxgiAdapter);
    pDxgiDevice->Release();
    if (FAILED(hr)) return hr;

    // Fetch Output Monitor
    IDXGIOutput* pDxgiOutput = nullptr;
    hr = pDxgiAdapter->GetOutput(m_monitorIndex, &pDxgiOutput);
    pDxgiAdapter->Release();
    if (FAILED(hr)) return hr;

    // Query Desktop output-5 for optimized multi-format duplicate mappings
    IDXGIOutput5* pDxgiOutput5 = nullptr;
    hr = pDxgiOutput->QueryInterface(__uuidof(IDXGIOutput5), (void**)&pDxgiOutput5);
    pDxgiOutput->Release();
    if (FAILED(hr)) {
        LOG_WARN("FRAME_GRABBER", "IDXGIOutput5 interface unsupported. Falling back to base output duplication.");
        IDXGIOutput* pOriginalOutput = nullptr;
        pDxgiOutput->QueryInterface(__uuidof(IDXGIOutput), (void**)&pOriginalOutput);
        hr = pOriginalOutput->DuplicateOutput(m_pd3dDevice, &m_pDeskDupl);
        pOriginalOutput->Release();
        return hr;
    }

    // Attempt to request hardware NV12 texture format if supported, fallback to default BGRA/RGBA textures
    DXGI_FORMAT captureFormats[] = { DXGI_FORMAT_NV12, DXGI_FORMAT_B8G8R8A8_UNORM };
    hr = pDxgiOutput5->DuplicateOutput1(m_pd3dDevice, 0, ARRAYSIZE(captureFormats), captureFormats, &m_pDeskDupl);
    pDxgiOutput5->Release();

    return hr;
}

bool FrameGrabber::Start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_isRunning) return true;

    if (!m_pDeskDupl) {
        LOG_ERROR("FRAME_GRABBER", "Cannot trigger loop. Grabber unit uninitialized.");
        return false;
    }

    m_isRunning = true;
    m_captureThread = std::thread(&FrameGrabber::CaptureLoop, this);
    LOG_INFO("FRAME_GRABBER", "DXGI Frame capture daemon spawned.");
    return true;
}

void FrameGrabber::Stop() {
    m_isRunning = false;
    if (m_captureThread.joinable()) {
        m_captureThread.join();
    }
}

void FrameGrabber::CaptureLoop() {
    LOG_INFO("FRAME_GRABBER", "Entering capture loop...");

    while (m_isRunning) {
        DXGI_OUTDUPL_FRAME_INFO frameInfo;
        IDXGIResource* pDesktopResource = nullptr;

        // Acquire with a 15ms timeout matching 60FPS interval
        HRESULT hr = m_pDeskDupl->AcquireNextFrame(15, &frameInfo, &pDesktopResource);
        
        if (hr == DXGI_ERROR_ACCESS_LOST) {
            LOG_WARN("FRAME_GRABBER", "Desktop session changed or display reconfigured. Attempting direct driver re-init.");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            std::lock_guard<std::mutex> lock(m_mutex);
            FreeResources();
            SetupDirect3D();
            SetupDuplication();
            continue;
        }

        if (FAILED(hr)) {
            // Likely timeout, loop again
            continue;
        }

        // Gather real presentation timestamps
        uint64_t timestamp = frameInfo.LastPresentTime.QuadPart;

        if (frameInfo.AccumulatedFrames > 0 && pDesktopResource != nullptr) {
            ID3D11Texture2D* pAcquiredTexture = nullptr;
            hr = pDesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&pAcquiredTexture);
            
            if (SUCCEEDED(hr) && pAcquiredTexture != nullptr) {
                // Return acquired native texture pointer up via callback for encoding
                m_callback(pAcquiredTexture, timestamp);
                pAcquiredTexture->Release();
            }
        }

        if (pDesktopResource) {
            pDesktopResource->Release();
        }

        m_pDeskDupl->ReleaseFrame();
    }

    LOG_INFO("FRAME_GRABBER", "Exited capture daemon safely.");
}

void FrameGrabber::FreeResources() {
    if (m_pSharedTexture) {
        m_pSharedTexture->Release();
        m_pSharedTexture = nullptr;
    }
    if (m_pDeskDupl) {
        m_pDeskDupl->Release();
        m_pDeskDupl = nullptr;
    }
    if (m_pImmediateContext) {
        m_pImmediateContext->Release();
        m_pImmediateContext = nullptr;
    }
    if (m_pd3dDevice) {
        m_pd3dDevice->Release();
        m_pd3dDevice = nullptr;
    }
}

void FrameGrabber::Shutdown() {
    Stop();
    std::lock_guard<std::mutex> lock(m_mutex);
    FreeResources();
    LOG_INFO("FRAME_GRABBER", "Capture Subsystem shutdown complete.");
}

} // namespace PlugScreen
