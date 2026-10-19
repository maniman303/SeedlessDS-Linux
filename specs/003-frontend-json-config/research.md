# Research: Frontend JSON Configuration

Research used the pinned local core and repository tooling only. No remote access or source updates were performed. All design unknowns are resolved below; implementation verification remains required.

## Configuration separation and JSON

**Decision**: Use `config.c/.h` for typed settings and file lifecycle, a private `config_json.c/.h` codec, `launch_options.c/.h` for argv, and `core_config.c/.h` for core mapping. Write a bounded strict JSON codec supporting the required object/string/boolean/integer grammar, with complete JSON string escaping and UTF-8/UTF-16 validation; unsupported JSON value forms are diagnosed as schema errors rather than loosely coerced. Reject duplicate and unknown keys.

**Rationale**: Existing `src/runner.c` mixes CLI and BIOS policy with signals and worker lifecycle. Independent modules keep settings logic host-testable and satisfy the user's explicit source-separation request. Existing CMake/dependency sources contain no JSON dependency. Strict schema-specific decoding avoids adding a shared library to the ARM64 runtime inventory or requiring remote vendoring.

**Alternatives considered**: More settings in runner (rejected by user); system JSON library (adds target dependency and setup work); fetching a parser (unnecessary remote retrieval); regex/string-search parsing (cannot validate JSON reliably). The custom codec is a correctness risk: require malformed syntax, escaping, Unicode, nesting/size, duplicate-key, numeric-overflow and truncation tests before integration.

## Visual defaults and supported values

**Decision**: JSON `threaded_3d=false`, `thread_count=0`, `auto_trim=false`, `fix_main_screen=false`, `disable_edge_marking=false`. Thread count accepts integer 0–15. Do not expose internal resolution in this feature.

**Rationale and evidence**: The existing runner supplies zero config bits. `external/SeedlessDS-Core/src/core/config.c` maps threaded 3D to bit 28, worker count to bits 16–19, auto-trim to bit 36, fixed main screen to bit 35, and disabled edge marking to bit 40. Layout is defined in `src/core/config.h`. `src/gpu/gpu3d/gpu3d.c` (`recon_threads_3d`) treats 0 as 1, limits by created workers and internal scale (native/scale <=2 caps at 4), and may round down for scheduler divisors. The encoded worker count is a request, not a guarantee of exactly that many host threads. Verify mapping in adapter tests rather than duplicating packed constants across frontend files.

**Alternatives considered**: Default 1 is effectively similar but changes the literal pinned default; unrestricted integers cannot fit the four-bit field; adding higher resolution exceeds the agreed scope.

## Fixed execution, clock, save format, and user profile

**Decision**: Keep JIT unconditionally enabled; OR RTC bit 39 and raw-save bit 50 into every ROM-open config word. Keep `nds_control_begin(-1, ...)` for a non-fixed debug clock. Use public `nds_set_firmware_user` with at most ten UTF-16 code units and packed value `1 | color << 8 | (month - 1) << 16 | day << 24`. The unexposed language value remains English (1). Store separate one-based month/day in JSON, never a year.

**Rationale and evidence**: `src/core/nds.c` initializes JIT to 1; `src/rtc.c` checks RTC system-time configuration; `src/core/config.c` maps RTC/raw bits. Public `include/seedlessds/nds.h` and `src/core/nds_api.c` define nickname limits and profile setter; `src/spi/firmware.c` adds 1 to stored month. Apply profile before firmware startup and ensure image profile does not overwrite selected settings.

**Alternatives considered**: Private layout writes violate the integration boundary; storing zero-based months in JSON is unclear; preserving a birthday year contradicts explicit user clarification. UTF-8 byte length is not the DS nickname limit.

## Exact asset and save paths

**Decision**: Add a tracked Linux compatibility patch with an explicit copied runtime-path API, asset validation, and save-error status. Keep this in the supported adapter boundary and register its hash in `patches/seedlessds-core/manifest.json`. Never edit the pinned checkout in place. Existing config bits/profile setters remain preferred for supported values.

**Rationale and evidence**: `src/core/nds_api_rom.c` hardcodes cache/save roots to `SeedlessDS`/`User`; `src/memory/mirror.c` opens fixed BIOS/firmware names; `src/cart/cart.c` inserts `/backup/`; modified firmware is written under `save_dir/system/nds_firmware_modified.bin` in `src/spi/firmware.c`. Merely selecting a save root does not produce the required direct `<rom>.sav` location. Add independent exact asset paths and direct cartridge-save path while leaving cache, modified firmware, savestates and other user data separate. Keep existing core firmware parser as the image-validity authority and propagate failure instead of inventing a file-size-only check.

**Alternatives considered**: Copying or symlinking BIOS into legacy names risks stale assets; changing CWD or a global root cannot represent independent BIOS paths and direct saves; intercepting arbitrary filesystem calls is fragile. Supported explicit bridge parameters are reviewable and preserve normal callers when overrides are absent.

## Lifecycle, precedence, and durable errors

**Decision**: CLI presence beats JSON, including empty save override. Validate all schema fields but only probe the effective save path. Publish missing defaults atomically without replacement; load a concurrent winner. Retain a thread-safe save failure flag through final flush and map it to exit 1. Lock the selected cartridge save for the session to prevent concurrent writers.

**Rationale**: Prevent default-file fallback, truncation races, hidden writes to overridden directories, and a successful result after save loss. Use explicit owned path strings with bounds checks against existing 0x400-byte core path buffers (maximum 1023 encoded bytes including any generated suffix budget).

**Alternatives considered**: Truthiness-based empty override handling violates precedence; overwriting existing configs loses edits; startup writability checks alone cannot detect disk-full errors during final flush.

## Build and operational integration

**Decision**: Add source files to the existing executable and a core-free host test target. Update frontend callers in README, validation/deploy scripts and relevant tests. Preserve deployment-specific options independently. Use local pinned sources and cached dependencies, inspect scripts before invoking, and require native/cross build plus real ARM64 validation during implementation.

**Rationale**: Root CMake intentionally rejects non-ARM64 runtime targets. `scripts/prepare-core.sh` is local-only but its embedded Python currently uses `tempfile.TemporaryDirectory` without importing tempfile; implementation must fix that prerequisite if still present before invoking configuration. CMake FetchContent can retrieve archives if missing: use disconnected/cache-only configuration and fail on absent caches rather than silently initiate external operations. No package installation or remote Git is part of planning.

**Alternatives considered**: Weakening runtime architecture gates just to run config tests obscures target assumptions; remote source/bootstrap operations conflict with repository restrictions when they involve GitHub or remote Git.
