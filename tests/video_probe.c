#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include "video/sdl_video.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int parse_seconds(const char *text, unsigned *seconds)
{
    if (!text || !*text) return 0;
    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 10);
    if (errno || !end || *end || value == 0 || value > 3600) return 0;
    *seconds = (unsigned)value;
    return 1;
}

static uint64_t now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static void sleep_ms(long milliseconds)
{
    struct timespec ts = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000L,
    };
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) { }
}

static void pixel(uint8_t *image, unsigned x, unsigned y,
                  uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t *p = image + ((size_t)y * SEEDLESS_VIDEO_SCREEN_WIDTH + x) * 4;
    p[0] = r; p[1] = g; p[2] = b; p[3] = 255;
}

static void paint_screen(uint8_t *image, uint64_t sequence, unsigned screen)
{
    for (unsigned y = 0; y < SEEDLESS_VIDEO_SCREEN_HEIGHT; ++y) {
        for (unsigned x = 0; x < SEEDLESS_VIDEO_SCREEN_WIDTH; ++x) {
            uint8_t c = ((x ^ y) & 1u) ? 224 : 24;
            pixel(image, x, y, c, c, c);
        }
    }
    static const uint8_t patches[3][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
    for (unsigned color = 0; color < 3; ++color)
        for (unsigned y = 8; y < 40; ++y)
            for (unsigned x = 8 + color * 40; x < 40 + color * 40; ++x)
                pixel(image, x, y, patches[color][0], patches[color][1], patches[color][2]);
    uint64_t id = sequence + (screen ? 0x80u : 0u);
    for (unsigned bit = 0; bit < 64; ++bit) {
        uint8_t value = (id >> (bit % 64)) & 1u ? 255 : 32;
        unsigned left = 136 + bit * 1u;
        if (left >= SEEDLESS_VIDEO_SCREEN_WIDTH) break;
        for (unsigned y = 56; y < 72; ++y) pixel(image, left, y, value, (uint8_t)(255 - value), screen ? 255 : 0);
    }
}

int main(int argc, char **argv)
{
#ifdef SEEDLESS_VIDEO_TESTING
    if (argc == 2 && !strcmp(argv[1], "--readback-test")) {
        char error[256] = {0};
        if (!seedless_sdl_video_readback_test(error, sizeof error)) {
            fprintf(stderr, "video-probe: readback test failed: %s\n", error);
            return 1;
        }
        puts("video-probe: nearest RGB readback passed for vertical/horizontal layouts and swaps");
        return 0;
    }
#endif
    unsigned seconds = 15;
    const char *config_path = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--seconds") && i + 1 < argc) {
            if (!parse_seconds(argv[++i], &seconds)) {
                fprintf(stderr, "video-probe: --seconds must be an integer from 1 to 3600\n");
                return 2;
            }
        } else if (!strcmp(argv[i], "--config") && i + 1 < argc) {
            if (config_path) {
                fprintf(stderr, "video-probe: --config may be specified once\n");
                return 2;
            }
            config_path = argv[++i];
            if (!*config_path) {
                fprintf(stderr, "video-probe: --config requires a nonempty path\n");
                return 2;
            }
        } else {
            fprintf(stderr, "Usage: seedlessds-video-probe [--seconds N] [--config PATH]\n");
            return 2;
        }
    }

    seedless_config_t config = {0};
    seedless_config_error_t config_error = {0};
    if (!seedless_config_load_or_create(&config, config_path, &config_error)) {
        fprintf(stderr, "video-probe: config=%s key=%s: %s\n",
                config.selected_path ? config.selected_path :
                (config_path ? config_path : "<default>"),
                config_error.key, config_error.message);
        seedless_config_free(&config);
        return 1;
    }

    seedless_sdl_video_t video = {0};
    char error[256] = {0};
    if (!seedless_sdl_video_init(&video, &config.video, error, sizeof error)) {
        fprintf(stderr, "video-probe: %s\n", error);
        seedless_config_free(&config);
        return 1;
    }
    uint8_t *images = calloc(2, SEEDLESS_VIDEO_IMAGE_BYTES);
    if (!images) {
        fprintf(stderr, "video-probe: out of memory\n");
        seedless_sdl_video_destroy(&video);
        seedless_config_free(&config);
        return 1;
    }
    seedless_video_frame_pair_t pair = {
        .top_rgba = images,
        .bottom_rgba = images + SEEDLESS_VIDEO_IMAGE_BYTES,
    };
    uint64_t start = now_ms();
    uint64_t next = start;
    uint64_t sequence = 0;
    int result = 0;
    while (now_ms() - start < (uint64_t)seconds * 1000u) {
        uint64_t now = now_ms();
        if (now < next) { sleep_ms(1); continue; }
        next = now + 16;
        paint_screen(pair.top_rgba, sequence, 0);
        paint_screen(pair.bottom_rgba, sequence, 1);
        pair.sequence = ++sequence;
        if (!seedless_sdl_video_present(&video, &pair, error, sizeof error)) {
            fprintf(stderr, "video-probe: %s\n", error);
            result = 1;
            break;
        }
        int should_quit = 0;
        if (!seedless_sdl_video_poll(&video, &should_quit, error, sizeof error)) {
            fprintf(stderr, "video-probe: %s\n", error);
            result = 1;
            break;
        }
        if (should_quit) break;
    }
    fprintf(stderr, "video-probe: frames=%" PRIu64 " result=%s\n",
            sequence, result ? "failed" : "completed");
    free(images);
    seedless_sdl_video_destroy(&video);
    seedless_config_free(&config);
    return result;
}
