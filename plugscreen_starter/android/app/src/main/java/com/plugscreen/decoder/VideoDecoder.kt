package com.plugscreen.decoder

import android.media.MediaCodec
import android.media.MediaFormat
import android.util.Log
import android.view.Surface
import java.nio.ByteBuffer
import java.util.concurrent.atomic.AtomicBoolean

/**
 * @brief Thread-safe low-latency Android MediaCodec H.264 decoder.
 * Feeds silicon-level queues and renders directly onto native surfaces.
 */
class VideoDecoder(private val surface: Surface) {

    companion object {
        private const val TAG = "VideoDecoder"
        private const val MIME_TYPE = MediaFormat.MIMETYPE_VIDEO_AVC
        private const val TIMEOUT_US = 8000L // Dequeue timeout of 8ms
    }

    private var codec: MediaCodec? = null
    private val isDecoding = AtomicBoolean(false)
    private var outputDrainThread: Thread? = null

    /**
     * @brief Instantiates the hardware AVC decoder unit matching native Host display grids.
     */
    fun configure(width: Int, height: Int): Boolean {
        if (isDecoding.get()) {
            release()
        }

        try {
            Log.d(TAG, "Configuring hardware AVC decoder context at: ${width}x${height}")
            val format = MediaFormat.createVideoFormat(MIME_TYPE, width, height)

            // Dynamic hardware optimizations
            format.setInteger(MediaFormat.KEY_COLOR_FORMAT, android.media.MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface)
            format.setInteger(MediaFormat.KEY_LATENCY, 1) // Android latency optimizations
            format.setInteger(MediaFormat.KEY_PRIORITY, 0) // Real-time high priority scheduling
            
            codec = MediaCodec.createDecoderByType(MIME_TYPE).apply {
                configure(format, surface, null, 0)
                start()
            }

            isDecoding.set(true)
            outputDrainThread = Thread(OutputDrainRunnable(), "MediaCodec-DrainWorker").apply { start() }

            Log.i(TAG, "Hardware H.264 decoder successfully started.")
            return true
        } catch (e: Exception) {
            Log.e(TAG, "Could not open MediaCodec codec profiles: ${e.message}", e)
            return false
        }
    }

    /**
     * @brief Feeds compressed video fragments from transport endpoints into hardware input spaces.
     */
    fun feedCompressedBuffer(data: ByteArray, timestampUs: Long, isKeyframe: Boolean) {
        val activeCodec = codec ?: return
        if (!isDecoding.get()) return

        try {
            val inputBufferIndex = activeCodec.dequeueInputBuffer(TIMEOUT_US)
            if (inputBufferIndex >= 0) {
                val inputBuffer = activeCodec.getInputBuffer(inputBufferIndex) ?: return
                inputBuffer.clear()
                inputBuffer.put(data)

                val flags = if (isKeyframe) MediaCodec.BUFFER_FLAG_KEY_FRAME else 0
                
                activeCodec.queueInputBuffer(
                    inputBufferIndex,
                    0,
                    data.size,
                    timestampUs,
                    flags
                )
            } else {
                Log.w(TAG, "H.264 Input buffer capacity full. Delaying frames...")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed indexing frame into decoder input pipeline: ${e.message}")
        }
    }

    /**
     * @brief Safe hardware teardown, releasing binding resources.
     */
    fun release() {
        if (!isDecoding.compareAndSet(true, false)) return

        Log.i(TAG, "Shutting down MediaCodec decoder loops...")
        
        try {
            outputDrainThread?.interrupt()
            outputDrainThread?.join(500)
        } catch (e: Exception) {
            Log.e(TAG, "Failed stopping output reader threads: ${e.message}")
        }
        outputDrainThread = null

        try {
            codec?.stop()
            codec?.release()
        } catch (e: Exception) {
            Log.e(TAG, "Failed releasing MediaCodec components: ${e.message}")
        }
        codec = null
        Log.i(TAG, "Decoder release processes complete.")
    }

    private inner class OutputDrainRunnable : Runnable {
        override fun run() {
            val bufferInfo = MediaCodec.BufferInfo()
            Log.i(TAG, "Output frame rendering thread online.")

            while (isDecoding.get()) {
                val activeCodec = codec ?: continue
                try {
                    val index = activeCodec.dequeueOutputBuffer(bufferInfo, TIMEOUT_US)
                    
                    if (index >= 0) {
                        // Passing TRUE directly commits the frame to the surface at the GPU level
                        // achieving zero-copy rendering to the viewport.
                        activeCodec.releaseOutputBuffer(index, true)
                    } else if (index == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                        Log.i(TAG, "Silicon formats updated: ${activeCodec.outputFormat}")
                    }
                } catch (e: Exception) {
                    if (isDecoding.get()) {
                        Log.e(TAG, "Decoder output drainage encountered an error: ${e.message}")
                    }
                    break
                }
            }

            Log.i(TAG, "Decoder output thread linked handles release completed.")
        }
    }
}
