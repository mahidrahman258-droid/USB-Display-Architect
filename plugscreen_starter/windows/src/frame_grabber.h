#ifndef PLUGSCREEN_WINDOWS_FRAME_GRABBER_H
#define PLUGSCREEN_WINDOWS_FRAME_GRABBER_H

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_5.h>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

namespace PlugScreen {

class FrameGrabber {
public:
    using FrameCallback = std::function<void(ID3D11Texture2D* pTexture, uint64_t timestampUs)>;

    FrameGrabber();
    ~FrameGrabber();

    bool Initialize(int monitorIndex, FrameCallback callback);
    bool Start();
    void Stop();
    void Shutdown();

    bool IsCapturing() const { return m_isRunning; }

private:
    void CaptureLoop();
    HRESULT SetupDirect3D();
    HRESULT SetupDuplication();
    void FreeResources();

    // D3D11 & DXGI Core Interfaces
    ID3D11Device*           m_pd3dDevice = nullptr;
    ID3D11DeviceContext*    m_pImmediateContext = nullptr;
    IDXGIOutputDuplication* m_pDeskDupl = nullptr;
    ID3D11Texture2D*        m_pSharedTexture = nullptr;

    FrameCallback           m_callback;
    std::thread             m_captureThread;
    std::atomic<bool>       m_isRunning;
    int                     m_monitorIndex = 0;
    std::mutex              m_mutex;
};

} // namespace PlugScreen

#endif // PLUGSCREEN_WINDOWS_FRAME_GRABBER_H
