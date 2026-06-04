#ifndef PLUGSCREEN_CAPTURE_SCREEN_CAPTURE_H
#define PLUGSCREEN_CAPTURE_SCREEN_CAPTURE_H

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

namespace PlugScreen {

/**
 * @brief Interfaces Windows Desktop Duplication APIs (DXGI 1.2+) on a dedicated thread loop.
 * Captures display textures, composites hardware mouse-pointers, and monitors device state changes.
 */
class ScreenCapture {
public:
    using FrameCallback = std::function<void(ID3D11Texture2D* pTexture, uint64_t timestampUs)>;

    ScreenCapture();
    ~ScreenCapture();

    /**
     * @brief Resolves target physical adapters, instantiates Direct3D11 device layers,
     * and constructs DXGI desktop duplication hooks.
     * @param adapterIndex Selection reference (0 represents default screen outputs).
     */
    bool Initialize(UINT adapterIndex, FrameCallback callback);

    /**
     * @brief Triggers the dedicated capture polling background loops.
     */
    bool Start();

    /**
     * @brief Signals threads to cease capture operations and cleans DXGI pipelines.
     */
    void Shutdown();

    /**
     * @brief Triggers immediate re-initialization of DXGI, useful when recovering connections
     * following screen timeouts or profile switches.
     */
    bool ResetDuplication();

    bool IsRunning() const { return m_isRunning.load(); }

private:
    void CaptureLoop();
    bool EstablishD3DDeviceAndDXGI(UINT adapterIndex);

    std::atomic<bool>      m_isRunning;
    std::thread            m_captureThread;
    FrameCallback          m_callback;

    // Direct3D 11 and DXGI pointers
    ID3D11Device*           m_d3dDevice = nullptr;
    ID3D11DeviceContext*    m_d3dContext = nullptr;
    IDXGIOutputDuplication* m_deskDupl = nullptr;
    ID3D11Texture2D*        m_cursorTexture = nullptr;

    // Monitored target attributes
    UINT                    m_adapterIndex = 0;
    DXGI_OUTPUT_DESC        m_outputDesc = {};
    std::mutex              m_apiMutex;

    // Emulation properties for fallback contexts
    bool                    m_isFallbackMode = false;
};

} // namespace PlugScreen

#endif // PLUGSCREEN_CAPTURE_SCREEN_CAPTURE_H
