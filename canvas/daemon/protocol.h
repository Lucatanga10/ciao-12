#ifndef OJBS_PROTOCOL_H
#define OJBS_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Deve restare identico a Protocol.kt dell'APK. Tutto little-endian, scritto
 * byte per byte: non ci si appoggia all'endianness dell'host.
 *
 * hello (APK -> daemon, una volta alla connessione):
 *   u32 width
 *   u32 height
 *
 * frame (daemon -> APK, ripetuto):
 *   u32 magic
 *   u32 payload_len
 *   payload:
 *     u16 count
 *     count * record:
 *       f32 left, top, right, bottom
 *       i32 argb
 *       f32 health        (negativo = nessuna barra)
 *       u8  name_len
 *       u8[name_len] name UTF-8
 */

#define OJBS_SOCKET_NAME    "ojbs_esp"
#define OJBS_MAGIC          0x31505345u   /* "ESP1" little-endian */
#define OJBS_HEADER_SIZE    8
#define OJBS_HELLO_SIZE     8
#define OJBS_MAX_BOXES      512
#define OJBS_MAX_NAME_BYTES 32
#define OJBS_BOX_FIXED_SIZE 25
#define OJBS_MAX_PAYLOAD    (2 + OJBS_MAX_BOXES * (OJBS_BOX_FIXED_SIZE + OJBS_MAX_NAME_BYTES))
#define OJBS_MAX_FRAME      (OJBS_HEADER_SIZE + OJBS_MAX_PAYLOAD)

typedef struct {
    float left;
    float top;
    float right;
    float bottom;
    int32_t argb;
    float health;
    char name[OJBS_MAX_NAME_BYTES + 1];
} esp_box;

/*
 * Serializza count box in out. Torna il numero di byte scritti, 0 se la
 * capacita' non basta, se count eccede OJBS_MAX_BOXES o se un box contiene
 * un float non finito.
 */
size_t proto_encode_frame(const esp_box *boxes, uint16_t count,
                          uint8_t *out, size_t capacity);

/* Decodifica l'hello di 8 byte. false se la lunghezza non e' esatta. */
bool proto_decode_hello(const uint8_t *data, size_t length,
                        uint32_t *width, uint32_t *height);

#endif /* OJBS_PROTOCOL_H */
