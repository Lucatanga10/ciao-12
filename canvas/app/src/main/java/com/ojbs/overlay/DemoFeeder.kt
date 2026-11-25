package com.ojbs.overlay

import android.graphics.Color

// Genera box sintetici senza toccare la memoria di nessuno. Serve a provare che
// la finestra overlay si disegna sopra il gioco prima che esista il reader.
class DemoFeeder(
    private val store: FrameStore,
    private val sizeProvider: () -> Pair<Int, Int>
) {

    @Volatile
    private var running = false

    private var thread: Thread? = null

    fun start() {
        if (running) {
            return
        }
        running = true
        val worker = Thread({ loop() }, "ojbs-demo")
        worker.isDaemon = true
        thread = worker
        worker.start()
    }

    fun stop() {
        running = false
        val worker = thread
        thread = null
        if (worker != null) {
            worker.interrupt()
            worker.join(500L)
        }
        store.clear()
    }

    private fun loop() {
        var tick = 0.0
        while (running) {
            val size = sizeProvider()
            val screenWidth = size.first
            val screenHeight = size.second

            if (screenWidth > 0 && screenHeight > 0) {
                store.publish(build(screenWidth, screenHeight, tick))
            }

            tick += 0.03
            try {
                Thread.sleep(16L)
            } catch (stopped: InterruptedException) {
                Thread.currentThread().interrupt()
                return
            }
        }
    }

    private fun build(screenWidth: Int, screenHeight: Int, tick: Double): List<Box> {
        val boxes = ArrayList<Box>(DEMO_COUNT)
        val baseHeight = screenHeight * 0.26f

        for (i in 0 until DEMO_COUNT) {
            val phase = tick + i * 1.7
            val swing = ((Math.sin(phase) + 1.0) / 2.0).toFloat()
            val bob = Math.sin(phase * 1.9).toFloat()
            val depth = ((Math.cos(phase * 0.8) + 1.0) / 2.0).toFloat()

            val centerX = screenWidth * (0.18f + 0.64f * swing)
            val centerY = screenHeight * (0.45f + 0.08f * bob)

            val boxHeight = baseHeight * (0.7f + 0.3f * depth)
            val boxWidth = boxHeight * 0.5f

            val health = ((Math.sin(phase * 0.6) + 1.0) / 2.0).toFloat()
            val color = if (i % 2 == 0) {
                Color.rgb(255, 72, 72)
            } else {
                Color.rgb(120, 170, 255)
            }

            boxes.add(
                Box(
                    left = centerX - boxWidth * 0.5f,
                    top = centerY - boxHeight * 0.5f,
                    right = centerX + boxWidth * 0.5f,
                    bottom = centerY + boxHeight * 0.5f,
                    argb = color,
                    health = health,
                    name = "demo_$i"
                )
            )
        }

        return boxes
    }

    private companion object {
        const val DEMO_COUNT = 5
    }
}
