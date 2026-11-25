package com.ojbs.overlay

// Protocollo sul socket, little-endian, lo stesso che dovra' emettere il daemon.
//
// hello (server -> client, una volta alla connessione):
//   u32 width
//   u32 height
//
// frame (client -> server, ripetuto a ogni tick del reader):
//   u32 magic        = Protocol.MAGIC
//   u32 payloadLen   = numero di byte del payload che seguono
//   payload:
//     u16 count
//     count * record:
//       f32 left
//       f32 top
//       f32 right
//       f32 bottom
//       i32 argb
//       f32 health      (0.0 .. 1.0; negativo = nessuna barra)
//       u8  nameLen
//       u8[nameLen] name UTF-8
object Protocol {

    const val SOCKET_NAME = "ojbs_esp"

    // "ESP1" letto come u32 little-endian.
    const val MAGIC: Int = 0x31505345

    const val HEADER_SIZE = 8
    const val HELLO_SIZE = 8

    const val MAX_BOXES = 512
    const val MAX_NAME_BYTES = 32

    // 4 float + i32 + f32 + u8
    const val BOX_FIXED_SIZE = 25

    const val MAX_PAYLOAD = 2 + MAX_BOXES * (BOX_FIXED_SIZE + MAX_NAME_BYTES)

    // Oltre questa eta' il frame e' considerato stantio e non viene disegnato.
    const val STALE_NANOS = 500_000_000L
}
