#ifndef SEEDLESSDS_VIDEO_SDL_VIDEO_H
#define SEEDLESSDS_VIDEO_SDL_VIDEO_H

#include "frame_mailbox.h"

typedef struct seedless_sdl_video {
    void *implementation;
    seedless_video_layout_t layout;
    int initialized;
} seedless_sdl_video_t;

int seedless_sdl_video_init(seedless_sdl_video_t *video,
                            const seedless_video_preferences_t *preferences,
                            char *error, size_t error_capacity);
int seedless_sdl_video_present(seedless_sdl_video_t *video,
                               const seedless_video_frame_pair_t *pair,
                               char *error, size_t error_capacity);
int seedless_sdl_video_poll(seedless_sdl_video_t *video,
                            int *should_quit,
                            char *error, size_t error_capacity);
void seedless_sdl_video_destroy(seedless_sdl_video_t *video);

#ifdef SEEDLESS_VIDEO_TESTING
int seedless_sdl_video_readback_test(char *error, size_t error_capacity);
#endif

#endif
