# Implementation Plan: Fullscreen Video Output

**Branch**: `main` (unchanged; feature context `004-fullscreen-video-output`) | **Date**: 2026-10-19 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/004-fullscreen-video-output/spec.md`

## Summary

Replace discarded host video with always-fullscreen SDL2 presentation. Keep two complete DS images visible, either on separate physical displays or in a single-display vertical/horizontal layout. Use a small synchronized core-to-frontend frame mailbox, main-thread SDL rendering, and explicit nearest-neighbor integer destination rectangles. Persist the three new preferences exclusively under `video`.

Choose SDL2 ≥2.0.12 for broad Linux ARM64 image compatibility, using the distribution's maintained version rather than treating the minimum as a device certification. Dual output is the primary implementation and hardware validation path. The device's actual SDL backend must pass an independent two-output probe; headless execution records cannot establish this.

**Execution prerequisite**: Feature 003 JSON configuration is not implemented in the current checkout. Its selection/default creation/validation, BIOS, profile, visual, and save behavior must land locally before this feature's configuration integration and end-to-end acceptance. Do not add a temporary parser or treat existing `--data-dir` as the completed configuration interface. Pure presentation components may be developed independently, but the feature cannot be marked complete with that dependency missing.

## Technical Context

**Language/Version**: C11 with existing GNU extensions; CMake ≥3.28, Ninja, Python 3.12+ tooling; existing ARM64 GCC toolchains.

**Primary Dependencies**: Pinned SeedlessDS-Core `1cae6f9d8ad7dc651a48452a5ebe23fd8c347d12`, existing maintained compatibility patches/dependencies, target SDL2 ≥2.0.12, pthreads, and the feature 003 configuration foundation. Shared SDL2 is an explicit new runtime dependency; SDL's platform graphics dependencies depend on the target backend.

**Storage**: Existing JSON configuration selection/default path; new `video` object with three members. Only fixed-size, ephemeral frame copies and SDL resources are added. No database or additional persistent file.

**Testing**: CTest on native ARM64; deterministic C tests for configuration, mode resolution, integer geometry and frame handoff; Python tests for modified inventory/device tooling; SDL integration probe using the real rendering path; physical ARM64 one/two-display acceptance. Pure tests may use software surfaces, but dummy video and screenshots alone do not establish physical fullscreen or dual-panel support.

**Target Platform**: ARM64 Linux, prioritizing RG DS Plus-class dual-screen devices, with generic single-display Linux support. Native Ubuntu ARM64 and amd64/WSL2 Ubuntu cross builds remain supported. No requirement to run the emulator on amd64.

**Project Type**: Local emulator frontend launched from a CLI; no web service, menu, audio output, or new gameplay input system.

**Performance Goals**: Nonblocking latest-frame delivery; fixed snapshot memory budget of three 393,216-byte pairs (1.125 MiB) plus SDL texture/renderer allocations. Render each consumed pair once across destinations; do not create an unbounded backlog. Preserve scheduler-boundary limits and existing core pacing. Observe presentation and produced/consumed/dropped sequence counts in hardware runs; no unsupported claim of a fixed emulation frame rate or scanout synchronization.

**Constraints**: Always fullscreen; two complete 256×192 converted images; uniform positive integer scale; black borders; no fractional stretch, blending, crop, or windowed option. All new configurable fields under `video`. SDL only on the main thread. Private core-hook use confined to a narrow Linux adapter; pixel conversion stays in the core. No GitHub actions, commits, remote Git, agent package installation requiring sudo, or hidden workflow acquisition.

**Scale/Scope**: One process/ROM session, one or two selected displays from any enumerated set, three mode choices, two layout choices, one boolean swap. Launch-time preferences. Display loss or impossible geometry causes clean failure; seamless reconfiguration is not required.

## Constitution Check

*GATE: Initial review before research and final review after Phase 1 design. Both design reviews pass; implementation/hardware checks below are future obligations, not completed tests.*

| Principle / workflow gate | Initial review | Post-design disposition |
| --- | --- | --- |
| I. Linux and ARM64 first | Pass: existing platform/toolchains retained | Pass: target SDL package/sysroot and real ARM64 checks defined |
| II. Portable frontend and display support | Pass: SDL choice and one/two-display requirements fixed | Pass: SDL2, two-output gate, vertical/horizontal fallback; backend/device details isolated |
| III. Core integration boundary | Pass: research implemented interfaces before selecting them | Pass: public RGBA conversion with existing synchronized hook confined to adapter; no reimplemented core graphics/layout logic |
| IV. Linux build and cross-compilation | Pass: both existing presets retained | Pass: target-only SDL discovery, offline preparation, inventory/licensing updates and native/cross validation specified |
| V. Hardware debugging and credentials | Pass: no credentials needed for planning | Pass: real-device evidence required later; reuse ignored connection settings, sanitized logs, no credentials in artifacts |
| VI. Local Git only | Pass: local inspection only; no extension hooks registered | Pass: no commits, remote Git, GitHub tools or endpoints; remove/fail-fast prohibited prepare-core fallback before configure |
| Sudo handoff | Pass: no privileged operation needed | Pass: future elevated package/session setup handed to user with exact commands and explicit completion confirmation |
| Build and display verification | Applicable to implementation | Pass in design: both build variants and real one/two-display acceptance required before implementation sign-off; documentation-only planning needs no executable build |

No constitutional exceptions are requested. The existing conditional clone in build preparation is a workflow hazard identified by inspection; the plan requires an offline failure path and does not invoke that script during planning.

## Project Structure

### Documentation (this feature)

```text
specs/004-fullscreen-video-output/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── video-config.schema.json
│   └── video-output.md
└── checklists/requirements.md
```

`tasks.md` belongs to the next `$speckit-tasks` phase and is not generated here.

### Source Code (repository root)

```text
src/
├── main.c
├── runner.c                         # CLI/config lifecycle, core worker, main SDL loop
├── runner.h
├── config.[ch]                      # feature 003 prerequisite; extend its existing model
├── video/
│   ├── layout.[ch]                  # pure mode resolution, screen assignment, integer rectangles
│   ├── frame_mailbox.[ch]           # owned paired frames and bounded sequence handoff
│   └── sdl_video.[ch]               # displays, fullscreen, renderers/textures/events/diagnostics
└── platform/linux/
    ├── core_video_bridge.[ch]       # private flip-hook boundary; public core RGBA conversion
    ├── software_video.c             # retain/rename current headless_video.c buffer/ABI shim
    ├── headless_audio.c             # retained
    └── platform.c                  # existing POSIX services; update platform identity
cmake/
├── SeedlessCore.cmake               # software ABI shim and adapter, Android GL stays excluded
├── SDLVideo.cmake                   # target SDL discovery/version/link contract
└── toolchains/aarch64-linux-gnu.cmake
scripts/
├── prepare-core.sh                  # missing local dependencies fail without remote fallback
├── setup-ubuntu.sh                  # manual prerequisite/check guidance
├── audit-runtime.py                # declared SDL/backend runtime model
├── debug-device.py                 # graphical preflight and --frontend-config
└── validate-headless.sh             # adapt legacy validation to visible/configured runner
tests/                              # existing test root; names below are planned
├── abi/core_layout.c
├── video_layout.c
├── video_mailbox.c
├── video_probe.c                    # test-only checkerboard/frame IDs; shares real SDL path
├── test_video_config.c
└── test_video_tooling.py
```

**Structure Decision**: Extend the existing C runner rather than create a second application. Source names under `src/video/` are planned new modules; `config.[ch]` names follow the prerequisite implementation if it chooses another location. Keep private core includes in adapter/core target compilation. Keep `seedlessds-headless` as the build/install filename for compatibility, but update help, platform identity, and documentation to describe fullscreen operation. Do not keep a user-facing headless switch. The test-only probe is a separate validation executable, not another product launch mode.

## Phase 0 — Research Results

Completed [research.md](research.md):

1. SDL2 ≥2.0.12 selected; system-installed target library, explicit shared-runtime accounting, no automatic acquisition.
2. Display indices 0 and 1 are the deterministic pair; physical roles must be recorded on target hardware and swapped by configuration if needed.
3. `vertical` default and `horizontal` layouts selected from the implemented separate-image interface, rather than unsupported declarations.
4. Existing producer flip hook plus core RGBA conversion selected for a paired latest-frame mailbox; unused `platform.video.present` is not a working integration point.
5. Main-thread SDL nearest rendering with actual pixel geometry selected; compositor scaling is a hardware acceptance concern.
6. Configuration prerequisite and build/runtime/deployment gaps identified with concrete integration requirements.

No design research markers remain. Installed-device/backend capability and prerequisite implementation are explicit execution gates with prescribed checks.

## Phase 1 — Design and Interfaces

### Configuration and presentation resolution

The normative fields and values are defined in [contracts/video-config.schema.json](contracts/video-config.schema.json) and [contracts/video-output.md](contracts/video-output.md). Extend feature 003's typed configuration loader/default generator. Do not copy settings into `visuals`, add display command-line overrides, or rewrite older files merely to add defaults.

Resolve requested mode after SDL enumeration: one usable output gives single presentation for all choices; two or more give dual for `auto`/`dual_screen`, while `single_screen` stays on display 0. Resolve layout and swap in pure code. Reject wrong JSON types/enums before SDL/core startup. Report a too-small layout before starting emulation. Do not treat a two-window initialization failure as a one-display configuration.

### Video lifecycle and frame ownership

1. Validate CLI, selected configuration, ROM/BIOS/save prerequisites; help/usage remain independent of SDL.
2. Initialize SDL video/events on the main thread. Enumerate and log displays, resolve mode, create fullscreen resources, verify placement and nearest mode, query renderer pixel sizes, compute rectangles, and clear black. If any step fails, destroy partial resources and exit with `operation=video` before core execution.
3. Create the core with retained software output storage. Allocate/initialize mailbox and attach the conversion hook after core buffer initialization and before the load/emulation worker starts. Fail cleanly if hook ownership or allocation is invalid.
4. Retain the existing load/emulation worker and scheduler control. In the flip hook, convert into owned staging, publish a pair/sequence through a short copy lock, return without any SDL or consumer acknowledgment. Validate complete-page selection under supported visual/thread configurations.
5. The main loop polls SDL events, checks core status/signals, copies the newest pair under the mailbox lock, then releases it and uploads/presents to all destinations. A finite ≤10 ms idle poll interval keeps loading/paused states responsive. Never hold mailbox/core locks during SDL calls.
6. Preserve core-boundary `--frames N` accounting; consume the last available pair before destroying video resources. A finite run need not present every scheduler boundary. In uninterrupted visual validation, verify continuing paired updates rather than equating render calls to emulation frames.
7. On quit, signal, video error, display loss, or core completion, request core stop; join the worker; detach the hook under the core page mutex; destroy the core and its remaining composition resources while mailbox storage is still valid; then free mailbox and SDL textures/renderers/windows and quit SDL. Ensure cleanup works from every partially initialized stage. Signal handlers only set flags.

On selected-display removal, fullscreen loss that cannot be immediately re-established, or renderer geometry shrinking below 1x, use the specification's permitted diagnostic/clean-shutdown policy. Do not resize into a window or add seamless hot-plug migration. `SDL_QUIT`/window-close events request normal clean completion; gameplay inputs remain outside this feature.

### Build and runtime compatibility

Add `cmake/SDLVideo.cmake` with target-only installed-package discovery and minimum-version validation; link the application to shared `SDL2::SDL2`, not SDL2main. Native builds use native target packages. Cross builds use a complete AArch64 SDL/graphics sysroot and CMake package root isolation; use `CMAKE_PREFIX_PATH` inside that root if required, never host x86 SDL.

Before any agent configure, make `prepare-core.sh` fail if the pinned local core/submodules/cache are missing instead of attempting its conditional clone. Preserve patch/source verification. Do not run repository-modifying or dependency-fetching workflows without inspecting transitive actions. Existing permitted archive fetching is not an authorization for remote Git/GitHub access.

Extend the runtime inventory with declared direct SDL dependency, compiled/runtime versions, linkage, backend capability expectations and dynamically loaded dependencies; retain strict architecture, ABI, pin/license and forbidden Android/OpenSL checks. Device checks must verify actual target loader resolution and selected backend initialization. Shared SDL is an explicitly documented dependency change from the original libc-only headless artifact.

Update deployment to forward an existing target configuration via helper `--frontend-config` without confusing it with private connection `--config`. Retain legacy executable filename, result records, and signal statuses; update existing launch/validation scripts to the feature 003 CLI. No install, remote deployment, or privileged session changes are performed during planning.

### Validation and traceability

| Requirement / outcome | Planned evidence |
| --- | --- |
| FR-001, FR-012; SC-004 | ROM video updates, 120-boundary run, five-minute real-device runs, signal/result/save regression checks |
| FR-002, FR-004; SC-001 | SDL probe verifies two distinct fullscreen destinations and logged actual placement; human physical panel confirmation |
| FR-003, FR-005; SC-001 | Pure mode matrix plus real one/two-output launches for all three modes |
| FR-006; SC-002 | Both swap values for every mode/output case; frame-ID pattern and recalculated per-display rectangles |
| FR-007, FR-008; SC-003 | Vertical/horizontal layout tests, common factor, ignored layout in effective dual presentation |
| FR-009; SC-003 | Pattern/readback tests and physical inspection at exact/nonmultiple/mismatched resolutions; verify final compositor path |
| FR-010; SC-005 | Valid defaults, missing object/member, null/type/enum errors, custom config selection and unchanged file hashes |
| FR-011; SC-006 | No-display, second-window failure, <1x, display removal, allocation/upload failure, partial cleanup checks |
| PC-001–PC-004 | Research evidence, target SDL/backend inventory, local core interface verification, constitution/tooling review |

The runnable scenarios and expected results are in [quickstart.md](quickstart.md). Tests are targeted to policy/geometry, paired-frame ownership, lifecycle failures, and changed tooling; avoid copying implementation internals into assertions. Build both intended ARM64 variants and perform physical validation before implementation completion. Planning completes with reviewed artifacts, not with an unbuilt-feature claim.
