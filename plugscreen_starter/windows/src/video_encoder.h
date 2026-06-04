#ifndef PLUGSCREEN_WINDOWS_VIDEO_ENCODER_H
#define PLUGSCREEN_WINDOWS_VIDEO_ENCODER_H

#include <windows.h>
#include <functional>
#include <vector>
#include <mutex>

// Modern FFmpeg imports inside C linkage bindings
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

namespace PlugScreen {

class VideoEncoder {
public:
    using EncodedCallback = std::function<void(const uint8_t* pData, size_t size, uint64_t timestampUs, bool isKeyframe)>;

    VideoEncoder();
    ~VideoEncoder();

    bool Initialize(uint16_t width, uint16_t height, uint16_t fps, uint32_t bitrate, EncodedCallback callback);
    
    /**
     * @brief Accepts a native D3D11 Texture pointer, maps it to CPU/GPU RAM,
     * changes color representations to YUV425p, compiles H.264 bitstream chunk.
     */
    bool EncodeFrame(ID3D11Texture2D* pTexture, ID3D11DeviceContext* pContext, uint64_t timestampUs);
    
    void Shutdown();

private:
    bool SetupFFmpegCtx();
    void CleanupFFmpeg();

    uint16_t            m_width = 1920;
    uint16_t            m_height = 1080;
    uint16_t            m_fps = 60;
    uint32_t            m_bitrate = 4000000;
    int64_t             m_frameCount = 0;

    EncodedCallback     m_callback;

    // FFmpeg state variables
    const AVCodec*      m_pCodec = nullptr;
    AVCodecContext*     m_pCodecContext = nullptr;
    AVFrame*            m_pYuvFrame = nullptr;
    AVPacket*           m_pPacket = nullptr;

    std::mutex          m_encoderMutex;
    bool                m_initialized = false;
};

} // namespace PlugScreen

#endif // PLUGSCREEN_WINDOWS_VIDEO_ENCODER_H
