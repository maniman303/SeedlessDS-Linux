#ifndef SEEDLESSDS_VIDEO_LAYOUT_H
#define SEEDLESSDS_VIDEO_LAYOUT_H

#include <stddef.h>
#include <stdint.h>

#define SEEDLESS_VIDEO_SCREEN_WIDTH 256
#define SEEDLESS_VIDEO_SCREEN_HEIGHT 192
#define SEEDLESS_VIDEO_IMAGE_BYTES 196608u
#define SEEDLESS_VIDEO_PITCH 1024
#define SEEDLESS_VIDEO_MAX_OUTPUTS 2

typedef enum seedless_video_requested_mode {
    SEEDLESS_VIDEO_MODE_AUTO,
    SEEDLESS_VIDEO_MODE_DUAL_SCREEN,
    SEEDLESS_VIDEO_MODE_SINGLE_SCREEN
} seedless_video_requested_mode_t;

typedef enum seedless_video_effective_mode {
    SEEDLESS_VIDEO_EFFECTIVE_DUAL,
    SEEDLESS_VIDEO_EFFECTIVE_SINGLE
} seedless_video_effective_mode_t;

typedef enum seedless_video_single_layout {
    SEEDLESS_VIDEO_LAYOUT_VERTICAL,
    SEEDLESS_VIDEO_LAYOUT_HORIZONTAL
} seedless_video_single_layout_t;

typedef enum seedless_video_display_role {
    SEEDLESS_VIDEO_DISPLAY_UNUSED,
    SEEDLESS_VIDEO_DISPLAY_PRIMARY,
    SEEDLESS_VIDEO_DISPLAY_SECONDARY
} seedless_video_display_role_t;

typedef struct seedless_video_preferences {
    seedless_video_requested_mode_t requested_mode;
    seedless_video_single_layout_t single_layout;
    int swap_screens;
} seedless_video_preferences_t;

typedef struct seedless_video_rect {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} seedless_video_rect_t;

typedef struct seedless_video_display {
    int sdl_index;
    char name[128];
    seedless_video_rect_t bounds;
    int32_t desktop_width;
    int32_t desktop_height;
    int32_t renderer_width_px;
    int32_t renderer_height_px;
    seedless_video_display_role_t role;
} seedless_video_display_t;

typedef struct seedless_video_destination {
    unsigned display_index;
    unsigned image_index;
    unsigned slot_index;
    uint32_t integer_scale;
    seedless_video_rect_t rect;
} seedless_video_destination_t;

typedef struct seedless_video_layout {
    seedless_video_requested_mode_t requested_mode;
    seedless_video_effective_mode_t effective_mode;
    unsigned output_count;
    unsigned destination_count;
    uint32_t common_scale;
    seedless_video_destination_t destinations[2];
} seedless_video_layout_t;

int seedless_video_compute_layout(const seedless_video_preferences_t *preferences,
                                  const seedless_video_display_t *displays,
                                  size_t display_count,
                                  seedless_video_layout_t *layout,
                                  char *error, size_t error_capacity);

#endif
