package com.ojbs.overlay

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat

class MainActivity : AppCompatActivity() {

    private lateinit var statusText: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        statusText = findViewById(R.id.status_text)

        findViewById<Button>(R.id.button_permission).setOnClickListener {
            requestOverlayPermission()
        }

        findViewById<Button>(R.id.button_notifications).setOnClickListener {
            requestNotificationPermission()
        }

        findViewById<Button>(R.id.button_start_socket).setOnClickListener {
            if (ensureOverlayPermission()) {
                OverlayService.start(this, false)
                refreshStatus()
            }
        }

        findViewById<Button>(R.id.button_start_demo).setOnClickListener {
            if (ensureOverlayPermission()) {
                OverlayService.start(this, true)
                refreshStatus()
            }
        }

        findViewById<Button>(R.id.button_stop).setOnClickListener {
            OverlayService.stop(this)
            refreshStatus()
        }
    }

    override fun onResume() {
        super.onResume()
        refreshStatus()
    }

    private fun ensureOverlayPermission(): Boolean {
        if (Settings.canDrawOverlays(this)) {
            return true
        }
        requestOverlayPermission()
        return false
    }

    private fun requestOverlayPermission() {
        if (Settings.canDrawOverlays(this)) {
            refreshStatus()
            return
        }
        val intent = Intent(
            Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
            Uri.parse("package:$packageName")
        )
        startActivity(intent)
    }

    private fun requestNotificationPermission() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            refreshStatus()
            return
        }
        if (hasNotificationPermission()) {
            refreshStatus()
            return
        }
        requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS), REQUEST_NOTIFICATIONS)
    }

    private fun hasNotificationPermission(): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            return true
        }
        val granted = ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.POST_NOTIFICATIONS
        )
        return granted == PackageManager.PERMISSION_GRANTED
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        refreshStatus()
    }

    private fun refreshStatus() {
        val overlay = if (Settings.canDrawOverlays(this)) "ok" else "mancante"
        val notifications = if (hasNotificationPermission()) "ok" else "mancante"
        statusText.text = getString(
            R.string.status_format,
            overlay,
            notifications,
            Protocol.SOCKET_NAME
        )
    }

    private companion object {
        const val REQUEST_NOTIFICATIONS = 101
    }
}
