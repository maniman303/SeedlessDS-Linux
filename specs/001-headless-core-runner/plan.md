# Implementation Plan: Headless Core Runner and Ubuntu Build Setup

**Branch**: `main` | **Date**: 2026-10-19 | **Spec**: [spec.md](spec.md)

**Input**: `specs/001-headless-core-runner/spec.md` and request for repeatable local tooling installation and an update-friendly Android portability analysis.

## Summary

Build a C command-line runner around the pinned SeedlessDS-Core using a frontend-owned Linux compatibility layer, CMake/Ninja, and native/cross presets. Preserve the upstream submodule and apply any necessary, narrowly scoped portability patches to an out-of-tree source staging directory. Provide `scripts/setup-ubuntu.sh` for contributors to install and verify tooling locally, including this Ubuntu WSL environment.

The public interface is less complete than its names suggest: `nds_create` ignores its platform argument, and `nds_run_frame` performs input bookkeeping rather than emulation. Run the blocking `nds_load_rom` dispatch on a worker and use the existing control interface to stop at a genuine frame boundary. See [research.md](research.md) for source evidence and patch decisions.

## Technical Context

**Language/Version**: GNU C11 and AArch64 GNU assembler; Bash for tooling; Python 3 standard library for integration checks.

**Primary Dependencies**: CMake >=3.28, Ninja, GCC/binutils native or `aarch64-linux-gnu`; POSIX threads, glibc, libm, libdl; core-pinned Lua 5.3.0, libarchive 3.8.9, unarr 1.1.1, LZMA SDK 9.20. Build a hash-pinned zlib from source for both configurations to avoid host/target library mixing. No SDL, GLES, OpenSL ES, JNI, or Android SDK required in this milestone.

**Linking Policy**: Statically link the core, Lua, archive libraries, zlib and their application-level transitive dependencies into the executable by default. Prefer static compiler support (`-static-libgcc` where compatible); avoid adding C++ runtime requirements to this C project. Keep glibc and necessary operating-system runtime components dynamically linked as the initial compatibility baseline, including libm/pthread/libdl where the target ABI exposes them separately. Full `-static` and a musl migration are not required for this milestone. No silent shared fallback: record explicit exceptions, reasons and runtime baselines. Size optimization must not undermine bundling or compatibility.

**Storage**: ROM input, writable per-run data root selected with `--data-dir`, dependency download cache, build artifacts, and validation records; no database.

**Testing**: CTest for CLI and Linux adapter contracts; Bash syntax/ShellCheck for setup; target ABI compile probes; ELF symbol/dependency audit; three real-device runs per build configuration and shutdown checks.

**Target Platform**: ARM64 glibc Linux runtime, including RG DS Plus class devices; Ubuntu 24.04 LTS and 26.04 LTS development hosts, native ARM64 or x86-64 cross-compilation, including WSL2. Device runtime compatibility must be verified against its libc/sysroot; an Ubuntu cross artifact is not automatically portable to older device distributions.

**Project Type**: CLI executable plus static core integration target.

**Performance Goals**: Exact requested completed-frame boundaries; ordinary shutdown within five seconds on the supported workload. No real-time emulation target.

**Constraints**: No audiovisual devices/services; source-built dependencies; no proprietary runtime assets distributed; no mutation of the tracked core checkout; actual core progress rather than synthetic frame counts; real hardware acceptance.

**Scale/Scope**: One runner process, one global core instance, one ROM per run; native and cross build configurations. No menus, display layouts, input devices, or emulator feature UI.

## Constitution Check

*Pre-research and post-design review: PASS with the explicit headless milestone scope below.*

| Principle | Design compliance |
| --- | --- |
| I. Linux and ARM64 first | ARM64 runtime is mandatory; no x86 emulation fallback substitutes for acceptance. |
| II. Portable frontend/display | This user-requested headless milestone has no windowing or interactive input. The core adapter isolates platform behavior; SDL and single/dual-screen presentation remain later work. This is a scoped deferral, not a replacement display architecture. |
| III. Integration boundary | CLI uses public `nds.h` and `control.h`; internal layout translations live only in the compatibility module. Core fixes are reviewable patches, never reimplemented emulation. |
| IV. Linux builds | Native ARM64 and x86-64 cross presets; Ubuntu setup and WSL2 instructions; target sysroot override. |
| V. Hardware and credentials | Real ARM64 acceptance records required; SSH keys/agent and local configuration; no committed credentials. |

No additional constitutional exception is introduced by the design. A device or ROM unavailable during implementation must be reported as outstanding acceptance work, not replaced with a passing host test.

## Project Structure

### Documentation (this feature)

```text
specs/001-headless-core-runner/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── cli.md
│   ├── build-and-setup.md
│   └── linux-core-adapter.md
└── checklists/requirements.md
```

`tasks.md` is generated by the subsequent speckit-tasks phase.

### Source Code (planned repository root)

```text
CMakeLists.txt
CMakePresets.json
cmake/
├── SeedlessCore.cmake
├── Dependencies.cmake
└── toolchains/aarch64-linux-gnu.cmake
src/
├── main.c
├── runner.c
├── runner.h
└── platform/linux/
    ├── platform.c
    ├── files.c
    ├── memory.c
    ├── headless_audio.c
    ├── headless_video.c
    └── compat.h
patches/seedlessds-core/
├── manifest.json
└── *.patch
scripts/
├── setup-ubuntu.sh
├── prepare-core.sh
└── validate-headless.sh
tests/
├── abi/
├── adapters/
└── integration/
external/SeedlessDS-Core/    # pinned upstream checkout, untouched
build/                     # ignored source staging, cache, artifacts, evidence
```

**Structure Decision**: A small single executable with separate core/platform targets. Use existing module source lists where possible; create the expected `recon_fn` target and replace a reviewed list of Android/presentation sources. Never glob the entire core or infer removals using broad filename matches. Account explicitly for upstream source-list changes during updates.

## Phase 0: Research and Decisions

Completed in [research.md](research.md). Decisions cover Ubuntu prerequisites, dependency isolation, callback limitations, blocking execution, Android file/audio/GL/memory coupling, glibc ABI hazards, and patch maintenance. No unresolved design clarification remains. Compiler and device probes remain implementation validation, not assumed successes.

## Phase 1: Design

Contracts and state model are in [data-model.md](data-model.md) and [contracts/](contracts/). The end-to-end validation guide is [quickstart.md](quickstart.md).

Execution order: validate CLI and data root; create the platform/core; set neutral input; initialize control tracking and target before launching the worker; run `nds_load_rom` on that worker. Main observes mutex-protected control status. At target frames and acknowledged pause, request quit, join worker, capture status, end control tracking, and destroy remaining resources. Never call `nds_run_frame` in a loop to simulate progress. A signal handler only records `sig_atomic_t`; main performs core shutdown. Read lifecycle flags with appropriate atomic synchronization.

Disable presentation and audio device operations at source-selection boundaries while retaining rasterization, buffers, emulated SPU, capture/control counters needed by core semantics, and real synchronization. GL entry points used as boot sentinels require an explicit invariant-preserving translation, not arbitrary fake success.

## Implementation Work Packages (input to task generation)

1. **Tooling first**: Implement `scripts/setup-ubuntu.sh` per its contract, with `--native`, `--cross`, `--check`, and `--dry-run`. Include complete local installation commands for CMake, Ninja, compilers, debugger, verification tools, certificates, Git, and archive utilities. On this x86-64 Ubuntu host run dry-run, then have the contributor run cross installation locally; verify installed versions, target triple, and a linked AArch64 probe. Repeat setup and verification. Do not silently change apt repositories or install Android tools.
2. **Dependency/build foundation**: Add CMake/presets/toolchain, pinned source dependency preparation including zlib, native/cross isolation and configurable device sysroot; preserve Lua's core-specific ABI options. Exercise CMake 3.x and 4.x dependency policy compatibility. Install runner plus attribution notices. Link application dependencies statically; add a final ELF dependency allowlist and fail on accidental application-library shared dependencies. Prefer static compiler support where safe, document any exceptions, and check transitive libraries and explicit runtime loading as well as DT_NEEDED. Retain the target-compatible glibc runtime and build against a suitable device sysroot rather than assuming bundling removes minimum-glibc requirements.
3. **Portability staging and inventory**: Record core revision and expected source membership; stage sources out of tree; create explicit platform/presentation exclusions; run ABI probes and link-symbol inventory before declaring the integration complete.
4. **Adapters and minimal patches**: Implement POSIX descriptor bridge, real shared-memory mapping, logging, headless audio/mic and presentation contracts. Patch platform registration and any confirmed glibc layout/boot/synchronization hazards. Keep each patch separately described, checked, and reproducible; retain Android behavior for upstream builds.
5. **Runner lifecycle**: Implement CLI, data-root preparation, runtime asset diagnostics, control target setup before worker launch, exact-frame completion, interruption, worker-result propagation, progress-watchdog diagnostics, and cleanup. Validation must impose a finite startup/run timeout so a core that never reaches a boundary cannot hang the test indefinitely; a timeout is a failed run, never a completed one. Disable unintended autosave/load/capture paths in the default configuration.
6. **Verification and documentation**: Add meaningful adapter/CLI/ABI checks; native and cross builds, final ELF audit, source-checkout cleanliness, repeatable setup checks, and real-device runs on a target without separately installed application libraries beyond declared exceptions. Record the runtime dependency inventory, minimum symbol versions and static-link exceptions. Record BIOS/firmware requirements discovered by actual boot and test ROM checksum. Update README with runnable instructions and core-update procedure.

Work packages 2–5 depend on prerequisite verification; ABI and real-frame validation are release gates. Exact task IDs belong in the next tasks phase.

## Complexity Tracking

No unjustified gate violations. A staging/patch mechanism and narrow compatibility layer are warranted by hard-coded Android calls and reconstructed ABI layouts; a callback-only integration cannot address them. Avoid a broad Android runtime emulator or an independently maintained core fork.
