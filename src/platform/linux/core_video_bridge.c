#include "core_video_bridge.h"
#include <seedlessds/nds.h>
#include <seedlessds/platform.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "frontend/video_out_gl.h"

extern void (*recon_present_flip_hook)(const void *page, unsigned idx);

static seedless_core_video_bridge_t *active_bridge;
static _Atomic uint64_t next_sequence;

void seedless_core_video_bridge_trace(const char *point)
{
    fprintf(stderr,
            "seedlessds[core-video-sync]: point=%s mutex_key=%p page_memory=%p words=",
            point ? point : "unknown", (void *)video_out_gl_state.mutex,
            video_out_gl_state.page_memory);
    for (unsigned i = 0; i < VIDEO_OUT_GL_MUTEX_WORDS; ++i)
        fprintf(stderr, "%s%08x", i ? "," : "",
                video_out_gl_state.mutex[i]);
    fputc('\n', stderr);
}

static int fail(char *error, size_t capacity, const char *message)
{
    if (error && capacity) snprintf(error, capacity, "%s", message);
    return 0;
}

static void copy_completed_flip(const void *page, unsigned index)
{
    (void)page;
    (void)index;
    seedless_core_video_bridge_t *bridge = active_bridge;
    if (!bridge || !bridge->mailbox || !bridge->core) return;
    seedless_video_frame_pair_t *staging =
        seedless_video_mailbox_staging_pair(bridge->mailbox);
    if (!staging || !nds_screens_rgba((nds_t *)bridge->core,
                                      staging->top_rgba,
                                      staging->bottom_rgba)) {
        seedless_video_mailbox_set_failure(bridge->mailbox,
                                           "core RGBA screen conversion failed");
        return;
    }
    uint64_t sequence = atomic_fetch_add_explicit(&next_sequence, 1,
                                                  memory_order_relaxed) + 1;
    if (!seedless_video_mailbox_publish(bridge->mailbox, sequence))
        seedless_video_mailbox_set_failure(bridge->mailbox,
                                           "completed screen pair publication failed");
}

static int lock_core(char *error, size_t capacity,
                     const nds_platform_threads_t **threads)
{
    const nds_platform_t *platform = nds_platform_default();
    int result = platform && platform->threads.mutex_lock ?
        platform->threads.mutex_lock(video_out_gl_state.mutex) : ENOSYS;
    if (!platform || !platform->threads.mutex_lock ||
        !platform->threads.mutex_unlock || result != 0) {
        fprintf(stderr,
                "seedlessds[core-video-sync]: lock_failed result=%d reason=%s platform=%p lock=%p unlock=%p\n",
                result, strerror(result), (const void *)platform,
                platform ? (void *)platform->threads.mutex_lock : NULL,
                platform ? (void *)platform->threads.mutex_unlock : NULL);
        seedless_core_video_bridge_trace("lock-failed");
        return fail(error, capacity, "could not lock core video page mutex");
    }
    *threads = &platform->threads;
    return 1;
}

int seedless_core_video_bridge_attach(seedless_core_video_bridge_t *bridge,
                                      void *core,
                                      seedless_video_frame_mailbox_t *mailbox,
                                      char *error, size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    if (!bridge || !core || !mailbox || !mailbox->storage || bridge->attached)
        return fail(error, error_capacity, "invalid or already attached core video bridge");
    seedless_core_video_bridge_trace("attach-before-lock");
    if (!video_out_gl_state.page_memory)
        return fail(error, error_capacity, "core software screen buffers are not initialized");
    const nds_platform_threads_t *threads = NULL;
    if (!lock_core(error, error_capacity, &threads)) return 0;
    if (recon_present_flip_hook || active_bridge) {
        threads->mutex_unlock(video_out_gl_state.mutex);
        return fail(error, error_capacity, "another owner already uses the core presentation hook");
    }
    memset(bridge, 0, sizeof *bridge);
    bridge->core = core;
    bridge->mailbox = mailbox;
    bridge->attached = 1;
    active_bridge = bridge;
    atomic_store_explicit(&next_sequence, 0, memory_order_relaxed);
    recon_present_flip_hook = copy_completed_flip;
    threads->mutex_unlock(video_out_gl_state.mutex);
    seedless_core_video_bridge_trace("attach-complete");
    return 1;
}

int seedless_core_video_bridge_detach(seedless_core_video_bridge_t *bridge,
                                      char *error, size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    if (!bridge || !bridge->attached) return 1;
    seedless_core_video_bridge_trace("detach-before-lock");
    const nds_platform_threads_t *threads = NULL;
    if (!lock_core(error, error_capacity, &threads)) return 0;
    int owned = recon_present_flip_hook == copy_completed_flip &&
                active_bridge == bridge;
    if (owned) recon_present_flip_hook = NULL;
    if (active_bridge == bridge) active_bridge = NULL;
    bridge->attached = 0;
    bridge->core = NULL;
    bridge->mailbox = NULL;
    threads->mutex_unlock(video_out_gl_state.mutex);
    if (!owned)
        return fail(error, error_capacity,
                    "core presentation hook ownership changed before detach");
    seedless_core_video_bridge_trace("detach-complete");
    return 1;
}

void seedless_core_video_bridge_detach_stopped(seedless_core_video_bridge_t *bridge)
{
    if (!bridge || !bridge->attached) return;
    seedless_core_video_bridge_trace("detach-stopped-before-unhook");
    if (recon_present_flip_hook == copy_completed_flip)
        recon_present_flip_hook = NULL;
    if (active_bridge == bridge) active_bridge = NULL;
    bridge->attached = 0;
    bridge->core = NULL;
    bridge->mailbox = NULL;
    seedless_core_video_bridge_trace("detach-stopped-complete");
}
