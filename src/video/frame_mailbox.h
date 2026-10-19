#ifndef SEEDLESSDS_VIDEO_FRAME_MAILBOX_H
#define SEEDLESSDS_VIDEO_FRAME_MAILBOX_H

#include "layout.h"
#include <pthread.h>
#include <stdint.h>

#define SEEDLESS_VIDEO_MAILBOX_IMAGE_STORAGE (3u * 2u * SEEDLESS_VIDEO_IMAGE_BYTES)

typedef struct seedless_video_frame_pair {
    uint8_t *top_rgba;
    uint8_t *bottom_rgba;
    uint64_t sequence;
} seedless_video_frame_pair_t;

typedef struct seedless_video_frame_mailbox {
    uint8_t *storage;
    seedless_video_frame_pair_t staging;
    seedless_video_frame_pair_t published;
    seedless_video_frame_pair_t consumer;
    pthread_mutex_t mutex;
    int mutex_initialized;
    int has_published;
    int failed;
    char failure[192];
} seedless_video_frame_mailbox_t;

int seedless_video_mailbox_init(seedless_video_frame_mailbox_t *mailbox);
void seedless_video_mailbox_destroy(seedless_video_frame_mailbox_t *mailbox);
seedless_video_frame_pair_t *seedless_video_mailbox_staging_pair(
    seedless_video_frame_mailbox_t *mailbox);
const seedless_video_frame_pair_t *seedless_video_mailbox_consumer_pair(
    const seedless_video_frame_mailbox_t *mailbox);
int seedless_video_mailbox_publish(seedless_video_frame_mailbox_t *mailbox,
                                   uint64_t sequence);
int seedless_video_mailbox_consume_latest(seedless_video_frame_mailbox_t *mailbox,
                                          uint64_t *sequence);
void seedless_video_mailbox_set_failure(seedless_video_frame_mailbox_t *mailbox,
                                        const char *message);
int seedless_video_mailbox_get_failure(seedless_video_frame_mailbox_t *mailbox,
                                       char *message, size_t capacity);

#endif
