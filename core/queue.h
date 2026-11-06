/* SPDX-License-Identifier: MIT */
#ifndef MS_QUEUE_H
#define MS_QUEUE_H
#include "../include/ms_protocol.h"
typedef struct ms_slot { ms_event event; uint64_t created_ms; } ms_slot;
typedef struct ms_queue {
    ms_slot slots[MS_CAPACITY];
    uint64_t next_sequence;
    uint32_t head, count;
} ms_queue;
void msq_init(ms_queue *q);
void msq_clear(ms_queue *q);
int msq_push(ms_queue *q, const ms_packet *p, uint64_t now);
const ms_slot *msq_front(const ms_queue *q);
int msq_pop(ms_queue *q, uint64_t sequence);
int msq_expired(const ms_queue *q, uint64_t now);
int ms_packet_valid(const ms_packet *p);
#endif
