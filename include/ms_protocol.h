/* SPDX-License-Identifier: MIT */
#ifndef MS_PROTOCOL_H
#define MS_PROTOCOL_H
#include <stdint.h>
#define MS_VERSION 1u
#define MS_CAPACITY 256u
#define MS_TIMEOUT_MS 250u
#define MS_PATH_CHARS 1024u
#define MS_FORWARD 0u
#define MS_DROP 1u
/* Fixed-width wire types; no pointers, size_t, or compiler bitfields. */
typedef struct ms_packet {
    uint16_t unit_id, flags;
    uint16_t buttons, button_data;
    uint32_t raw_buttons;
    int32_t x, y;
    uint32_t information;
} ms_packet;
typedef struct ms_event { uint64_t sequence; ms_packet packet; } ms_event;
typedef struct ms_decision {
    uint64_t sequence;
    uint32_t action, reserved;
    ms_packet packet;
} ms_decision;
typedef struct ms_info {
    uint32_t version, capturing, pending, timeout_ms;
} ms_info;
/* FILE_DEVICE_UNKNOWN, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA. */
#define MS_IOCTL(n) ((0x22u << 16) | (3u << 14) | ((n) << 2))
#define MS_GET_INFO MS_IOCTL(0x800u)
#define MS_START    MS_IOCTL(0x801u)
#define MS_STOP     MS_IOCTL(0x802u)
#define MS_RECEIVE  MS_IOCTL(0x803u)
#define MS_RESOLVE  MS_IOCTL(0x804u)
#define MS_INJECT   MS_IOCTL(0x805u)
#endif
