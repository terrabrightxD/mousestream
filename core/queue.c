/* SPDX-License-Identifier: MIT */
#include "queue.h"
void msq_init(ms_queue *q) {
    q->head = q->count = 0; q->next_sequence = 1;
}
void msq_clear(ms_queue *q) { q->head = q->count = 0; }
int msq_push(ms_queue *q, const ms_packet *p, uint64_t now) {
    ms_slot *s;
    if (q->count == MS_CAPACITY || q->next_sequence == 0) return 0;
    s = &q->slots[(q->head + q->count) % MS_CAPACITY];
    s->event.sequence = q->next_sequence++;
    s->event.packet = *p; s->created_ms = now; ++q->count; return 1;
}
const ms_slot *msq_front(const ms_queue *q) {
    return q->count ? &q->slots[q->head] : 0;
}
int msq_pop(ms_queue *q, uint64_t sequence) {
    if (!q->count || q->slots[q->head].event.sequence != sequence) return 0;
    q->head = (q->head + 1) % MS_CAPACITY; --q->count; return 1;
}
int msq_expired(const ms_queue *q, uint64_t now) {
    const ms_slot *s = msq_front(q);
    return s && now >= s->created_ms && now - s->created_ms >= MS_TIMEOUT_MS;
}
int ms_packet_valid(const ms_packet *p) {
    /* Absolute coordinates, virtual desktop, attributes/no-coalesce, shadow. */
    return !(p->flags & ~0x10fu) && !(p->buttons & ~0xfffu);
}
