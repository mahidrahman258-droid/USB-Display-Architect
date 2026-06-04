package com.plugscreen.usb

import android.hardware.usb.UsbAccessory
import android.hardware.usb.UsbManager
import android.os.ParcelFileDescriptor
import android.util.Log
import java.io.ByteArrayOutputStream
import java.io.FileDescriptor
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong

/**
 * @brief High-performance binary transport layer using Android Open Accessory (AOA).
 * Synchronizes frames from Host and schedules outward multi-touch transactions.
 */
class AoaTransport(
    private val usbManager: UsbManager,
    private val callback: TransportCallback
) {

    interface TransportCallback {
        fun onVideoFrameReceived(data: ByteArray, timestampUs: Long, isKeyframe: Boolean)
        fun onTransportError(error: String)
        fun onTransportStatusChanged(connected: Boolean, details: String)
        fun onHandshakeCompleted(width: Int, height: Int, fps: Int, bitrate: Long, name: String)
    }

    companion object {
        private const val TAG = "AoaTransport"
        private val MAGIC_BYTES = byteArrayOf('P'.code.toByte(), 'S'.code.toByte(), 'C'.code.toByte(), 'R'.code.toByte())
        private const val HEADER_SIZE = 22

        // Protocol types matching Host definitions
        const val TYPE_HANDSHAKE_REQUEST: Byte = 0x01
        const val TYPE_HANDSHAKE_RESPONSE: Byte = 0x02
        const val TYPE_HEARTBEAT: Byte = 0x03
        const val TYPE_VIDEO_FRAME: Byte = 0x04
        const val TYPE_INPUT_EVENT: Byte = 0x05
        const val TYPE_DISCONNECT: Byte = 0x06

        // Slice frame flags
        const val FLAG_KEYFRAME: Byte = 0x01
        const val FLAG_FIRST_SLICE: Byte = 0x02
        const val FLAG_LAST_SLICE: Byte = 0x04
    }

    private var fileDescriptor: ParcelFileDescriptor? = null
    private var inputStream: FileInputStream? = null
    private var outputStream: FileOutputStream? = null

    private val isRunning = AtomicBoolean(false)
    private var readerThread: Thread? = null
    private var heartbeatThread: Thread? = null

    // Heartbeat tracking
    private val lastActivityTime = AtomicLong(System.currentTimeMillis())

    // Frame synchronization / slice reassembly
    private var sliceAssembleStream = ByteArrayOutputStream()
    private var sliceFirstTimestampUs = 0L
    private var sliceIsKeyframe = false

    // Single threaded pool dedicated to bulk transfers without stalling main UI or acquisition
    private val writeExecutor = ThreadPoolExecutor(
        1, 1, 0L, TimeUnit.MILLISECONDS,
        LinkedBlockingQueue(128)
    )

    /**
     * @brief Hooks native accessories, obtains system file descriptors and starts packetizer loop.
     */
    fun connect(accessory: UsbAccessory): Boolean {
        if (isRunning.get()) return true

        try {
            Log.d(TAG, "Accessing accessory descriptor endpoints...")
            fileDescriptor = usbManager.openAccessory(accessory)
            val fd: FileDescriptor? = fileDescriptor?.fileDescriptor

            if (fd == null) {
                Log.e(TAG, "Failed to resolve file descriptor. Root context null.")
                callback.onTransportError("Could not retrieve USB Accessory file descriptor")
                return false
            }

            inputStream = FileInputStream(fd)
            outputStream = FileOutputStream(fd)

            isRunning.set(true)
            lastActivityTime.set(System.currentTimeMillis())

            readerThread = Thread(ReaderRunnable(), "AoaReader-Worker").apply { start() }
            heartbeatThread = Thread(HeartbeatRunnable(), "AoaHeartbeat-Worker").apply { start() }

            callback.onTransportStatusChanged(true, "Connected to physical USB accessory: ${accessory.model}")
            
            // Instantly send backward handshake advertisement to Windows monitor host
            sendHandshakePacket(accessory.model ?: "Android Client Display")

            return true
        } catch (e: Exception) {
            Log.e(TAG, "AOA link establishment failed: ${e.message}", e)
            callback.onTransportError("AOA Exception: ${e.message}")
            close()
            return false
        }
    }

    /**
     * @brief Transmit handshake response packet back to Host to synchronize dimensions.
     */
    private fun sendHandshakePacket(deviceName: String) {
        val nameBytes = deviceName.toByteArray(Charsets.UTF_8)
        val stringLen = nameBytes.size.coerceAtMost(32)

        // HandshakePayload Struct: Width:2, Height:2, FPS:2, Bitrate:4, DeviceName:32 -> total 42 bytes payload
        val payload = ByteBuffer.allocate(42).apply {
            order(ByteOrder.LITTLE_ENDIAN)
            putShort(1920.toShort()) // Desired Width
            putShort(1080.toShort()) // Desired Height
            putShort(60.toShort())   // Preferred Framerate
            putInt(6000000)          // targetBitrate: 6 Mbps default
            put(nameBytes, 0, stringLen)
            // Fill remainder with zeros
            if (stringLen < 32) {
                put(ByteArray(32 - stringLen))
            }
        }.array()

        sendPacket(TYPE_HANDSHAKE_RESPONSE, payload)
    }

    /**
     * @brief Asynchronously write coordinate backflow packet onto active endpoint.
     */
    fun sendInputGesture(action: Byte, touchId: Short, normalizedX: Short, normalizedY: Short) {
        // Payload size: 7 bytes (action: 1, touchId: 2, coordX: 2, coordY: 2)
        val payload = ByteBuffer.allocate(7).apply {
            order(ByteOrder.LITTLE_ENDIAN)
            put(action)
            putShort(touchId)
            putShort(normalizedX)
            putShort(normalizedY)
        }.array()

        sendPacket(TYPE_INPUT_EVENT, payload)
    }

    /**
     * @brief Queues serial stream operations into file endpoints.
     */
    private fun sendPacket(type: Byte, payload: ByteArray) {
        if (!isRunning.get()) return

        writeExecutor.execute {
            val stream = outputStream ?: return@execute
            try {
                // Header payload structure layout matching standard Spec
                val header = ByteBuffer.allocate(HEADER_SIZE).apply {
                    order(ByteOrder.LITTLE_ENDIAN)
                    put(MAGIC_BYTES)
                    put(type)
                    putInt(payload.size)
                    putLong(System.nanoTime() / 1000) // Timestamp microseconds
                    putInt(0) // Sequence default
                    put(0.toByte()) // Flags
                }

                synchronized(stream) {
                    stream.write(header.array())
                    if (payload.isNotEmpty()) {
                        stream.write(payload)
                    }
                    stream.flush()
                }
            } catch (e: Exception) {
                Log.e(TAG, "Packet transmit failure. Type ${"0x%02X".format(type)}: ${e.message}")
            }
        }
    }

    /**
     * @brief Closes physical transfers, releasing sockets.
     */
    fun close() {
        if (!isRunning.compareAndSet(true, false)) return

        Log.i(TAG, "Tearing down transportation links...")
        
        try {
            fileDescriptor?.close()
        } catch (e: IOException) {
            Log.e(TAG, "Descriptor closure error: ${e.message}")
        }

        inputStream = null
        outputStream = null
        fileDescriptor = null

        callback.onTransportStatusChanged(false, "Disconnected")

        try {
            readerThread?.interrupt()
            readerThread?.join(500)
        } catch (e: Exception) {
            Log.e(TAG, "Failed joining reader worker: ${e.message}")
        }
        readerThread = null

        try {
            heartbeatThread?.interrupt()
            heartbeatThread?.join(500)
        } catch (e: Exception) {
            Log.e(TAG, "Failed joining heartbeat worker: ${e.message}")
        }
        heartbeatThread = null
    }

    /**
     * @brief Performs periodic handshake verification and streams heartbeat echoes.
     */
    private inner class HeartbeatRunnable : Runnable {
        override fun run() {
            Log.i(TAG, "AOA heartbeat worker online.")
            while (isRunning.get()) {
                try {
                    Thread.sleep(1500)
                    
                    // Connection wellness Check: Timeout after 5 seconds of total silence
                    if (System.currentTimeMillis() - lastActivityTime.get() > 5000) {
                        Log.e(TAG, "Heartbeat failure! Host has stop responding. Requesting re-link.")
                        callback.onTransportError("Heartbeat timeout: transport silent for more than 5000ms")
                        close()
                        break
                    }
                    
                    // Frame ping bidirection status
                    val pingBytes = ByteBuffer.allocate(8).apply {
                        order(ByteOrder.LITTLE_ENDIAN)
                        putLong(System.nanoTime() / 1000)
                    }.array()
                    sendPacket(TYPE_HEARTBEAT, pingBytes)

                } catch (e: InterruptedException) {
                    break
                } catch (e: Exception) {
                    Log.e(TAG, "Heartbeat scheduler error: ${e.message}")
                }
            }
            Log.i(TAG, "Heartbeat worker shutdown complete.")
        }
    }

    private inner class ReaderRunnable : Runnable {
        override fun run() {
            val stream = inputStream ?: return
            val headerBuffer = ByteBuffer.allocate(HEADER_SIZE).apply {
                order(ByteOrder.LITTLE_ENDIAN)
            }

            Log.i(TAG, "Physical AOA listener thread online.")

            while (isRunning.get()) {
                try {
                    headerBuffer.clear()

                    // 1. Sliding window alignment matching "PSCR" Magic sequence
                    var aligned = false
                    while (!aligned && isRunning.get()) {
                        val first = stream.read()
                        if (first == -1) throw IOException("EOF stream reached on endpoint.")

                        if (first == MAGIC_BYTES[0].toInt()) {
                            val second = stream.read()
                            val third = stream.read()
                            val fourth = stream.read()

                            if (second == MAGIC_BYTES[1].toInt() &&
                                third == MAGIC_BYTES[2].toInt() &&
                                fourth == MAGIC_BYTES[3].toInt()) {
                                aligned = true
                            }
                        }
                    }

                    if (!aligned) continue

                    // 2. Read full header details beyond matching magic
                    var bytesRead = 0
                    while (bytesRead < HEADER_SIZE - 4) {
                        val chunk = stream.read(
                            headerBuffer.array(),
                            4 + bytesRead,
                            (HEADER_SIZE - 4) - bytesRead
                        )
                        if (chunk == -1) throw IOException("Unexpected socket termination in header block.")
                        bytesRead += chunk
                    }

                    // Parse header components
                    headerBuffer.position(4)
                    val packetType = headerBuffer.get()
                    val payloadLength = headerBuffer.getInt()
                    val timestampUs = headerBuffer.getLong()
                    val sequenceIndex = headerBuffer.getInt()
                    val flags = headerBuffer.get()

                    // Sanity check allocation limits to prevent OOM
                    if (payloadLength < 0 || payloadLength > 10 * 1024 * 1024) {
                        Log.e(TAG, "Received anomalous packet size of: $payloadLength. Aborting alignment.")
                        continue
                    }

                    // 3. Receive exact payload matching sizes
                    val payload = ByteArray(payloadLength)
                    var payloadRead = 0
                    while (payloadRead < payloadLength) {
                        val chunk = stream.read(payload, payloadRead, payloadLength - payloadRead)
                        if (chunk == -1) throw IOException("End of file reading payload contents.")
                        payloadRead += chunk
                    }

                    // Register activity tick to prevent heartbeat timeouts
                    lastActivityTime.set(System.currentTimeMillis())

                    // 4. Distribute processed frames to observers
                    when (packetType) {
                        TYPE_VIDEO_FRAME -> { 
                            val isKeyframe = (flags.toInt() and FLAG_KEYFRAME.toInt()) != 0
                            val isFirstSlice = (flags.toInt() and FLAG_FIRST_SLICE.toInt()) != 0
                            val isLastSlice = (flags.toInt() and FLAG_LAST_SLICE.toInt()) != 0

                            if (isFirstSlice || isLastSlice) {
                                // Frame slice de-fragmentation
                                synchronized(sliceAssembleStream) {
                                    if (isFirstSlice) {
                                        sliceAssembleStream.reset()
                                        sliceFirstTimestampUs = timestampUs
                                        sliceIsKeyframe = isKeyframe
                                    }
                                    try {
                                        sliceAssembleStream.write(payload)
                                    } catch (e: Exception) {
                                        Log.e(TAG, "Failed appending packet slice: ${e.message}")
                                    }
                                    if (isLastSlice) {
                                        val fullFrame = sliceAssembleStream.toByteArray()
                                        sliceAssembleStream.reset()
                                        callback.onVideoFrameReceived(fullFrame, sliceFirstTimestampUs, sliceIsKeyframe)
                                    }
                                }
                            } else {
                                // Single unit frame payload
                                callback.onVideoFrameReceived(payload, timestampUs, isKeyframe)
                            }
                        }
                        TYPE_HANDSHAKE_REQUEST -> { 
                            val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
                            val w = buffer.short.toInt()
                            val h = buffer.short.toInt()
                            val fps = buffer.short.toInt()
                            val bitrate = buffer.int.toLong()
                            val devNameBytes = ByteArray(32)
                            buffer.get(devNameBytes)
                            val name = String(devNameBytes).trim { it <= ' ' }
                            
                            callback.onHandshakeCompleted(w, h, fps, bitrate, name)

                            // Respond back instantly with HandshakeResponse to notify we aligned the resolution
                            sendHandshakePacket(android.os.Build.MODEL ?: "Android Client Display")
                        }
                        TYPE_HEARTBEAT -> {
                            // Heartbeat response / echo can also be handled here to update timestamps
                        }
                        TYPE_DISCONNECT -> { 
                            Log.w(TAG, "Host requested stream disconnection.")
                            callback.onTransportStatusChanged(false, "Host requested disconnect")
                            isRunning.set(false)
                        }
                    }

                } catch (e: Exception) {
                    if (isRunning.get()) {
                        Log.e(TAG, "Transportation execution loop errored: ${e.message}")
                        callback.onTransportError("Aoa IO Error: ${e.message}")
                        isRunning.set(false)
                    }
                    break
                }
            }

            Log.i(TAG, "AOA reader worker thread joined safely.")
        }
    }
}
