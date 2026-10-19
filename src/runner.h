#ifndef SEEDLESSDS_LINUX_RUNNER_H
#define SEEDLESSDS_LINUX_RUNNER_H

#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <seedlessds/nds.h>
#include "video/sdl_video.h"
#include "platform/linux/core_video_bridge.h"

typedef enum seedless_run_state {
    SEEDLESS_RUN_VALIDATED,
    SEEDLESS_RUN_INITIALIZED,
    SEEDLESS_RUN_RUNNING,
    SEEDLESS_RUN_STOPPING,
    SEEDLESS_RUN_STOPPED,
    SEEDLESS_RUN_FAILED
} seedless_run_state_t;

typedef enum seedless_run_outcome {
    SEEDLESS_OUTCOME_NONE,
    SEEDLESS_OUTCOME_COMPLETED,
    SEEDLESS_OUTCOME_INTERRUPTED,
    SEEDLESS_OUTCOME_FAILED
} seedless_run_outcome_t;

typedef struct seedless_run_session {
    nds_t *core;
    seedless_sdl_video_t video;
    seedless_video_frame_mailbox_t video_mailbox;
    seedless_core_video_bridge_t video_bridge;
    pthread_t worker;
    const char *rom_path;
    uint64_t requested_frames;
    uint64_t frames;
    int frame_limit_set;
    int worker_started;
    int control_started;
    int video_mailbox_started;
    int load_succeeded;
    sig_atomic_t signal_number;
    seedless_run_state_t state;
    seedless_run_outcome_t outcome;
    _Atomic int worker_done;
} seedless_run_session_t;

int seedless_runner_main(int argc, char **argv);
#endif
