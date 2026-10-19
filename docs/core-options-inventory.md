# SeedlessDS-Core options inventory

This inventory describes the settings and runtime capabilities available to a Linux frontend in the pinned `external/SeedlessDS-Core` source. It separates core support from frontend work: the repository's `seedlessds-headless` program is a bounded runner, not an interactive emulator frontend. It accepts a ROM plus Linux JSON config/save options, validates BIOS files, and discards audio output; fullscreen video is covered separately by the video-output feature.

## Support levels

- **Core API**: a public interface or core configuration exists; the frontend still needs to expose and persist it.
- **Core config**: the setting exists in `nds_config_t` or the packed config word, but there is no dedicated public setter for that individual field.
- **Frontend work**: presentation, device selection, configuration UI, file management, or OS integration must be implemented in SeedlessDS-Linux.
- **Not confirmed**: a field exists but its accepted values or user-facing semantics are not adequately established by the inspected source. Avoid presenting guessed values as supported choices.

## 1. Emulation and performance

| Option | Core support and notes | Frontend-facing choice |
|---|---|---|
| CPU execution backend | Core command/config handling supports recompiler/JIT and interpreter. JIT runtime flag is initialized during machine setup. | Automatic/default, JIT, interpreter (debug/compatibility fallback). Confirm architecture availability; this project targets ARM64. |
| Frame pacing / frameskip | `nds_set_frameskip()` exists; config has frameskip type, value, and safe mode. `nds_control_begin()` also accepts no-frameskip. | Auto vs fixed/manual frameskip and safe pacing, once value ranges are verified against implementation. A no-frameskip accuracy option is directly supported by control API. |
| Fast forward | Config contains enable and speed; command parsing also supports fast-forward. | Toggle plus speed multiplier or cap. Exact speed mapping needs to be checked in the core before exposing numeric presets. |
| Run limit | ROM open accepts `run_limit_us`; config unpack maps it to a millisecond limit. | Optional bounded run duration; useful for automation, not necessarily a normal settings screen. |
| Benchmark | ROM open and config parser support benchmark frame count and benchmark modes; benchmark turns on fast-forward. | Diagnostic/benchmark command or developer menu. |
| 3D threading | `threaded_3d`, `thread_count`, and compose-thread API exist. | Auto/off/on or worker count after validating supported limits and performance on target hardware. |
| Internal 3D resolution | `hires_3d` config and `nds_set_internal_resolution()` exist. | Native and higher internal resolution presets, with target performance testing. |
| 3D edge/render options | Config includes `disable_edge_marking`, `fix_main_screen`, and `auto_trim`. | Compatibility/rendering toggles; names need clearer user labels and game testing. |
| Game-card limit | `ignore_gamecard_limit` is configurable. | Compatibility toggle; explain only if its effect is established by core behavior. |
| ROM loading | Core can inspect ROM type, size, icon, and title metadata; archive open accepts `auto_trim`. ROM cache choice is represented by `in_cache_dir`/`rom_in_cache_dir`. | Library metadata, supported archive/file handling as provided by core, optional cache policy. |

## 2. Firmware, system setup, and user profile

| Option / requirement | Core support and notes | Frontend-facing choice |
|---|---|---|
| Required BIOS | Linux runner currently expects ARM9 and ARM7 BIOS images under `<data-dir>/system/`; replacement BIOS filenames are also accepted by the runner validation. | First-run setup status, file picker/instructions, readable/size validation, and clear missing-file diagnostics. Preserve user-supplied files; do not bundle copyrighted firmware. |
| Firmware image | Core defines `nds_firmware_modified.bin`; data path constants include firmware. Boot-from-firmware config exists. | Optional firmware path/status and boot mode when the core's complete firmware behavior is available. |
| Firmware boot | `boot_from_firmware` config field. | Boot directly vs boot through emulated firmware, after confirming required firmware availability and behavior. |
| DS profile | Public `nds_set_firmware_user()` accepts nickname and packed profile data; config has language, favorite color, birthday month/day. | Nickname, language, favorite color, birthday. Validate packed field interpretation and supported language enum before UI. |
| Clock/RTC | `rtc_system_time` config field; RTC state is serialized. | System clock vs configured/fixed clock is a candidate setting; verify the field semantics first. |
| System data directory | Core data path constants identify system files, and Linux runner uses a data root with `system/` plus `User/`. | Data-root selection, writable-space/status display, per-system assets and database placement. |
| Cheats database | Core loads `usrcheat.dat` from user data and exposes built-in/custom cheat APIs. | Show available game cheats, folder organization, enable/disable, add/remove custom codes; core has APIs for these operations. |
| Lua/scripts | Config has `lua_enabled`; script API exposes activity, overrides, axes, and rotation. Script file management/UI is frontend work. | Optional scripting toggle and script management only if script runtime/data paths are safely exposed. |

## 3. Display and layout

The public core API gives the frontend top/bottom screen buffers, RGBA conversion, screenshots, frame size, draw/compose calls, present mode, screen snapshots, and internal resolution controls. A Linux SDL presentation loop, window and renderer are frontend responsibilities.

| Option | Support and practical choices |
|---|---|
| Dual-screen layout | Core screen mode supports a wide mode and frontend script overrides for layout/swap/overlay. Frontend can offer vertical stack, side-by-side, or single screen if composed from buffers. |
| Screen selection / swap | Screen-swap override and two screen buffers are present. Offer top only, bottom only, both, and swap where the layout supports it. |
| Scale and aspect | Core reports frame dimensions/scale; SDL frontend should fit integer or fractional scaling while preserving DS screen aspect ratio. |
| Rotation | Script rotation field/API is present; mapping of supported rotations to ordinary frontend controls is not fully established. |
| Fullscreen / window size | OS/window policy; no core setting. SDL implementation can offer windowed/fullscreen and resolution or scale choices. |
| Filtering / shaders | Core has FX/ext-FX recipe APIs and shader-related code, but Linux headless stubs reject shader recipe loads. Renderer/backend work is required before offering filters. |
| Overlays / OSD | Core has script overlay override and a performance word API. Menus, FPS/status display, notifications and controller hints belong in frontend. |
| Capture | Core exposes screenshot and snapshot-to-slot/file calls. File dialogs, image encoding/extension, and capture destination policy are frontend concerns; inspect exact output pixel formats before selecting formats. |
| Color depth | Core config parser accepts 16- or 32-bit texture format; public config also carries screenshot channel-order bit. Offer only after validating the SDL upload path and color correctness. |

Constitutional requirement: support a useful single-screen mode and make dual-screen devices a primary use case. Device-specific hinge/screen behavior must not prevent launch on generic Linux hardware.

## 4. Controls and input devices

The public `nds_input_t` accepts a key bitmask, mask, touch-down flag and touch coordinates. Separate API calls pass accelerometer and gyro values, rumble state can be queried, and hinge state can be set. The core also has a button profile/turbo pattern and input recording/playback internals.

- **Button mapping**: frontend-owned mapping from keyboard, gamepad, or handheld controls to DS buttons; offer rebinding and reset-to-default.
- **Keyboard/gamepad navigation**: map directions, A/B/X/Y, Start/Select, L/R, and system actions; these are frontend mappings, not core settings.
- **Touch**: map pointer/touch coordinates to the bottom-screen rectangle, including scaled/rotated layouts; expose press/release and optional mouse input.
- **Turbo**: core `button_profile` changes pulse patterns for masked buttons. Make turbo assignment/configuration explicit if using `keys_mask`; the exact profiles are implementation detail and should be tested before labeling.
- **Lid/hinge**: `nds_hinge()` and internal lid state support open/closed behavior. Map a hotkey or device sensor if useful.
- **Motion input**: accelerometer and gyro APIs exist. Bind supported host sensors when present; otherwise leave disabled and allow manual mapping only if sensible.
- **Rumble**: core reports rumble state. The frontend needs Linux gamepad/haptic output and a user toggle/intensity policy.
- **Input recording/playback**: core parser supports input-record and input-playback paths. UI, file selection, compatibility, and user-facing file extension need validation.
- **Pause/reset/quit**: public core calls support pause, reset, and quit; provide consistent hotkeys/menu actions and save prompts if desired.

## 5. Audio and microphone

Core APIs include volume and white-noise controls; config includes sound enabled and audio mode. However, this repository's Linux adapter deliberately discards audio and supplies a failed/silent microphone recorder. Therefore these are **core capabilities, not currently usable frontend features**.

- Audio output device selection, mute, volume, latency/buffer size, and channel policy require a real SDL/Linux audio backend.
- `nds_audio_volume()` and packed config volume are core controls; audio mode/config bits need interpretation before exposing choices.
- Microphone input needs a host capture backend. Current Linux code marks microphone initialization failed and provides no capture.
- Audio capture/recording is not established as a public consumer feature by the inspected API; control-runner audio capture should not be confused with ordinary frontend playback.

## 6. Save data and formats

The core distinguishes cartridge backup saves from savestates. They have different formats, paths, and user expectations.

| Data | Core behavior | Frontend options / caveats |
|---|---|---|
| Cartridge save (battery backup) | `raw_sav_format` selects `.sav` vs `.dsv` under `User/backup/`; default is `.dsv`. Core detects save type/size from game database and mounts backup memory. | Keep per-game save path stable; optionally choose raw `.sav` or `.dsv` only if external compatibility is required. Do not call these savestates. Consider import/export and backup copy UI. |
| Savestates | Public save/load by integer slot; files are `<save_dir>/savestates/<rom-name>_<slot>.dss`. Core file header magic is `SDS2`, format version 1, stream version 15; it has sectioned state data and optional flags including compression/screens/backup memory. | Save/load slot UI, slot selection, timestamps and preview screenshots. Current core slot path uses `.dss`; do not promise interchangeability with other emulator state formats. |
| Savestate controls | Config has enable, copies, extra-data fields; API supports autosave interval. | Enable/disable, autosave interval, number of retained copies, include extra data, per-game state slots. Confirm copy semantics and slot bounds before exposing. |
| Automatic save | Core backup persistence is handled in its user save directory. Savestate autosave is available; input code also writes autosave to slot 9 when interval expires. | Expose savestate auto interval and visible completion/failure status; preserve battery-save flush on clean shutdown. |
| Save location | Core start parameters receive a save directory; the current Linux runner uses `User/`. | Configurable user data/save root and per-game organization should be frontend policy. Keep paths writable and stable across upgrades. |
| Save-state screenshots | API supports snapshots by slot or file and state format optionally has screen section. | Offer preview thumbnails only after establishing a documented image format and lifecycle. |
| Rewind | No public rewind-buffer API was found in the inspected header. | Do not list rewind as currently supported; it would require separate core/frontend work. |

## 7. Library, metadata, and content actions

- ROM path is mandatory for current runner; core supports NDS ROM inspection, type/size, icon and title extraction, plus GBA insertion.
- A frontend library can show filename, title, icon, ROM type and file size from core APIs. Recent games, favorites, search, sorting, and playlists are frontend-owned metadata.
- GBA slot-2 insertion exists in the core API. Actual GBA emulation/slot-2 devices depend on the supported core path and content availability; present only after testing.
- Cheat database lookup is rooted in user data. Per-game cheat selection and custom cheat editing can use public APIs.
- Per-game config files are named in core data path constants (`%s.cfg`); global config names also exist. The current runner does not expose a config-file management UI. A frontend should define precedence and persistence rather than silently assuming the core's Android-era configuration files are a portable UI contract.

## 8. Configuration and persistence recommendations

Keep the frontend's own settings in a versioned Linux-native configuration file (for example JSON or TOML) under the user's config directory, separate from ROMs, BIOS/firmware, battery saves, and savestates. Suggested groups:

1. **Paths**: library roots, system-data root, save root, screenshot/capture directory.
2. **Per-game emulation**: backend, frameskip, fast-forward, 3D resolution/threading, compatibility toggles, boot mode.
3. **Display**: layout, screen selection/order, scale, rotation, fullscreen, filtering, OSD.
4. **Input**: keyboard/gamepad bindings, touch mapping, turbo, hinge, motion, rumble.
5. **Audio**: backend/device, volume, latency, microphone (once implemented).
6. **Saves**: battery-save path/format policy, savestate slots/copies, autosave, previews.
7. **Firmware profile**: nickname and DS user profile fields.
8. **System**: language, clock policy, scripting and cheat preferences.

Use global defaults with optional per-game overrides. Validate settings when loading, retain unknown keys for forward compatibility where practical, and report whether a choice is active, unsupported by the current backend, or requires restart.

## 9. Current project gaps before presenting this as an emulator UI

1. Replace headless video/audio sinks with SDL video and audio backends while preserving headless operation where useful.
2. Add an application lifecycle and frontend UI: library, setup/asset checks, settings, in-game menu and status reporting.
3. Implement keyboard/gamepad/touch mapping and optionally sensors/rumble.
4. Expose supported configuration fields through a typed frontend config model instead of hard-coding packed bit positions in UI code.
5. Add safe per-game and global config persistence and migrations.
6. Add save management around the existing `.sav`/`.dsv` battery files and `.dss` state slots.
7. Verify uncertain config ranges, performance options, presentation modes, and firmware/RTC semantics against core implementation and device testing before assigning user-facing labels.

## Source references

- Public API: `external/SeedlessDS-Core/include/seedlessds/nds.h` and `control.h`.
- Core config fields and packed config mapping: `src/core/config.h`, `src/core/config.c`, `src/core/nds_api.c`.
- ROM setup and core save roots: `src/core/nds_api_rom.c`, `src/core/nds.c`, `src/core/data_paths.h`.
- Input/runtime handling: `src/frontend/input.c`, `src/frontend/input_record.c`.
- Save-state format and slots: `src/core/state_format.h`, `src/core/state.c`.
- Cartridge save format: `src/cart/cart.c`, `src/cart/backup.c`.
- Current Linux limitations: `src/platform/linux/headless_video.c`, `src/platform/linux/headless_audio.c`, and this repository's `src/runner.c`.


## Implemented Linux frontend configuration (SeedlessDS-Linux)

The frontend now exposes the bounded subset described in [`specs/003-frontend-json-config/contracts/configuration.md`](../specs/003-frontend-json-config/contracts/configuration.md): 3D threading/render toggles, explicit ARM7/ARM9 BIOS paths, optional firmware path, DS nickname/birthday month/day/color, and battery-save directory. The file defaults to `$HOME/.config/seedlessds/config.json`; CLI `--config` selects an authoritative alternative and `--save-dir` overrides the JSON save directory. JIT, real-time clock and raw `.sav` are fixed frontend policies. Other core features in this inventory remain unexposed.
