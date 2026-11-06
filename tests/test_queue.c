/* SPDX-License-Identifier: MIT */
#include "../core/queue.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    ms_queue q;
    ms_packet p = {0};
    uint64_t last;
    unsigned i, round;
    assert(sizeof(ms_packet) == 24 && sizeof(ms_event) == 32 && sizeof(ms_decision) == 40);
    msq_init(&q); assert(!msq_front(&q) && !msq_pop(&q, 1));
    p.x = -123; p.y = 456; p.buttons = 0x400; p.button_data = (uint16_t)-120;
    assert(ms_packet_valid(&p));
    for (round = 0; round < 50; ++round) {
        for (i = 0; i < MS_CAPACITY; ++i) { p.x = (int32_t)i; assert(msq_push(&q, &p, 100)); }
        assert(!msq_push(&q, &p, 100));
        assert(!msq_expired(&q, 99)); assert(!msq_expired(&q, 349)); assert(msq_expired(&q, 350));
        for (i = 0; i < MS_CAPACITY; ++i) {
            const ms_slot *s = msq_front(&q);
            assert(s && s->event.packet.x == (int32_t)i);
            last = s->event.sequence;
            assert(!msq_pop(&q, last + 1));
            assert(msq_front(&q)->event.sequence == last); /* receive is non-destructive */
            assert(msq_pop(&q, last)); assert(!msq_pop(&q, last));
        }
        assert(!msq_front(&q));
    }
    assert(msq_push(&q, &p, 0)); last = msq_front(&q)->event.sequence;
    msq_clear(&q); assert(msq_push(&q, &p, 0));
    assert(!msq_pop(&q, last)); /* stale decision after recovery/re-arm */
    msq_clear(&q); q.next_sequence = UINT64_MAX;
    assert(msq_push(&q, &p, 0)); assert(!msq_push(&q, &p, 0)); /* never reuse a sequence */
    p.flags = 0x8000; assert(!ms_packet_valid(&p));
    p.flags = 0; p.buttons = 0x8000; assert(!ms_packet_valid(&p));
    puts("PASS: ABI, FIFO, overflow, wraparound, watchdog, stale decisions, packet validation");
    return 0;
}
