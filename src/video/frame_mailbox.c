#include "frame_mailbox.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int seedless_video_mailbox_init(seedless_video_frame_mailbox_t *mailbox)
{
    if (!mailbox) return 0;
    memset(mailbox, 0, sizeof *mailbox);
    mailbox->storage = calloc(1, SEEDLESS_VIDEO_MAILBOX_IMAGE_STORAGE);
    if (!mailbox->storage) return 0;
    if (pthread_mutex_init(&mailbox->mutex, NULL) != 0) {
        free(mailbox->storage);
        memset(mailbox, 0, sizeof *mailbox);
        return 0;
    }
    mailbox->mutex_initialized = 1;
    uint8_t *cursor = mailbox->storage;
    seedless_video_frame_pair_t *pairs[] = {
        &mailbox->staging, &mailbox->published, &mailbox->consumer,
    };
    for (size_t i = 0; i < sizeof pairs / sizeof pairs[0]; ++i) {
        pairs[i]->top_rgba = cursor;
        cursor += SEEDLESS_VIDEO_IMAGE_BYTES;
        pairs[i]->bottom_rgba = cursor;
        cursor += SEEDLESS_VIDEO_IMAGE_BYTES;
    }
    return 1;
}

void seedless_video_mailbox_destroy(seedless_video_frame_mailbox_t *mailbox)
{
    if (!mailbox) return;
    if (mailbox->mutex_initialized) pthread_mutex_destroy(&mailbox->mutex);
    free(mailbox->storage);
    memset(mailbox, 0, sizeof *mailbox);
}

seedless_video_frame_pair_t *seedless_video_mailbox_staging_pair(
    seedless_video_frame_mailbox_t *mailbox)
{
    return mailbox && mailbox->storage ? &mailbox->staging : NULL;
}

const seedless_video_frame_pair_t *seedless_video_mailbox_consumer_pair(
    const seedless_video_frame_mailbox_t *mailbox)
{
    return mailbox && mailbox->storage ? &mailbox->consumer : NULL;
}

int seedless_video_mailbox_publish(seedless_video_frame_mailbox_t *mailbox,
                                   uint64_t sequence)
{
    if (!mailbox || !mailbox->mutex_initialized || !mailbox->storage || !sequence)
        return 0;
    pthread_mutex_lock(&mailbox->mutex);
    if (mailbox->failed || sequence <= mailbox->published.sequence) {
        if (!mailbox->failed) {
            mailbox->failed = 1;
            snprintf(mailbox->failure, sizeof mailbox->failure,
                     "frame publication sequence did not increase");
        }
        pthread_mutex_unlock(&mailbox->mutex);
        return 0;
    }
    memcpy(mailbox->published.top_rgba, mailbox->staging.top_rgba,
           SEEDLESS_VIDEO_IMAGE_BYTES);
    memcpy(mailbox->published.bottom_rgba, mailbox->staging.bottom_rgba,
           SEEDLESS_VIDEO_IMAGE_BYTES);
    mailbox->published.sequence = sequence;
    mailbox->has_published = 1;
    pthread_mutex_unlock(&mailbox->mutex);
    return 1;
}

int seedless_video_mailbox_consume_latest(seedless_video_frame_mailbox_t *mailbox,
                                          uint64_t *sequence)
{
    if (!mailbox || !mailbox->mutex_initialized || !mailbox->storage) return 0;
    pthread_mutex_lock(&mailbox->mutex);
    if (!mailbox->has_published ||
        mailbox->published.sequence <= mailbox->consumer.sequence) {
        pthread_mutex_unlock(&mailbox->mutex);
        return 0;
    }
    memcpy(mailbox->consumer.top_rgba, mailbox->published.top_rgba,
           SEEDLESS_VIDEO_IMAGE_BYTES);
    memcpy(mailbox->consumer.bottom_rgba, mailbox->published.bottom_rgba,
           SEEDLESS_VIDEO_IMAGE_BYTES);
    mailbox->consumer.sequence = mailbox->published.sequence;
    if (sequence) *sequence = mailbox->consumer.sequence;
    pthread_mutex_unlock(&mailbox->mutex);
    return 1;
}

void seedless_video_mailbox_set_failure(seedless_video_frame_mailbox_t *mailbox,
                                        const char *message)
{
    if (!mailbox || !mailbox->mutex_initialized) return;
    pthread_mutex_lock(&mailbox->mutex);
    if (!mailbox->failed) {
        mailbox->failed = 1;
        snprintf(mailbox->failure, sizeof mailbox->failure, "%s",
                 message ? message : "video frame publication failed");
    }
    pthread_mutex_unlock(&mailbox->mutex);
}

int seedless_video_mailbox_get_failure(seedless_video_frame_mailbox_t *mailbox,
                                       char *message, size_t capacity)
{
    if (!mailbox || !mailbox->mutex_initialized) return 0;
    pthread_mutex_lock(&mailbox->mutex);
    int failed = mailbox->failed;
    if (failed && message && capacity)
        snprintf(message, capacity, "%s", mailbox->failure);
    pthread_mutex_unlock(&mailbox->mutex);
    return failed;
}
