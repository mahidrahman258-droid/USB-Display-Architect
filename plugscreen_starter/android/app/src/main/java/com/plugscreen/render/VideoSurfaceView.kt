package com.plugscreen.render

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Typeface
import android.util.AttributeSet
import android.util.Log
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView

/**
 * @brief High-performance video surface frame presentation and touch event translator view.
 * Maps local display layouts onto protocol coordinates (0-10000 ranges).
 */
class VideoSurfaceView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : SurfaceView(context, attrs, defStyleAttr), SurfaceHolder.Callback {

    interface SurfaceListener {
        fun onSurfaceCreated(holder: SurfaceHolder)
        fun onSurfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int)
        fun onSurfaceDestroyed(holder: SurfaceHolder)
        fun onTouchGesture(action: Byte, id: Short, normX: Short, normY: Short)
    }

    companion object {
        private const val TAG = "VideoSurfaceView"
    }

    private var listener: SurfaceListener? = null
    private val overlayPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#22D3EE") // High-contrast fluorescent cyan
        strokeWidth = 4f
        style = Paint.Style.STROKE
    }

    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textSize = 32f
        typeface = Typeface.MONOSPACE
    }

    init {
        holder.addCallback(this)
        setWillNotDraw(false) // Ensures onDraw() gets scheduled if we paint local diagnostics overlays
    }

    /**
     * @brief Hooks interaction delegates.
     */
    fun setListener(listener: SurfaceListener) {
        this.listener = listener
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.i(TAG, "Native Presentation Surface initialized.")
        listener?.onSurfaceCreated(holder)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.i(TAG, "Presentation dimensions updated to: ${width}x${height}")
        listener?.onSurfaceChanged(holder, format, width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.i(TAG, "Presentation Surface destroyed.")
        listener?.onSurfaceDestroyed(holder)
    }

    /**
     * @brief Translates phone screen gestures onto backflow protocol structures (normalized coordinates).
     */
    override fun onTouchEvent(event: MotionEvent?): Boolean {
        if (event == null || listener == null) return false

        // Resolve Motion indices
        val action: Byte = when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> 0x00 // Down
            MotionEvent.ACTION_MOVE -> 0x01                                  // Move
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> 0x02     // Up
            else -> return false
        }

        val pointerIndex = event.actionIndex
        val touchId = event.getPointerId(pointerIndex).toShort()

        // Normalize coordinates to target scale bounds [0, 10000]
        val viewWidth = width.coerceAtLeast(1)
        val viewHeight = height.coerceAtLeast(1)

        val rawX = event.getX(pointerIndex)
        val rawY = event.getY(pointerIndex)

        // Map and coerce coordinate points
        val normalizedX = ((rawX / viewWidth) * 10000).toInt().coerceIn(0, 10000).toShort()
        val normalizedY = ((rawY / viewHeight) * 10000).toInt().coerceIn(0, 10000).toShort()

        listener?.onTouchGesture(action, touchId, normalizedX, normalizedY)
        return true
    }

    /**
     * @brief Direct diagnostic views rendered on the device prior to incoming Host streaming frames.
     */
    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        
        // Draw elegant high-frequency frame lines to define boundaries
        val padding = 16f
        canvas.drawRect(
            padding, padding,
            width.toFloat() - padding,
            height.toFloat() - padding,
            overlayPaint
        )

        // Draw HUD diagnostics text overlay
        canvas.drawText("PLUGSCREEN // DISPLAY DISCOVERY ACTIVE", 40f, 80f, textPaint)
        canvas.drawText("PORT RECEPTACLE: USB AOA ACC_MODE", 40f, 130f, textPaint)
    }
}
