#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "frontend/video_out_gl.h"
#include "seedlessds/platform.h"
#include "core_internals.h"

video_out_gl_t video_out_gl_state;
gl_default_program_t gl_default_program_state;
const gl_recipe_tag_t gl_recipe_tags[VIDEO_OUT_GL_RECIPE_TAG_COUNT];

int video_out_gl_context_init(void)
{
    video_out_gl_t *state = &video_out_gl_state;
    void *pages = NULL;
    memset(state, 0, sizeof *state);
    if (posix_memalign(&pages, 0x40, 0x300000u) != 0)
        return ENOMEM;
    memset(pages, 0, 0x300000u);
    state->page_memory = pages;
    state->page[0] = pages;
    state->page[1] = (uint8_t *)pages + 0x180000u;
    state->bits_per_pixel = 32;
    state->gl_type = 0x1401u;
    state->gl_format = 0x1908u;
    for (unsigned i = 0; i < 2; ++i) {
        state->screen_width[i] = 256;
        state->screen_height[i] = VIDEO_OUT_GL_DEFAULT_SCREEN_HEIGHT;
    }
    const nds_platform_t *platform = nds_platform_default();
    if (platform->threads.mutex_init(state->mutex) != 0) {
        free(pages);
        memset(state, 0, sizeof *state);
        return errno ? errno : EINVAL;
    }
    if (platform->threads.cond_init(state->cond) != 0) {
        platform->threads.mutex_destroy(state->mutex);
        free(pages);
        memset(state, 0, sizeof *state);
        return errno ? errno : EINVAL;
    }
    return 0;
}

void video_out_gl_set_texture_format(int32_t bpp)
{
    video_out_gl_state.bits_per_pixel = bpp;
    video_out_gl_state.gl_type = bpp == 16 ? 0x8363u : 0x1401u;
    video_out_gl_state.gl_format = bpp == 16 ? 0x1907u : 0x1908u;
}
void *video_out_gl_clear_framebuffers(void *arg) { return arg; }
void video_out_gl_clear_screens(unsigned a, unsigned b) { (void)a; (void)b; }
void video_out_gl_draw_screen_rect2(uint32_t a, uint32_t b) { (void)a; (void)b; }
void video_out_gl_draw_screen_rect4(uint32_t a, uint32_t b) { (void)a; (void)b; }
void video_out_gl_call_renderer_object_slot0(int a,int b,int c,int d,int e,int f) {(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;}
void video_out_gl_call_renderer_object_slot1(int a,int b,int c,int d,int e,int f) {(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;}
void video_out_gl_render_frame(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t f,uint32_t g,uint32_t h,uint32_t i,uint32_t j) {(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;(void)j;}
void video_out_gl_render_single_screen(uint64_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t f) {(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;}
int32_t video_out_gl_load_shader_recipe_slot0(const char *a,uint32_t b,uint32_t c) {(void)a;(void)b;(void)c;return -1;}
int32_t video_out_gl_load_shader_recipe_slot1(const char *a,uint32_t b,uint32_t c) {(void)a;(void)b;(void)c;return -1;}

void video_out_gl_init_hook_noop(void) { }
void video_out_gl_renderer_release(void *renderer, int32_t release) { (void)renderer; (void)release; }
void platform_config_record_reset(void) { }

void seedless_linux_video_shutdown(void)
{
    const nds_platform_t *platform = nds_platform_default();
    if (video_out_gl_state.page_memory) free(video_out_gl_state.page_memory);
    platform->threads.cond_destroy(video_out_gl_state.cond);
    platform->threads.mutex_destroy(video_out_gl_state.mutex);
    memset(&video_out_gl_state, 0, sizeof video_out_gl_state);
}
