#include "protocol.h"

#include <math.h>
#include <string.h>

static void put_u16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void put_u32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
    out[2] = (uint8_t)((value >> 16) & 0xFFu);
    out[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static void put_i32(uint8_t *out, int32_t value)
{
    uint32_t raw;
    memcpy(&raw, &value, sizeof(raw));
    put_u32(out, raw);
}

static void put_f32(uint8_t *out, float value)
{
    uint32_t raw;
    memcpy(&raw, &value, sizeof(raw));
    put_u32(out, raw);
}

static uint32_t get_u32(const uint8_t *in)
{
    return (uint32_t)in[0]
         | ((uint32_t)in[1] << 8)
         | ((uint32_t)in[2] << 16)
         | ((uint32_t)in[3] << 24);
}

static bool box_finite(const esp_box *box)
{
    if (!isfinite(box->left) || !isfinite(box->top)) {
        return false;
    }
    if (!isfinite(box->right) || !isfinite(box->bottom)) {
        return false;
    }
    return true;
}

size_t proto_encode_frame(const esp_box *boxes, uint16_t count,
                          uint8_t *out, size_t capacity)
{
    size_t cursor;
    size_t payload_start;
    uint16_t i;

    if (out == NULL) {
        return 0;
    }
    if (count > 0 && boxes == NULL) {
        return 0;
    }
    if (count > OJBS_MAX_BOXES) {
        return 0;
    }
    if (capacity < (size_t)OJBS_HEADER_SIZE + 2u) {
        return 0;
    }

    put_u32(out, OJBS_MAGIC);
    /* payload_len si riempie alla fine, quando e' noto */
    payload_start = OJBS_HEADER_SIZE;
    cursor = payload_start;

    put_u16(out + cursor, count);
    cursor += 2;

    for (i = 0; i < count; ++i) {
        const esp_box *box = &boxes[i];
        size_t name_len;

        if (!box_finite(box)) {
            return 0;
        }

        name_len = strnlen(box->name, OJBS_MAX_NAME_BYTES);

        if (cursor + (size_t)OJBS_BOX_FIXED_SIZE + name_len > capacity) {
            return 0;
        }

        put_f32(out + cursor, box->left);
        cursor += 4;
        put_f32(out + cursor, box->top);
        cursor += 4;
        put_f32(out + cursor, box->right);
        cursor += 4;
        put_f32(out + cursor, box->bottom);
        cursor += 4;
        put_i32(out + cursor, box->argb);
        cursor += 4;
        put_f32(out + cursor, isfinite(box->health) ? box->health : -1.0f);
        cursor += 4;

        out[cursor] = (uint8_t)name_len;
        cursor += 1;

        if (name_len > 0) {
            memcpy(out + cursor, box->name, name_len);
            cursor += name_len;
        }
    }

    put_u32(out + 4, (uint32_t)(cursor - payload_start));
    return cursor;
}

bool proto_decode_hello(const uint8_t *data, size_t length,
                        uint32_t *width, uint32_t *height)
{
    if (data == NULL || width == NULL || height == NULL) {
        return false;
    }
    if (length != (size_t)OJBS_HELLO_SIZE) {
        return false;
    }

    *width = get_u32(data);
    *height = get_u32(data + 4);
    return true;
}
