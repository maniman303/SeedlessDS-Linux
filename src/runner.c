#define _GNU_SOURCE
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "runner.h"
#include "platform/linux/compat.h"
#include <seedlessds/control.h>
#include <seedlessds/nds.h>
#include <seedlessds/platform.h>

static volatile sig_atomic_t stop_signal;
static seedless_run_session_t run;

static void on_signal(int signal_number)
{
    stop_signal = signal_number;
}

static void *load_worker(void *unused)
{
    (void)unused;
    nds_rom_open_t open = { .path = run.rom_path, .state_slot = -1 };
    run.load_succeeded = nds_load_rom(run.core, &open);
    atomic_store_explicit(&run.worker_done, 1, memory_order_release);
    return NULL;
}

static int parse_limit(const char *text, uint64_t *out)
{
    uint64_t value = 0;
    if (!text || !*text) return 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p < '0' || *p > '9') return 0;
        unsigned digit = (unsigned)(*p - '0');
        if (value > (UINT64_MAX - digit) / 10u) return 0;
        value = value * 10u + digit;
    }
    if (value == 0) return 0;
    *out = value;
    return 1;
}

static void usage(FILE *stream)
{
    fprintf(stream, "Usage: seedlessds-headless [--frames N] [--data-dir DIR] [--] ROM\n");
}

int seedless_runner_main(int argc, char **argv)
{
    const char *data_dir = ".";
    uint64_t requested = 0;
    int have_frames = 0, have_data = 0, positional = 0, i, result = 1;
    const char *failure_operation = "load";
    char *canonical_rom = NULL, *canonical_data = NULL;
    struct stat st;
    nds_control_status_t status;
    struct sigaction action;

    memset(&run, 0, sizeof run);
    atomic_init(&run.worker_done, 0);
    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (!positional && strcmp(arg, "--help") == 0) { usage(stdout); return 0; }
        if (!positional && strcmp(arg, "--") == 0) { positional = 1; continue; }
        if (!positional && strcmp(arg, "--frames") == 0) {
            if (have_frames || ++i >= argc || !parse_limit(argv[i], &requested)) {
                usage(stderr); fprintf(stderr, "Invalid or duplicate --frames value\n"); return 2;
            }
            have_frames = 1; continue;
        }
        if (!positional && strcmp(arg, "--data-dir") == 0) {
            if (have_data || ++i >= argc || !*argv[i]) {
                usage(stderr); fprintf(stderr, "Invalid or duplicate --data-dir value\n"); return 2;
            }
            data_dir = argv[i]; have_data = 1; continue;
        }
        if (!positional && arg[0] == '-') {
            usage(stderr); fprintf(stderr, "Unknown option: %s\n", arg); return 2;
        }
        if (run.rom_path) { usage(stderr); fprintf(stderr, "Expected exactly one ROM path\n"); return 2; }
        run.rom_path = arg;
    }
    if (!run.rom_path) { usage(stderr); fprintf(stderr, "A ROM path is required\n"); return 2; }
    canonical_rom = realpath(run.rom_path, NULL);
    if (!canonical_rom || stat(canonical_rom, &st) != 0 || !S_ISREG(st.st_mode) || access(canonical_rom, R_OK) != 0) {
        fprintf(stderr, "ROM must be a readable regular file: %s\n", run.rom_path);
        free(canonical_rom); return 2;
    }
    canonical_data = realpath(data_dir, NULL);
    if (!canonical_data || stat(canonical_data, &st) != 0 || !S_ISDIR(st.st_mode) ||
        access(canonical_data, W_OK | X_OK) != 0) {
        fprintf(stderr, "Data directory must exist and be writable: %s\n", data_dir);
        free(canonical_rom); free(canonical_data); return 2;
    }
    run.rom_path = canonical_rom;
    run.requested_frames = requested;
    run.frame_limit_set = have_frames;
    run.state = SEEDLESS_RUN_VALIDATED;
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0 || page_size > 0x4000 || (0x4000 % page_size) != 0) {
        fprintf(stderr, "Unsupported host page size for core fixed mappings: %ld\n", page_size);
        free(canonical_rom); free(canonical_data); return 1;
    }
    if (setenv("SEEDLESS_ROOT", canonical_data, 1) != 0) {
        perror("SEEDLESS_ROOT");
        printf("result=failed frames=0 operation=initialize\n");
        free(canonical_rom); free(canonical_data); return 1;
    }

    memset(&action, 0, sizeof action);
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    run.core = nds_create(seedless_linux_platform(), 0, 0);
    if (!run.core) {
        fprintf(stderr, "Core initialization failed\n");
        printf("result=failed frames=0 operation=initialize\n");
        free(canonical_rom); free(canonical_data); return 1;
    }
    run.state = SEEDLESS_RUN_INITIALIZED;
    nds_control_begin(-1, 1, 1);
    run.control_started = 1;
    if (have_frames && !nds_control_run_to(requested)) {
        fprintf(stderr, "Could not arm frame target\n");
        failure_operation = "control";
        goto cleanup;
    }
    int thread_error = pthread_create(&run.worker, NULL, load_worker, NULL);
    if (thread_error != 0) {
        errno = thread_error;
        fprintf(stderr, "Could not create core worker\n");
        failure_operation = "worker";
        goto cleanup;
    }
    run.worker_started = 1;
    run.state = SEEDLESS_RUN_RUNNING;
    for (;;) {
        nds_control_status(&status);
        if (status.error) {
            fprintf(stderr, "Core runtime error: %d\n", status.error);
            failure_operation = "runtime";
            run.state = SEEDLESS_RUN_STOPPING;
            nds_quit(run.core);
            break;
        }
        if (stop_signal) {
            run.signal_number = stop_signal;
            run.state = SEEDLESS_RUN_STOPPING;
            nds_quit(run.core);
            if (atomic_load_explicit(&run.worker_done, memory_order_acquire)) break;
            usleep(10000);
            continue;
        }
        if (have_frames && status.paused && status.frames >= requested) {
            result = 0; run.outcome = SEEDLESS_OUTCOME_COMPLETED;
            run.state = SEEDLESS_RUN_STOPPING; nds_quit(run.core); break;
        }
        if (atomic_load_explicit(&run.worker_done, memory_order_acquire)) break;
        usleep(10000);
    }
    if (run.worker_started) {
        int join_error = pthread_join(run.worker, NULL);
        if (join_error != 0) {
            errno = join_error;
            perror("Could not join core worker");
            failure_operation = "worker-join";
            result = 1;
            run.outcome = SEEDLESS_OUTCOME_FAILED;
        }
    }
    nds_control_status(&status);
    run.frames = status.frames;
    run.signal_number = stop_signal;
    if (stop_signal) {
        run.outcome = SEEDLESS_OUTCOME_INTERRUPTED;
        printf("result=interrupted frames=%" PRIu64 " signal=%s\n", status.frames, stop_signal == SIGINT ? "SIGINT" : "SIGTERM");
    }
    else if (result == 0 && run.load_succeeded && run.outcome == SEEDLESS_OUTCOME_COMPLETED)
        printf("result=completed frames=%" PRIu64 " requested=%" PRIu64 "\n", status.frames, requested);
    else {
        run.outcome = SEEDLESS_OUTCOME_FAILED;
        run.state = SEEDLESS_RUN_FAILED;
        printf("result=failed frames=%" PRIu64 " operation=%s\n", status.frames, failure_operation);
    }
    if (run.control_started) nds_control_end();
    nds_destroy(run.core);
    run.core = NULL;
    run.state = run.outcome == SEEDLESS_OUTCOME_FAILED ? SEEDLESS_RUN_FAILED : SEEDLESS_RUN_STOPPED;
    free(canonical_rom); free(canonical_data);
    return stop_signal ? (stop_signal == SIGINT ? 130 : 143) : result;

cleanup:
    printf("result=failed frames=0 operation=%s\n", failure_operation);
    if (run.control_started) nds_control_end();
    nds_destroy(run.core);
    run.core = NULL;
    run.state = SEEDLESS_RUN_FAILED;
    free(canonical_rom); free(canonical_data);
    return 1;
}
