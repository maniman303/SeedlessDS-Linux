# Quickstart Validation: Fullscreen Video Output

This guide validates the fullscreen SDL2 frontend, its shared feature 003 JSON configuration, and its `video` preferences. Commands assume an existing Linux terminal at the repository root.

## Prerequisites and workflow gate

- Have the feature 003 JSON configuration foundation and this feature built. Provide a legal, readable test ROM, valid BIOS images, an otherwise valid frontend JSON configuration, and writable save destinations. Neither ROM nor BIOS content is supplied by this project.
- Use native ARM64 Ubuntu or the established amd64/WSL2 cross toolchain. Have CMake ≥3.28, Ninja, Python 3.12+, and target SDL2 ≥2.0.12 development/runtime packages. Prefer the maintained distribution version. SDL headers/libraries for amd64 do not satisfy ARM64 cross linking.
- For amd64 cross builds, use Ubuntu/Debian's `libsdl2-dev:arm64` multiarch package or supply an ARM64 sysroot containing SDL2 headers, shared library, pkg-config metadata, and target graphics dependencies. CMake isolates pkg-config to the AArch64 package path and never consumes amd64 SDL2. Package/sysroot provisioning is a user/environment prerequisite, not an automatic repository download.
- `scripts/prepare-core.sh` requires the pinned core and dependency sources to be available locally and fails clearly when a required local input is missing. Configure does not initialize submodules or contact Git remotes.
- If native SDL prerequisites are missing and installation requires elevation, the user may manually run `sudo apt-get install libsdl2-dev`. For cross builds, installing `libsdl2-dev:arm64` may require enabling Ubuntu's arm64 multiarch package architecture first; package and apt-source changes remain explicit user operations. Agents must stop before privileged operations and resume dependent work only after the user confirms success.
- For hardware runs use an existing authorized graphical session or functioning KMSDRM environment on real ARM64 Linux hardware. Do not elevate the emulator, change DRM permissions, or assume SSH alone provides graphical access.
- Use existing ignored device connection configuration where SSH is needed. Never display connection credentials. These steps do not create commits or contact Git/GitHub remotes.

## Build and local checks

After confirming the workflow gate above, choose the applicable build.

Native ARM64:

```sh
cmake --preset linux-arm64-native
cmake --build --preset linux-arm64-native
ctest --preset linux-arm64-native
file build/linux-arm64-native/bin/seedlessds
readelf -d build/linux-arm64-native/bin/seedlessds
```

Cross build from amd64/WSL2, replacing the sysroot path with your prepared target root:

```sh
export SEEDLESS_TARGET_SYSROOT=/absolute/path/to/arm64-sysroot
cmake --preset linux-arm64-cross
cmake --build --preset linux-arm64-cross
file build/linux-arm64-cross/bin/seedlessds
aarch64-linux-gnu-readelf -d build/linux-arm64-cross/bin/seedlessds
python3 -m unittest discover -s tests -p 'test_video_tooling.py'
```

Use a clean preset build directory when changing sysroots. The binary must be AArch64; runtime inventory must match its current hash, explicitly declare SDL2 and backend requirements, and retain Android/OpenSL rejection. Do not run AArch64 binaries on amd64 as a substitute for native tests. SDL-dependent CTest probe entries must support a separate manual hardware path rather than failing all non-graphical test sessions.

## Validate the graphical environment first

On the target, from its existing graphical session (or owner-provided corresponding environment), run the planned test-only executable:

```sh
./build/linux-arm64-native/bin/seedlessds-video-probe --seconds 15
```

For a transferred cross artifact, use its deployed probe path instead. The probe uses the production SDL display/renderer/layout path, with generated checkerboards, solid color channel patches, and distinct changing top/bottom frame IDs. It accepts `--seconds N` and an optional `--config PATH`; absent config uses only video defaults and requires no ROM/BIOS. No probe options are added to the gameplay executable.

Expected: two fullscreen, independent outputs on the dual-screen target; one default vertical canvas on a genuinely single-output setup. Record SDL versions/backend, physical panel order versus indices, actual pixel sizes, renderer, scale, and rectangles. Confirm that a two-panel device does not merely expose a mirrored/single output. Inspect one-source-pixel checkerboards at their integer-expanded block boundaries. A blurred pattern fails physical acceptance even if readback is sharp.

If an output fails enumeration/targeting, stop dual-screen acceptance and diagnose the shipped SDL/backend, existing session access and drivers. A successful one-panel fallback is not evidence that the primary dual-panel requirement passed. The probe does not require privileged execution or new packages at runtime beyond declared prerequisites.

## Prepare an isolated full configuration

On the machine running the emulator, set the ROM path and copy your known-valid full feature 003 configuration to a temporary validation directory:

```sh
export SEEDLESS_VIDEO_ROM=/absolute/path/to/test.nds
export SEEDLESS_VIDEO_WORKDIR=$(mktemp -d /tmp/seedlessds-video.XXXXXX)
export SEEDLESS_VIDEO_CONFIG="$SEEDLESS_VIDEO_WORKDIR/config.json"
cp "$HOME/.config/seedlessds/config.json" "$SEEDLESS_VIDEO_CONFIG"
python3 - <<'PY'
import json, os
from pathlib import Path
path = Path(os.environ['SEEDLESS_VIDEO_CONFIG'])
config = json.loads(path.read_text())
config['video'] = {'display_mode': 'auto', 'swap_screens': False, 'single_screen_layout': 'vertical'}
path.write_text(json.dumps(config, indent=2) + '\n')
PY
```

Verify that BIOS/ROM/save paths in that copy are valid on this machine. For SSH deployment the configuration is a target file with target paths, not a host file to be forwarded blindly. See [configuration/output contract](contracts/video-output.md) for helper `--frontend-config` versus device connection `--config`.

Launch from an ARM64 build tree:

```sh
sha256sum "$SEEDLESS_VIDEO_CONFIG"
./build/linux-arm64-native/bin/seedlessds --config "$SEEDLESS_VIDEO_CONFIG" --frames 120 "$SEEDLESS_VIDEO_ROM"
sha256sum "$SEEDLESS_VIDEO_CONFIG"
```

Expected: fullscreen changing top/bottom content, unchanged config hash, exit 0 and `result=completed frames=120 requested=120`. Replace only the binary path for a deployed cross build. The `seedlessds` executable always opens fullscreen video.

## Mode, layout, swap, and sharpness matrix

Edit only the temporary file's `video` members between launches. The following helper command changes all three values; replace the three positional values to cover the matrix:

```sh
python3 - "$SEEDLESS_VIDEO_CONFIG" single_screen horizontal true <<'PY'
import json, sys
from pathlib import Path
path = Path(sys.argv[1])
config = json.loads(path.read_text())
config['video'] = {'display_mode': sys.argv[2], 'single_screen_layout': sys.argv[3], 'swap_screens': sys.argv[4] == 'true'}
path.write_text(json.dumps(config, indent=2) + '\n')
PY
./build/linux-arm64-native/bin/seedlessds --config "$SEEDLESS_VIDEO_CONFIG" --frames 120 "$SEEDLESS_VIDEO_ROM"
```

Repeat on genuinely one-output and two-output setups:

| Mode | Outputs | Expected destinations |
| --- | --- | --- |
| `auto` | 1 | Both images on primary |
| `dual_screen` | 1 | Both images on primary; configured mode retained |
| `single_screen` | 1 | Both images on primary |
| `auto` | 2 | Separate outputs |
| `dual_screen` | 2 | Separate outputs |
| `single_screen` | 2 | Both images on primary; no frontend output on secondary |

For each row test `swap_screens` false/true. In effective single output test both `vertical`/`horizontal`. In effective dual output change layout once and verify it has no drawing effect. Use the configured probe (`seedlessds-video-probe --config "$SEEDLESS_VIDEO_CONFIG" --seconds 15`) when a ROM does not give clearly distinguishable test images.

Check black borders, aspect, no cropped source pixels, no overlaps, and exact integer geometry using the examples/formula in [data-model.md](data-model.md). Exercise nonmultiple sizes, mismatched dual resolutions, and the 1x minimum in geometry tests/probe environments. With a 640×480 output, expect dual 2x per screen and either single-display layout at 1x. Verify red/green/blue patterns for channel-order mistakes and paired frame IDs for tearing/mixed publication.

A forced single-screen configuration on dual hardware covers that mode but does not replace the one-usable-display auto/dual fallback check. Use a real single-output device or an owner-controlled one-output session to validate fallback; no panel-disabling operation is automated by this guide.

## Defaults and failures

Using disposable copies of the full configuration:

1. Remove `video`, then remove individual members, then use `{}`. Expect documented defaults with unchanged file hash. Generate a fresh full default file in an isolated writable location via feature 003's creation behavior; verify all three video defaults are under `video`.
2. Try `video: null`, a non-object, invalid mode/layout strings, and nonboolean swap. Expect field-specific diagnostics before SDL/core execution; existing file remains unchanged. Retest default/custom file precedence with conflicting video preferences.
3. Verify `--help` succeeds without ROM, usable config, or graphical session. Invalid CLI/missing ROM must exit before video initialization.
4. Run without an accessible graphical environment, and run normal gameplay with `SDL_VIDEODRIVER=dummy`. Expect a clear video failure, exit 1 and `result=failed frames=0 operation=video`; no invisible continuing session.
5. Exercise test-injected second-window/renderer/texture/allocation failures and sizes below the selected layout's 1x bounds. Verify partial resources are released, no fallback conceals two-output failure, and no core worker remains.
6. On an owner-controlled test setup, remove a selected output or invalidate fullscreen during play. Expect a diagnostic, clean failed shutdown, and released resources; no windowed gameplay.
7. Test while the worker is loading, paused at a boundary, and publishing. Inject stop/failure at those states; verify no hook use-after-free, reversed lock order, or display-acknowledgment deadlock. Repeat supported visual/composition settings from feature 003.

## Real hardware sessions and sign-off

Run without `--frames` for at least five minutes on the dual-screen ARM64 target and on a single-output setup. Confirm ongoing changes on both images, stable fullscreen, sharp pixels, and bounded mailbox memory. Test SIGINT and SIGTERM separately; existing records/exits must remain `result=interrupted ... signal=SIGINT`/130 and `SIGTERM`/143. Check configured saves still write/reload correctly. Test the normal SDL close request for a clean exit.

Record sanitized evidence under ignored `build/validation/device/`: artifact/inventory hash, SDL/runtime/backend, kernel/architecture, display enumeration and physical roles, mode/layout/swap, rectangle/scale results, test duration, sequence diagnostics, exit/result records and human physical observations. No credentials or ROM/BIOS content. Mark each SC-001 through SC-006 with concrete evidence or an explicit failure. Existing headless records cannot close these checks.

Finish only after the feature 003 dependency, native/cross builds, configuration/mode/geometry/lifecycle tests, and physical one/two-display checks pass. Backend failures remain implementation work requiring an evidenced fix or design revision; do not relabel a single-panel run as dual-screen success.
