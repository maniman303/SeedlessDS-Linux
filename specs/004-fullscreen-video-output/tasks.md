# Tasks: Fullscreen Video Output

**Input**: Design documents in `specs/004-fullscreen-video-output/`: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/video-output.md](contracts/video-output.md), [contracts/video-config.schema.json](contracts/video-config.schema.json), and [quickstart.md](quickstart.md).

**Prerequisites**: Feature 003's configuration foundation, locally available pinned core/dependencies, target SDL2 ≥2.0.12, Linux ARM64 native/cross toolchains, and real hardware for final display acceptance. T001 is an explicit external-dependency gate, not an instruction to silently implement another feature inside this one.

**Tests**: Include targeted automated and physical checks because FR-012, SC-001–SC-006 and PC-003 explicitly require configuration/mode, geometry, failure, bounded-run and real-hardware validation. Test tasks precede their corresponding implementation where practical; establish the expected failure before implementing the behavior. No unrelated test expansion or general TDD requirement is introduced.

**Organization**: Four story phases in specification priority order: US1 fullscreen dual output (P1), US2 configurable mode/fallback (P1), US3 swap (P2), US4 single-display layouts (P3).

## Format: `[ID] [P?] [Story] Description`

- Every task has an unchecked checkbox, sequential ID, concrete file paths, and a story label in story phases only.
- `[P]` means a task can overlap the explicitly identified sibling work once its listed prerequisites are complete. It does not override dependencies or authorize delegation.
- Paths are relative to the repository root unless explicitly described as ignored validation output.
- `src/config.c` and `src/config.h` mean feature 003's shared configuration files; if that prerequisite uses different paths, record the actual mapping in `specs/004-fullscreen-video-output/implementation-notes.md` before edits. Do not add a second loader.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish the configuration dependency and a permitted, reproducible SDL build path.

- [X] T001 Verify feature 003 against `specs/003-frontend-json-config/spec.md`, `src/runner.c`, and its shared configuration implementation; record file mapping and evidence for config selection/default creation/validation, independent BIOS paths, profile/visual/save settings, fixed policies and `--config PATH` in `specs/004-fullscreen-video-output/implementation-notes.md`. Keep this task incomplete while the prerequisite is absent; report that dependency without introducing a temporary parser or completing unrelated feature 003 work under this task.
- [X] T002 [P] Remove the missing-submodule remote clone fallback from `scripts/prepare-core.sh` and replace it with a descriptive local-prerequisite failure, preserving pinned revision/cache, patch hash and source inventory validation; add missing-local-input regression coverage in `tests/test_prepare_core_offline.py` proving no remote command is attempted. Inspect all transitive configure actions before any build; this can proceed independently of T001.
- [X] T003 [P] Add `cmake/SDLVideo.cmake` to find installed target SDL2 ≥2.0.12 and expose the shared `SDL2::SDL2` target without SDL2main, downloads or host fallback; preserve target-only package/include/library discovery in `cmake/toolchains/aarch64-linux-gnu.cmake`. Keep root `CMakeLists.txt` wiring for T008; this can overlap T001/T002.
- [X] T004 [P] Extend SDL development/runtime checks and manual native-package, AArch64 multiarch-package, or custom-sysroot guidance in `scripts/setup-ubuntu.sh` and `README.md`, without automatic privileged installation or enabling multiarch; require user execution and explicit success confirmation for any needed sudo command. This can overlap T001–T003 and must not invoke configure before T002.

**Checkpoint**: T001's external dependency and T002–T004 must be satisfied before the foundational phase is considered ready. If T001 is unavailable, continue independent permitted setup work, then report the unmet prerequisite; do not mark it complete based on a specification alone.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Define shared video interfaces and retain core/build invariants before story implementation.

- [X] T005 Define shared preference enums, inventory/rectangle/output types, owned-pair/mailbox interfaces, SDL facade and adapter lifetime contracts in `src/video/layout.h`, `src/video/frame_mailbox.h`, `src/video/sdl_video.h`, and `src/platform/linux/core_video_bridge.h`. Encode 256×192 RGBA32 images, pitch 1024, 196,608 bytes per image, monotonic publication sequence, and per-session display metadata; retain the model constraints “SDL index valid for the current enumeration; never persisted as stable device identity” and “Diagnostic OS/SDL name, not a unique key”. Keep SDL types/private core internals out of shared configuration types.
- [X] T006 Rename `src/platform/linux/headless_video.c` to `src/platform/linux/software_video.c` while retaining software page allocation, texture-format metadata, mutex/condition lifecycle, ABI objects and required GL compatibility stubs; update `cmake/SeedlessCore.cmake` and `tests/abi/core_layout.c` as needed to keep original Android GL presentation excluded and validate the existing flip-hook/conversion integration boundary without using declared-but-undefined core APIs.
- [X] T007 Implement the declared shared SDL runtime model in `scripts/audit-runtime.py`: record target SDL version/SONAME/linkage, direct runtime dependencies and declared dynamically loaded backend requirements, preserving AArch64/ABI/pin/hash checks and Android/OpenSL/unresolved-core-GL rejection; add focused accepted/rejected inventory fixtures in `tests/test_video_tooling.py` rather than disabling the libc-only audit wholesale. Depends on T003's linkage contract.
- [X] T008 Wire target SDL discovery, software adapter linkage and incremental story source/test registration support into `CMakeLists.txt` and `cmake/SeedlessCore.cmake`, retaining native/cross presets and naming the executable `seedlessds`. Keep non-graphical tests runnable without a graphical session; add new implementation/test sources only when created by the corresponding story tasks. Depends on T005–T007.

**Checkpoint**: Configuration foundation, safe local preparation, shared contracts, software framebuffer ABI and declared SDL build/runtime dependencies are ready. No physical video support is claimed yet.

---

## Phase 3: User Story 1 — Play fullscreen on a dual-screen device (Priority: P1) — MVP

**Goal**: Default launches show a changing top/bottom pair on separate fullscreen physical outputs with sharp integer scaling. Include the minimum default vertical single-output path needed to preserve the portable frontend baseline; selectable modes/layouts arrive in later phases.

**Independent Test**: With new video preferences omitted, launch a distinguishable-screen ROM on dual-screen ARM64 hardware; verify independent fullscreen destinations, sharp native/aspect-correct images and the maximum integer factor per output. Confirm bounded completion, interruption cleanup and a default vertical presentation on a single usable output.

### Tests for User Story 1

- [X] T009 [P] [US1] Add baseline dual/default-vertical geometry acceptance cases in `tests/video_layout.c`: independent factors on 1024×768/640×480, 640×480 → `(64,48,512,384)`, nonmultiple/odd borders, zero/nonpositive/overflow dimensions, and rejection below 1x; quote/enforce the model rules “Odd leftover pixels place the extra border pixel on the right/bottom.” and “Use checked arithmetic and reject invalid/nonpositive dimensions or overflow.” Depends on T005/T008.
- [X] T010 [P] [US1] Add concurrent paired-frame ownership/overwrite/slow-consumer tests in `tests/video_mailbox.c`, asserting the constraints “Both images must come from the same completed core page flip.”, “Replacing an unread pair is allowed.”, fixed three-pair/1.125 MiB storage, monotonic sequence and no consumer-acknowledgment dependency. Use paired stamps to detect mixed/torn copies rather than mirroring private implementation fields. Can overlap T009/T011.
- [X] T011 [P] [US1] Add focused adapter/startup/teardown acceptance fixtures in `tests/video_lifecycle.c`: hook attach only after buffer initialization, refusal to overwrite another owner, conversion failure propagation, no SDL producer calls, partial resource cleanup and worker-stop/hook-detach ordering. Provide test-only injection seams through the interfaces from T005, not gameplay flags; can overlap T009/T010.

### Implementation for User Story 1

- [X] T012 [US1] Implement baseline auto display resolution and integer dual/default-vertical rectangle computation in `src/video/layout.c`, using display indices 0/1 and leaving additional outputs unused; fit each dual 256×192 destination independently and fit the default 256×384 canvas with one factor using floor division, renderer pixels, centered black borders, and clean <1x failure. Depends on T009; creates the base extended by T023/T029/T034.
- [X] T013 [P] [US1] Implement fixed-capacity owned staging/mailbox/consumer pairs in `src/video/frame_mailbox.c` with short copy locking and thread-safe publication/error state; enforce “Producer never waits for display acknowledgment and main thread never renders from live core pages.” and “Do not retain or expose callback pointers after the hook returns.” Depends on T010 and can overlap T012.
- [ ] T014 [US1] Implement the Linux-only synchronized conversion bridge in `src/platform/linux/core_video_bridge.c`: attach/remove `recon_present_flip_hook` under the core page mutex, call implemented `nds_screens_rgba` at a completed flip into owned staging, and publish without SDL. Enforce “No path acquires the core lock while holding the mailbox lock.”; keep storage alive through producer quiescence and preserve patched screen wait/signal no-ops. Validate baseline RGB565/BGRA/wide conversion through the core and do not enable private native/hires modes. Depends on T011/T013.
- [X] T015 [US1] Implement auto/default output lifecycle in `src/video/sdl_video.c`: enumerate/log actual displays, reject normal dummy/offscreen gameplay, select “Primary (index 0), secondary (index 1), or unused”, create display-targeted `SDL_WINDOW_FULLSCREEN_DESKTOP` windows, verify distinct actual destinations/fullscreen, query renderer pixel sizes, allocate RGBA32 streaming textures, disable blending, set/verify nearest sampling, clear black and present. Try accelerated then software renderers, avoid serial blocking vsync, and fail required-output initialization rather than silently falling back. Depends on T012; all SDL operations stay on the main thread.
- [X] T016 [US1] Integrate video initialization, bridge attachment, the main-thread event/newest-pair loop and staged teardown in `src/runner.c` and `src/runner.h`; preserve the core worker, ≤10 ms idle event polling, scheduler-boundary `--frames`, signal flags and final pair drain. Stop/join the worker, detach under the core mutex, destroy core before freeing mailbox, then destroy SDL resources; help/usage/config errors must precede SDL. Depends on T014/T015.
- [X] T017 [US1] Add contract diagnostics and completion/error handling in `src/runner.c`, `src/video/sdl_video.c`, and `src/platform/linux/platform.c`: report SDL versions/backend, display names/bounds/roles, output pixels, renderer, rectangles/factors and default effective mode; update platform identity. Preserve bounded result records and signal exits 130/143; video failures use exit 1 and `result=failed frames=N operation=video`, normal SDL close uses clean exit 0, and display loss/unrecoverable fullscreen or <1x geometry terminates cleanly. Depends on T016.
- [X] T018 [US1] Add the test-only `seedlessds-video-probe` executable in `tests/video_probe.c` and `CMakeLists.txt`, sharing production SDL/layout code and using checkerboards, RGB channel patches and paired changing screen IDs. Support `--seconds N`, default video preferences without ROM/BIOS, and finite cleanup; do not add probe/headless flags to gameplay. Depends on T015/T017.
- [ ] T019 [US1] Run the new native acceptance tests, a 15-second two-output probe and a distinguishable-screen 120-boundary ROM run plus SIGINT/SIGTERM cleanup on ARM64 hardware; record hashes, physical panel/index correspondence, fullscreen/pixel observations and exact result/status evidence in ignored `build/validation/device/video-us1.json`. Also check the baseline vertical path on one usable output. Use [quickstart.md](quickstart.md), and leave hardware checks incomplete if only mirrored/dummy/headless output is available. Depends on T009–T018.

**Checkpoint**: The default fullscreen MVP is usable and independently verified. It does not close the configurable-mode, swap or selectable-layout stories, nor the final five-minute acceptance obligations.

---

## Phase 4: User Story 2 — Choose display behavior with predictable fallback (Priority: P1)

**Goal**: Persist all new output preferences under `video`, accept all three modes, and apply the exact one/two-display mode table without changing saved preferences.

**Independent Test**: Run `auto`, `dual_screen`, and `single_screen` on one and two usable outputs with default layout/swap; verify the six expected destinations, omitted-setting defaults, generated configuration, custom-file selection, field-specific rejection and unchanged existing file hashes.

### Tests for User Story 2

- [X] T020 [P] [US2] Add configuration contract cases in `tests/test_video_config.c` against `contracts/video-config.schema.json`: all 12 mode/swap/layout combinations, missing object/member, `{}`, null/non-object/invalid enums/wrong member types, fresh defaults, custom-file precedence and unchanged existing-file hashes; assert 100% of new output properties are under `video` and existing `visuals` remain intact. Depends on T001/T005/T008; can overlap T021.
- [X] T021 [P] [US2] Extend `tests/video_layout.c` with zero/one/two/three-output mode resolution: one output always uses display 0, `auto`/`dual_screen` with ≥2 use indices 0/1, `single_screen` with ≥2 uses only 0, and a second-window failure does not become fallback. Assert requested mode remains distinct from effective mode; depends on US1 geometry and can overlap T020.

### Implementation for User Story 2

- [X] T022 [US2] Extend the shared model/default generator/loader in `src/config.h` and `src/config.c` for top-level `video` only: `video.display_mode` is string enum `auto`, `dual_screen`, `single_screen`, default `auto`; `video.swap_screens` is boolean `false`, `true`, default `false`; `video.single_screen_layout` is string enum `vertical`, `horizontal`, default `vertical`. Enforce verbatim “Missing is distinct from null. Null/wrong types and unsupported enum values are configuration errors identifying the full field path.”, “An empty object is valid.”, “New default files contain all three members.” and “Existing files are not rewritten on load.” Retain common unknown-key behavior, fixed fullscreen/integer policies, and no root/`visuals` aliases. Depends on T020; swap/horizontal application is completed in US3/US4.
- [X] T023 [US2] Implement the requested/effective mode table in `src/video/layout.c` and `src/video/sdl_video.c`, preserving the persisted `auto`/`dual_screen` distinction and index-0-only single mode; report genuine one-display fallback while treating two-output resource/placement failures as errors. Retain selected layout without applying it in dual output. Depends on T021/T022.
- [X] T024 [US2] Bind selected configuration preferences to the presentation lifecycle in `src/runner.c`, and add optional `--config PATH` video-preference loading to `tests/video_probe.c` using the shared loader rather than another parser. Probe defaults require no ROM/BIOS; keep product launch input categories unchanged beyond feature 003's existing `--config` spelling and preserve configuration errors before SDL/core. Depends on T022/T023.
- [X] T025 [US2] Adapt `scripts/debug-device.py` and `scripts/validate-video.sh` to the feature 003 launch interface: forward an existing target configuration via helper `--frontend-config` to frontend `--config`, preserve helper `--config` for private connection settings, replace obsolete `--data-dir` launches, and retain binary/result/hash/host-verification safeguards. Do not include credentials in commands or logs. Depends on T024; graphical runtime preflight is completed in T038.
- [X] T026 [US2] Extend `tests/test_video_tooling.py` and affected `tests/test_debug_device_diagnostics.py` cases to verify target-configuration forwarding, argument quoting, connection/frontend-config separation, preserved result parsing and failure behavior without making device/network connections. Depends on T025.
- [ ] T027 [US2] Execute the six-mode/output matrix, missing/default/custom/invalid-config cases and no-rewrite hash checks from `quickstart.md` on ARM64, recording evidence in ignored `build/validation/device/video-us2.json`; a forced single mode on dual hardware does not replace a genuine one-output fallback run. Depends on T020–T026.

**Checkpoint**: All configuration and display mode scenarios are independently verifiable; persisted swap/layout preferences have their final types/defaults while the remaining stories implement their selectable behavior.

---

## Phase 5: User Story 3 — Swap the DS screens (Priority: P2)

**Goal**: Exchange the two destination assignments in dual output and the two image positions in effective single output, including fallback.

**Independent Test**: Compare swap off/on for all six mode/display cases using paired screen IDs; verify identical arrangement, exchanged identities, no lost image, and fresh per-destination scale on mismatched displays.

### Tests for User Story 3

- [X] T028 [US3] Extend `tests/video_layout.c` with swap off/on assertions for dual output, the default vertical single path, and auto/dual fallback; enforce the model rule “DS screen identity remains top/bottom throughout; only destinations change.” Include mismatched-resolution expectations showing that each destination keeps its own fitting scale. Depends on US1/US2.

### Implementation for User Story 3

- [X] T029 [US3] Apply `video.swap_screens` in `src/video/layout.c` by exchanging top/bottom image assignments between existing output/slot destinations without changing core configuration, layout choice, or saved preference; recompute per-display geometry for its actual destination. Depends on T028.
- [X] T030 [US3] Use resolved image assignments for texture upload/drawing and diagnostics in `src/video/sdl_video.c`, verifying both images belong to the same consumed pair and swap adds no fractional transform or extra synchronization. Depends on T029.
- [X] T031 [P] [US3] Run native swap/paired-publication geometry tests and record their mismatched-display and fallback results in ignored `build/validation/device/video-us3-geometry.json`. Depends on T028–T030 and can overlap T032 because these tests do not create physical outputs.
- [ ] T032 [P] [US3] Run physical screen-ID comparisons for swap false/true across the six mode/output cases using `tests/video_probe.c` and configured ROM launches; record panel correspondence, integer scale and unchanged configuration hashes in ignored `build/validation/device/video-us3-panels.json`. Depends on T028–T030 and can overlap T031; horizontal swap is verified after US4.

**Checkpoint**: Swap is independently validated on dual output and the default single/fallback layout. US4 extends the same assignment mechanism to horizontal placement.

---

## Phase 6: User Story 4 — Play with a basic single-display layout (Priority: P3)

**Goal**: Offer default vertical and optional horizontal arrangements with both complete images, one common integer factor, and the same swap rules; dual output ignores layout preference for drawing.

**Independent Test**: Launch both layouts with each swap value on a real single output and a forced single output on dual hardware; verify documented positions, common scale, black borders, fit/rejection boundaries and retained-but-ignored layout in dual output.

### Tests for User Story 4

- [X] T033 [US4] Extend `tests/video_layout.c` with 256×384 vertical and 512×192 horizontal canvases, both swap values, exact/nonmultiple/odd dimensions, documented 640×480 rectangles, vertical-on-256×383 rejection, horizontal minimum bounds and dual-layout independence; enforce “Single mode uses one common `k` for the entire canvas. No gap or overlap.” Depends on US1–US3.

### Implementation for User Story 4

- [X] T034 [US4] Implement configurable `vertical`/`horizontal` canvas/slot selection in `src/video/layout.c` and consume it in `src/video/sdl_video.c`: vertical slots `(0,0,256,192)`/`(0,192,256,192)`, horizontal slots `(0,0,256,192)`/`(256,0,256,192)`, default vertical, uniform integer scale, swapped assignments and clean selected-layout <1x failure without choosing another layout. Preserve the persisted preference while ignoring it for effective dual output. Depends on T033.
- [X] T035 [P] [US4] Extend `tests/video_probe.c` with test-only nearest-pattern/readback assertions for both single-display layouts, RGB channels and source-pixel block boundaries; use production destination geometry and compare against expected source colors rather than duplicating rendering logic. Register non-physical assertions appropriately in `CMakeLists.txt`; depends on T034 and can overlap T036 on a separate test surface/session.
- [ ] T036 [P] [US4] Validate both layouts and both swaps physically with the existing checkerboard/screen-ID probe and a ROM on one-output and forced-single dual setups; confirm ignored layout under effective dual output, no overlap/crop, and maximum common factor, and record observations in ignored `build/validation/device/video-us4.json`. Depends on T034, uses the existing T018/T024 probe path, and can overlap T035 without depending on its new readback assertions.

**Checkpoint**: All four stories deliver their specified behavior. Final cross-cutting acceptance and hardware evidence remain required.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Close runtime packaging, failure coverage, build variants and physical acceptance across the complete feature.

- [X] T037 [P] Update `README.md`, `docs/core-options-inventory.md`, `specs/004-fullscreen-video-output/quickstart.md`, SDL licensing material in `licenses/SDL2.txt`, and installation entries in `CMakeLists.txt` to describe always-fullscreen behavior under the `seedlessds` executable name, all `video` keys/defaults, black-border integer fit, target SDL/backend requirements, native/cross setup and manual privileged-operation handoff. Use an installed/local SDL license, not remote Git acquisition. Depends on all story implementation tasks and can overlap T038/T039.
- [ ] T038 [P] Complete graphical/runtime preflight and sanitized inventory forwarding in `scripts/debug-device.py` and `tests/test_video_tooling.py`: verify target shared SDL resolution/version/backend dependencies and existing session/display access, distinguish SSH connectivity from graphical capability, retain hash/host verification and avoid elevated emulator launch or permission bypass. Depends on T007/T025/T026 and story completion; can overlap T037/T039.
- [ ] T039 [P] Complete and run cross-story fault/lifecycle coverage in `tests/video_lifecycle.c`, `tests/video_mailbox.c`, and `tests/test_video_config.c`: no display/dummy backend, second-window/renderer/texture/conversion/allocation failures, too-small geometry, selected-display removal/fullscreen loss, loading/paused/publication stop races, hook ownership/detach, signals, normal close, final-frame drain and no orphan workers/windows. Repeat supported feature 003 visual/thread settings and verify paired frame stamps and no deadlock/use-after-free; depends on all stories and can overlap T037/T038.
- [ ] T040 Build both intended ARM64 variants using `CMakePresets.json` after T002 and all code/tooling changes; run native CTest and changed Python tooling tests, inspect target SDL lookup and ELF/runtime inventory/license contents, and record compiler/sysroot/hash/results in ignored `build/validation/device/video-builds.json`. Reject host SDL linkage and undeclared dependencies. If native hardware/toolchain/sysroot packages are unavailable, report the exact unmet environment prerequisite and leave the corresponding acceptance incomplete; never fabricate a build result. Depends on T037–T039.
- [ ] T041 Run the completed two-output `seedlessds-video-probe` on the actual ARM64 dual-screen device, recording installed/compiled SDL versions, backend/renderer, usable output inventory, physical top/bottom index mapping, simultaneous fullscreen placement and sharp final pixel path in ignored `build/validation/device/video-compatibility.json`. Resolve backend limitations with evidence before sign-off; one mirrored/logical output or texture readback alone cannot pass dual-screen acceptance. Depends on T040 and applicable authorized device/session access.
- [ ] T042 Run at least five minutes of changing paired ROM content on the real dual-screen ARM64 target, then separate bounded 120-boundary, SIGINT, SIGTERM and normal-close checks; verify exact records/exits, stable fullscreen, bounded storage and configured save write/reload behavior, recording evidence in ignored `build/validation/device/video-dual-session.json`. Depends on T041; use existing ignored connection settings and sanitized logs.
- [ ] T043 Run at least five minutes on a genuine single-usable-output ARM64 setup, exercise both layouts/swaps and auto/dual fallback, then verify bounded/signal cleanup and save regression behavior; record evidence in ignored `build/validation/device/video-single-session.json`. Depends on T040 and the corrected graphical environment from T041; run sequentially with T042 when they share hardware/session resources.
- [ ] T044 Execute the final combined mode × output × swap × effective-layout matrix and physical checkerboard checks at representative exact/nonmultiple/mismatched resolutions, including >2-output selection and <1x/display-loss failures where supported; use `specs/004-fullscreen-video-output/quickstart.md` and record results plus any fixture-only limits in ignored `build/validation/device/video-final-matrix.json`. Reuse already valid T019/T027/T032/T036 evidence when the final artifact/affected behavior is unchanged; do not repeat passing work without a new change or unresolved concern. Depends on T040–T043.
- [ ] T045 Reconcile FR-001–FR-012, SC-001–SC-006 and PC-001–PC-004 against final artifact hashes and evidence in `specs/004-fullscreen-video-output/implementation-notes.md`; record completed checks and any remaining failure/environment limitation, update the validation guide only for evidenced changes, and leave unfinished tasks unchecked. Do not change reviewer-owned checklist markers or claim completion without the configuration dependency, both build variants and real one/two-display acceptance. Depends on T044.

---

## Dependencies & Execution Order

### Phase dependencies

```text
External feature 003 implementation ──→ T001 ──┐
Local offline preparation / target SDL ─→ T002–T004
                                             ↓
                                Foundation T005–T008
                                             ↓
                  US1 T009–T019: fullscreen/default output
                                             ↓
                  US2 T020–T027: configuration/mode table
                                             ↓
                  US3 T028–T032: swap destinations
                                             ↓
                  US4 T033–T036: selectable single layout
                                             ↓
               T037–T039 in parallel → T040 builds
                                             ↓
             T041 physical compatibility → T042/T043 sessions
                                             ↓
                        T044 combined checks → T045 closure
```

- T002–T004 can proceed while T001's external dependency is being resolved. This list does not authorize unplanned feature 003 implementation; missing configuration blocks foundation completion and integrated video acceptance.
- Within US1, T009/T010/T011 are independent tests; T012 and T013 can proceed concurrently once their respective tests/contracts are ready. T014 consumes T013; T015 consumes T012; both precede runner integration.
- Later stories share `src/video/layout.c`, `src/video/sdl_video.c` and runner/configuration interfaces, so implement their production edits in story order. Their acceptance criteria remain independently executable using a configured fixture after prerequisites are met. Do not claim false cross-story edit parallelism.
- T020/T021 may run together once earlier edits to their files are complete. T025/T026 precede the final device-preflight changes in T038.
- T031/T032 and T035/T036 are explicitly disjoint validation/work branches with the prerequisites stated above. Hardware runs sharing a device or display session remain sequential.
- No configure runs before T002; no physical acceptance is inferred from dummy video, unrun tests, missing equipment, or old headless records. An environment-dependent task remains incomplete until its evidence exists.

### Requirement and contract coverage

| Scope | Primary tasks |
| --- | --- |
| FR-001, FR-012: core video, paired ownership, lifecycle and legacy execution | T006, T010–T019, T039, T042–T043 |
| FR-002, FR-004: fullscreen, physical display identity and selection | T005, T012, T015, T017–T019, T023, T041 |
| FR-003, FR-005, FR-010: mode configuration, fallback, defaults and grouping | T020–T027; schema/output contract consumed by T022/T024 |
| FR-006: dual/single/fallback swap | T028–T032, T033–T036, T044 |
| FR-007, FR-008: layout enumeration/default and ignored dual preference | T022–T024, T033–T036 |
| FR-009: integer/aspect/nearest/black borders | T009, T012, T015, T028–T036, T041, T044 |
| FR-011: video, geometry and display failures | T011, T015–T017, T021, T039, T044 |
| SC-001, SC-002, SC-003: complete matrix, swap and pixels | T019, T027, T031–T036, T041, T044 |
| SC-004: real five-minute sessions and exact bounded/signal completion | T042–T043 |
| SC-005, SC-006: configuration integrity and clean failures | T020–T027, T039, T044 |
| PC-001–PC-004: research choices, runtime/build compatibility and governance | T001–T008, T014, T037–T045; research.md is the existing decision record |

## Parallel Execution Examples

These examples describe independent work; they do not automatically invoke other agents.

### User Story 1

```text
After T005/T008:
  T009 tests/video_layout.c
  T010 tests/video_mailbox.c
  T011 tests/video_lifecycle.c
After their matching test contracts:
  T012 src/video/layout.c  ||  T013 src/video/frame_mailbox.c
Then converge at bridge/SDL integration and T016 runner lifecycle.
```

### User Story 2

```text
After US1 and shared config foundation:
  T020 tests/test_video_config.c  ||  T021 tests/video_layout.c
Then T022 → T023 → T024; no concurrent writes to shared layout/SDL code.
```

### User Story 3

```text
After T028–T030:
  T031 pure native geometry/mailbox checks → video-us3-geometry.json
  T032 physical probe comparisons         → video-us3-panels.json
Pure tests create no physical outputs and do not modify the probe sources.
```

### User Story 4

```text
After T034:
  T035 probe readback assertions on a separate software test surface/session
  T036 hardware observations with the already-built T018/T024 probe
Keep the T036 binary fixed during that run; build new probe assertions separately.
```

## Implementation Strategy

### MVP first

Complete the external configuration prerequisite, Setup, Foundation and US1. Validate default dual fullscreen output, real paired updates, exact integer geometry and bounded/signal cleanup before continuing. Include the minimal default vertical one-output path so the MVP preserves portable single-screen operation. This checkpoint is a local review/demo increment, not full feature completion.

### Incremental delivery

Add US2's persistent choices and safe fallback, then US3's destination swapping, then US4's second layout. Re-run only checks affected by each new change, followed by the combined final acceptance gate. Require physical output evidence even when pure tests and the renderer probe pass.

### Completion discipline

Only check a task after its code or evidence is complete. Leave missing feature 003/environment/hardware work visible, continue permitted independent work, and report the precise dependency. The repository constitution overrides the template's commit-after-task note: agents create no commits and perform no GitHub or remote Git operations. Package/session changes requiring sudo are handed to the user with the exact manual command and resumed only after explicit confirmation of success. Do not bypass those rules through scripts, hooks or other tools.
