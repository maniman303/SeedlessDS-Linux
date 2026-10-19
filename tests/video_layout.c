#include "video/layout.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static seedless_video_display_t display(int width, int height)
{
    seedless_video_display_t result = {0};
    result.renderer_width_px = width;
    result.renderer_height_px = height;
    return result;
}

static seedless_video_layout_t compute(seedless_video_preferences_t preferences,
                                      seedless_video_display_t *displays,
                                      size_t count)
{
    seedless_video_layout_t result;
    char error[256] = {0};
    assert(seedless_video_compute_layout(&preferences, displays, count,
                                         &result, error, sizeof error));
    return result;
}

static void expect_failure(seedless_video_preferences_t preferences,
                           seedless_video_display_t *displays,
                           size_t count)
{
    seedless_video_layout_t result;
    char error[256] = {0};
    assert(!seedless_video_compute_layout(&preferences, displays, count,
                                          &result, error, sizeof error));
    assert(error[0]);
}

int main(void)
{
    seedless_video_preferences_t defaults = {
        .requested_mode = SEEDLESS_VIDEO_MODE_AUTO,
        .single_layout = SEEDLESS_VIDEO_LAYOUT_VERTICAL,
        .swap_screens = 0,
    };
    seedless_video_display_t dual[] = { display(1024, 768), display(640, 480) };
    seedless_video_layout_t layout = compute(defaults, dual, 2);
    assert(layout.effective_mode == SEEDLESS_VIDEO_EFFECTIVE_DUAL);
    assert(layout.requested_mode == SEEDLESS_VIDEO_MODE_AUTO);
    assert(layout.output_count == 2 && layout.destination_count == 2);
    assert(layout.destinations[0].integer_scale == 4);
    assert(layout.destinations[0].rect.x == 0 && layout.destinations[0].rect.y == 0);
    assert(layout.destinations[0].rect.width == 1024 && layout.destinations[0].rect.height == 768);
    assert(layout.destinations[1].integer_scale == 2);
    assert(layout.destinations[1].rect.x == 64 && layout.destinations[1].rect.y == 48);
    assert(layout.destinations[1].rect.width == 512 && layout.destinations[1].rect.height == 384);

    seedless_video_display_t one = display(640, 480);
    defaults.requested_mode = SEEDLESS_VIDEO_MODE_SINGLE_SCREEN;
    layout = compute(defaults, &one, 1);
    assert(layout.effective_mode == SEEDLESS_VIDEO_EFFECTIVE_SINGLE);
    assert(layout.requested_mode == SEEDLESS_VIDEO_MODE_SINGLE_SCREEN);
    assert(layout.output_count == 1 && layout.destination_count == 2);
    assert(layout.common_scale == 1);
    assert(layout.destinations[0].rect.x == 192 && layout.destinations[0].rect.y == 48);
    assert(layout.destinations[1].rect.x == 192 && layout.destinations[1].rect.y == 240);
    assert(layout.destinations[0].rect.width == 256 && layout.destinations[1].rect.width == 256);
    assert(layout.destinations[0].rect.height == 192 && layout.destinations[1].rect.height == 192);

    seedless_video_display_t odd = display(521, 399);
    defaults.requested_mode = SEEDLESS_VIDEO_MODE_AUTO;
    layout = compute(defaults, &odd, 1);
    assert(layout.common_scale == 1);
    /* Floor-centered origins leave any odd extra border pixel on right/bottom. */
    assert(layout.destinations[0].rect.x == 132);
    assert(layout.destinations[0].rect.y == 7);
    assert(layout.destinations[1].rect.y == 199);

    seedless_video_display_t too_small = display(255, 383);
    expect_failure(defaults, &too_small, 1);
    expect_failure(defaults, &one, 0);
    seedless_video_display_t invalid[] = { display(0, 480), display(640, -1) };
    expect_failure(defaults, invalid, 2);

    seedless_video_display_t largest = display(INT_MAX, INT_MAX);
    layout = compute(defaults, &largest, 1);
    assert(layout.common_scale == (uint32_t)(INT_MAX / 384));
    assert(layout.destinations[1].rect.y >= layout.destinations[0].rect.y + 192);

    seedless_video_display_t three[] = {
        display(1024, 768), display(640, 480), display(800, 600),
    };
    defaults.requested_mode = SEEDLESS_VIDEO_MODE_DUAL_SCREEN;
    defaults.single_layout = SEEDLESS_VIDEO_LAYOUT_HORIZONTAL;
    layout = compute(defaults, three, 3);
    assert(layout.requested_mode == SEEDLESS_VIDEO_MODE_DUAL_SCREEN);
    assert(layout.effective_mode == SEEDLESS_VIDEO_EFFECTIVE_DUAL);
    assert(layout.output_count == 2);
    assert(layout.destinations[0].display_index == 0);
    assert(layout.destinations[1].display_index == 1);
    assert(layout.destinations[0].rect.height == 768);
    assert(layout.destinations[1].rect.height == 384);

    defaults.requested_mode = SEEDLESS_VIDEO_MODE_SINGLE_SCREEN;
    layout = compute(defaults, three, 3);
    assert(layout.requested_mode == SEEDLESS_VIDEO_MODE_SINGLE_SCREEN);
    assert(layout.effective_mode == SEEDLESS_VIDEO_EFFECTIVE_SINGLE);
    assert(layout.output_count == 1 && layout.destination_count == 2);
    assert(layout.destinations[0].display_index == 0 &&
           layout.destinations[1].display_index == 0);

    defaults.requested_mode = SEEDLESS_VIDEO_MODE_DUAL_SCREEN;
    defaults.single_layout = SEEDLESS_VIDEO_LAYOUT_VERTICAL;
    layout = compute(defaults, &one, 1);
    assert(layout.requested_mode == SEEDLESS_VIDEO_MODE_DUAL_SCREEN);
    assert(layout.effective_mode == SEEDLESS_VIDEO_EFFECTIVE_SINGLE);
    assert(layout.output_count == 1 && layout.destinations[0].display_index == 0);

    defaults.requested_mode = SEEDLESS_VIDEO_MODE_AUTO;
    defaults.swap_screens = 1;
    layout = compute(defaults, three, 3);
    assert(layout.destinations[0].image_index == 1);
    assert(layout.destinations[1].image_index == 0);
    assert(layout.destinations[0].integer_scale == 4);
    assert(layout.destinations[1].integer_scale == 2);
    layout = compute(defaults, &one, 1);
    assert(layout.effective_mode == SEEDLESS_VIDEO_EFFECTIVE_SINGLE);
    assert(layout.destinations[0].image_index == 1);
    assert(layout.destinations[1].image_index == 0);
    assert(layout.destinations[0].rect.y == 48 && layout.destinations[1].rect.y == 240);
    defaults.requested_mode = SEEDLESS_VIDEO_MODE_DUAL_SCREEN;
    layout = compute(defaults, &one, 1);
    assert(layout.effective_mode == SEEDLESS_VIDEO_EFFECTIVE_SINGLE);
    assert(layout.destinations[0].image_index == 1 && layout.destinations[1].image_index == 0);

    defaults.requested_mode = SEEDLESS_VIDEO_MODE_SINGLE_SCREEN;
    defaults.single_layout = SEEDLESS_VIDEO_LAYOUT_HORIZONTAL;
    defaults.swap_screens = 0;
    layout = compute(defaults, &one, 1);
    assert(layout.common_scale == 1);
    assert(layout.destinations[0].rect.x == 64 && layout.destinations[0].rect.y == 144);
    assert(layout.destinations[1].rect.x == 320 && layout.destinations[1].rect.y == 144);
    assert(layout.destinations[1].rect.x == layout.destinations[0].rect.x + 256);
    defaults.swap_screens = 1;
    layout = compute(defaults, &one, 1);
    assert(layout.destinations[0].image_index == 1 && layout.destinations[1].image_index == 0);
    seedless_video_display_t horizontal_minimum = display(512, 192);
    layout = compute(defaults, &horizontal_minimum, 1);
    assert(layout.common_scale == 1);
    seedless_video_display_t horizontal_odd = display(521, 399);
    layout = compute(defaults, &horizontal_odd, 1);
    assert(layout.destinations[0].rect.x == 4 && layout.destinations[0].rect.y == 103);
    assert(layout.destinations[1].rect.x == 260 && layout.destinations[1].rect.y == 103);
    seedless_video_display_t horizontal_too_small = display(511, 192);
    expect_failure(defaults, &horizontal_too_small, 1);
    seedless_video_display_t vertical_too_small = display(256, 383);
    defaults.single_layout = SEEDLESS_VIDEO_LAYOUT_VERTICAL;
    expect_failure(defaults, &vertical_too_small, 1);
    defaults.requested_mode = SEEDLESS_VIDEO_MODE_AUTO;
    defaults.single_layout = SEEDLESS_VIDEO_LAYOUT_HORIZONTAL;
    layout = compute(defaults, three, 3);
    seedless_video_layout_t horizontal_dual = layout;
    defaults.single_layout = SEEDLESS_VIDEO_LAYOUT_VERTICAL;
    layout = compute(defaults, three, 3);
    assert(layout.effective_mode == SEEDLESS_VIDEO_EFFECTIVE_DUAL);
    assert(layout.destinations[0].rect.x == horizontal_dual.destinations[0].rect.x);
    assert(layout.destinations[0].rect.y == horizontal_dual.destinations[0].rect.y);
    assert(layout.destinations[1].rect.width == horizontal_dual.destinations[1].rect.width);
    puts("video layout acceptance passed");
    return 0;
}
