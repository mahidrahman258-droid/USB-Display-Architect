#include "video_encoder.h"
#include "logger.h"

namespace PlugScreen {

VideoEncoder::VideoEncoder() : m_initialized(false) {}

VideoEncoder::~VideoEncoder() {
    Shutdown();
}

bool VideoEncoder::Initialize(uint16_t width, uint16_t height, uint16_t fps, uint32_t bitrate, EncodedCallback callback) {
    std::lock_guard<std::mutex> lock(m_encoderMutex);
    
    m_width = width;
    m_height = height;
    m_fps = fps;
    m_bitrate = bitrate;
    m_callback = callback;
    m_frameCount = 0;

    LOG_INFO("ENCODER", "Configuring H.264 real-time compression engine (" + 
                       std::to_string(width) + "x" + std::to_string(height) + " @ " + 
                       std::to_string(fps) + "fps, target " + std::to_string(bitrate / 1000) + " kbps)");

    if (!SetupFFmpegCtx()) {
        LOG_ERROR("ENCODER", "Could not load or structure FFmpeg encoder contexts.");
        return false;
    }

    m_initialized = true;
    return true;
}

bool VideoEncoder::SetupFFmpegCtx() {
    // 1. Discover H.264 encoder
    m_pCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!m_pCodec) {
        LOG_ERROR("ENCODER", "FFmpeg failed to discover native libx264 or compatible H.264 encoder.");
        return false;
    }

    // 2. Allocate configuration context
    m_pCodecContext = avcodec_alloc_context3(m_pCodec);
    if (!m_pCodecContext) return false;

    // 3. Configure low-latency visual mirroring variables
    m_pCodecContext->width = m_width;
    m_pCodecContext->height = m_height;
    m_pCodecContext->pix_fmt = AV_PIX_FMT_YUV420P; // Highly compatible standard mobile decoding format
    m_pCodecContext->time_base = { 1, static_cast<int>(m_fps) };
    m_pCodecContext->framerate = { static_cast<int>(m_fps), 1 };
    
    // Constant Bitrate Tuning Setup
    m_pCodecContext->bit_rate = m_bitrate;
    m_pCodecContext->rc_max_rate = m_bitrate;
    m_pCodecContext->rc_min_rate = m_bitrate;
    m_pCodecContext->rc_buffer_size = static_cast<int>(m_bitrate / 10); // Ultra-frequent rate checking loops

    // GOP structures to handle fast stream entry & error boundaries
    m_pCodecContext->gop_size = static_cast<int>(m_fps / 2); // Send I-frames twice a second
    m_pCodecContext->keyint_min = static_cast<int>(m_fps / 4);
    m_pCodecContext->max_b_frames = 0; // STRICT: Zero bidirected frames to eliminate pipeline packing latency

    // 4. Inject specific low-lantence h264 presets via dict args
    av_opt_set(m_pCodecContext->priv_data, "preset", "ultrafast", 0);
    av_opt_set(m_pCodecContext->priv_data, "tune", "zerolatency", 0);

    // Complete registration
    int ret = avcodec_open2(m_pCodecContext, m_pCodec, nullptr);
    if (ret < 0) {
        LOG_ERROR("ENCODER", "avcodec_open2 initialization failed with code: " + std::to_string(ret));
        return false;
    }

    // 5. Structure active working frames
    m_pYuvFrame = av_frame_alloc();
    m_pYuvFrame->format = m_pCodecContext->pix_fmt;
    m_pYuvFrame->width = m_width;
    m_pYuvFrame->height = m_height;

    ret = av_image_alloc(m_pYuvFrame->data, m_pYuvFrame->linesize, m_width, m_height, m_pCodecContext->pix_fmt, 32);
    if (ret < 0) {
        LOG_ERROR("ENCODER", "FFmpeg pixel workspace allocation failed.");
        return false;
    }

    m_pPacket = av_packet_alloc();
    return true;
}

bool VideoEncoder::EncodeFrame(ID3D11Texture2D* pTexture, ID3D11DeviceContext* pContext, uint64_t timestampUs) {
    std::lock_guard<std::mutex> lock(m_encoderMutex);
    if (!m_initialized) return false;

    // --- GPU vram texture fetch to CPU-staged mapping ---
    D3D11_TEXTURE2D_DESC desc;
    pTexture->GetDesc(&desc);

    ID3D11Device* pDevice = nullptr;
    pContext->GetDevice(&pDevice);

    // Create a CPU-accessible staging asset to read GPU memory contents
    ID3D11Texture2D* pStagingTexture = nullptr;
    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;

    HRESULT hr = pDevice->CreateTexture2D(&stagingDesc, nullptr, &pStagingTexture);
    pDevice->Release();
    if (FAILED(hr)) return false;

    // Copy original HW textures to CPU Stage
    pContext->CopyResource(pStagingTexture, pTexture);

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = pContext->Map(pStagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        pStagingTexture->Release();
        return false;
    }

    // --- Color conversion emulation & packing straight to YUV420p workspace ---
    // In full-production, users map YUV12/BGRA frames into target chroma space via FFmpeg swscale or libyuv.
    const uint8_t* pSrc = reinterpret_cast<const uint8_t*>(mapped.pData);
    
    // Quick demo: paint dummy YUV sequence with basic color mappings
    for (int y = 0; y < m_height; ++y) {
        std::memset(m_pYuvFrame->data[0] + y * m_pYuvFrame->linesize[0], 0x7F, m_width); // Y plane
    }
    for (int y = 0; y < m_height / 2; ++y) {
        std::memset(m_pYuvFrame->data[1] + y * m_pYuvFrame->linesize[1], 0x80, m_width / 2); // U plane
        std::memset(m_pYuvFrame->data[2] + y * m_pYuvFrame->linesize[2], 0x80, m_width / 2); // V plane
    }

    pContext->Unmap(pStagingTexture, 0);
    pStagingTexture->Release();

    // Assign frame indices
    m_pYuvFrame->pts = m_frameCount++;

    // 6. Push raw video payload blocks into active FFmpeg thread
    int ret = avcodec_send_frame(m_pCodecContext, m_pYuvFrame);
    if (ret < 0) {
        LOG_WARN("ENCODER", "Error during avcodec_send_frame payload pushes.");
        return false;
    }

    // 7. Extract compressed chunks
    while (ret >= 0) {
        ret = avcodec_receive_packet(m_pCodecContext, m_pPacket);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            LOG_ERROR("ENCODER", "Media compile loop failures.");
            return false;
        }

        bool isKeyframe = (m_pPacket->flags & AV_PKT_FLAG_KEY) != 0;

        // Propagate encoded packet upwards to transport handlers
        m_callback(m_pPacket->data, m_pPacket->size, timestampUs, isKeyframe);

        av_packet_unref(m_pPacket);
    }

    return true;
}

void VideoEncoder::CleanupFFmpeg() {
    if (m_pPacket) {
        av_packet_free(&m_pPacket);
        m_pPacket = nullptr;
    }
    if (m_pYuvFrame) {
        if (m_pYuvFrame->data[0]) {
            av_freep(&m_pYuvFrame->data[0]);
        }
        av_frame_free(&m_pYuvFrame);
        m_pYuvFrame = nullptr;
    }
    if (m_pCodecContext) {
        avcodec_free_context(&m_pCodecContext);
        m_pCodecContext = nullptr;
    }
    m_pCodec = nullptr;
}

void VideoEncoder::Shutdown() {
    std::lock_guard<std::mutex> lock(m_encoderMutex);
    CleanupFFmpeg();
    m_initialized = false;
    LOG_INFO("ENCODER", "FFmpeg compression codecs released.");
}

} // namespace PlugScreen
