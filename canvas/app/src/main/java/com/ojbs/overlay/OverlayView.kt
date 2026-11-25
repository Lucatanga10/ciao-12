package com.ojbs.overlay

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.util.TypedValue
import android.view.Choreographer
import android.view.View

// Coordinate in ingresso: pixel fisici della superficie overlay, origine in alto
// a sinistra. Le stesse che il server annuncia nell'hello. Nessuna scalatura qui.
class OverlayView(
    context: Context,
    private val store: FrameStore,
    private val statusProvider: () -> String
) : View(context) {

    private val outlinePaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val boxPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val barBackPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val barPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val textOutlinePaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val hudPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val hudOutlinePaint = Paint(Paint.ANTI_ALIAS_FLAG)

    private val barWidth: Float
    private val barGap: Float
    private val nameGap: Float
    private val hairline: Float

    private var frameCount = 0
    private var lastFpsNanos = 0L
    private var fps = 0

    private val callback = object : Choreographer.FrameCallback {
        override fun doFrame(frameTimeNanos: Long) {
            if (!isAttachedToWindow) {
                return
            }
            invalidate()
            Choreographer.getInstance().postFrameCallback(this)
        }
    }

    init {
        setWillNotDraw(false)
        setBackgroundColor(Color.TRANSPARENT)

        hairline = dp(1.0f)
        barWidth = dp(2.5f)
        barGap = dp(3.0f)
        nameGap = dp(4.0f)

        outlinePaint.style = Paint.Style.STROKE
        outlinePaint.color = Color.argb(190, 0, 0, 0)
        outlinePaint.strokeWidth = dp(3.0f)

        boxPaint.style = Paint.Style.STROKE
        boxPaint.strokeWidth = dp(1.4f)

        barBackPaint.style = Paint.Style.FILL
        barBackPaint.color = Color.argb(190, 0, 0, 0)

        barPaint.style = Paint.Style.FILL

        textPaint.color = Color.WHITE
        textPaint.textSize = sp(10.0f)
        textPaint.textAlign = Paint.Align.CENTER

        textOutlinePaint.color = Color.argb(210, 0, 0, 0)
        textOutlinePaint.textSize = sp(10.0f)
        textOutlinePaint.textAlign = Paint.Align.CENTER
        textOutlinePaint.style = Paint.Style.STROKE
        textOutlinePaint.strokeWidth = dp(1.6f)

        hudPaint.color = Color.argb(210, 170, 255, 170)
        hudPaint.textSize = sp(10.0f)
        hudPaint.textAlign = Paint.Align.LEFT

        hudOutlinePaint.color = Color.argb(210, 0, 0, 0)
        hudOutlinePaint.textSize = sp(10.0f)
        hudOutlinePaint.textAlign = Paint.Align.LEFT
        hudOutlinePaint.style = Paint.Style.STROKE
        hudOutlinePaint.strokeWidth = dp(1.6f)
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        lastFpsNanos = System.nanoTime()
        frameCount = 0
        Choreographer.getInstance().postFrameCallback(callback)
    }

    override fun onDetachedFromWindow() {
        Choreographer.getInstance().removeFrameCallback(callback)
        super.onDetachedFromWindow()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        tickFps()

        val frame = store.snapshot()
        val age = System.nanoTime() - frame.nanos
        val fresh = frame.nanos != 0L && age < Protocol.STALE_NANOS

        var drawn = 0
        if (fresh) {
            for (box in frame.boxes) {
                if (offscreen(box)) {
                    continue
                }
                drawBox(canvas, box)
                drawn += 1
            }
        }

        val hud = statusProvider() + " | box " + drawn + " | " + fps + " fps"
        val hudX = dp(8.0f)
        val hudY = dp(24.0f)
        canvas.drawText(hud, hudX, hudY, hudOutlinePaint)
        canvas.drawText(hud, hudX, hudY, hudPaint)
    }

    private fun tickFps() {
        frameCount += 1
        val now = System.nanoTime()
        val elapsed = now - lastFpsNanos
        if (elapsed >= 1_000_000_000L) {
            fps = ((frameCount * 1_000_000_000L) / elapsed).toInt()
            frameCount = 0
            lastFpsNanos = now
        }
    }

    private fun offscreen(box: Box): Boolean {
        if (box.right < 0.0f || box.bottom < 0.0f) {
            return true
        }
        if (box.left > width.toFloat() || box.top > height.toFloat()) {
            return true
        }
        return false
    }

    private fun drawBox(canvas: Canvas, box: Box) {
        canvas.drawRect(box.left, box.top, box.right, box.bottom, outlinePaint)

        boxPaint.color = box.argb
        canvas.drawRect(box.left, box.top, box.right, box.bottom, boxPaint)

        if (box.health >= 0.0f) {
            drawHealth(canvas, box)
        }

        if (box.name.isNotEmpty()) {
            val centerX = (box.left + box.right) * 0.5f
            val baseline = box.top - nameGap
            canvas.drawText(box.name, centerX, baseline, textOutlinePaint)
            canvas.drawText(box.name, centerX, baseline, textPaint)
        }
    }

    private fun drawHealth(canvas: Canvas, box: Box) {
        val clamped = when {
            box.health > 1.0f -> 1.0f
            box.health < 0.0f -> 0.0f
            else -> box.health
        }

        val right = box.left - barGap
        val left = right - barWidth

        canvas.drawRect(
            left - hairline,
            box.top - hairline,
            right + hairline,
            box.bottom + hairline,
            barBackPaint
        )

        val filledTop = box.bottom - box.height * clamped
        barPaint.color = healthColor(clamped)
        canvas.drawRect(left, filledTop, right, box.bottom, barPaint)
    }

    private fun healthColor(fraction: Float): Int {
        val red = ((1.0f - fraction) * 255.0f).toInt()
        val green = (fraction * 230.0f).toInt()
        return Color.rgb(
            red.coerceIn(0, 255),
            green.coerceIn(0, 255),
            40
        )
    }

    private fun dp(value: Float): Float {
        return TypedValue.applyDimension(
            TypedValue.COMPLEX_UNIT_DIP,
            value,
            resources.displayMetrics
        )
    }

    private fun sp(value: Float): Float {
        return TypedValue.applyDimension(
            TypedValue.COMPLEX_UNIT_SP,
            value,
            resources.displayMetrics
        )
    }
}
