#define _GNU_SOURCE
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <link.h>
#include <sys/stat.h>
#include <ucontext.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "runner.h"
#include "config.h"
#include "launch_options.h"
#include "core_config.h"
#include "platform/linux/compat.h"
#include <seedlessds/control.h>
#include <seedlessds/nds.h>
#include <seedlessds/platform.h>

static volatile sig_atomic_t stop_signal;
static volatile sig_atomic_t runner_phase;
static seedless_run_session_t run;
static uintptr_t executable_base;
static uintptr_t executable_end;
static _Thread_local unsigned char fatal_signal_stack[64 * 1024];

static void install_thread_fatal_stack(void)
{
    stack_t stack = { .ss_sp = fatal_signal_stack, .ss_size = sizeof fatal_signal_stack, .ss_flags = 0 };
    if (sigaltstack(&stack, NULL) != 0) perror("sigaltstack");
}

static size_t append_text(char *buffer, size_t used, size_t capacity, const char *text)
{
    while (*text && used < capacity) buffer[used++] = *text++;
    return used;
}

static size_t append_uint(char *buffer, size_t used, size_t capacity, unsigned value)
{
    char digits[16];
    size_t count = 0;
    do { digits[count++] = (char)('0' + value % 10u); value /= 10u; } while (value && count < sizeof digits);
    while (count && used < capacity) buffer[used++] = digits[--count];
    return used;
}

static size_t append_hex(char *buffer, size_t used, size_t capacity, uintptr_t value)
{
    static const char digits[] = "0123456789abcdef";
    char reversed[2 * sizeof value];
    size_t count = 0;
    do { reversed[count++] = digits[value & 0xfu]; value >>= 4; } while (value && count < sizeof reversed);
    while (count && used < capacity) buffer[used++] = reversed[--count];
    return used;
}

static int remember_main_image(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)size;
    (void)data;
    if (info->dlpi_name && info->dlpi_name[0]) return 0;
    executable_base = (uintptr_t)info->dlpi_addr;
    executable_end = executable_base;
    for (ElfW(Half) i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr) *header = &info->dlpi_phdr[i];
        if (header->p_type == PT_LOAD) {
            uintptr_t end = executable_base + (uintptr_t)header->p_vaddr + (uintptr_t)header->p_memsz;
            if (end > executable_end) executable_end = end;
        }
    }
    return 1;
}

static const char *fatal_signal_code_name(int number, int code)
{
    if (number == SIGBUS) {
        if (code == BUS_ADRALN) return "BUS_ADRALN";
        if (code == BUS_ADRERR) return "BUS_ADRERR";
        if (code == BUS_OBJERR) return "BUS_OBJERR";
    }
    if (number == SIGSEGV) {
        if (code == SEGV_MAPERR) return "SEGV_MAPERR";
        if (code == SEGV_ACCERR) return "SEGV_ACCERR";
    }
    return "unknown";
}

static uintptr_t parse_map_hex(const char *text, size_t *cursor)
{
    uintptr_t value = 0;
    size_t i = *cursor;
    for (;;) {
        unsigned digit;
        char ch = text[i];
        if (ch >= '0' && ch <= '9') digit = (unsigned)(ch - '0');
        else if (ch >= 'a' && ch <= 'f') digit = (unsigned)(ch - 'a') + 10u;
        else if (ch >= 'A' && ch <= 'F') digit = (unsigned)(ch - 'A') + 10u;
        else break;
        value = (value << 4) | digit;
        ++i;
    }
    *cursor = i;
    return value;
}

static size_t append_pc_mapping(char *message, size_t used, size_t capacity, uintptr_t pc)
{
    char maps[16384];
    int fd = open("/proc/self/maps", O_RDONLY);
    if (fd < 0) return append_text(message, used, capacity, " pc_map=unavailable");
    ssize_t length = read(fd, maps, sizeof maps - 1);
    (void)close(fd);
    if (length <= 0) return append_text(message, used, capacity, " pc_map=unavailable");
    maps[length] = '\0';
    size_t cursor = 0;
    while (cursor < (size_t)length) {
        size_t line = cursor;
        size_t end_cursor;
        uintptr_t start = parse_map_hex(maps, &cursor);
        if (maps[cursor] != '-') goto next_line;
        ++cursor;
        end_cursor = cursor;
        uintptr_t end = parse_map_hex(maps, &cursor);
        if (cursor == end_cursor || maps[cursor] != ' ') goto next_line;
        if (pc >= start && pc < end) {
            used = append_text(message, used, capacity, " pc_map=0x");
            used = append_hex(message, used, capacity, start);
            used = append_text(message, used, capacity, "-0x");
            used = append_hex(message, used, capacity, end);
            used = append_text(message, used, capacity, " perms=");
            ++cursor;
            for (unsigned i = 0; i < 4 && maps[cursor] && maps[cursor] != '\n'; ++i, ++cursor)
                if (used < capacity) message[used++] = maps[cursor];
            return used;
        }
next_line:
        cursor = line;
        while (cursor < (size_t)length && maps[cursor] != '\n') ++cursor;
        if (cursor < (size_t)length) ++cursor;
    }
    return append_text(message, used, capacity, " pc_map=unmapped_or_unlisted");
}

static const char *fatal_signal_name(int number)
{
    switch (number) {
    case SIGSEGV: return "SIGSEGV";
    case SIGBUS: return "SIGBUS";
    case SIGILL: return "SIGILL";
    case SIGFPE: return "SIGFPE";
    case SIGABRT: return "SIGABRT";
    default: return "SIGUNKNOWN";
    }
}

static const char *runner_phase_name(sig_atomic_t phase)
{
    switch (phase) {
    case 1: return "core-initialize";
    case 2: return "rom-load";
    case 3: return "emulation";
    default: return "runner";
    }
}

static void fatal_signal_handler(int number, siginfo_t *info, void *context)
{
    char message[768];
    size_t used = 0;
    used = append_text(message, used, sizeof message, "seedlessds[fatal]: signal=");
    used = append_text(message, used, sizeof message, fatal_signal_name(number));
    used = append_text(message, used, sizeof message, " code=");
    used = append_uint(message, used, sizeof message, info ? (unsigned)info->si_code : 0u);
    used = append_text(message, used, sizeof message, " code_name=");
    used = append_text(message, used, sizeof message,
                       fatal_signal_code_name(number, info ? info->si_code : 0));
    used = append_text(message, used, sizeof message, " address=0x");
    used = append_hex(message, used, sizeof message, info ? (uintptr_t)info->si_addr : 0u);
    used = append_text(message, used, sizeof message, " phase=");
    used = append_text(message, used, sizeof message, runner_phase_name(runner_phase));
#if defined(__aarch64__)
    if (context) {
        const ucontext_t *uc = (const ucontext_t *)context;
        uintptr_t pc = (uintptr_t)uc->uc_mcontext.pc;
        uintptr_t lr = (uintptr_t)uc->uc_mcontext.regs[30];
        uintptr_t sp = (uintptr_t)uc->uc_mcontext.sp;
        used = append_text(message, used, sizeof message, " pc=0x");
        used = append_hex(message, used, sizeof message, pc);
        used = append_text(message, used, sizeof message, " lr=0x");
        used = append_hex(message, used, sizeof message, lr);
        used = append_text(message, used, sizeof message, " sp=0x");
        used = append_hex(message, used, sizeof message, sp);
        used = append_text(message, used, sizeof message, " pc_mod4=");
        used = append_uint(message, used, sizeof message, (unsigned)(pc & 3u));
        used = append_text(message, used, sizeof message, " x16=0x");
        used = append_hex(message, used, sizeof message, (uintptr_t)uc->uc_mcontext.regs[16]);
        used = append_text(message, used, sizeof message, " x17=0x");
        used = append_hex(message, used, sizeof message, (uintptr_t)uc->uc_mcontext.regs[17]);
        used = append_text(message, used, sizeof message, " x1=0x");
        used = append_hex(message, used, sizeof message, (uintptr_t)uc->uc_mcontext.regs[1]);
        used = append_text(message, used, sizeof message, " x11=0x");
        used = append_hex(message, used, sizeof message, (uintptr_t)uc->uc_mcontext.regs[11]);
        used = append_text(message, used, sizeof message, " x28=0x");
        used = append_hex(message, used, sizeof message, (uintptr_t)uc->uc_mcontext.regs[28]);
        if (executable_end > executable_base && pc >= executable_base && pc < executable_end) {
            used = append_text(message, used, sizeof message, " image_offset=0x");
            used = append_hex(message, used, sizeof message, pc - executable_base);
        } else {
            used = append_text(message, used, sizeof message, " pc_outside_main_image=1");
        }
        used = append_pc_mapping(message, used, sizeof message, pc);
    }
#else
    (void)context;
#endif
    if (used < sizeof message) message[used++] = '\n';
    size_t written = 0;
    while (written < used) {
        ssize_t count = write(STDERR_FILENO, message + written, used - written);
        if (count <= 0) break;
        written += (size_t)count;
    }
    (void)kill(getpid(), number);
    _exit(128 + number);
}

static void install_fatal_signal_diagnostics(void)
{
    struct sigaction action;
    install_thread_fatal_stack();
    memset(&action, 0, sizeof action);
    action.sa_sigaction = fatal_signal_handler;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
    sigemptyset(&action.sa_mask);
    const int signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
    for (size_t i = 0; i < sizeof signals / sizeof signals[0]; ++i) {
        if (sigaction(signals[i], &action, NULL) != 0) perror("sigaction fatal diagnostic");
    }
    (void)dl_iterate_phdr(remember_main_image, NULL);
}

static void on_signal(int signal_number)
{
    stop_signal = signal_number;
}

static void *load_worker(void *unused)
{
    (void)unused;
    nds_rom_open_t open = { .path = run.rom_path, .config_bits = seedless_core_config_bits(), .state_slot = -1 };
    install_thread_fatal_stack();
    runner_phase = 2;
    run.load_succeeded = nds_load_rom(run.core, &open);
    fprintf(stderr, "seedlessds[info]: ROM load %s\n", run.load_succeeded ? "completed" : "failed");
    runner_phase = 3;
    atomic_store_explicit(&run.worker_done, 1, memory_order_release);
    return NULL;
}

static void usage(FILE *stream) { seedless_launch_usage(stream); }

int seedless_runner_main(int argc, char **argv)
{
    uint64_t requested = 0;
    int have_frames = 0, result = 1;
    const char *failure_operation = "load";
    char *canonical_rom = NULL;
    seedless_launch_options_t options;
    seedless_config_t config = {0};
    seedless_runtime_config_t runtime = {0};
    seedless_config_error_t config_error = {0};
    char launch_error[256] = {0}, core_error[256] = {0};
    struct stat st;
    nds_control_status_t status;
    struct sigaction action;

    memset(&run, 0, sizeof run);
    atomic_init(&run.worker_done, 0);
    if (!seedless_launch_parse(argc, argv, &options, launch_error, sizeof launch_error)) {
        usage(stderr);
        fprintf(stderr, "seedlessds[error]: %s\n", launch_error);
        printf("result=failed frames=0 operation=arguments\n");
        return 2;
    }
    if (options.help) { usage(stdout); return 0; }
    requested = options.frames;
    have_frames = options.have_frames;
    canonical_rom = realpath(options.rom_path, NULL);
    if (!canonical_rom || stat(canonical_rom, &st) != 0 || !S_ISREG(st.st_mode) || access(canonical_rom, R_OK) != 0) {
        fprintf(stderr, "seedlessds[error]: ROM must be a readable regular file: %s\n", options.rom_path);
        printf("result=failed frames=0 operation=rom\n");
        free(canonical_rom); return 2;
    }
    if (!seedless_config_load_or_create(&config, options.have_config ? options.config_path : NULL, &config_error)) {
        fprintf(stderr, "seedlessds[error]: config=%s key=%s: %s%s%s\n",
                config.selected_path ? config.selected_path : (options.config_path ? options.config_path : "<default>"),
                config_error.key, config_error.message,
                config_error.system_errno ? ": " : "",
                config_error.system_errno ? strerror(config_error.system_errno) : "");
        printf("result=failed frames=0 operation=config\n");
        free(canonical_rom); seedless_config_free(&config); return 1;
    }
    if (!seedless_config_resolve(&config, canonical_rom, &options, &runtime, &config_error)) {
        fprintf(stderr, "seedlessds[error]: config=%s key=%s: %s%s%s\n", config.selected_path,
                config_error.key, config_error.message,
                config_error.system_errno ? ": " : "",
                config_error.system_errno ? strerror(config_error.system_errno) : "");
        printf("result=failed frames=0 operation=%s\n",
               !strncmp(config_error.key, "firmware.", 9) ? "firmware" :
               !strncmp(config_error.key, "emulation.", 10) ? "save" : "config");
        free(canonical_rom); seedless_config_free(&config); seedless_runtime_config_free(&runtime);
        return 1;
    }
    free(canonical_rom); canonical_rom = NULL;
    if (!seedless_core_config_apply(&config, &runtime, core_error, sizeof core_error)) {
        fprintf(stderr, "seedlessds[error]: %s\n", core_error);
        seedless_core_config_reset(); seedless_config_free(&config); seedless_runtime_config_free(&runtime);
        return 1;
    }
    run.rom_path = runtime.rom_path;
    run.requested_frames = requested;
    run.frame_limit_set = have_frames;
    run.state = SEEDLESS_RUN_VALIDATED;
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0 || page_size > 0x4000 || (0x4000 % page_size) != 0) {
        fprintf(stderr, "Unsupported host page size for core fixed mappings: %ld\n", page_size);
        seedless_core_config_reset(); seedless_config_free(&config); seedless_runtime_config_free(&runtime); return 1;
    }

    memset(&action, 0, sizeof action);
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    install_fatal_signal_diagnostics();

    runner_phase = 1;
    run.core = nds_create(seedless_linux_platform(), 0, 0);
    if (!run.core) {
        fprintf(stderr, "Core initialization failed\n");
        printf("result=failed frames=0 operation=initialize\n");
        seedless_core_config_reset(); seedless_config_free(&config); seedless_runtime_config_free(&runtime); return 1;
    }
    fprintf(stderr, "seedlessds[info]: core initialization completed\n");
    run.state = SEEDLESS_RUN_INITIALIZED;
    runner_phase = 2;
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
    fprintf(stderr, "seedlessds[info]: ROM load started; bounded_frames=%s\n",
            have_frames ? "enabled" : "disabled");
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
    if (run.control_started) nds_control_end();
    nds_destroy(run.core);
    run.core = NULL;
    run.state = run.outcome == SEEDLESS_OUTCOME_FAILED ? SEEDLESS_RUN_FAILED : SEEDLESS_RUN_STOPPED;
    int save_error = seedless_core_config_save_error();
    if (save_error) {
        fprintf(stderr, "seedlessds[error]: operation=save: core reported a cartridge save write or flush failure\n");
        result = 1; failure_operation = "save"; run.outcome = SEEDLESS_OUTCOME_FAILED; run.state = SEEDLESS_RUN_FAILED;
    }
    if (save_error) {
        run.outcome = SEEDLESS_OUTCOME_FAILED;
        run.state = SEEDLESS_RUN_FAILED;
        printf("result=failed frames=%" PRIu64 " operation=save\n", status.frames);
    }
    else if (stop_signal) {
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
    seedless_core_config_reset();
    seedless_config_free(&config); seedless_runtime_config_free(&runtime);
    return save_error ? 1 : (stop_signal ? (stop_signal == SIGINT ? 130 : 143) : result);

cleanup:
    printf("result=failed frames=0 operation=%s\n", failure_operation);
    if (run.control_started) nds_control_end();
    nds_destroy(run.core);
    run.core = NULL;
    run.state = SEEDLESS_RUN_FAILED;
    if (seedless_core_config_save_error()) failure_operation = "save";
    seedless_core_config_reset(); seedless_config_free(&config); seedless_runtime_config_free(&runtime);
    return 1;
}
