#define _GNU_SOURCE
#include "core_config.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>
#include <seedlessds/linux_config.h>

static int save_lock_fd = -1;
static int save_failed;
static uint64_t config_bits;

int seedless_core_config_apply(const seedless_config_t *c,
                               const seedless_runtime_config_t *r,
                               char *error, size_t error_size)
{
    if (!c || !r || !r->save_path) {
        snprintf(error, error_size, "invalid core configuration");
        return 0;
    }
    char *root = strdup(r->config_path);
    if (!root) { snprintf(error, error_size, "out of memory"); return 0; }
    char *slash = strrchr(root, '/');
    if (slash == root) slash[1] = 0; else if (slash) *slash = 0;
    else { free(root); root = strdup("."); }
    if (!root || setenv("SEEDLESS_ROOT", root, 1) != 0) {
        snprintf(error, error_size, "cannot set private core data root: %s", strerror(errno));
        free(root); return 0;
    }
    free(root);
    char *lock_path = malloc(strlen(r->save_path) + 6);
    if (!lock_path) { snprintf(error, error_size, "out of memory"); return 0; }
    sprintf(lock_path, "%s.lock", r->save_path);
    save_lock_fd = open(lock_path, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    free(lock_path);
    if (save_lock_fd < 0 || flock(save_lock_fd, LOCK_EX | LOCK_NB) != 0) {
        int e = errno;
        if (save_lock_fd >= 0) close(save_lock_fd);
        save_lock_fd = -1;
        snprintf(error, error_size, "save is already in use or cannot be locked: %s", strerror(e));
        return 0;
    }
    nds_linux_config_paths_t paths = {
        .struct_size = sizeof paths,
        .bios_arm7 = r->bios_arm7,
        .bios_arm9 = r->bios_arm9,
        .firmware_path = r->firmware_path,
        .save_path = r->save_path,
        .nickname = c->user.nickname_utf16,
        .nickname_units = c->user.nickname_units,
        .color = c->user.color,
        .birthday_month = c->user.birthday_month,
        .birthday_day = c->user.birthday_day
    };
    if (!nds_linux_config_set_paths(&paths)) {
        snprintf(error, error_size, "core rejected explicit configuration paths");
        seedless_core_config_reset();
        return 0;
    }
    config_bits = (uint64_t)c->visuals.thread_count << 16;
    config_bits |= (uint64_t)!!c->visuals.threaded_3d << 28;
    config_bits |= (uint64_t)!!c->visuals.auto_trim << 36;
    config_bits |= (uint64_t)!!c->visuals.fix_main_screen << 35;
    config_bits |= (uint64_t)!!c->visuals.disable_edge_marking << 40;
    config_bits |= UINT64_C(1) << 39; /* real-time RTC */
    config_bits |= UINT64_C(1) << 50; /* raw .sav format */
    save_failed = 0;
    return 1;
}
uint64_t seedless_core_config_bits(void) { return config_bits; }
void seedless_core_config_reset(void) {
    if (save_lock_fd >= 0) { (void)flock(save_lock_fd, LOCK_UN); close(save_lock_fd); save_lock_fd = -1; }
    nds_linux_config_reset();
    save_failed = 0; config_bits = 0;
}
int seedless_core_config_save_error(void) {
    if (nds_linux_config_save_failed()) save_failed = 1;
    return save_failed;
}
void seedless_core_config_set_core(nds_t *core) { (void)core; }
