#include "video/frame_mailbox.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { FRAME_COUNT = 80, FIRST_CONCURRENT_SEQUENCE = 11 };
static seedless_video_frame_mailbox_t mailbox;

static void fill(uint8_t *data, uint8_t stamp)
{
    memset(data, stamp, SEEDLESS_VIDEO_IMAGE_BYTES);
}

static void *producer(void *unused)
{
    (void)unused;
    seedless_video_frame_pair_t *pair = seedless_video_mailbox_staging_pair(&mailbox);
    for (unsigned i = FIRST_CONCURRENT_SEQUENCE;
         i < FIRST_CONCURRENT_SEQUENCE + FRAME_COUNT; ++i) {
        uint8_t stamp = (uint8_t)i;
        fill(pair->top_rgba, stamp);
        fill(pair->bottom_rgba, (uint8_t)~stamp);
        assert(seedless_video_mailbox_publish(&mailbox, i));
    }
    return NULL;
}

int main(void)
{
    assert(SEEDLESS_VIDEO_MAILBOX_IMAGE_STORAGE == 1179648u);
    assert(seedless_video_mailbox_init(&mailbox));
    seedless_video_frame_pair_t *staging = seedless_video_mailbox_staging_pair(&mailbox);
    for (unsigned i = 1; i < FIRST_CONCURRENT_SEQUENCE; ++i) {
        fill(staging->top_rgba, (uint8_t)i);
        fill(staging->bottom_rgba, (uint8_t)~(uint8_t)i);
        assert(seedless_video_mailbox_publish(&mailbox, i));
    }
    uint64_t previous = 0;
    assert(seedless_video_mailbox_consume_latest(&mailbox, &previous));
    assert(previous == FIRST_CONCURRENT_SEQUENCE - 1);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, producer, NULL) == 0);
    unsigned observations = 0;
    for (;;) {
        uint64_t sequence = 0;
        if (seedless_video_mailbox_consume_latest(&mailbox, &sequence)) {
            const seedless_video_frame_pair_t *pair =
                seedless_video_mailbox_consumer_pair(&mailbox);
            assert(sequence > previous);
            previous = sequence;
            uint8_t top = pair->top_rgba[0];
            uint8_t bottom = pair->bottom_rgba[0];
            assert(top == (uint8_t)sequence);
            assert(bottom == (uint8_t)~top);
            for (size_t i = 0; i < SEEDLESS_VIDEO_IMAGE_BYTES; i += 4093) {
                assert(pair->top_rgba[i] == top);
                assert(pair->bottom_rgba[i] == bottom);
            }
            observations++;
        }
        int done = previous >= FIRST_CONCURRENT_SEQUENCE + FRAME_COUNT - 1;
        if (done) break;
        sched_yield();
    }
    assert(pthread_join(thread, NULL) == 0);
    assert(observations > 0 && observations <= FRAME_COUNT);
    assert(previous == FIRST_CONCURRENT_SEQUENCE + FRAME_COUNT - 1);
    seedless_video_mailbox_destroy(&mailbox);
    puts("video mailbox paired ownership/overwrite acceptance passed");
    return 0;
}
