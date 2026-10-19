# Video Configuration and Output Contract

This contract refines [spec.md](../spec.md). It describes the feature after implementation; it does not imply the current headless runner already provides these interfaces.

## Configuration

Extend the selected frontend JSON file with:

```json
{
  "video": {
    "display_mode": "auto",
    "swap_screens": false,
    "single_screen_layout": "vertical"
  }
}
```

This is a video fragment, not a complete bootable configuration. All other configuration requirements remain defined by feature 003. Normative types and enum spellings are in [video-config.schema.json](video-config.schema.json).

- `display_mode`: `auto` (default), `dual_screen`, `single_screen`. Auto and dual screen are separate valid persisted choices; each uses two usable outputs when present and one otherwise.
- `swap_screens`: boolean, default `false`.
- `single_screen_layout`: `vertical` (default) or `horizontal`. Vertical is top above bottom; horizontal is top left of bottom. Swap reverses image positions in either arrangement.
- Missing `video` or individual members apply defaults without rewriting the selected file. Null is invalid. New default configurations include the entire object and all three default members.
- Every new configurable video output preference belongs under `video`; no duplicate root/`visuals` aliases are introduced. Existing core `visuals` settings retain their locations.
- Fullscreen, integer scaling and nearest sampling are fixed policies; this contract adds no configuration switches for them and no new display command-line options.
- A setting error identifies its JSON path, for example `video.display_mode`, and exits before SDL initialization/emulation. The overlay schema allows other fields so it can coexist with the common configuration schema; unknown-key policy remains the common loader's responsibility.

## Launch selection

Keep the existing executable filename `seedlessds-headless` for compatibility; it now presents fullscreen video for normal gameplay. After feature 003's foundation is implemented, the frontend configuration override is spelled `--config PATH`; the existing ROM, help, `--frames N`, and configured save behavior continue under that feature's contract. Do not add a `--headless` escape hatch or retain `--data-dir` as a substitute for feature 003 configuration.

For the existing device deployment helper only, `--frontend-config PATH` means an existing configuration on the target to forward to the frontend's `--config`. Its existing `--config` continues to mean private device connection settings. The two paths must not be confused or copied into diagnostic records as credentials.

## Display resolution and placement

| Usable SDL physical outputs | Requested mode | Presentation |
| --- | --- | --- |
| 0 | Any | Diagnostic and clean failure |
| 1 | Any | Both DS images on display index 0 using selected layout |
| ≥2 | `auto`, `dual_screen` | One DS image on each of display indices 0 and 1 |
| ≥2 | `single_screen` | Both DS images on display index 0 using selected layout |

Indices are session-local. Primary is SDL display 0 and secondary display 1; further outputs are unused. Normal assignment is top to primary/first layout slot, bottom to secondary/second slot. `swap_screens` exchanges destinations. Layout is validated even when dual output ignores it.

Create fullscreen-desktop windows targeted at the selected display indices. Verify fullscreen status and distinct actual destination displays before gameplay. Fail if a required destination cannot initialize; do not reinterpret renderer errors as a one-display fallback. Record physical panel correspondence in RG DS Plus acceptance evidence. If the OS primary is the physical bottom panel, document/use the existing swap setting rather than invent another preference.

SDL dummy/offscreen backends cannot satisfy normal gameplay. A mirrored backend exposing one logical output cannot establish dual-screen support on a physical two-panel target. Distinct hardware outputs must be demonstrated by the validation probe.

## Pixel output

Both source images are the core's 256×192 RGBA conversion, not raw strided core page pointers. Compute geometry using actual renderer pixel dimensions as specified in [data-model.md](../data-model.md).

- Dual: independent maximum fitting positive integer factor per display.
- Single: one factor for the entire 256×384 vertical or 512×192 horizontal canvas.
- Center the image/canvas, with any odd extra border pixel on the right/bottom; clear unused space opaque black.
- Set and verify nearest texture sampling and disable blending. No crop, stretch, fractional transform, custom shader, or windowed game output.
- If 1x cannot fit, fail before emulation. Do not automatically change the selected layout merely to make it fit.
- Physical output acceptance includes the compositor/display path; application screenshots alone cannot establish final pixel sharpness.

A new pair replaces the previously pending pair; no promise is made to show every emulation frame or synchronize physical scanout between displays. Paired images must come from one producer flip. `--frames` counts scheduler boundaries exclusively, independent of presentation/diagnostic counters.

## Runtime and shutdown

Video/display initialization precedes core execution. Help and CLI validation do not initialize SDL. While loading/running/paused, the main loop services events at a finite idle interval and checks existing termination/core-failure state. SDL operations occur on the main thread only.

Chosen display-removal behavior is a diagnostic and clean failed shutdown. Size changes recompute integer geometry if fullscreen and destination identity remain valid; <1x or inability to maintain fullscreen causes failed shutdown. Close/SDL quit requests normal clean shutdown. SIGINT/SIGTERM preserve the current interruption records and exits 130/143.

The worker never waits for consumer presentation. Cleanup stops/joins execution, safely detaches the frame hook, destroys core and frame storage in ownership order, then releases SDL resources on the main thread. Failed partial startup follows the same ownership rules. No orphan fullscreen window or callback into freed storage is acceptable.

## Diagnostics and exit behavior

Preserve existing result records:

```text
result=completed frames=N requested=N
result=interrupted frames=N signal=SIGINT
result=interrupted frames=N signal=SIGTERM
```

Video startup/runtime failures produce a readable stderr message and:

```text
result=failed frames=N operation=video
```

`N=0` for startup before core execution; otherwise it is the final scheduler-boundary count. Exit status is 1 for video failure, 0 for successful bounded completion/normal window close, and existing 130/143 for signals. Configuration/usage failures retain the common frontend's established codes. A simultaneous failure and signal must follow the runner's existing interruption precedence, without hiding the preceding video error in stderr.

Startup diagnostics report requested/effective mode, fallback reason when applicable, swap/layout, SDL versions, selected backend, enumerated/selected display indices and names/bounds, actual output sizes, renderer drivers, integer factors and rectangles. Keep diagnostics free of credentials, ROM/BIOS content, and private connection settings. Report SDL error context for failed resource/API calls. A display probe may add test-only pattern identifiers and publication/presentation counts.

## Build/runtime contract

SDL2 ≥2.0.12 is an explicit target runtime dependency, discovered from installed target CMake metadata/sysroot. No automatic SDL fetch or host-library fallback. The runtime inventory must declare actual SDL linkage/version/SONAME, direct dependency closure, and dynamically loaded backend requirements. Device preflight verifies shared library resolution and graphical access; a successful ELF load alone is insufficient.

Before configuring, missing pinned local core/submodule sources must fail without remote acquisition. No GitHub action, remote Git operation, commit, or elevation is authorized by this contract.
