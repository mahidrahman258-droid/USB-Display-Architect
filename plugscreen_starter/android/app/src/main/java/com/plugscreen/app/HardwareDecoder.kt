package com.plugscreen.app

import android.media.MediaCodec
import android.media.MediaFormat
import android.util.Log
import android.view.Surface
import java.nio.ByteBuffer

/**
 * @brief Manages Android low-level MediaCodec video decoding pipeline.
 * Bypasses virtual memory copying by decoding directly onto a native Surface.
 */
class HardwareDecoder(private val targetSurface: Surface) : Runnable {

    companion object {
        private const val TAG = "HardwareDecoder"
        private const val MIME_TYPE = MediaFormat.MIMETYPE_VIDEO_AVC // H.264 standard
        private const val DEQUEUE_TIMEOUT_US = 5000L // 5ms timeout check
    }

    private var decoder: MediaCodec? = null
    private var isPlaying = false
    private var decodeThread: Thread? = null

    /**
     * @brief Instantiates the hardware AVC decoder unit matching host screen sizes.
     */
    fun configure(width: Int, height: Int) {
        try {
            val format = MediaFormat.createVideoFormat(MIME_TYPE, width, height)
            
            // Ultra-low latency decoding configuration
            format.setInteger(MediaFormat.KEY_COLOR_FORMAT, android.media.MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface)
            format.setInteger(MediaFormat.KEY_LATENCY, 1) // Force single frame low latency decoding
            format.setInteger(MediaFormat.KEY_PRIORITY, 0) // Max scheduling priority
            
            decoder = MediaCodec.createDecoderByType(MIME_TYPE).apply {
                configure(format, targetSurface, null, 0)
                start()
            }
            
            isPlaying = true
            decodeThread = Thread(this, "PlugScreen-DecoderThread").apply { start() }
            Log.i(TAG, "Hardware decoder spawned. Viewport: ${width}x${height}")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to initialize native MediaCodec decoder: ${e.message}", e)
        }
    }

    /**
     * @brief Accepts raw binary H.264 slice streams coming from the USB thread,
     * and queues them directly inside the silicon chips' input buffers.
     */
    fun onFrameDataReceived(compressedData: ByteArray, presentationTimeUs: Long, isKeyframe: Boolean) {
        val codec = decoder ?: return
        if (!isPlaying) return

        try {
            val inputBufferIndex = codec.dequeueInputBuffer(DEQUEUE_TIMEOUT_US)
            if (inputBufferIndex >= 0) {
                val inputBuffer: ByteBuffer = codec.getInputBuffer(inputBufferIndex) ?: return
                inputBuffer.clear()
                inputBuffer.put(compressedData)

                val flags = if (isKeyframe) MediaCodec.BUFFER_FLAG_KEY_FRAME else 0
                
                codec.queueInputBuffer(
                    inputBufferIndex,
                    0,
                    compressedData.size,
                    presentationTimeUs,
                    flags
                )
            }
        } catch (e: Exception) {
            Log.e(TAG, "Input queue feed failed: ${e.message}")
        }
    }

    override fun run() {
        val bufferInfo = MediaCodec.BufferInfo()
        Log.i(TAG, "Running frame retrieval thread...")

        while (isPlaying) {
            val codec = decoder ?: continue
            try {
                // Dequeue finished output frames
                val outputBufferIndex = codec.dequeueOutputBuffer(bufferInfo, DEQUEUE_TIMEOUT_US)
                
                if (outputBufferIndex >= 0) {
                    // CRUCIAL: Passing true routes the decoded yuv texture directly
                    // to the mapped OpenGL screen Surface with zero heap memory overhead!
                    codec.releaseOutputBuffer(outputBufferIndex, true)
                } else if (outputBufferIndex == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                    val newFormat = codec.outputFormat
                    Log.i(TAG, "Output format updated: $newFormat")
                }
            } catch (e: Exception) {
                Log.e(TAG, "Rendering cycle loop errored: ${e.message}")
            }
        }
    }

    /**
     * @brief Shuts down working threads and releases MediaCodec pointers.
     */
    fun release() {
        isPlaying = false
        try {
            decodeThread?.join(1000)
        } catch (e: InterruptedException) {
            Log.e(TAG, "Join thread interrupted.")
        }
        
        try {
            decoder?.apply {
                stop()
                release()
            }
            decoder = null
            Log.i(TAG, "Decoder release complete.")
        } catch (e: Exception) {
            Log.e(TAG, "Encountered failures during decoder teardowns: ${e.message}")
        }
    }
}
