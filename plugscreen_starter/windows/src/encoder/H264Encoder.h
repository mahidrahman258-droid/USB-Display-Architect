#ifndef PLUGSCREEN_ENCODER_H264_ENCODER_H
#define PLUGSCREEN_ENCODER_H264_ENCODER_H

#include <d3d11.h>
#include <stdint.h>
#include <functional>
#include <string>
#include <mutex>
#include <vector>

// Forward declare FFmpeg core contexts to maintain namespace cleanliness
struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace PlugScreen {

/**
 * @brief Handles live frame encoding into compact AVC H.264 elementary streams using FFmpeg.
 * Supports hardware acceleration drivers and guarantees zero input-to-output caching.
 */
class H264Encoder {
public:
    using EncodedFrameCallback = std::function<void(const uint8_t* pData, size_t size, uint64_t timestampUs, bool isKeyframe)>;

    H264Encoder();
    ~H264Encoder();

    /**
     * @brief Allocates FFmpeg video context nodes matching parameters.
     * Automatically attempts hardware NVENC, AMF, QSV, or x264 backends.
     */
    bool Initialize(uint16_t width, uint16_t height, uint16_t frameRate, uint32_t bitrate, EncodedFrameCallback callback);

    /**
     * @brief Accepts standard D3D11 textures, resolves mapping offsets,
     * performs swscale color-space translations (BGRA -> YUV420P), and feeds the encoding pipeline.
     */
    bool EncodeFrame(ID3D11Texture2D* pTexture, ID3D11DeviceContext* pContext, uint64_t timestampUs);

    /**
     * @brief Software rendering fallback when no physical texturing is possible.
     * Directly generates and encodes synthetic H.264 stream slices.
     */
    bool EncodeRawBgraBuffer(const uint8_t* bgraBuffer, int stride, uint64_t timestampUs);

    /**
     * @brief Safely shuts down codecs, flushes pending packets, and releases FFmpeg handles.
     */
    void Shutdown();

    bool IsInitialized() const { return m_initialized; }

private:
    bool SetupEncoderContext(const std::string& codecName);
    void MapBgraToYuvFrame(const uint8_t* pBgra, int stride);
    void PullCompressedPackets(uint64_t timestampUs);

    bool                  m_initialized = false;
    uint16_t              m_width = 1920;
    uint16_t              m_height = 1080;
    uint32_t              m_bitrate = 6000000;
    uint16_t              m_frameRate = 60;
    EncodedFrameCallback  m_callback;

    // FFmpeg objects
    AVCodecContext*       m_codecContext = nullptr;
    AVFrame*              m_yuvFrame = nullptr;
    AVPacket*             m_packet = nullptr;
    SwsContext*           m_swsContext = nullptr;

    // Staging CPU resources
    std::vector<uint8_t>  m_stagingBuffer;
    ID3D11Texture2D*      m_stagingTexture = nullptr;
    std::mutex            m_encoderMutex;

    // Sequence indices tracking stream orders
    int64_t               m_ptsCounter = 0;
};

} // namespace PlugScreen

#endif // PLUGSCREEN_ENCODER_H264_ENCODER_H
