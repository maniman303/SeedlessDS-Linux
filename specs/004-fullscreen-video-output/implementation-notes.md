# Implementation Notes: Fullscreen Video Output

## Feature 003 configuration mapping

T001 is complete for the shared configuration foundation required by video.
Feature 004 extends this loader and does not add a second configuration parser.

- `src/launch_options.c` parses one nonempty `--config PATH` override, rejects
  duplicate/unknown options, and leaves help independent of file access.
- `src/runner.c` selects the custom path or default through
  `seedless_config_load_or_create`, then resolves validated runtime paths before
  core creation.
- `src/config.c` owns `$HOME/.config/seedlessds/config.json` selection,
  first-run default generation, strict typed object validation, and immutable
  existing-file loading. It validates separate absolute `firmware.bios_arm7`
  and `firmware.bios_arm9` paths and their expected readable file sizes.
- The existing groups are `visuals`, `firmware`, `user`, and `emulation`.
  `visuals` has the five booleans/count setting, `user` has nickname and
  separate birthday month/day/color, and `emulation.save_dir` participates in
  CLI-over-config save destination precedence.
- `src/core_config.c` applies visual/profile settings and fixed RTC/raw-save
  policy bits. Feature 004 must leave existing `visuals` intact and add only
  the new preferences under top-level `video`.

Feature 003's own remaining core bridge/save integration and hardware
acceptance tasks are still unchecked in `specs/003-frontend-json-config/tasks.md`.
They are not a reason to create another loader; any incompatibility discovered
while adding `video` will be recorded as a dependency gap.

## Setup work

T002–T004 are complete. Core preparation now uses only a pinned local checkout
or cache, SDL2 discovery uses an installed pkg-config target with cross-sysroot
isolation, and setup guidance leaves package installation to the user.

Validation completed: `tests/test_prepare_core_offline.py` proves missing local
Lua inputs fail without any `git clone`; the existing device diagnostic tests
pass; shell syntax and ShellCheck pass; an isolated CMake configure resolved
installed SDL2 2.32.10 and exposed `SDL2::SDL2`; and `scripts/setup-ubuntu.sh
--cross --dry-run` printed manual host/sysroot guidance without running package
installation. The feature 003 configuration foundation is present. A clean
cross configure was attempted after implementation and stops at target SDL2
discovery; details are recorded below.

## Feature 004 implementation and local validation

The frontend now initializes fullscreen SDL2 output after command/config/BIOS
preflight, converts completed core frames into owned paired RGBA images, and
renders the newest pair on the main thread. The video configuration remains
inside the shared loader's top-level `video` object. Integer geometry supports
dual outputs and vertical/horizontal one-output canvases, including destination
swaps and the specified one-output fallback. The legacy executable name is
retained for compatibility.

Native checks passed for layout, mailbox ownership/overwrite, bridge attachment
and teardown, video config validation/default generation, and SDL nearest-pixel
readback across both layouts and swaps. Python runtime-tooling (5 tests),
offline preparation (1 test), and device diagnostics (7 tests) passed. Shell
syntax, ShellCheck, and `git diff --check` passed. The geometry results and
scope limitation are recorded in ignored `build/validation/device/video-us3-geometry.json`.

The cross compiler and AArch64 SDL2 2.32.10 development package are available.
A clean cross configure and full 263-target build passed in the canonical
`build/linux-arm64-cross` directory. CMake downloaded and verified the pinned
zlib, libarchive, unarr, and LZMA SDK archives. The artifact is
`build/linux-arm64-cross/bin/seedlessds`, links `libSDL2-2.0.so.0` from
`/usr/lib/aarch64-linux-gnu`, and records only SDL2, libc, and the dynamic loader
as direct shared dependencies. Its maximum GLIBC requirement is 2.34. The
cross-safe CTest suite passed 2/2 tests, and the probe's nearest-pixel readback
passed under QEMU with the target SDL2 library; these checks do not establish
physical display access. Native ARM64 build and actual one/two-display hardware
acceptance remain unverified and are deliberately left unchecked in `tasks.md`.

## ROCKNIX SDL2 build compatibility

The prior sysroot-only assumption was too restrictive. Local inspection of
`~/projects/DSperate` found an AArch64 GNU cross-toolchain, SDL2 discovery via
pkg-config/CMake, and shared SDL2 linkage; SDL is not vendored by that project.
The target's supplied `/usr/lib` listing confirms SDL2 2.32.10 and the
`libSDL2-2.0.so.0` SONAME. This frontend now supports Ubuntu/Debian's target
multiarch pkg-config directory as well as a custom `SEEDLESS_TARGET_SYSROOT`.
The intended build uses the distro's AArch64 development package and leaves
runtime SDL2/backend loading to ROCKNIX.

A fake pkg-config fixture first verified that `cmake/SDLVideo.cmake` searches
only `/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig` without a
sysroot prefix. After the target package became available, the actual cross build
found SDL2 2.32.10, compiled all 256 production build steps, and generated the
runtime inventory. QEMU ran `--help`, the pixel readback test, and a dummy-video
rejection against the AArch64 SDL2 library. `scripts/setup-ubuntu.sh --cross
--check` and CTest’s two cross-safe Python tests passed.
