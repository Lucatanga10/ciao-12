package com.ojbs.overlay

import java.util.concurrent.atomic.AtomicReference

// Il thread del socket pubblica, il thread UI legge. Nessun lock sul percorso
// di disegno: si scambia solo il riferimento allo snapshot.
class FrameStore {

    data class Frame(val boxes: List<Box>, val nanos: Long)

    private val current = AtomicReference(Frame(emptyList(), 0L))

    fun publish(boxes: List<Box>) {
        current.set(Frame(boxes, System.nanoTime()))
    }

    fun snapshot(): Frame {
        return current.get()
    }

    fun clear() {
        current.set(Frame(emptyList(), 0L))
    }
}
