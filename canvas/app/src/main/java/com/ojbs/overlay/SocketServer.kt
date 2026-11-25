package com.ojbs.overlay

import android.net.LocalServerSocket
import android.net.LocalSocket
import android.util.Log
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

// Server su abstract unix socket (@ojbs_esp). Un solo client alla volta: il
// daemon. Ogni handle viene chiuso su ogni percorso di uscita, errore compreso.
class SocketServer(
    private val store: FrameStore,
    private val sizeProvider: () -> Pair<Int, Int>,
    private val onClientState: (Boolean) -> Unit
) {

    @Volatile
    private var running = false

    private var server: LocalServerSocket? = null

    @Volatile
    private var client: LocalSocket? = null

    private var thread: Thread? = null

    fun start(): Boolean {
        if (running) {
            return true
        }

        val socket = try {
            LocalServerSocket(Protocol.SOCKET_NAME)
        } catch (failure: IOException) {
            Log.e(TAG, "bind @" + Protocol.SOCKET_NAME + " fallito", failure)
            return false
        }

        server = socket
        running = true

        val worker = Thread({ acceptLoop(socket) }, "ojbs-socket")
        worker.isDaemon = true
        thread = worker
        worker.start()
        return true
    }

    fun stop() {
        running = false

        val activeClient = client
        client = null
        if (activeClient != null) {
            closeQuietly { activeClient.close() }
        }

        val activeServer = server
        server = null
        if (activeServer != null) {
            closeQuietly { activeServer.close() }
        }

        val worker = thread
        thread = null
        if (worker != null) {
            worker.join(1000L)
        }

        store.clear()
        onClientState(false)
    }

    private fun acceptLoop(socket: LocalServerSocket) {
        while (running) {
            val accepted = try {
                socket.accept()
            } catch (closed: IOException) {
                if (running) {
                    Log.w(TAG, "accept interrotto", closed)
                }
                return
            }

            client = accepted
            onClientState(true)

            try {
                serve(accepted)
            } catch (failure: IOException) {
                Log.w(TAG, "connessione chiusa", failure)
            } finally {
                closeQuietly { accepted.close() }
                client = null
                store.clear()
                onClientState(false)
            }
        }
    }

    private fun serve(socket: LocalSocket) {
        val input = socket.inputStream
        val output = socket.outputStream

        writeHello(output)

        val header = ByteArray(Protocol.HEADER_SIZE)
        val payload = ByteArray(Protocol.MAX_PAYLOAD)

        while (running) {
            if (!readFully(input, header, Protocol.HEADER_SIZE)) {
                return
            }

            val head = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN)
            val magic = head.int
            val payloadLen = head.int

            if (magic != Protocol.MAGIC) {
                Log.w(TAG, "magic errato: 0x" + Integer.toHexString(magic))
                return
            }
            if (payloadLen < 2 || payloadLen > Protocol.MAX_PAYLOAD) {
                Log.w(TAG, "payloadLen fuori range: $payloadLen")
                return
            }

            if (!readFully(input, payload, payloadLen)) {
                return
            }

            val boxes = FrameDecoder.decodePayload(payload, payloadLen)
            if (boxes == null) {
                Log.w(TAG, "payload malformato, chiudo")
                return
            }

            store.publish(boxes)
        }
    }

    private fun writeHello(output: OutputStream) {
        val size = sizeProvider()
        val hello = ByteBuffer.allocate(Protocol.HELLO_SIZE).order(ByteOrder.LITTLE_ENDIAN)
        hello.putInt(size.first)
        hello.putInt(size.second)
        output.write(hello.array())
        output.flush()
    }

    // Una lettura parziale e' un fallimento, non uno zero.
    private fun readFully(input: InputStream, destination: ByteArray, length: Int): Boolean {
        var read = 0
        while (read < length) {
            val chunk = input.read(destination, read, length - read)
            if (chunk <= 0) {
                return false
            }
            read += chunk
        }
        return true
    }

    private fun closeQuietly(action: () -> Unit) {
        try {
            action()
        } catch (ignored: IOException) {
            Log.v(TAG, "close fallita", ignored)
        }
    }

    private companion object {
        const val TAG = "ojbs-socket"
    }
}
