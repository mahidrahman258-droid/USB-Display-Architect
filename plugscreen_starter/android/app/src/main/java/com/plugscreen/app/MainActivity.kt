package com.plugscreen.app

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.hardware.usb.UsbAccessory
import android.hardware.usb.UsbManager
import android.os.Bundle
import android.os.IBinder
import android.util.Log
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowManager
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * @brief Principal user screen representing mobile client.
 * Registers SurfaceHolder layout callbacks, intercepts multi-touch bounds, and logs tracebacks.
 */
class MainActivity : AppCompatActivity(), SurfaceHolder.Callback, View.OnTouchListener {

    companion object {
        private const val TAG = "PlugScreenMainActivity"
    }

    private var usbService: UsbReceiverService? = null
    private var isBound = false

    private var surfaceView: SurfaceView? = null
    private var statusText: TextView? = null
    private var logsConsoleText: TextView? = null
    private var decoder: HardwareDecoder? = null

    /**
     * @brief Establishes service-connections to binder loops.
     */
    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            val binder = service as UsbReceiverService.LocalBinder
            usbService = binder.getService()
            isBound = true
            
            appendLogUi("Connected to USB Background Communication Service.")
            
            // Check if activity was launched by a USB Accessory attach event
            val intentAccessory = intent.getParcelableExtra<UsbAccessory>(UsbManager.EXTRA_ACCESSORY)
            if (intentAccessory != null) {
                appendLogUi("USB Auto-Launch intent confirmed model: ${intentAccessory.model}")
                initiateUsbConnection(intentAccessory)
            } else {
                appendLogUi("Manual launch detected. Searching for active accessory adapters...")
                discoverAttachedAccessory()
            }
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            usbService = null
            isBound = false
            appendLogUi("[Warn] Communication Service unexpectedly disconnected.")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        // Force full screen, eye-safe landscape and maintain screen-awake locks
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN)

        // Dyn-Builder layout configuration
        createDynamicLayout()

        // Bind Service
        val bindIntent = Intent(this, UsbReceiverService::class.java)
        bindService(bindIntent, serviceConnection, Context.BIND_AUTO_CREATE)
    }

    /**
     * @brief Generates responsive viewport and log panels programmatically.
     */
    private fun createDynamicLayout() {
        val rootLayout = android.widget.FrameLayout(this).apply {
            layoutParams = android.widget.FrameLayout.LayoutParams(
                android.widget.FrameLayout.LayoutParams.MATCH_PARENT,
                android.widget.FrameLayout.LayoutParams.MATCH_PARENT
            )
            setBackgroundColor(android.graphics.Color.parseColor("#090D16"))
        }

        // Live video feed Surface rendering container
        surfaceView = SurfaceView(this).apply {
            layoutParams = android.widget.FrameLayout.LayoutParams(
                android.widget.FrameLayout.LayoutParams.MATCH_PARENT,
                android.widget.FrameLayout.LayoutParams.MATCH_PARENT
            )
            holder.addCallback(this@MainActivity)
            setOnTouchListener(this@MainActivity)
        }
        rootLayout.addView(surfaceView)

        // Overlay diagnostic panels representing connections
        val panel = android.widget.LinearLayout(this).apply {
            orientation = android.widget.LinearLayout.VERTICAL
            layoutParams = android.widget.FrameLayout.LayoutParams(
                450, // Width bounds
                android.widget.FrameLayout.LayoutParams.MATCH_PARENT
            )
            setBackgroundColor(android.graphics.Color.parseColor("#E60B0F17"))
            setPadding(24, 24, 24, 24)
        }

        val title = TextView(this).apply {
            text = "PLUGSCREEN CLIENT"
            textSize = 14f
            setTextColor(android.graphics.Color.parseColor("#22D3EE")) // Cyan text
            typeface = android.graphics.Typeface.MONOSPACE
            setPadding(0, 0, 0, 16)
        }
        panel.addView(title)

        statusText = TextView(this).apply {
            text = "STATE: OFFLINE"
            textSize = 11f
            setTextColor(android.graphics.Color.WHITE)
            typeface = android.graphics.Typeface.MONOSPACE
            setPadding(0, 0, 0, 12)
        }
        panel.addView(statusText)

        logsConsoleText = TextView(this).apply {
            text = "[System Log Console]\nInitializing systems..."
            textSize = 9f
            setTextColor(android.graphics.Color.parseColor("#94A3B8"))
            typeface = android.graphics.Typeface.MONOSPACE
        }
        panel.addView(logsConsoleText)

        rootLayout.addView(panel)
        setContentView(rootLayout)
    }

    private fun discoverAttachedAccessory() {
        val usbManager = getSystemService(Context.USB_SERVICE) as UsbManager
        val accessoryList = usbManager.accessoryList
        if (accessoryList != null && accessoryList.isNotEmpty()) {
            appendLogUi("Located ${accessoryList.size} custom USB accessories. Linking...")
            initiateUsbConnection(accessoryList[0])
        } else {
            appendLogUi("No USB accessories found. Plug in USB cable to proceed.")
        }
    }

    private fun initiateUsbConnection(accessory: UsbAccessory) {
        usbService?.let { service ->
            appendLogUi("Establishing direct USB streams...")
            val success = service.startCommunication(accessory)
            if (success) {
                statusText?.text = "STATE: LINK_ESTABLISHED"
                statusText?.setTextColor(android.graphics.Color.parseColor("#34D399")) // Green text
                appendLogUi("Active USB transport ready. Ready to receive screen feeds.")
            } else {
                statusText?.text = "STATE: ESTABLISH_FAILED"
                statusText?.setTextColor(android.graphics.Color.parseColor("#EF4444")) // Red text
                appendLogUi("[Error] Could not initialize USB channels.")
            }
        }
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        appendLogUi("Surface canvas initialized. Booting GPU decoder contexts.")
        decoder = HardwareDecoder(holder.surface).apply {
            // Emulate standard 2K monitor sizes configurations
            configure(1920, 1080)
        }
        
        // Link decoder outputs to the streaming service
        usbService?.attachDecoder(decoder!!)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        appendLogUi("Viewport dimensions updated: ${width}x${height}")
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        appendLogUi("Surface destroyed. Releasing locks.")
        decoder?.release()
        decoder = null
    }

    /**
     * @brief Translates phone screen gestures to binary backflow touch coordinates matching 0-10000 scaler boundaries.
     */
    override fun onTouch(v: View?, event: MotionEvent?): Boolean {
        if (event == null || usbService == null) return false

        // Translate android Motion actions to protocol mappings
        val actionByte: Byte = when (event.action) {
            MotionEvent.ACTION_DOWN -> 0x00
            MotionEvent.ACTION_MOVE -> 0x01
            MotionEvent.ACTION_UP   -> 0x02
            else -> return false
        }

        // Scale coordinates to normalized ranges mapped in protocol
        val viewWidth = v?.width ?: 1
        val viewHeight = v?.height ?: 1
        
        val normalizedX = (event.x / viewWidth * 10000).toInt().coerceIn(0, 10000)
        val normalizedY = (event.y / viewHeight * 10000).toInt().coerceIn(0, 10000)

        // Package touch event payload: [action: 1, touchId: 2, scaleX: 2, scaleY: 2] -> 7 bytes payload
        val payload = ByteBuffer.allocate(7).apply {
            order(ByteOrder.LITTLE_ENDIAN)
            put(actionByte)
            putShort(event.getPointerId(0).toShort())
            putShort(normalizedX.toShort())
            putShort(normalizedY.toShort())
        }.array()

        // Stream coordinate payload back across USB bulk IN threads
        usbService?.sendDataPacket(0x05, payload) // Type 0x05 mapping InputEvent
        return true
    }

    private fun appendLogUi(msg: String) {
        runOnUiThread {
            Log.d(TAG, msg)
            val current = logsConsoleText?.text.toString()
            val lines = current.split("\n")
            // Keep container counts small
            val pruned = if (lines.size > 15) lines.drop(lines.size - 15).joinToString("\n") else current
            logsConsoleText?.text = "$pruned\n> $msg"
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        if (isBound) {
            unbindService(serviceConnection)
            isBound = false
        }
        decoder?.release()
    }
}
