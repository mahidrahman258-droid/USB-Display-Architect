package com.plugscreen.app

import android.app.Service
import android.content.Context
import android.content.Intent
import android.hardware.usb.UsbAccessory
import android.hardware.usb.UsbManager
import android.os.Binder
import android.os.IBinder
import android.os.ParcelFileDescriptor
import android.util.Log
import java.io.FileDescriptor
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * @brief Background Service managing raw Android Open Accessory (AOA) bulk streams.
 * Performs real-time binary packet alignment and framing loops.
 */
class UsbReceiverService : Service() {

    companion object {
        private const val TAG = "UsbReceiverService"
        private const val MAGIC_VAL = 0x50534352 // "PSCR" in big endian
        private const val HEADER_SIZE = 22
    }

    private val binder = LocalBinder()
    private var usbManager: UsbManager? = null
    private var fileDescriptor: ParcelFileDescriptor? = null
    private var inputStream: FileInputStream? = null
    private var outputStream: FileOutputStream? = null
    
    private var isListening = false
    private var ioThread: Thread? = null
    private var decoderRef: HardwareDecoder? = null

    inner class LocalBinder : Binder() {
        fun getService(): UsbReceiverService = this@UsbReceiverService
    }

    override fun onCreate() {
        super.onCreate()
        usbManager = getSystemService(Context.USB_SERVICE) as UsbManager
        Log.i(TAG, "USB receiver background service initialized.")
    }

    override fun onBind(intent: Intent?): IBinder = binder

    /**
     * @brief Hooks a live active GL decoder stream reference to pump raw payloads.
     */
    fun attachDecoder(decoder: HardwareDecoder) {
        this.decoderRef = decoder
    }

    /**
     * @brief Acquires the low-level AOA system file descriptors and starts bulk parsing loops.
     */
    fun startCommunication(accessory: UsbAccessory): Boolean {
        if (isListening) return true

        fileDescriptor = usbManager?.openAccessory(accessory)
        val fd: FileDescriptor? = fileDescriptor?.fileDescriptor

        if (fd == null) {
            Log.e(TAG, "Failed to open USB accessory. FD descriptor returned null.")
            return false
        }

        inputStream = FileInputStream(fd)
        outputStream = FileOutputStream(fd)

        isListening = true
        ioThread = Thread(Runnable { runReaderLoop() }, "PlugScreen-IOThread").apply { start() }
        Log.i(TAG, "USB Accessory bulk streams successfully opened. Pipeline active.")
        return true
    }

    /**
     * @brief Packetizer alignment loop. Implements sliding window search to find [P,S,C,R] magic,
     * unpacks header structures in native Little-Endian format, reads exact payload bytes.
     */
    private fun runReaderLoop() {
        val stream = inputStream ?: return
        val headerBuffer = ByteBuffer.allocate(HEADER_SIZE).apply {
            order(ByteOrder.LITTLE_ENDIAN)
        }

        Log.i(TAG, "Starting binary stream parsing loop.")
        while (isListening) {
            try {
                headerBuffer.clear()
                
                // 1. Read MAGIC signature byte by byte to handle alignment
                var aligned = false
                while (!aligned && isListening) {
                    val singleByte = stream.read()
                    if (singleByte == -1) throw IOException("End of file stream reached.")
                    
                    if (singleByte == 'P'.code) {
                        val s = stream.read()
                        val c = stream.read()
                        val r = stream.read()
                        if (s == 'S'.code && c == 'C'.code && r == 'R'.code) {
                            aligned = true
                        }
                    }
                }

                if (!aligned) continue

                // Magic aligned! Unpack the remaining 18 bytes of the Header
                var bytesRead = 0
                while (bytesRead < HEADER_SIZE - 4) {
                    val r = stream.read(headerBuffer.array(), 4 + bytesRead, (HEADER_SIZE - 4) - bytesRead)
                    if (r == -1) throw IOException("Unexpected socket close inside packet header.")
                    bytesRead += r
                }

                // 2. Unpack parameters
                headerBuffer.position(4) // Move past MAGIC
                val packetType = headerBuffer.get()
                val payloadLength = headerBuffer.getInt()
                val timestampUs = headerBuffer.getLong()
                val sequenceIndex = headerBuffer.getInt()
                val flags = headerBuffer.get()

                // 3. Unpack following Payload bytes
                val payload = ByteArray(payloadLength)
                var payloadBytesRead = 0
                while (payloadBytesRead < payloadLength) {
                    val r = stream.read(payload, payloadBytesRead, payloadLength - payloadBytesRead)
                    if (r == -1) throw IOException("Socket close inside payload body.")
                    payloadBytesRead += r
                }

                // 4. Propagate frames
                if (packetType == 0x04) { // VideoFrame type
                    val isKeyframe = (flags.toInt() and 0x01) != 0
                    decoderRef?.onFrameDataReceived(payload, timestampUs, isKeyframe)
                }

            } catch (e: IOException) {
                Log.e(TAG, "USB streaming loop disconnected: ${e.message}")
                isListening = false
                break;
            }
        }
        Log.i(TAG, "Binary stream parser loop stopped.")
    }

    /**
     * @brief Sends touchscreen gestures and handshake details back over WinUSB bulk streams.
     */
    fun sendDataPacket(packetType: Byte, payload: ByteArray) {
        val stream = outputStream ?: return
        Thread {
            try {
                val header = ByteBuffer.allocate(HEADER_SIZE).apply {
                    order(ByteOrder.LITTLE_ENDIAN)
                    put('P'.code.toByte())
                    put('S'.code.toByte())
                    put('C'.code.toByte())
                    put('R'.code.toByte())
                    put(packetType)
                    putInt(payload.size)
                    putLong(System.nanoTime() / 1000)
                    putInt(0) // Sequence
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
                Log.e(TAG, "Sending packet failed: ${e.message}")
            }
        }.start()
    }

    override fun onDestroy() {
        super.onDestroy()
        isListening = false
        try {
            fileDescriptor?.close()
        } catch (e: IOException) {
            Log.e(TAG, "Tear down failed.")
        }
        Log.i(TAG, "USB receiver background service released.")
    }
}
