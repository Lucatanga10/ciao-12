package com.ojbs.overlay

import java.nio.ByteBuffer
import java.nio.ByteOrder

// Funzione pura: nessuna dipendenza da Android, verificabile senza il gioco.
// Un payload malformato torna null e il chiamante chiude la connessione; un
// frame parziale non viene mai pubblicato a meta'.
object FrameDecoder {

    fun decodePayload(data: ByteArray, length: Int): List<Box>? {
        if (length < 2 || length > data.size) {
            return null
        }

        val buf = ByteBuffer.wrap(data, 0, length).order(ByteOrder.LITTLE_ENDIAN)
        val count = buf.short.toInt() and 0xFFFF
        if (count > Protocol.MAX_BOXES) {
            return null
        }

        val out = ArrayList<Box>(count)
        for (i in 0 until count) {
            if (buf.remaining() < Protocol.BOX_FIXED_SIZE) {
                return null
            }

            val left = buf.float
            val top = buf.float
            val right = buf.float
            val bottom = buf.float
            val argb = buf.int
            val health = buf.float
            val nameLen = buf.get().toInt() and 0xFF

            if (nameLen > Protocol.MAX_NAME_BYTES || buf.remaining() < nameLen) {
                return null
            }

            val nameBytes = ByteArray(nameLen)
            buf.get(nameBytes)

            if (!finite(left) || !finite(top) || !finite(right) || !finite(bottom)) {
                return null
            }
            if (right <= left || bottom <= top) {
                continue
            }

            val usableHealth = if (finite(health)) health else -1.0f

            out.add(
                Box(
                    left = left,
                    top = top,
                    right = right,
                    bottom = bottom,
                    argb = argb,
                    health = usableHealth,
                    name = String(nameBytes, Charsets.UTF_8)
                )
            )
        }

        return out
    }

    private fun finite(value: Float): Boolean {
        return !value.isNaN() && !value.isInfinite()
    }
}
