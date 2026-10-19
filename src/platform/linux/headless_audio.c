#include <stdint.h>
#include <string.h>
#include "core_internals.h"
#include "frontend/audio_out.h"
#include "platform/android/audio.h"
#include "spu/spu.h"

platform_audio_t platform_audio_state;
int32_t audio_out_mode = 2;
uint32_t audio_out_enabled = 1;
float audio_out_volume = 4.0f;

void platform_audio_output_feed_frame(spu_t *spu) { (void)spu; }
void audio_out_set_mode_word(uint32_t value) { audio_out_mode = (int32_t)value; }
void audio_out_set_runtime_flag(uint32_t value) { (void)value; }
void audio_out_set_enabled_flag(uint32_t value) { audio_out_enabled = value; }
void audio_out_set_volume_level(int32_t level) { audio_out_volume = (float)(level < 0 ? 0 : level > 3 ? 3 : level); }
long audio_out_set_runtime_word(long value) { (void)value; return value; }
int audio_out_start(unsigned char *machine) { platform_audio_state.machine = (struct nds *)machine; return 0; }
int audio_out_stop_and_mute(spu_t *spu) { (void)spu; return 0; }
void audio_out_resume_after_stop(spu_t *spu, uint32_t token) { (void)spu; (void)token; }
void audio_out_flush_queues(void) { }
void audio_out_set_volume_db(uint32_t volume) { audio_out_volume = (float)(volume > 100 ? 100 : volume); }

void audio_out_channel_recalc(spu_t *spu, spu_channel_t *channel)
{
    uint8_t flags = channel->recalc_flags;
    if (flags & 2) {
        uint32_t control = channel->regs->cnt;
        uint32_t master = spu->regs->soundcnt & 0x7f;
        uint32_t volume = control & 0x7f;
        uint32_t divisor = (control >> 8) & 3;
        uint32_t shift = divisor == 3 ? 0 : 4 - divisor;
        if (volume == 0x7f) volume = 0x80;
        if (master == 0x7f) master = 0x80;
        uint32_t pan = (control >> 16) & 0x7f;
        uint32_t product = (master * volume) << shift;
        channel->volume_left = (uint16_t)((product * (pan ^ 0x7f)) >> 13);
        channel->volume_right = (uint16_t)((product * pan) >> 13);
    }
    if (flags & 1) {
        uint32_t timer = 0x10000u - channel->regs->tmr;
        uint32_t divisor = timer * spu->mixer.sample_rate;
        uint64_t step = divisor ? UINT64_C(0x01006f4300000000) / divisor : 0;
        channel->step = step;
        if (channel->capture_slot != 0xff)
            spu->capture[channel->capture_slot].step = step;
    }
    channel->recalc_flags = 0;
}

int audio_out_load_wav(spu_t *spu, const char *path)
{
    (void)spu;
    (void)path;
    return -1;
}

int mic_init_recorder(long context)
{
    spu_t *spu = (spu_t *)(uintptr_t)context;
    memset(spu->mic_buffer, 0, sizeof spu->mic_buffer);
    spu->mixer.mic_rate = 0;
    spu->mixer.mic_sample_bytes = 1;
    spu->mixer.mic_loaded = 1;
    spu->mixer.mic_init_failed = 1;
    return 0;
}
