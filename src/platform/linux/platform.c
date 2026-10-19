#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include "compat.h"
#include "frontend/video_out_gl.h"

extern void platform_set(const nds_platform_t *platform);

static nds_platform_t linux_platform;
static int linux_platform_ready;
#define SYNC_SLOTS 128
typedef struct mutex_slot { const void *key; pthread_mutex_t value; } mutex_slot_t;
typedef struct cond_slot { const void *key; pthread_cond_t value; } cond_slot_t;
static mutex_slot_t mutex_slots[SYNC_SLOTS];
static cond_slot_t cond_slots[SYNC_SLOTS];
static pthread_mutex_t slot_guard = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t *mutex_for(const void *key, int create)
{
    pthread_mutex_t *free_slot = NULL;
    pthread_mutex_lock(&slot_guard);
    for (unsigned i = 0; i < SYNC_SLOTS; ++i) {
        if (mutex_slots[i].key == key) { pthread_mutex_unlock(&slot_guard); return &mutex_slots[i].value; }
        if (!mutex_slots[i].key && !free_slot) free_slot = &mutex_slots[i].value;
    }
    if (create && free_slot) {
        mutex_slot_t *slot = (mutex_slot_t *)((char *)free_slot - offsetof(mutex_slot_t, value));
        if (pthread_mutex_init(free_slot, NULL) == 0) slot->key = key;
        else free_slot = NULL;
    } else free_slot = NULL;
    pthread_mutex_unlock(&slot_guard);
    return free_slot;
}

static pthread_cond_t *cond_for(const void *key, int create)
{
    pthread_cond_t *free_slot = NULL;
    pthread_mutex_lock(&slot_guard);
    for (unsigned i = 0; i < SYNC_SLOTS; ++i) {
        if (cond_slots[i].key == key) { pthread_mutex_unlock(&slot_guard); return &cond_slots[i].value; }
        if (!cond_slots[i].key && !free_slot) free_slot = &cond_slots[i].value;
    }
    if (create && free_slot) {
        cond_slot_t *slot = (cond_slot_t *)((char *)free_slot - offsetof(cond_slot_t, value));
        if (pthread_cond_init(free_slot, NULL) == 0) slot->key = key;
        else free_slot = NULL;
    } else free_slot = NULL;
    pthread_mutex_unlock(&slot_guard);
    return free_slot;
}

static int linux_mutex_init(void *key) { return mutex_for(key, 1) ? 0 : ENOMEM; }
static int linux_mutex_destroy(void *key)
{
    int result = EINVAL;
    pthread_mutex_lock(&slot_guard);
    for (unsigned i = 0; i < SYNC_SLOTS; ++i) if (mutex_slots[i].key == key) {
        result = pthread_mutex_destroy(&mutex_slots[i].value);
        if (key == video_out_gl_state.mutex)
            fprintf(stderr, "seedlessds[core-video-sync]: mutex_destroy key=%p slot=%u result=%d\n", key, i, result);
        if (!result) mutex_slots[i].key = NULL;
        break;
    }
    pthread_mutex_unlock(&slot_guard);
    return result;
}
static int linux_mutex_lock(void *key)
{
    pthread_mutex_t *m = mutex_for(key, 0);
    int result = m ? pthread_mutex_lock(m) : EINVAL;
    if (key == video_out_gl_state.mutex && result != 0)
        fprintf(stderr, "seedlessds[core-video-sync]: platform_lock_failed key=%p registered=%s result=%d\n", key, m ? "yes" : "no", result);
    return result;
}
static int linux_mutex_unlock(void *key) { pthread_mutex_t *m = mutex_for(key, 0); return m ? pthread_mutex_unlock(m) : EINVAL; }
static int linux_cond_init(void *key) { return cond_for(key, 1) ? 0 : ENOMEM; }
static int linux_cond_destroy(void *key)
{
    int result = EINVAL;
    pthread_mutex_lock(&slot_guard);
    for (unsigned i = 0; i < SYNC_SLOTS; ++i) if (cond_slots[i].key == key) {
        result = pthread_cond_destroy(&cond_slots[i].value);
        if (!result) cond_slots[i].key = NULL;
        break;
    }
    pthread_mutex_unlock(&slot_guard);
    return result;
}
static int linux_cond_wait(void *cond_key, void *mutex_key)
{
    pthread_cond_t *cond = cond_for(cond_key, 0);
    pthread_mutex_t *mutex = mutex_for(mutex_key, 0);
    return cond && mutex ? pthread_cond_wait(cond, mutex) : EINVAL;
}
static int linux_cond_signal(void *key) { pthread_cond_t *c = cond_for(key, 0); return c ? pthread_cond_signal(c) : EINVAL; }
static int linux_cond_broadcast(void *key) { pthread_cond_t *c = cond_for(key, 0); return c ? pthread_cond_broadcast(c) : EINVAL; }


static uint64_t linux_now_us(void *user)
{
    struct timespec ts;
    (void)user;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * UINT64_C(1000000) + (uint64_t)ts.tv_nsec / 1000u;
}
static int64_t linux_now_seconds(void *user) { (void)user; return (int64_t)time(NULL); }
static void linux_wall_clock(void *user, nds_wall_clock_t *out)
{
    time_t now = time(NULL);
    struct tm value;
    (void)user;
    if (!localtime_r(&now, &value)) { memset(out, 0, sizeof *out); return; }
    out->year = value.tm_year + 1900; out->month = value.tm_mon + 1;
    out->day = value.tm_mday; out->weekday = value.tm_wday;
    out->hour = value.tm_hour; out->minute = value.tm_min; out->second = value.tm_sec;
}

static void linux_log(void *user, int level, const char *message)
{
    static const char *const names[] = {"debug", "info", "warning", "error"};
    (void)user;
    if (level < 0 || level > 3) level = 2;
    fprintf(stderr, "seedlessds[%s]: %s\n", names[level], message ? message : "");
}

const nds_platform_t *seedless_linux_platform(void)
{
    if (!linux_platform_ready) {
        linux_platform = *nds_platform_default();
        linux_platform.name = "linux-sdl2-fullscreen";
        linux_platform.log = linux_log;
        linux_platform.time.now_us = linux_now_us;
        linux_platform.time.now_seconds = linux_now_seconds;
        linux_platform.time.wall_clock = linux_wall_clock;
        linux_platform.threads.mutex_init = linux_mutex_init;
        linux_platform.threads.mutex_destroy = linux_mutex_destroy;
        linux_platform.threads.mutex_lock = linux_mutex_lock;
        linux_platform.threads.mutex_unlock = linux_mutex_unlock;
        linux_platform.threads.cond_init = linux_cond_init;
        linux_platform.threads.cond_destroy = linux_cond_destroy;
        linux_platform.threads.cond_wait = linux_cond_wait;
        linux_platform.threads.cond_signal = linux_cond_signal;
        linux_platform.threads.cond_broadcast = linux_cond_broadcast;
        linux_platform_ready = 1;
    }
    return &linux_platform;
}

void seedless_linux_fatal_longjmp(int value)
{
    longjmp(seedlessds_linux_fatal_jump, value);
}
