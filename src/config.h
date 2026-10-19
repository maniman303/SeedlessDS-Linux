#ifndef SEEDLESSDS_CONFIG_H
#define SEEDLESSDS_CONFIG_H
#include <stddef.h>
#include <stdint.h>
#include "config_json.h"
#include "video/layout.h"

typedef struct seedless_config_visuals {
    int threaded_3d;
    unsigned thread_count;
    int disable_edge_marking;
    int fix_main_screen;
    int auto_trim;
} seedless_config_visuals_t;
typedef struct seedless_config_firmware {
    char *bios_arm7;
    char *bios_arm9;
    char *firmware_path;
} seedless_config_firmware_t;
typedef struct seedless_config_user {
    char *nickname;
    uint16_t nickname_utf16[10];
    size_t nickname_units;
    unsigned birthday_month;
    unsigned birthday_day;
    unsigned color;
} seedless_config_user_t;
typedef struct seedless_config_emulation { char *save_dir; } seedless_config_emulation_t;
typedef struct seedless_config {
    seedless_config_visuals_t visuals;
    seedless_video_preferences_t video;
    seedless_config_firmware_t firmware;
    seedless_config_user_t user;
    seedless_config_emulation_t emulation;
    char *selected_path;
} seedless_config_t;
typedef struct seedless_runtime_config {
    char *rom_path;
    char *config_path;
    char *bios_arm7;
    char *bios_arm9;
    char *firmware_path;
    char *save_dir;
    char *save_path;
} seedless_runtime_config_t;
typedef struct seedless_config_error {
    int code;
    int system_errno;
    char key[96];
    char message[256];
} seedless_config_error_t;

typedef struct seedless_launch_options seedless_launch_options_t;
void seedless_config_init(seedless_config_t *config);
void seedless_config_free(seedless_config_t *config);
void seedless_runtime_config_free(seedless_runtime_config_t *runtime);
int seedless_config_load_or_create(seedless_config_t *config, const char *override_path,
                                   seedless_config_error_t *error);
int seedless_config_resolve(seedless_config_t *config, const char *rom_path,
                            const seedless_launch_options_t *options,
                            seedless_runtime_config_t *runtime,
                            seedless_config_error_t *error);
#endif
