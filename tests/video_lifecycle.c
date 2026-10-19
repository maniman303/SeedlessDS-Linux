#include "platform/linux/core_video_bridge.h"
#include <assert.h>
#include <seedlessds/nds.h>
#include <seedlessds/platform.h>
#include <stdio.h>
#include <string.h>
#include "frontend/video_out_gl.h"

extern void (*recon_present_flip_hook)(const void *page, unsigned idx);
void (*recon_present_flip_hook)(const void *page, unsigned idx);
video_out_gl_t video_out_gl_state;
gl_default_program_t gl_default_program_state;
const gl_recipe_tag_t gl_recipe_tags[VIDEO_OUT_GL_RECIPE_TAG_COUNT];

static unsigned core_lock_depth;
static int worker_running;
static int conversion_fails;
static uint8_t frame_stamp;

static int fake_lock(void *mutex) { (void)mutex; core_lock_depth++; return 0; }
static int fake_unlock(void *mutex) { (void)mutex; assert(core_lock_depth); core_lock_depth--; return 0; }
static nds_platform_t platform = {
    .name = "video-lifecycle-test",
    .threads = { .mutex_lock = fake_lock, .mutex_unlock = fake_unlock },
};

const nds_platform_t *nds_platform_default(void) { return &platform; }

int nds_screens_rgba(nds_t *core, void *top, void *bottom)
{
    (void)core;
    assert(core_lock_depth == 1);
    assert(worker_running);
    memset(top, frame_stamp, SEEDLESS_VIDEO_IMAGE_BYTES);
    memset(bottom, (uint8_t)~frame_stamp, SEEDLESS_VIDEO_IMAGE_BYTES);
    return !conversion_fails;
}

static void foreign_hook(const void *page, unsigned index)
{
    (void)page;
    (void)index;
}

int main(void)
{
    seedless_video_frame_mailbox_t mailbox = {0};
    seedless_core_video_bridge_t bridge = {0};
    char error[256] = {0};
    uint8_t page_marker = 0;
    int fake_core = 0;
    assert(seedless_video_mailbox_init(&mailbox));

    assert(!seedless_core_video_bridge_attach(&bridge, &fake_core, &mailbox,
                                               error, sizeof error));
    assert(!recon_present_flip_hook);
    video_out_gl_state.page_memory = &page_marker;
    recon_present_flip_hook = foreign_hook;
    assert(!seedless_core_video_bridge_attach(&bridge, &fake_core, &mailbox,
                                               error, sizeof error));
    assert(recon_present_flip_hook == foreign_hook);

    recon_present_flip_hook = NULL;
    assert(seedless_core_video_bridge_attach(&bridge, &fake_core, &mailbox,
                                              error, sizeof error));
    assert(bridge.attached && recon_present_flip_hook);
    worker_running = 1;
    frame_stamp = 73;
    fake_lock(video_out_gl_state.mutex);
    recon_present_flip_hook(&page_marker, 1);
    fake_unlock(video_out_gl_state.mutex);
    uint64_t sequence = 0;
    assert(seedless_video_mailbox_consume_latest(&mailbox, &sequence));
    assert(sequence == 1);
    const seedless_video_frame_pair_t *pair =
        seedless_video_mailbox_consumer_pair(&mailbox);
    assert(pair->top_rgba[0] == 73 && pair->bottom_rgba[0] == (uint8_t)~73u);

    conversion_fails = 1;
    fake_lock(video_out_gl_state.mutex);
    recon_present_flip_hook(&page_marker, 0);
    fake_unlock(video_out_gl_state.mutex);
    assert(seedless_video_mailbox_get_failure(&mailbox, error, sizeof error));
    assert(strstr(error, "conversion failed"));

    /* Detach happens only after the caller has quiesced its producer worker. */
    worker_running = 0;
    assert(seedless_core_video_bridge_detach(&bridge, error, sizeof error));
    assert(!bridge.attached && !recon_present_flip_hook);
    recon_present_flip_hook = foreign_hook;
    seedless_core_video_bridge_t competing = {0};
    assert(!seedless_core_video_bridge_attach(&competing, &fake_core, &mailbox,
                                               error, sizeof error));
    assert(recon_present_flip_hook == foreign_hook);
    recon_present_flip_hook = NULL;
    seedless_video_mailbox_destroy(&mailbox);
    puts("video bridge lifecycle/ownership acceptance passed");
    return 0;
}
