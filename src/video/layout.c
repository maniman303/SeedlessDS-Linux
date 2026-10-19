#include "layout.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int fail(char *error, size_t capacity, const char *message)
{
    if (error && capacity) snprintf(error, capacity, "%s", message);
    return 0;
}

static int fit(int32_t width, int32_t height, int32_t canvas_width,
               int32_t canvas_height, uint32_t *scale,
               int32_t *origin_x, int32_t *origin_y)
{
    if (width <= 0 || height <= 0 || canvas_width <= 0 || canvas_height <= 0)
        return 0;
    int64_t x_scale = width / canvas_width;
    int64_t y_scale = height / canvas_height;
    int64_t factor = x_scale < y_scale ? x_scale : y_scale;
    if (factor < 1 || factor > UINT32_MAX) return 0;
    int64_t used_width = (int64_t)canvas_width * factor;
    int64_t used_height = (int64_t)canvas_height * factor;
    if (used_width > width || used_height > height ||
        used_width > INT32_MAX || used_height > INT32_MAX)
        return 0;
    int64_t x = ((int64_t)width - used_width) / 2;
    int64_t y = ((int64_t)height - used_height) / 2;
    if (x > INT32_MAX || y > INT32_MAX) return 0;
    *scale = (uint32_t)factor;
    *origin_x = (int32_t)x;
    *origin_y = (int32_t)y;
    return 1;
}

int seedless_video_compute_layout(const seedless_video_preferences_t *preferences,
                                  const seedless_video_display_t *displays,
                                  size_t display_count,
                                  seedless_video_layout_t *layout,
                                  char *error, size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    if (!preferences || !layout || (display_count && !displays))
        return fail(error, error_capacity, "invalid video layout input");
    if (preferences->requested_mode < SEEDLESS_VIDEO_MODE_AUTO ||
        preferences->requested_mode > SEEDLESS_VIDEO_MODE_SINGLE_SCREEN)
        return fail(error, error_capacity, "invalid requested display mode");
    if (preferences->single_layout < SEEDLESS_VIDEO_LAYOUT_VERTICAL ||
        preferences->single_layout > SEEDLESS_VIDEO_LAYOUT_HORIZONTAL)
        return fail(error, error_capacity, "invalid single-screen layout");
    if (display_count == 0)
        return fail(error, error_capacity, "no usable SDL displays are available");

    memset(layout, 0, sizeof *layout);
    layout->requested_mode = preferences->requested_mode;
    int dual = display_count >= 2 &&
        preferences->requested_mode != SEEDLESS_VIDEO_MODE_SINGLE_SCREEN;
    layout->effective_mode = dual ? SEEDLESS_VIDEO_EFFECTIVE_DUAL :
                                    SEEDLESS_VIDEO_EFFECTIVE_SINGLE;
    layout->output_count = dual ? 2u : 1u;
    layout->destination_count = 2;

    if (dual) {
        for (unsigned i = 0; i < 2; ++i) {
            uint32_t scale;
            int32_t x, y;
            const seedless_video_display_t *display = &displays[i];
            if (!fit(display->renderer_width_px, display->renderer_height_px,
                     SEEDLESS_VIDEO_SCREEN_WIDTH, SEEDLESS_VIDEO_SCREEN_HEIGHT,
                     &scale, &x, &y))
                return fail(error, error_capacity,
                            "dual-screen output is invalid or too small for integer scaling");
            seedless_video_destination_t *destination = &layout->destinations[i];
            destination->display_index = i;
            destination->slot_index = 0;
            destination->image_index = i ^ (preferences->swap_screens ? 1u : 0u);
            destination->integer_scale = scale;
            destination->rect = (seedless_video_rect_t){
                .x = x, .y = y,
                .width = SEEDLESS_VIDEO_SCREEN_WIDTH * (int32_t)scale,
                .height = SEEDLESS_VIDEO_SCREEN_HEIGHT * (int32_t)scale,
            };
        }
        return 1;
    }

    const int horizontal = preferences->single_layout == SEEDLESS_VIDEO_LAYOUT_HORIZONTAL;
    const int32_t canvas_width = horizontal ? 2 * SEEDLESS_VIDEO_SCREEN_WIDTH :
                                              SEEDLESS_VIDEO_SCREEN_WIDTH;
    const int32_t canvas_height = horizontal ? SEEDLESS_VIDEO_SCREEN_HEIGHT :
                                               2 * SEEDLESS_VIDEO_SCREEN_HEIGHT;
    uint32_t scale;
    int32_t origin_x, origin_y;
    if (!fit(displays[0].renderer_width_px, displays[0].renderer_height_px,
             canvas_width, canvas_height, &scale, &origin_x, &origin_y))
        return fail(error, error_capacity,
                    "single-screen output is invalid or too small for selected integer layout");
    layout->common_scale = scale;
    for (unsigned slot = 0; slot < 2; ++slot) {
        seedless_video_destination_t *destination = &layout->destinations[slot];
        int32_t slot_x = horizontal ? (int32_t)slot * SEEDLESS_VIDEO_SCREEN_WIDTH : 0;
        int32_t slot_y = horizontal ? 0 : (int32_t)slot * SEEDLESS_VIDEO_SCREEN_HEIGHT;
        destination->display_index = 0;
        destination->slot_index = slot;
        destination->image_index = slot ^ (preferences->swap_screens ? 1u : 0u);
        destination->integer_scale = scale;
        destination->rect = (seedless_video_rect_t){
            .x = origin_x + slot_x * (int32_t)scale,
            .y = origin_y + slot_y * (int32_t)scale,
            .width = SEEDLESS_VIDEO_SCREEN_WIDTH * (int32_t)scale,
            .height = SEEDLESS_VIDEO_SCREEN_HEIGHT * (int32_t)scale,
        };
    }
    return 1;
}
