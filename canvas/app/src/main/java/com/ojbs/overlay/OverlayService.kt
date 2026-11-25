package com.ojbs.overlay

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.graphics.PixelFormat
import android.graphics.drawable.Icon
import android.os.Build
import android.os.IBinder
import android.provider.Settings
import android.util.Log
import android.view.Gravity
import android.view.WindowManager

class OverlayService : Service() {

    private val store = FrameStore()

    private var windowManager: WindowManager? = null
    private var view: OverlayView? = null
    private var socketServer: SocketServer? = null
    private var demoFeeder: DemoFeeder? = null

    @Volatile
    private var clientConnected = false

    @Volatile
    private var demoMode = false

    @Volatile
    private var socketBound = false

    override fun onBind(intent: Intent?): IBinder? {
        return null
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val action = intent?.action
        if (action == ACTION_STOP) {
            teardown()
            stopSelf()
            return START_NOT_STICKY
        }

        demoMode = intent?.getBooleanExtra(EXTRA_DEMO, false) ?: false

        if (!Settings.canDrawOverlays(this)) {
            Log.e(TAG, "permesso overlay mancante, mi fermo")
            teardown()
            stopSelf()
            return START_NOT_STICKY
        }

        startForegroundNotification()

        if (view == null) {
            if (!attachOverlay()) {
                teardown()
                stopSelf()
                return START_NOT_STICKY
            }
        }

        startSource()
        return START_STICKY
    }

    override fun onDestroy() {
        teardown()
        super.onDestroy()
    }

    private fun attachOverlay(): Boolean {
        val manager = getSystemService(Context.WINDOW_SERVICE) as WindowManager
        val overlay = OverlayView(this, store, { statusLine() })

        val params = WindowManager.LayoutParams(
            WindowManager.LayoutParams.MATCH_PARENT,
            WindowManager.LayoutParams.MATCH_PARENT,
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE or
                WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN or
                WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS or
                WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED,
            PixelFormat.TRANSLUCENT
        )
        params.gravity = Gravity.TOP or Gravity.START
        params.x = 0
        params.y = 0

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            params.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        }

        return try {
            manager.addView(overlay, params)
            windowManager = manager
            view = overlay
            true
        } catch (failure: WindowManager.BadTokenException) {
            Log.e(TAG, "addView rifiutato", failure)
            false
        } catch (failure: IllegalStateException) {
            Log.e(TAG, "addView fallito", failure)
            false
        }
    }

    private fun startSource() {
        stopSource()

        if (demoMode) {
            val feeder = DemoFeeder(store, { surfaceSize() })
            feeder.start()
            demoFeeder = feeder
            socketBound = false
            return
        }

        val server = SocketServer(
            store,
            { surfaceSize() },
            { connected -> clientConnected = connected }
        )
        socketBound = server.start()
        if (socketBound) {
            socketServer = server
        } else {
            Log.e(TAG, "socket non avviato, l'overlay resta vuoto")
        }
    }

    private fun stopSource() {
        val feeder = demoFeeder
        demoFeeder = null
        feeder?.stop()

        val server = socketServer
        socketServer = null
        server?.stop()

        clientConnected = false
        socketBound = false
    }

    // Percorso di pulizia unico: init fallita e shutdown normale passano di qui.
    private fun teardown() {
        stopSource()

        val overlay = view
        val manager = windowManager
        view = null
        windowManager = null

        if (overlay != null && manager != null) {
            try {
                manager.removeView(overlay)
            } catch (ignored: IllegalArgumentException) {
                Log.v(TAG, "view gia' rimossa", ignored)
            }
        }

        store.clear()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            stopForeground(Service.STOP_FOREGROUND_REMOVE)
        } else {
            @Suppress("DEPRECATION")
            stopForeground(true)
        }
    }

    private fun surfaceSize(): Pair<Int, Int> {
        val overlay = view
        if (overlay != null && overlay.width > 0 && overlay.height > 0) {
            return Pair(overlay.width, overlay.height)
        }
        val metrics = resources.displayMetrics
        return Pair(metrics.widthPixels, metrics.heightPixels)
    }

    private fun statusLine(): String {
        if (demoMode) {
            return "demo"
        }
        if (!socketBound) {
            return "socket ko"
        }
        if (clientConnected) {
            return "@" + Protocol.SOCKET_NAME
        }
        return "attesa daemon"
    }

    private fun startForegroundNotification() {
        val manager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val existing = manager.getNotificationChannel(CHANNEL_ID)
            if (existing == null) {
                val channel = NotificationChannel(
                    CHANNEL_ID,
                    getString(R.string.channel_name),
                    NotificationManager.IMPORTANCE_LOW
                )
                channel.setShowBadge(false)
                manager.createNotificationChannel(channel)
            }
        }

        val stopIntent = Intent(this, OverlayService::class.java)
        stopIntent.action = ACTION_STOP

        val pendingFlags = PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        val stopPending = PendingIntent.getService(this, 1, stopIntent, pendingFlags)

        val builder = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Notification.Builder(this, CHANNEL_ID)
        } else {
            @Suppress("DEPRECATION")
            Notification.Builder(this)
        }

        val notification = builder
            .setContentTitle(getString(R.string.notification_title))
            .setContentText(getString(R.string.notification_text))
            .setSmallIcon(android.R.drawable.ic_menu_view)
            .setOngoing(true)
            .addAction(
                Notification.Action.Builder(
                    null as Icon?,
                    getString(R.string.action_stop),
                    stopPending
                ).build()
            )
            .build()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            startForeground(
                NOTIFICATION_ID,
                notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE
            )
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    companion object {
        const val ACTION_START = "com.ojbs.overlay.START"
        const val ACTION_STOP = "com.ojbs.overlay.STOP"
        const val EXTRA_DEMO = "demo"

        private const val TAG = "ojbs-service"
        private const val CHANNEL_ID = "ojbs_overlay"
        private const val NOTIFICATION_ID = 0x0E59

        fun start(context: Context, demo: Boolean) {
            val intent = Intent(context, OverlayService::class.java)
            intent.action = ACTION_START
            intent.putExtra(EXTRA_DEMO, demo)
            context.startForegroundService(intent)
        }

        fun stop(context: Context) {
            val intent = Intent(context, OverlayService::class.java)
            intent.action = ACTION_STOP
            context.startService(intent)
        }
    }
}
