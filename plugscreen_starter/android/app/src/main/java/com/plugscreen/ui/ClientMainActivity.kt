package com.plugscreen.ui

import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.hardware.usb.UsbAccessory
import android.hardware.usb.UsbManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.SurfaceHolder
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.plugscreen.decoder.VideoDecoder
import com.plugscreen.render.VideoSurfaceView
import com.plugscreen.usb.AoaTransport

/**
 * @brief Main UI dashboard and pipeline coordinator of the Android second screen system.
 * Handles auto-launch intents, registers platform callbacks, and logs diagnostics.
 */
class ClientMainActivity : AppCompatActivity(), VideoSurfaceView.SurfaceListener, AoaTransport.TransportCallback {

    companion object {
        private const val TAG = "ClientMainActivity"
    }

    private var usbManager: UsbManager? = null
    private var aoaTransport: AoaTransport? = null
    private var videoDecoder: VideoDecoder? = null

    // Layout elements
    private var surfaceView: VideoSurfaceView? = null
    private var statusValueText: TextView? = null
    private var diagnosticsLogText: TextView? = null
    private var streamMetaText: TextView? = null

    private val mainHandler = Handler(Looper.getMainLooper())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Lock landscape orientations and keep the physical display panel alive
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN)

        usbManager = getSystemService(Context.USB_SERVICE) as UsbManager
        aoaTransport = AoaTransport(usbManager!!, this)

        buildTerminalHudLayout()

        // Handle cold boot triggered by USB hot-plug attachments
        handleIntent(intent)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleIntent(intent)
    }

    private fun handleIntent(intent: Intent) {
        val accessory = intent.getParcelableExtra<UsbAccessory>(UsbManager.EXTRA_ACCESSORY)
        if (accessory != null) {
            appendLog("Auto-launched via USB hot-plug accessory trigger!")
            connectToAccessory(accessory)
        } else {
            appendLog("Manual launch. Scanning connected accessories...")
            scanAndConnect()
        }
    }

    /**
     * @brief Programmatically constructs a high-performance sci-fi tactical monitor grid
     * to eliminate Gradle file-loading dependency failures during builds.
     */
    private fun buildTerminalHudLayout() {
        val root = FrameLayout(this).apply {
            layoutParams = ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)
            setBackgroundColor(Color.parseColor("#060814"))
        }

        // 1. Core Hardware presentation renderer view
        surfaceView = VideoSurfaceView(this).apply {
            layoutParams = FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT)
            setListener(this@ClientMainActivity)
        }
        root.addView(surfaceView)

        // 2. HUD diagnostic panel overlay on the left
        val hudPanel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = FrameLayout.LayoutParams(480, FrameLayout.LayoutParams.MATCH_PARENT)
            setBackgroundColor(Color.parseColor("#B30A0E1A")) // Elegant semi-transparent deep slate
            setPadding(32, 32, 32, 32)
        }

        val headerText = TextView(this).apply {
            text = "PLUGSCREEN RECEIVER"
            textSize = 14f
            setTextColor(Color.parseColor("#22D3EE")) // Cyberpunk cyan accent
            typeface = android.graphics.Typeface.MONOSPACE
            setPadding(0, 0, 0, 16)
        }
        hudPanel.addView(headerText)

        // Status field
        val statusLabel = TextView(this).apply {
            text = "SYSTEM STATE:"
            textSize = 10f
            setTextColor(Color.parseColor("#64748B"))
            typeface = android.graphics.Typeface.MONOSPACE
        }
        hudPanel.addView(statusLabel)

        statusValueText = TextView(this).apply {
            text = "DISCONNECTED"
            textSize = 12f
            setTextColor(Color.parseColor("#EF4444")) // Alert red
            typeface = android.graphics.Typeface.MONOSPACE
            setPadding(0, 0, 0, 16)
        }
        hudPanel.addView(statusValueText)

        // Stream Metrics Info
        val streamLabel = TextView(this).apply {
            text = "STREAM METRICS:"
            textSize = 10f
            setTextColor(Color.parseColor("#64748B"))
            typeface = android.graphics.Typeface.MONOSPACE
        }
        hudPanel.addView(streamLabel)

        streamMetaText = TextView(this).apply {
            text = "WAITING FOR RESOLUTION..."
            textSize = 10f
            setTextColor(Color.parseColor("#94A3B8"))
            typeface = android.graphics.Typeface.MONOSPACE
            setPadding(0, 0, 0, 24)
        }
        hudPanel.addView(streamMetaText)

        // Live Log Window
        val logLabel = TextView(this).apply {
            text = "DIAGNOSTIC LOGS:"
            textSize = 10f
            setTextColor(Color.parseColor("#64748B"))
            typeface = android.graphics.Typeface.MONOSPACE
            setPadding(0, 0, 0, 8)
        }
        hudPanel.addView(logLabel)

        diagnosticsLogText = TextView(this).apply {
            text = "System Online.\n"
            textSize = 8f
            setTextColor(Color.parseColor("#94A3B8"))
            typeface = android.graphics.Typeface.MONOSPACE
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 
                LinearLayout.LayoutParams.WRAP_CONTENT, 
                1.0f
            )
        }
        hudPanel.addView(diagnosticsLogText)

        root.addView(hudPanel)
        setContentView(root)
    }

    private fun scanAndConnect() {
        val accessoryList = usbManager?.accessoryList
        if (!accessoryList.isNullOrEmpty()) {
            appendLog("Discovered accessory device. Triggering AOA protocol hook...")
            connectToAccessory(accessoryList[0])
        } else {
            appendLog("Awaiting physical USB PlugScreen Accessory hook...")
        }
    }

    private fun connectToAccessory(accessory: UsbAccessory) {
        val success = aoaTransport?.connect(accessory) ?: false
        if (success) {
            updateStatusText("LINKING", "#F59E0B") // Amber
            appendLog("AOA interface pipes bound. Exchanging system handshakes.")
        } else {
            updateStatusText("FAILED", "#EF4444") // Red
            appendLog("Failed binding USB interface endpoints.")
        }
    }

    // --- SurfaceListener callbacks ---

    override fun onSurfaceCreated(holder: SurfaceHolder) {
        appendLog("Graphics Canvas ready. Activating CPU MediaCodec decode maps.")
        videoDecoder = VideoDecoder(holder.surface).apply {
            // Preset size mappings matching standard HD. Resolves automatically on handshake.
            configure(1920, 1080)
        }
    }

    override fun onSurfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        appendLog("Physical Viewport mapped: ${width}x${height}")
    }

    override fun onSurfaceDestroyed(holder: SurfaceHolder) {
        appendLog("Graphics surface torn down. Halting decoder contexts.")
        videoDecoder?.release()
        videoDecoder = null
    }

    override fun onTouchGesture(action: Byte, id: Short, normX: Short, normY: Short) {
        // Broadcast user interaction back to Windows host machine
        aoaTransport?.sendInputGesture(action, id, normX, normY)
    }

    // --- TransportCallback events ---

    override fun onVideoFrameReceived(data: ByteArray, timestampUs: Long, isKeyframe: Boolean) {
        // Stream frames directly to underlying hardware decoder context
        videoDecoder?.feedCompressedBuffer(data, timestampUs, isKeyframe)
    }

    override fun onTransportError(error: String) {
        appendLog("[TRANSPORT_ERR] $error")
        updateStatusText("ERROR", "#EF4444")
    }

    override fun onTransportStatusChanged(connected: Boolean, details: String) {
        if (connected) {
            updateStatusText("STREAMING", "#10B981") // Green
            appendLog("Active transport channel online. Feed ready.")
        } else {
            updateStatusText("OFFLINE", "#EF4444") // Red
            appendLog("Transport lost: $details")
            videoDecoder?.release()
        }
    }

    override fun onHandshakeCompleted(width: Int, height: Int, fps: Int, bitrate: Long, name: String) {
        appendLog("Handshake completed with Host: $name")
        appendLog("Active layout: ${width}x${height} @${fps}Hz @${(bitrate / 1000000)}Mbps")
        
        mainHandler.post {
            streamMetaText?.text = "${width}x${height} @${fps}Hz\nBitrate: ${(bitrate / 1000000)} Mbps"
            // Dynamically resize internal hardware decoder scaling configuration
            videoDecoder?.configure(width, height)
        }
    }

    // --- Helper methods ---

    private fun updateStatusText(status: String, hexColor: String) {
        mainHandler.post {
            statusValueText?.text = status
            statusValueText?.setTextColor(Color.parseColor(hexColor))
        }
    }

    private fun appendLog(line: String) {
        Log.d(TAG, line)
        mainHandler.post {
            val text = diagnosticsLogText?.text.toString()
            val lines = text.split("\n")
            val truncatedLines = if (lines.size > 18) lines.drop(lines.size - 18).joinToString("\n") else text
            diagnosticsLogText?.text = "$truncatedLines\n> $line"
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        aoaTransport?.close()
        videoDecoder?.release()
    }
}
