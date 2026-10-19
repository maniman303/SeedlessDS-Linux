#ifndef SEEDLESSDS_LINUX_CORE_VIDEO_BRIDGE_H
#define SEEDLESSDS_LINUX_CORE_VIDEO_BRIDGE_H

#include "../../video/frame_mailbox.h"

typedef struct seedless_core_video_bridge {
    void *core;
    seedless_video_frame_mailbox_t *mailbox;
    int attached;
} seedless_core_video_bridge_t;

int seedless_core_video_bridge_attach(seedless_core_video_bridge_t *bridge,
                                      void *core,
                                      seedless_video_frame_mailbox_t *mailbox,
                                      char *error, size_t error_capacity);
int seedless_core_video_bridge_detach(seedless_core_video_bridge_t *bridge,
                                      char *error, size_t error_capacity);
/* Use only after the core worker has been joined; the core may have destroyed its mutex. */
void seedless_core_video_bridge_detach_stopped(seedless_core_video_bridge_t *bridge);
void seedless_core_video_bridge_trace(const char *point);

#endif
