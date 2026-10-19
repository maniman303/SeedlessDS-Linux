#ifndef SEEDLESSDS_LAUNCH_OPTIONS_H
#define SEEDLESSDS_LAUNCH_OPTIONS_H
#include <stdint.h>
#include <stdio.h>
typedef struct seedless_launch_options {
    const char *rom_path;
    const char *config_path;
    const char *save_dir;
    uint64_t frames;
    int have_config;
    int have_save_dir;
    int have_frames;
    int help;
} seedless_launch_options_t;
int seedless_launch_parse(int argc, char **argv, seedless_launch_options_t *options,
                          char *error, unsigned error_size);
void seedless_launch_usage(FILE *stream);
#endif
