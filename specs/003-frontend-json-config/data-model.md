# Configuration Data Model

## Persisted configuration

`seedless_config_t` owns four typed structures corresponding exactly to the groups and fields in [the JSON contract](contracts/configuration.md). Owned strings have explicit init/free lifetimes. Preserve the persisted `emulation.save_dir` separately from the effective runtime value. Do not retain JSON node pointers after parsing.

- Visuals: booleans for threaded 3D, disabled edge marking, fixed main screen, and auto-trim; unsigned validated thread count.
- Firmware: owned ARM7/ARM9 and optional firmware image path strings.
- User: owned UTF-8 nickname plus validated UTF-16 representation/length for the core; integer month/day/color. No year.
- Emulation: owned save-directory string, possibly empty.

Every object/field is required; strict unknown-key handling keeps typos and attempted forbidden policies visible. Validate integer type and bounds before narrowing. Core limits constrain path lengths, Unicode code units, and worker counts, never unchecked C string copies.

## Launch options

`seedless_launch_options_t` holds ROM, optional config path, save override string plus a separate presence flag, optional uint64 frame limit plus presence flag, and help outcome. Parse errors carry an option name and reason. CLI-owned copies or argv borrows have a documented lifetime through runtime resolution.

## Effective session settings

An immutable `seedless_runtime_config_t` owns canonical ROM/config/asset paths, the resolved direct `.sav` path, applied visuals and user profile, and fixed policies. Relationship: one selected JSON plus one launch request produce one session. Config never changes because of a launch override. Resolve save choice by presence first (CLI if supplied, otherwise JSON), then convert empty to ROM parent.

Core startup creates any necessary private non-save data directories under the application home without exposing a legacy data-root launch option. Such directories must never redirect explicit BIOS, firmware, or cartridge save paths. Prefer the selected config's parent as the private compatibility data root so a complete custom config does not require HOME; unrelated core files remain under its existing `User/` layout, separate from the direct save path.

## State and errors

ArgumentValidated → ConfigSelected → ExistingLoaded or MissingLogged/DefaultsPublished → SchemaValidated → OverridesApplied → EffectivePathsValidated → CoreConfigured → Running → SaveFlushed → Stopped.

Any failure stops with a structured error containing stage, optional dotted setting, selected config/effective asset/save path, and reason/errno. A persistent thread-safe save-error flag survives teardown so the runner cannot report success after a failed final flush. Absent save file is a normal first-run state; unreadable/corrupt existing save is a failure and is not overwritten silently. Concurrent sessions writing the same save are unsupported; detect contention with an exclusive advisory lock for the effective save file/session and fail rather than allow corruption.

Temporary default-file state is private until complete publication; losing a creation race transitions to ExistingLoaded. Invalid existing content transitions to failure, never replacement.
