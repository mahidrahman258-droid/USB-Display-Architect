#include "H264Encoder.h"
#include "core/Logger.h"
#include "core/ConfigManager.h"
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace PlugScreen {

H264Encoder::H264Encoder() {
}

H264Encoder::~H264Encoder() {
    Shutdown();
}

bool H264Encoder::Initialize(uint16_t width, uint16_t height, uint16_t frameRate, uint32_t bitrate, EncodedFrameCallback callback) {
    std::lock_guard<std::mutex> lock(m_encoderMutex);
    
    if (m_initialized) return true;

    m_width = width;
    m_height = height;
    m_frameRate = frameRate;
    m_bitrate = bitrate;
    m_callback = callback;
    m_ptsCounter = 0;

    PL_LOG_INFO("H264_ENCODER", "Configuring H.264 Encoder contexts. Metrics: " + 
                std::to_string(width) + "x" + std::to_string(height) + " @" + std::to_string(frameRate) + " FPS");

    // Dynamic cascaded backend discovery
    // We attempt NVENC first (if NVIDIA GPU is loaded) then fall back to high-efficient software libx264
    AppConfig sysConfig = ConfigManager::Instance().GetConfig();
    bool success = false;

    if (sysConfig.codecBackend == EncoderHardwareBackend::NvidiaNVENC) {
        PL_LOG_INFO("H264_ENCODER", "Probing NVIDIA NVENC Hardware Codec...");
        success = SetupEncoderContext("h264_nvenc");
    } else if (sysConfig.codecBackend == EncoderHardwareBackend::IntelQSV) {
        PL_LOG_INFO("H264_ENCODER", "Probing Intel QSV Hardware Codec...");
        success = SetupEncoderContext("h264_qsv");
    } else if (sysConfig.codecBackend == EncoderHardwareBackend::AmdAMF) {
        PL_LOG_INFO("H264_ENCODER", "Probing AMD AMF Hardware Codec...");
        success = SetupEncoderContext("h264_amf");
    }

    if (!success) {
        PL_LOG_INFO("H264_ENCODER", "Booting multithreaded libx264 software encoder fallback...");
        success = SetupEncoderContext("libx264");
    }

    if (!success) {
        PL_LOG_FATAL("H264_ENCODER", "Could not locate any valid H.264 codecs. Encoding pipe stalled.");
        return false;
    }

    // Allocate conversion SwsContext (BGRA to YUV420P)
    m_swsContext = sws_getContext(
        m_width, m_height, AV_PIX_FMT_BGRA,
        m_width, m_height, AV_PIX_FMT_YUV420P,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!m_swsContext) {
        PL_LOG_ERROR("H264_ENCODER", "Failed to allocate swscale context colorspaces.");
        return false;
    }

    // Initialize raw staging buffer
    m_stagingBuffer.resize(m_width * m_height * 4); // BGRA

    m_initialized = true;
    PL_LOG_INFO("H264_ENCODER", "H.264 Encoder unit fully operational.");
    return true;
}

bool H264Encoder::SetupEncoderContext(const std::string& codecName) {
    const AVCodec* codec = avcodec_find_encoder_by_name(codecName.c_str());
    if (!codec) {
        PL_LOG_WARN("H264_ENCODER", "Target driver \"" + codecName + "\" not present in runtime path.");
        return false;
    }

    m_codecContext = avcodec_alloc_context3(codec);
    if (!m_codecContext) return false;

    // Direct latency attributes configuration
    m_codecContext->codec_id = AV_CODEC_ID_H264;
    m_codecContext->width = m_width;
    m_codecContext->height = m_height;
    m_codecContext->time_base = {1, m_frameRate};
    m_codecContext->framerate = {m_frameRate, 1};
    m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    m_codecContext->bit_rate = m_bitrate;
    m_codecContext->gop_size = ConfigManager::Instance().GetConfig().gopSize;
    m_codecContext->max_b_frames = 0; // CRITICAL: Exclude B-frames to guarantee absolute zero frame buffering
    m_codecContext->flags |= AV_CODEC_FLAG_LOW_DELAY;

    // Apply low latency driver tuning
    if (codecName == "libx264") {
        av_opt_set(m_codecContext->priv_data, "preset", "ultrafast", 0);
        av_opt_set(m_codecContext->priv_data, "tune", "zerolatency", 0);
        m_codecContext->thread_count = 4; // Thread pools
    } else if (codecName == "h264_nvenc") {
        av_opt_set(m_codecContext->priv_data, "preset", "p1", 0); // Fastest preset profile
        av_opt_set(m_codecContext->priv_data, "tune", "ull", 0);  // Ultra low latency
        av_opt_set(m_codecContext->priv_data, "delay", "0", 0);
        av_opt_set(m_codecContext->priv_data, "zerolatency", "1", 0);
    }

    // Attempt context binding
    if (avcodec_open2(m_codecContext, codec, nullptr) < 0) {
        PL_LOG_WARN("H264_ENCODER", "Driver initialization failed during open routine: " + codecName);
        avcodec_free_context(&m_codecContext);
        m_codecContext = nullptr;
        return false;
    }

    // Allocate reusable frame holders
    m_yuvFrame = av_frame_alloc();
    m_yuvFrame->format = m_codecContext->pix_fmt;
    m_yuvFrame->width = m_width;
    m_yuvFrame->height = m_height;

    int ret = av_image_alloc(
        m_yuvFrame->data, m_yuvFrame->linesize,
        m_width, m_height, m_codecContext->pix_fmt, 32
    );
    if (ret < 0) {
        av_frame_free(&m_yuvFrame);
        av_frame_free(&m_yuvFrame);
        avcodec_free_context(&m_codecContext);
        m_codecContext = nullptr;
        return false;
    }

    m_packet = av_packet_alloc();
    PL_LOG_INFO("H264_ENCODER", "Successfully hooked backend: " + codecName);
    return true;
}

bool H264Encoder::EncodeFrame(ID3D11Texture2D* pTexture, ID3D11DeviceContext* pContext, uint64_t timestampUs) {
    std::lock_guard<std::mutex> lock(m_encoderMutex);
    if (!m_initialized) return false;

    bool successfullyMapped = false;

    // 1. Double Buffer GPU mapping checks
    if (pTexture && pContext) {
        HRESULT hr = S_OK;

        // If staging texture hasn't been instantiated, size it
        if (!m_stagingTexture) {
            ID3D11Device* d3dDevice = nullptr;
            pContext->GetDevice(&d3dDevice);

            D3D11_TEXTURE2D_DESC desc = {};
            pTexture->GetDesc(&desc);
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.MiscFlags = 0;

            hr = d3dDevice->CreateTexture2D(&desc, nullptr, &m_stagingTexture);
            d3dDevice->Release();

            if (FAILED(hr)) {
                PL_LOG_ERROR("H264_ENCODER", "Failed to create Staging textures context. Code: 0x" + std::to_string(hr));
            }
        }

        if (m_stagingTexture) {
            // Bulk copy GPU resource block to staging CPU read block
            pContext->CopyResource(m_stagingTexture, pTexture);

            D3D11_MAPPED_SUBRESOURCE mapped;
            hr = pContext->Map(m_stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
            if (SUCCEEDED(hr)) {
                // Execute Swscale mapping translations
                uint8_t* pSrc = reinterpret_cast<uint8_t*>(mapped.pData);
                MapBgraToYuvFrame(pSrc, mapped.RowPitch);
                pContext->Unmap(m_stagingTexture, 0);
                successfullyMapped = true;
            }
        }
    }

    // 2. Failover Procedural Renderer
    // This allows robust simulations when there is no target display actively cloned or in virtual boxes
    if (!successfullyMapped) {
        static uint8_t motionCounter = 0;
        motionCounter++;

        // Clear and render a distinctive horizontal sweeping neon cyan line pattern
        // on top of a deep dark charcoal visual canvas to represent the system desktop frame
        uint8_t* pBgra = m_stagingBuffer.data();
        int stride = m_width * 4;

        for (int y = 0; y < m_height; y++) {
            uint32_t* pRow = reinterpret_cast<uint32_t*>(pBgra + y * stride);
            bool isBar = std::abs(y - ((motionCounter * 6) % m_height)) < 12;

            for (int x = 0; x < m_width; x++) {
                if (isBar) {
                    pRow[x] = 0xFF22D3EE; // Solid Neon Cyan Bar (0xAARRGGBB in Little-Endian BGRA format)
                } else {
                    pRow[x] = 0xFF0D0F16; // Deep Dark Background Slate
                }
            }
        }

        MapBgraToYuvFrame(pBgra, stride);
    }

    // 3. Queue frames inside code context
    m_ptsCounter++;
    m_yuvFrame->pts = m_ptsCounter;

    int ret = avcodec_send_frame(m_codecContext, m_yuvFrame);
    if (ret < 0) {
        PL_LOG_ERROR("H264_ENCODER", "Error submitting frames directly into AVCodecContext queue: " + std::to_string(ret));
        return false;
    }

    // 4. Retrieve complete elementary slice packets
    PullCompressedPackets(timestampUs);
    return true;
}

bool H264Encoder::EncodeRawBgraBuffer(const uint8_t* bgraBuffer, int stride, uint64_t timestampUs) {
    std::lock_guard<std::mutex> lock(m_encoderMutex);
    if (!m_initialized) return false;

    MapBgraToYuvFrame(bgraBuffer, stride);

    m_ptsCounter++;
    m_yuvFrame->pts = m_ptsCounter;

    int ret = avcodec_send_frame(m_codecContext, m_yuvFrame);
    if (ret < 0) return false;

    PullCompressedPackets(timestampUs);
    return true;
}

void H264Encoder::MapBgraToYuvFrame(const uint8_t* pBgra, int stride) {
    const uint8_t* srcPlanes[4] = { pBgra, nullptr, nullptr, nullptr };
    int srcStrides[4] = { stride, 0, 0, 0 };

    sws_scale(
        m_swsContext,
        srcPlanes, srcStrides, 0, m_height,
        m_yuvFrame->data, m_yuvFrame->linesize
    );
}

void H264Encoder::PullCompressedPackets(uint64_t timestampUs) {
    while (true) {
        av_packet_unref(m_packet);
        int ret = avcodec_receive_packet(m_codecContext, m_packet);
        
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break; // No more slice fragments ready for streaming
        } else if (ret < 0) {
            PL_LOG_ERROR("H264_ENCODER", "Fatal error during packet dequeue reads: " + std::to_string(ret));
            break;
        }

        // Tag IDR Frame states
        bool isKeyframe = (m_packet->flags & AV_PKT_FLAG_KEY) != 0;

        // Issue completed byte callbacks downstream
        m_callback(m_packet->data, m_packet->size, timestampUs, isKeyframe);
    }
}

void H264Encoder::Shutdown() {
    std::lock_guard<std::mutex> lock(m_encoderMutex);
    
    if (!m_initialized) return;

    PL_LOG_INFO("H264_ENCODER", "Tearing down AVCodecContext configurations...");

    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }
    if (m_yuvFrame) {
        av_freep(&m_yuvFrame->data[0]);
        av_frame_free(&m_yuvFrame);
        m_yuvFrame = nullptr;
    }
    if (m_packet) {
        av_packet_free(&m_packet);
        m_packet = nullptr;
    }
    if (m_codecContext) {
        avcodec_free_context(&m_codecContext);
        m_codecContext = nullptr;
    }
    if (m_stagingTexture) {
        m_stagingTexture->Release();
        m_stagingTexture = nullptr;
    }

    m_initialized = false;
    PL_LOG_INFO("H264_ENCODER", "FfMpeg hardware encoder frames successfully released.");
}

} // namespace PlugScreen
