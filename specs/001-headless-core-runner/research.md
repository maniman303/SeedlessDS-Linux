# Research: Linux Headless Integration

**Date**: 2026-10-19
**Inspected core revision**: `1cae6f9d8ad7dc651a48452a5ebe23fd8c347d12`

## 1. Build host and tooling

**Decision**: Support Ubuntu 24.04 and 26.04 LTS; use distro CMake >=3.28 and Ninja, GNU C11/AArch64 assembler, GCC native/cross, and Bash installation automation. The current local `/etc/os-release` reports Ubuntu 26.04.1; inspection found GCC but no CMake, Ninja, cross GCC, or ShellCheck on PATH. PowerShell is absent, so planning setup used the repository's equivalent Bash `setup-plan.sh --json`.

**Rationale**: Matches the Linux/WSL constitution and existing CMake module lists. Ubuntu lists CMake 3.28 for 24.04 and 4.2 for 26.04; old third-party CMake policies must therefore be checked with both, and patched only in staged dependency sources if necessary. Sources: [Ubuntu noble CMake](https://packages.ubuntu.com/noble/cmake), [Ubuntu resolute CMake](https://packages.ubuntu.com/resolute/cmake).

**Alternatives considered**: Android NDK would preserve Android coupling; a handwritten Make build would duplicate module knowledge. Requiring a global pip or third-party apt repository is unnecessary.

## 2. Build composition and dependency isolation

**Decision**: Own the root CMake project and create the `recon_fn` core target expected by upstream source lists. Consume reviewed modules and explicitly substitute platform/presentation implementations. Build pinned dependencies for the selected target, including a frontend-pinned zlib release with verified checksum. Use the core's pinned Lua and archive dependency versions/options; do not substitute Ubuntu Lua because upstream requires `LUA_32BITS`, `LUA_COMPAT_5_2`, and redirected fopen.

**Evidence**: `src/CMakeLists.txt` unconditionally includes `platform/android/CMakeLists.txt`; `third_party/CMakeLists.txt` is intended for add_subdirectory context, uses `CMAKE_CURRENT_SOURCE_DIR/lua`, fetches hash-checked archives, and links unarr to bare `z`. Lua's nested submodule is currently empty. Correct directory context and a target zlib resolution are required; a raw include from repository root is wrong.

**Rationale**: A source-built target dependency set avoids requiring Ubuntu ARM64 multiarch apt configuration on an amd64 host. Pin zlib URL/hash in the implementation, do not invent or copy an unverified hash. Keep package/tool verification separate from network dependency acquisition. CMake documents toolchain files and root-path selection for cross compilation: [CMake toolchains](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html).

**Alternatives considered**: Host system libraries risk architecture mixing; rewriting all upstream CMake lists increases update cost. Do not depend on a new target alias conflicting with a built-in `z` system-library lookup: explicitly wire target zlib to consumers in the wrapper/staged build.

## 3. Public platform callback limitations

**Decision**: Add a small patch making `nds_create` register the supplied platform via existing `platform_set`, with defined default/null and teardown behavior. Supply POSIX callbacks but also translate direct legacy dependencies.

**Evidence**: `include/seedlessds/platform.h` describes callbacks; `src/platform/platform.c` implements `platform_set`, but `src/core/nds_api.c:nds_create` explicitly discards its `platform` argument and always initializes GL state. Several modules call libc or platform globals directly; memory callback presence does not imply that mirror creation uses them.

**Alternatives considered**: Calling internal `platform_set` from the executable would work around registration but leak private APIs into the runner. A platform callback table alone leaves real Android coupling unresolved.

## 4. Real execution and exact frame limits

**Decision**: Run synchronous `nds_load_rom` on one worker. Before launching it, call `nds_control_begin(-1, 1, 0)` and, for a bounded run, `nds_control_run_to(N)` and verify success. Observe `nds_control_status` until `frames == N` and `paused != 0`, then request `nds_quit` and join. Use zero compose workers initially to minimize dependencies on presentation synchronization. Keep the normal emulation scheduler and software rendering active.

**Evidence**: `core/nds_api_rom.c:nds_load_rom` calls `nds_start_game`; `core/nds.c` enters JIT/scheduler dispatch. `core/nds_api.c:nds_run_frame` merely invokes input processing. Genuine boundaries flow from `core/sched.c` through GPU close and `frontend/input_record.c` to `input_poll_and_pause`. `debug/control.c` increments control frames and requests pause at target, provides mutex-protected status, and accepts a target while active with zero frames. No load-time `nds_control_begin` reset exists in the inspected core. Quit wakes input pause and the core's normal unwind path.

**Rationale**: Prearming the target avoids polling overshoot. Frame evidence must come from scheduler-triggered boundaries, never externally invoked input bookkeeping. Verify N=1 as well as N=120 to catch ordering errors. `nds_load_rom` returns a boolean derived from `nds_start_game`; retain separate runtime/stop reason so an unwind does not become a false success.

**Alternatives considered**: A naive run-frame loop falsely reports progress; polling and issuing quit at N without prearmed pause can overshoot. A new frame-step API is unnecessary unless implementation reveals an actual boundary defect.

## 5. Android coupling inventory and disposition

| Location | Observed coupling | Linux design |
| --- | --- | --- |
| `src/platform/android/JNI_OnLoad.c`, `JNI_OnUnload.c`, `files.c` | JNI types, Android startup and file access | Exclude; reuse/harden `src/platform/posix/files.c` descriptor bridge. |
| `src/platform/files.c` | Calls JNI-named descriptor helpers even outside Android | Keep calling convention and implement real POSIX semantics behind it; names do not require a JVM. |
| `src/platform/android/audio.c`, `frontend/audio_out.c`, `frontend/mic.c` | OpenSL object/interface vtables, queues and shared platform audio state | Replace selected device-dependent implementations with headless equivalents; preserve state and functions needed by emulation, drain/discard output, neutralize mic input, never create fake interface pointers. |
| `src/core/nds.c`, `src/memory/mirror.c` | Include Android-named `audio.h` for shared layout | Header itself uses ordinary C types; retain initially, preserve layout, document legacy name. Renaming is not needed for Linux execution. |
| `src/memory/mirror.c:mirror_shm_create` | `libandroid.so`, `ASharedMemory_create`, ashmem ioctls | Patch platform branch to Linux memfd plus ftruncate; preserve shared alias mapping semantics and error cleanup. Never return a fake fd. |
| `src/core/nds_api.c`, `nds_api_rom.c`, `frontend/video_out_gl.c`, `frontend/shaders.c`, API FX/video sources | GL initialization, framebuffer operations, GL symbol addresses and shader calls | Replace device presentation operations, retain required buffers/locks and software rasterization. Explicitly account for retained callers; exclude unreachable FX implementations or return unsupported, never pretend rendering happened. |
| `src/core/nds_api.c` and frontend boot checks | Stores obfuscated glViewport address as a boot word | Trace producer and consumer together; retain matching headless sentinel symbol or patch both to a named invariant. Removing the assignment alone is unsafe. |
| `src/gpu/gpu3d/gpu3d.c` | Direct `__android_log_print` symbol | Provide a narrowly scoped variadic stderr logger, with native priority mapping. |
| `src/platform/crash_report.c` | Android logging/properties under `__ANDROID__` | Leave Android disabled; provide only genuinely required crash hooks, preserve ordinary Linux failure status. |
| `src/frontend/video_out.c:script_io_init` | Lua module named `android` | Keep compatibility name; no Android runtime dependency in that string. |

**Decision**: Hybrid of Linux source replacements, legacy-symbol translation, and a few semantic patches. Do not remove Android support from upstream to make the Linux product Android-free.

**Alternatives considered**: Emulating JNI/OpenSL/GLES broadly adds an unnecessary platform runtime. Blind no-op stubs can hide required state, synchronization, memory mapping, or failure behavior.

## 6. glibc and architecture hazards

**Decision**: Compile target ABI probes before a successful build may be claimed. Patch confirmed mismatches at the narrowest boundary, preserving fixed core layout where required by offsets or assembly.

**Evidence**: `core/nds_state.h` reserves `fatal_jump[0x100]` and passes it to `_setjmp`/`longjmp`; target glibc size/alignment must be checked. `frontend/video_out_gl.h` reserves raw mutex/condition storage; compare target pthread sizes and alignments. `platform/files.c` forwards opaque stat buffers to libc; `core/state.c` uses a 0x80-byte buffer and reads mtime at offset 88. These are reconstructed ABI assumptions, not portable typed calls.

**Resolution design**: Use host-owned typed jump state accessed through core-specific helpers if the existing slot cannot safely hold target state, rather than enlarging a structure and silently moving assembly offsets. Any setjmp must execute in a still-live dispatch frame (directly or through an appropriate macro); a helper that returns before longjmp would be invalid. Use a documented legacy-stat translation or typed patch with verified consumers; never write native stat into an undersized buffer. For synchronization, use properly initialized native storage only when compile assertions prove fit/alignment; otherwise use adapter-owned objects keyed by legacy slot address and explicit lifetime. Do not globally interpose libc symbols to fix individual core buffers.

**Further mandatory probes**: All direct pthread calls and embedded storage, target page-size assumptions (core uses 16 KiB regions), fixed mappings/reservation ownership, allocation/protection errors, instruction-cache synchronization after JIT writes, executable permissions, libc aliases/fortify definitions, signal-context and jump behavior. Use a linked ELF undefined-symbol inventory, not keyword search alone.

**Alternatives considered**: Assuming Bionic and glibc ABI equivalence risks memory corruption. Disabling JIT is not a portability solution for a core whose runtime/build is AArch64-specific.

## 7. Core update strategy

**Decision**: Keep frontend adapters in this repository; maintain a core revision manifest and small ordered patches applied to `build/<preset>/core-src`. Manifest records base commit, patch hashes, excluded/replaced sources and rationale. Preparation verifies a clean base, copies without modifying source, checks every patch, and fails on drift; no fuzzy application or silent rewrite. Preserve core/dependency license notices.

**Rationale**: Updates remain an explicit compatibility review. Rebuild, rerun ABI/symbol checks and real-device acceptance after any core bump. Local modifications must be diagnosed, not discarded.

**Alternatives considered**: Editing the submodule in place obscures upstream status; a permanent fork expands maintenance scope. Linker wrapping every Android function conceals semantic mismatches.

## 8. Validation boundaries

**Decision**: Builds and smoke runs are implementation work. Planning performs source analysis only; no runtime compatibility has been proven. Initial reference builds use the Ubuntu ARM64 ABI. For devices with older glibc, use an explicit compatible toolchain/sysroot and validate dynamic symbol versions on hardware. QEMU may assist debugging but cannot satisfy real-hardware acceptance.

**Rationale**: This host currently lacks build prerequisites, and the test ROM/runtime assets and target device have not been identified in this session. Record those as implementation acceptance inputs, without inventing device details or bundling BIOS files.

## 9. Static linking within compatibility limits

**Decision**: Embed the core and all application libraries (Lua, archive libraries, LZMA, zlib, and their required application dependencies) as static archives in the final executable. Binary size is not a constraint. Prefer static GCC support where safe. Retain dynamically linked glibc and necessary OS runtime components initially; do not require full static glibc or change libc to musl for this milestone. An optional fully static configuration must pass the same ABI and device checks before it can be described as supported.

**Rationale**: The user explicitly prioritizes fewer deployment dependencies and compatibility over output size. Selective static linkage achieves the application-library goal while preserving the existing glibc target strategy. GCC supports selective compiler-runtime linkage independently of other libraries: [GCC link options](https://gcc.gnu.org/onlinedocs/gcc/Link-Options.html). Static glibc also needs deliberate evaluation of runtime loading and name-service behavior; it is not a universal compatibility switch: [glibc build configuration](https://sourceware.org/glibc/manual/latest/html_node/Configuring-and-compiling.html). These are design choices, not claims of measured compatibility.

**Validation**: Capture final ELF interpreter, DT_NEEDED and symbol-version requirements plus any retained dlopen paths. Permit only documented OS runtime libraries and explicit compatibility exceptions. Verify no shared core/Lua/archive/zlib libraries are needed. Test the unchanged executable on real ARM64 hardware without those application libraries installed. A missing static archive fails configuration unless an explicit exception is selected; do not silently use a shared library. Record source/license notices and static redistribution obligations alongside the artifact.

**Alternatives considered**: Full `-static` by default would add libc and dynamic-loading investigation before evidence that it improves this core's compatibility; musl would expand the existing ABI port. Shipping all libraries as adjacent shared objects does not meet the requested preference to include them in the executable. A mostly static build still requires a compatible glibc baseline/sysroot and kernel; it is not an arbitrary-Linux binary.
