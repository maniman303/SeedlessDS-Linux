# Research: Fullscreen Video Output

**Date**: 2026-10-19  
**Scope**: Phase 0 for [spec.md](spec.md). Local pinned sources and official SDL/distribution documentation only. No GitHub endpoints, commits, or remote Git operations were used. Research was divided between core video integration, SDL compatibility, and build/runtime inspection.

## 1. Presentation library and compatibility

**Decision**: Use SDL2, minimum API version 2.0.12, through an installed target package and `SDL2::SDL2`. Prefer the target distribution's maintained, recent SDL2 release. Do not embed an automatic SDL download or maintain SDL2 and SDL3 implementations simultaneously.

**Rationale**: SDL2 supplies the required Linux display enumeration, fullscreen windows, streaming textures, and explicit texture scaling. Version 2.0.12 introduces `SDL_SetTextureScaleMode`. Ubuntu's package index documents SDL2 across older supported Linux images, including ARM64 packages on Ubuntu 24.04; SDL3 availability is less broad across those image generations. This favors compatibility with handheld distributions and the existing Ubuntu/WSL cross workflow over selecting a newer major version solely by age.

The minimum is an API floor, not a guarantee of working dual-screen KMSDRM. Runtime version, enabled backends, display permissions, and device drivers must be recorded and tested. Recent SDL2 secondary-screen KMSDRM fixes show why neither “SDL2 always supports this device” nor “SDL2 KMSDRM only supports one display” is a defensible assumption.

**Alternatives considered**:

- SDL3: modern display APIs and KMSDRM properties, but a new runtime requirement for older target images; no verified advantage on this particular device has been established. Reconsider only if the hardware acceptance probe identifies an SDL2 limitation that SDL3 demonstrably solves; record a design amendment before changing the dependency.
- Custom DRM/EGL presentation: duplicates portable platform work and increases device-specific scope.
- Simultaneous SDL2/SDL3 support: unnecessary implementation and testing cost for this feature.

**Primary sources**:

- [Ubuntu SDL package availability](https://packages.ubuntu.com/search?keywords=libsdl)
- [Ubuntu 24.04 ARM64 SDL2 development package](https://packages.ubuntu.com/noble-updates/libsdl2-dev)
- [SDL2 CMake integration](https://wiki.libsdl.org/SDL2/README-cmake)
- [SDL2 texture scaling API](https://wiki.libsdl.org/SDL2/SDL_SetTextureScaleMode)
- [SDL2 secondary-screen KMSDRM fix](https://discourse.libsdl.org/t/sdl-fixed-secondary-screens-on-kmsdrm-10535-21e03/53548)
- [SDL2 per-display KMSDRM initialization update](https://discourse.libsdl.org/t/sdl-kmsdrm-keep-fd-around-if-we-can-drop-master/54428)

## 2. Fullscreen, display selection, and backend limits

**Decision**: Create one `SDL_WINDOW_FULLSCREEN_DESKTOP` window per selected physical output, preserving the desktop/native display mode. Dual output selects SDL display indices 0 and 1; single output selects index 0; additional displays are unused. Use display-index-aware creation positions (`SDL_WINDOWPOS_CENTERED_DISPLAY(index)`) and validate the resulting display association and fullscreen flags. Confirm actual placement on hardware; do not infer placement from a position request alone.

Log the enumeration order, display names/bounds/modes, selected indices, actual renderer pixel sizes, SDL compiled/runtime versions, video driver, and renderer driver. Display 0 is the documented primary role and display 1 the secondary role. On the RG DS Plus, a physical top/bottom identification run must record which panel each index represents. `video.swap_screens` is the supported correction when the OS primary is the physical bottom panel; do not invent a model-specific mapping without evidence. The unswapped order remains primary then secondary as the specification permits.

**Rationale**: SDL already supplies platform selection and fullscreen behavior. Desktop fullscreen avoids changing resolution merely to obtain an integer-sized image. Using the first two outputs is deterministic for a stable OS enumeration. Wayland compositor output placement and scaling, and direct KMSDRM access, are deployment conditions requiring validation.

If two physical outputs exist but the backend cannot enumerate or independently target both, record this as a failed dual-screen compatibility check. A one-output run may be useful diagnostically but does not satisfy dual-device acceptance. If enumeration finds two usable outputs and creating the second fullscreen destination fails, fail startup; do not hide a rendering failure behind the one-display fallback. That fallback is for a genuinely one-usable-display setup.

KMSDRM requires appropriate existing DRM master/session access. Do not run the emulator elevated or disable the DRM master requirement as a supposed rendering workaround. Through SSH, use the device owner's existing graphical/session environment; an SSH shell alone proves no access to either panel. Unknown target graphical environment is an explicit hardware verification item, not an unresolved design choice.

**Alternatives considered**: Exclusive resolution switching (unnecessary), a window spanning outputs (does not provide independent per-display scale), and unconditional single-output fallback on any failure (conceals primary-use-case failure).

**Primary sources**:

- [SDL2 fullscreen semantics](https://wiki.libsdl.org/SDL2/SDL_SetWindowFullscreen)
- [SDL2 window creation and high-DPI dimensions](https://wiki.libsdl.org/SDL2/SDL_CreateWindow)
- [SDL2 actual window display association](https://wiki.libsdl.org/SDL2/SDL_GetWindowDisplayIndex)
- [SDL2 renderer output pixel dimensions](https://wiki.libsdl.org/SDL2/SDL_GetRendererOutputSize)
- [SDL2 KMSDRM DRM-master requirement](https://wiki.libsdl.org/SDL2/SDL_HINT_KMSDRM_REQUIRE_DRM_MASTER)
- [SDL Wayland platform notes](https://wiki.libsdl.org/SDL3/README-wayland) describe compositor constraints; this is background, not an SDL3 dependency or proof of identical SDL2 APIs.

## 3. Implemented core interfaces and layout choices

**Decision**: Present two native 256×192 RGBA images obtained through the existing `nds_screens_rgba()` conversion. Offer `vertical` (default: top above bottom) and `horizontal` (top left of bottom right). Compose the layout only in the Linux frontend. Swapping exchanges image assignments, not core configuration or emulated screen identity.

**Rationale and local evidence**:

- `external/SeedlessDS-Core/include/seedlessds/nds.h` declares separate screen buffers and RGBA conversion.
- `external/SeedlessDS-Core/src/core/nds_api_video.c::nds_screens_rgba` actually implements conversion into two tightly packed 256×192 images: 196,608 bytes each, pitch 1,024. It handles the baseline RGB565/BGRA formats and wide-buffer sampling without interpolating pixels.
- `external/SeedlessDS-Core/src/frontend/video_out.c::video_out_screen_buffer` chooses the current parity and screen offset; these are not necessarily tightly packed host RGBA buffers. Direct texture upload of returned raw pointers would be incorrect.
- `nds_frame_size`, `nds_dump_page`, `nds_set_internal_resolution`, `nds_set_present_mode`, and compose-thread setters are declared but have no source definitions in the pinned checkout. Do not build the design around these declarations.
- The baseline has native output disabled with internal/output scale defaults of 2 in `src/gpu/gpu3d/gpu3d.c`, `dynamic_scale.c`, and `scale.c`. Feature 003 exposes no native/high-resolution selector. This feature consumes the implemented fixed-size conversion without enabling private high-resolution modes. A future arbitrary native-scale feature needs separate conversion work; it is not silently supported here.
- Core script layout/swap controls exist, but arbitrary frontend rectangles are not a documented unified host-window layout API. The separate images suffice for these two basic arrangements; no emulation behavior is reimplemented.

**Alternatives considered**: Top-only/bottom-only (fails this feature's both-screens-visible requirement), asymmetric large/small screens, gaps, rotation, overlays and arbitrary scale controls (unneeded first-version scope), private core layout commands (unnecessary for arranging existing images).

## 4. Frame synchronization and lifetime

**Decision**: Confine use of the existing `recon_present_flip_hook` to a Linux core adapter. At the completed-page flip, call the core's RGBA conversion under its existing page mutex, then publish an owned pair to a bounded latest-frame mailbox. Keep every SDL call on the main thread. Preserve the patched no-op `nds_signal_screen`/`nds_wait_screen` behavior.

**Rationale and local evidence**:

- `external/SeedlessDS-Core/include/seedlessds/platform.h` declares `video.present`, but source inspection found no call to it. Setting that callback alone yields no video.
- `src/platform/threads.c::platform_doublebuffer_flip_signal` flips page parity and invokes `recon_present_flip_hook(page, index)` while holding `VIDEO_OUT_GL->mutex`.
- `src/frontend/video_out.c::recon_page_list` normally chooses the opposite parity for writing. The flip notification is the available completed-page handoff; validate with paired frame stamps and supported composition/thread settings.
- `nds_screens_rgba` does not lock. Calling it freely from the main thread risks different parities for the two screen pointers. Its 16-bit branch also lacks the same null checks as its 32-bit branch: install the hook only after buffer initialization succeeds.
- `video_out_screen_buffer` updates diagnostic output counters and may invoke capture hooks. Keep conversion on a single producer. Never use its output counters as the execution limit.
- `patches/seedlessds-core/0002-headless-present.patch` suppresses the old GL upload and public screen waits. Restoring those waits is unnecessary for SDL and risks blocking shutdown.

Use producer staging, one mailbox pair, and one consumer pair: three fixed pairs total, 1,179,648 bytes (1.125 MiB), plus textures. Conversion occurs before mailbox publication. A short mutex protects pair copying and sequence updates; neither side holds that mutex over SDL operations or waits for another frame. New frames overwrite the pending pair when necessary. A slow display may skip presentation of intermediate frames; emulation scheduler boundaries remain unchanged. SDL texture upload happens only after the main thread releases the mailbox lock.

Registration and removal occur under the core page mutex. Teardown requests core stop, joins the load/emulation worker, detaches the hook under that mutex, and keeps mailbox storage alive until core destruction has quiesced any remaining composition work. Avoid reverse mailbox-to-core lock acquisition. The adapter must not replace a non-null hook owned by another integration silently. Its private dependency is documented and guarded by ABI/build checks; continue using the public conversion instead of duplicating pixel/composition logic.

**Alternatives considered**: Calling SDL from the producer (violates main-thread rendering requirements), polling unlocked pointers (tearing/races), waiting for a renderer acknowledgment on each frame (couples emulation speed and shutdown to displays), or writing a new conversion pipeline (duplicates core behavior).

**Primary source**: [SDL2 main-thread rendering requirement](https://wiki.libsdl.org/SDL2/SDL_RenderPresent). Core sources above are local, pinned evidence.

## 5. Integer scaling and renderer behavior

**Decision**: Use explicit destination rectangles in renderer-output pixels, RGBA32 streaming textures, `SDL_BLENDMODE_NONE`, and per-texture `SDL_ScaleModeNearest`. Verify the selected texture scale mode. Prefer an accelerated renderer, with a software-renderer fallback if creation or nearest mode cannot be supported. Fail if neither can satisfy the contract.

For dual output, fit each 256×192 image independently. For single output fit a 256×384 vertical canvas or 512×192 horizontal canvas with one common factor. Formula and tie-breaking are fixed in [data-model.md](data-model.md). No logical-size viewport, renderer fractional scale, window-size coordinate conversion, gap, or postprocessing layer is needed. Clear black every present. Query dimensions after fullscreen creation and relevant size/display events.

Do not request per-renderer blocking vsync initially: serial waits on two renderers can reduce output cadence. Keep the core's existing pacing and render the newest available pair. Event polling must continue while loading or paused and use a short finite timeout when no new pair exists. The contract does not promise synchronized physical scanout across independent displays.

Compositor fractional scaling can alter the final physical pixels even when SDL rectangles are correct. Hardware acceptance requires a native-resolution, nonfractionally rescaled output path; reject a blurred or stretched acceptance result and correct the deployment environment or backend before sign-off. Texture/geometry checks alone are insufficient proof of sharp physical output.

**Alternatives considered**: Bilinear filtering/fractional fill (explicitly disallowed), cropping to eliminate borders (disallowed), and SDL logical integer scaling (valid helper but less direct for independent destinations and layout rectangles).

**Primary sources**: [Texture scaling](https://wiki.libsdl.org/SDL2/SDL_SetTextureScaleMode), [output pixel sizes](https://wiki.libsdl.org/SDL2/SDL_GetRendererOutputSize), [integer logical scaling](https://wiki.libsdl.org/SDL2/SDL_RenderSetIntegerScale), and [backbuffer clearing/main-thread presentation](https://wiki.libsdl.org/SDL2/SDL_RenderPresent).

## 6. Configuration dependency and schema

**Decision**: Put exactly the new selectable preferences in `video`: `display_mode`, `swap_screens`, and `single_screen_layout`. Defaults are `auto`, `false`, and `vertical`. Fullscreen and nearest integer scaling are fixed policies, not extra user-selectable toggles. Missing object or members apply defaults without rewriting existing files. A present non-object `video`, including null, or a wrong member type/value is an error.

**Rationale**: This directly encodes the user's clarification while retaining distinct `auto` and `dual_screen` values. Keep the existing `visuals` settings in their existing group. Do not introduce aliases at the root or under `visuals` for these new options.

**Dependency finding**: Feature 003 is a specification only. The current runner has `--data-dir` and no JSON configuration implementation. Implement feature 003 before video configuration integration and end-to-end acceptance. This plan does not quietly claim that foundation is present, implement it, or create a temporary second configuration parser. Choose `--config PATH` as the canonical frontend configuration override for the integration contract; feature 003 left that spelling to planning. Use `--frontend-config` on the device deployment helper because its existing `--config` already selects private device connection settings.

**Alternatives considered**: New `visuals` properties or root options (contradicts clarification), a separate video file/parser (duplicates the configuration foundation), and runtime menu/hotkeys (out of scope).

## 7. Build, packaging, and permitted workflows

**Decision**: Link shared target SDL2 deliberately and rename the executable to `seedlessds` now that video is always presented. Keep Android GL presentation excluded and retain the software framebuffer ABI objects. Rename the replacement shim source to reflect software buffers rather than a headless runtime.

Use target SDL CMake configuration from the native installation, Ubuntu/Debian AArch64 multiarch package paths, or a custom ARM64 sysroot. Reject host-architecture library discovery. The application runtime inventory must declare SDL's actual version/linkage/SONAME and its OS/backend runtime dependencies, including dynamically loaded graphics libraries that ELF `DT_NEEDED` does not describe. Retain Android/OpenSL and unresolved-core-GL checks; replace the old libc-only allowlist with an explicit declared dependency model, not a blanket acceptance of every shared library. SDL's internally loaded Linux GL/EGL drivers are distinct from restoring the removed core Android GL frontend.

**ROCKNIX compatibility refinement**: The target device already provides SDL2 2.32.10 at the stable `libSDL2-2.0.so.0` SONAME. A ROCKNIX SDK/sysroot is therefore unnecessary for SDL: cross-link against Ubuntu/Debian's AArch64 SDL2 development package (or a custom AArch64 sysroot), keep API usage at the SDL2 2.0.12 baseline, and dynamically resolve SDL2 from the device at runtime. Never copy SDL2 into the app bundle or overwrite the device library. The local `~/projects/DSperate` source follows the corresponding general pattern: GNU AArch64 cross compiler in `cmake/aarch64-linux-gnu.cmake`, SDL discovery through pkg-config/CMake in root `CMakeLists.txt`, and dynamic SDL linking in `src/frontend/sdl/CMakeLists.txt`; it does not vendor an SDL SDK. This validates the approach, while its exact release build environment is not inferred from the local source alone.

`CMakeLists.txt` invokes `scripts/prepare-core.sh`, whose missing-submodule path currently runs `git clone`. Make preparation fail fast on missing local dependencies without attempting remote acquisition before running configure in an agent workflow. Existing pins, local source/caches, patch checksums, and source inventory remain authoritative. No build is run during planning. Package installation remains a manual user operation if elevated privileges are required.

Update `scripts/audit-runtime.py`, `scripts/debug-device.py`, `scripts/validate-video.sh`, setup checks, licensing inventory, and README together with SDL integration. Device preflight must check the selected shared runtime and graphical access, not just glibc/interpreter compatibility.

**Primary sources**: [SDL Linux build/runtime loading behavior](https://wiki.libsdl.org/SDL2/README-linux), [CMake target integration](https://wiki.libsdl.org/SDL2/README-cmake), and local build/tooling files above.

## Research closure and evidence limits

All design choices needed for Phase 1 are resolved. The real RG DS Plus graphical backend, installed SDL runtime, display enumeration, and physical pixel path have not been measured. Existing sanitized headless records establish ARM64 execution, not video support. These are explicit implementation acceptance gates with diagnostic steps in [quickstart.md](quickstart.md); they are not claimed as passed. Failure of the target's two-output probe requires fixing the deployment/backend or revising the design with evidence before declaring the feature implemented.
