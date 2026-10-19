#define _GNU_SOURCE
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *base_prefix =
    "{\"visuals\":{\"threaded_3d\":false,\"thread_count\":0,\"disable_edge_marking\":false,\"fix_main_screen\":false,\"auto_trim\":false},"
    "\"firmware\":{\"bios_arm7\":\"/bios/a7\",\"bios_arm9\":\"/bios/a9\",\"firmware_path\":\"\"},"
    "\"user\":{\"nickname\":\"TEST\",\"birthday_month\":3,\"birthday_day\":2,\"color\":0},"
    "\"emulation\":{\"save_dir\":\"\"}";

static void write_config(const char *path, const char *video)
{
    FILE *file = fopen(path, "wb");
    assert(file);
    fprintf(file, "%s%s}\n", base_prefix, video ? video : "");
    assert(fclose(file) == 0);
}

static int load(const char *path, seedless_config_t *config,
                seedless_config_error_t *error)
{
    seedless_config_init(config);
    return seedless_config_load_or_create(config, path, error);
}

int main(void)
{
    char directory[] = "/tmp/seedless-video-config.XXXXXX";
    assert(mkdtemp(directory));
    char path[512];
    snprintf(path, sizeof path, "%s/config.json", directory);
    seedless_config_t config = {0};
    seedless_config_error_t error = {0};

    static const char *const modes[] = {"auto", "dual_screen", "single_screen"};
    static const char *const layouts[] = {"vertical", "horizontal"};
    for (size_t mode = 0; mode < 3; ++mode) {
        for (int swap = 0; swap <= 1; ++swap) {
            for (size_t layout = 0; layout < 2; ++layout) {
                char video[192];
                snprintf(video, sizeof video,
                         ",\"video\":{\"display_mode\":\"%s\",\"swap_screens\":%s,\"single_screen_layout\":\"%s\"}",
                         modes[mode], swap ? "true" : "false", layouts[layout]);
                write_config(path, video);
                assert(load(path, &config, &error));
                assert(config.video.requested_mode == (seedless_video_requested_mode_t)mode);
                assert(config.video.swap_screens == swap);
                assert(config.video.single_layout == (seedless_video_single_layout_t)layout);
                assert(config.visuals.thread_count == 0);
                seedless_config_free(&config);
            }
        }
    }

    write_config(path, ",\"video\":{}");
    assert(load(path, &config, &error));
    assert(config.video.requested_mode == SEEDLESS_VIDEO_MODE_AUTO);
    assert(!config.video.swap_screens);
    assert(config.video.single_layout == SEEDLESS_VIDEO_LAYOUT_VERTICAL);
    seedless_config_free(&config);

    write_config(path, NULL);
    assert(load(path, &config, &error));
    assert(config.video.requested_mode == SEEDLESS_VIDEO_MODE_AUTO);
    seedless_config_free(&config);

    static const struct { const char *json; const char *key; } invalid[] = {
        {",\"video\":null", "video"},
        {",\"video\":{\"display_mode\":null}", "video.display_mode"},
        {",\"video\":{\"display_mode\":4}", "video.display_mode"},
        {",\"video\":{\"display_mode\":\"windowed\"}", "video.display_mode"},
        {",\"video\":{\"swap_screens\":\"yes\"}", "video.swap_screens"},
        {",\"video\":{\"single_screen_layout\":\"stacked\"}", "video.single_screen_layout"},
    };
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        write_config(path, invalid[i].json);
        assert(!load(path, &config, &error));
        assert(!strcmp(error.key, invalid[i].key));
        seedless_config_free(&config);
    }

    char contents_before[1024], contents_after[1024];
    write_config(path, ",\"video\":{}");
    FILE *file = fopen(path, "rb");
    assert(file && fgets(contents_before, sizeof contents_before, file));
    fclose(file);
    assert(load(path, &config, &error));
    seedless_config_free(&config);
    file = fopen(path, "rb");
    assert(file && fgets(contents_after, sizeof contents_after, file));
    fclose(file);
    assert(!strcmp(contents_before, contents_after));
    assert(strstr(contents_after, "\"visuals\""));
    assert(strstr(contents_after, "\"video\""));

    char home[512];
    snprintf(home, sizeof home, "%s/home", directory);
    assert(mkdir(home, 0700) == 0);
    const char *old_home = getenv("HOME");
    char *saved_home = old_home ? strdup(old_home) : NULL;
    assert(setenv("HOME", home, 1) == 0);
    char config_parent[768], default_path[896];
    snprintf(config_parent, sizeof config_parent, "%s/.config", home);
    assert(mkdir(config_parent, 0700) == 0);
    snprintf(config_parent, sizeof config_parent, "%s/.config/seedlessds", home);
    assert(mkdir(config_parent, 0700) == 0);
    snprintf(default_path, sizeof default_path, "%s/.config/seedlessds/config.json", home);
    file = fopen(default_path, "wb");
    assert(file && fputs("deliberately invalid default must not be read\n", file) >= 0);
    fclose(file);
    file = fopen(default_path, "rb");
    assert(file && fgets(contents_before, sizeof contents_before, file));
    fclose(file);
    write_config(path, ",\"video\":{\"display_mode\":\"single_screen\"}");
    assert(load(path, &config, &error));
    assert(config.video.requested_mode == SEEDLESS_VIDEO_MODE_SINGLE_SCREEN);
    assert(!strcmp(config.selected_path, path));
    seedless_config_free(&config);
    file = fopen(default_path, "rb");
    assert(file && fgets(contents_after, sizeof contents_after, file));
    fclose(file);
    assert(!strcmp(contents_before, contents_after));
    unlink(default_path);

    seedless_config_init(&config);
    assert(seedless_config_load_or_create(&config, NULL, &error));
    assert(config.video.requested_mode == SEEDLESS_VIDEO_MODE_AUTO);
    assert(!config.video.swap_screens);
    assert(config.video.single_layout == SEEDLESS_VIDEO_LAYOUT_VERTICAL);
    file = fopen(config.selected_path, "rb");
    assert(file);
    char generated[2048] = {0};
    assert(fread(generated, 1, sizeof generated - 1, file) > 0);
    fclose(file);
    assert(strstr(generated, "\"video\": {\"display_mode\": \"auto\", \"swap_screens\": false, \"single_screen_layout\": \"vertical\"}"));
    seedless_config_free(&config);
    if (saved_home) { assert(setenv("HOME", saved_home, 1) == 0); free(saved_home); }
    else unsetenv("HOME");

    unlink(path);
    char generated_path[768];
    snprintf(generated_path, sizeof generated_path, "%s/.config/seedlessds/config.json", home);
    unlink(generated_path);
    char generated_parent[768];
    snprintf(generated_parent, sizeof generated_parent, "%s/.config/seedlessds", home);
    rmdir(generated_parent);
    snprintf(generated_parent, sizeof generated_parent, "%s/.config", home);
    rmdir(generated_parent);
    rmdir(home);
    rmdir(directory);
    puts("video configuration acceptance passed");
    return 0;
}
