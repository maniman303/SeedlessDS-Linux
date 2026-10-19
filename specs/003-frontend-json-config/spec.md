# Feature Specification: Frontend JSON Configuration

**Feature Branch**: No branch created; feature directory is independent of the current branch.

**Created**: 2026-10-19

**Status**: Draft

**Input**: User description: Linux frontend configuration in JSON at `$HOME/.config/seedlessds/config.json`, with an authoritative custom configuration launch argument, automatic creation of missing configuration, separate ARM7/ARM9 BIOS paths, grouped visual, firmware, user-profile and save settings, fixed JIT/real-time/raw-save policies, and configuration, ROM, help, `--frames`, and save-directory launch arguments.

## Clarifications

### Session 2026-10-19

- Q: How should the birthday be represented in configuration? → A: Use separate integer `user.birthday_month` and `user.birthday_day` fields, defaulting to 3 and 2, without a year or combined date string.

- Q: Should the existing `--frames` launch argument be retained? → A: Yes; keep `--frames` with its existing behavior.
- Q: Should the birthday year be preserved? → A: No; store only month and day, defaulting to March 2 (`birthday_month: 3`, `birthday_day: 2`).
- Q: Which additional settings and fixed policies should this configuration support? → A: Group 3D threading and edge/render settings under `visuals`; BIOS and optional firmware paths under `firmware`; validated nickname, birthday, and color under `user`; and save directory under `emulation`. Always use JIT, real-time RTC, and raw `.sav` saves. Default profile to SEEDLESS, birthday month 3 and day 2, and color 0. Empty firmware means no external firmware; empty save directory means the ROM directory. Add a save-directory launch override, and always give launch arguments precedence over configuration values.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Launch with persistent BIOS settings (Priority: P1)

A Linux user edits one configuration file to choose independent ARM7 and ARM9 BIOS images, then launches a ROM without repeating BIOS paths on the command line.

**Why this priority**: Persistent settings make routine launches predictable and avoid BIOS path arguments.

**Independent Test**: With valid BIOS images and a launchable ROM, configure distinct BIOS paths in the default file and verify that the frontend uses both selected images across repeated launches.

**Acceptance Scenarios**:

1. **Given** a valid default configuration and no custom configuration argument, **When** the user launches a ROM, **Then** the frontend reads `$HOME/.config/seedlessds/config.json` and uses its separate ARM7 and ARM9 BIOS settings.
2. **Given** an existing valid configuration with edited BIOS paths, **When** the frontend launches again, **Then** those values are used without rewriting the file or requiring BIOS arguments.
3. **Given** valid images at the configured locations, **When** the ROM starts, **Then** each image is supplied for its corresponding processor rather than swapped or replaced by legacy BIOS discovery.

---

### User Story 2 - Select an alternative configuration (Priority: P1)

A user launching from a script or another installation can select a particular configuration file and reliably use only that file for the launch.

**Why this priority**: Silent fallback would launch with settings the user did not select.

**Independent Test**: Set conflicting BIOS paths in default and custom files, select the custom file, and verify that only its settings are used.

**Acceptance Scenarios**:

1. **Given** readable default and custom configurations with different settings, **When** the user selects the custom file, **Then** the frontend uses the custom settings exclusively.
2. **Given** a missing custom file and an existing default file, **When** the user selects the custom path and supplies a ROM, **Then** the frontend logs the missing custom file, creates defaults at that custom path, and uses the created file without reading or modifying the default file.
3. **Given** a selected custom file that is unreadable or invalid, **When** the user launches, **Then** the frontend logs its path and the failure, exits cleanly with a nonzero status before emulation, and leaves both files unchanged without falling back.

---

### User Story 3 - Recover missing configuration and understand launch errors (Priority: P1)

A first-time user receives a usable configuration automatically, while users with launch or filesystem errors receive a clear diagnostic and a clean exit.

**Why this priority**: First launch and failure handling are necessary for reliable unattended and desktop launches.

**Independent Test**: Launch with an isolated home directory and a ROM, inspect generated defaults, then repeat with an unwritable selected destination and with no ROM.

**Acceptance Scenarios**:

1. **Given** the default configuration and its parent directories do not exist, **When** the user supplies a ROM without a custom configuration argument, **Then** the frontend logs the missing file, creates necessary parent directories and a valid JSON configuration, and continues startup using it.
2. **Given** either selected configuration is absent, **When** it is created, **Then** it contains the `visuals`, `firmware`, `user`, and `emulation` objects with the defaults specified below, including independent ARM7 and ARM9 BIOS path settings with defaults `$HOME/.config/seedlessds/bios/drastic_bios_arm7.bin` and `$HOME/.config/seedlessds/bios/drastic_bios_arm9.bin`, respectively, resolved for the launching user.
3. **Given** a missing selected configuration whose directories or file cannot be created or fully written, **When** startup is attempted, **Then** the frontend logs the selected path and creation failure and exits cleanly with a nonzero status before emulation.
4. **Given** no ROM argument and no help request, **When** the frontend is launched, **Then** it logs that the ROM argument is missing and exits cleanly with a nonzero status before creating configuration or starting emulation.
5. **Given** a help request, **When** the frontend is launched without a ROM or usable configuration, **Then** it displays help and exits successfully without reading or creating configuration or starting emulation.
6. **Given** BIOS-path arguments, legacy `--data-dir` arguments, malformed or duplicate `--frames` arguments, unknown arguments, duplicate configuration arguments, a configuration argument without a value, a missing or duplicate save-directory argument value, or multiple ROM arguments, **When** the frontend is launched, **Then** it reports invalid usage and exits cleanly with a nonzero status without emulation.

7. **Given** a valid ROM and configuration, **When** the user supplies `--frames N`, **Then** the frontend retains the existing bounded-run behavior and stops successfully after N emulation scheduler boundaries.
8. **Given** a valid ROM and configuration without `--frames`, **When** emulation starts, **Then** it continues until interrupted with SIGINT or SIGTERM, preserving existing behavior.

### User Story 4 - Customize visuals, firmware, profile, and saves (Priority: P1)

A user keeps visual preferences and a validated DS profile in the configuration, optionally supplies firmware, and chooses where portable raw saves are stored. A launch script can temporarily override the save directory.

**Why this priority**: These settings complete the requested configuration while fixed execution, clock, and save-format policies keep launches consistent.

**Independent Test**: Launch a ROM with edited settings, verify the applied profile and rendering choices, write and reload a raw save, then repeat with a conflicting save-directory launch argument.

**Acceptance Scenarios**:

1. **Given** a newly generated configuration, **When** inspected, **Then** BIOS paths and an empty firmware path are under `firmware`, 3D threading and edge/render settings are under `visuals`, nickname `SEEDLESS`, `birthday_month: 3` and `birthday_day: 2`, and color `0` are under `user`, and an empty save-directory value is under `emulation`.
2. **Given** supported visual values and a valid profile, **When** a ROM launches, **Then** the selected 3D threading and edge/render settings and user nickname, birthday month/day, and color are applied.
3. **Given** an invalid date, unsupported color, overlong nickname, or invalid visual value, **When** configuration is loaded, **Then** the frontend identifies the setting in a diagnostic and exits cleanly before emulation without silently truncating or clamping it.
4. **Given** an empty firmware path, **When** a ROM launches, **Then** no external firmware image is required; a nonempty path selects that exact image, and an unreadable or invalid selected image causes a logged clean startup failure.
5. **Given** an empty save-directory setting without an override, **When** a game saves, **Then** its raw `.sav` is stored directly beside its `.nds` file and is reloaded on the next launch.
6. **Given** a configured save directory A and launch override B, **When** the game loads or writes a save, **Then** it uses B exclusively for that launch, preserves A in the configuration, and a later launch without the override uses A.
7. **Given** any valid configuration, **When** emulation runs, **Then** CPU execution uses recompiler/JIT, RTC uses current real time, and cartridge saves use raw `.sav`; configuration and launch inputs cannot select alternative policies.
8. **Given** a selected save directory that cannot be created or used, **When** startup is attempted, **Then** a diagnostic identifies that directory and the frontend exits cleanly before emulation without falling back to another directory.

### Edge Cases

- A custom path containing spaces or a relative custom path selects that exact file; relative custom paths resolve from the launch working directory.
- Missing custom parent directories receive the same creation treatment as missing default parent directories.
- An existing directory at the selected file path, permission denial, malformed JSON, missing BIOS settings, empty BIOS paths, or non-string BIOS settings is a configuration error; existing contents are preserved and startup stops.
- A full filesystem or incomplete write cannot be treated as successful configuration creation; the frontend must not start emulation with an unusable file.
- A newly created configuration may reference BIOS files that are not installed. Configuration creation does not create or download BIOS images; unavailable or invalid BIOS images produce the existing clean BIOS failure behavior using the configured paths.
- If the home directory cannot be resolved when required for the default configuration or default BIOS paths, the frontend logs the problem and exits cleanly rather than guessing another location.
- An empty save-directory launch value explicitly selects the ROM directory, overriding any nonempty configured directory. Relative nonempty save paths resolve against the launch working directory, whether supplied in JSON or as launch arguments.
- Missing save directories are created; permission errors, a file in place of a directory, and failed save reads/writes are logged, never redirected silently. A runtime save failure causes a clean nonzero exit.
- Birthday validation accepts month 2/day 29 and rejects impossible month/day pairs such as month 2/day 30, month 4/day 31, month 13, and day 0. Both fields must be integers; missing fields, strings, booleans, and fractional numbers are rejected. Birthday values containing a year, unsupported colors, and nicknames beyond the supported limit are rejected.
- An unreadable selected custom file never causes the default file to be used, even if that default file is valid.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The frontend MUST use a JSON configuration file at `$HOME/.config/seedlessds/config.json` unless an alternative configuration path is supplied at launch.
- **FR-002**: The frontend MUST accept one optional custom configuration path argument. When supplied, that path MUST remain the sole configuration source and creation destination for that launch; the default file MUST NOT be used as fallback.
- **FR-003**: Before emulation, the frontend MUST load the selected existing configuration and use its independent ARM7 and ARM9 BIOS paths for their corresponding processors. Loading a valid existing file MUST NOT rewrite it.
- **FR-004**: If the selected configuration does not exist, the frontend MUST log that it is missing, identifying its path, create necessary parent directories and a complete JSON file with default settings at that path, and continue startup using the created configuration.
- **FR-005**: If creating required directories or creating, writing, or subsequently loading the configuration fails, the frontend MUST log the selected path and failure reason and exit cleanly with a nonzero status before emulation.
- **FR-006**: The `firmware` object MUST contain an ARM7 BIOS path defaulting to `$HOME/.config/seedlessds/bios/drastic_bios_arm7.bin` and an ARM9 BIOS path defaulting to `$HOME/.config/seedlessds/bios/drastic_bios_arm9.bin`. Generated defaults MUST contain absolute paths resolved from the launching user's home, even when the configuration itself is elsewhere.
- **FR-007**: The frontend MUST accept only a ROM to launch, an optional alternative configuration path, an optional `--frames N` limit, an optional save-directory override, and help as launch inputs. It MUST NOT offer or accept BIOS path arguments or retain other operational launch options, including the current data directory option. The existing `--frames N` option MUST remain supported with its existing validation and bounded-run behavior; omitting it MUST preserve running until SIGINT or SIGTERM. Unsupported or malformed launch inputs MUST produce a diagnostic and clean nonzero exit before emulation.
- **FR-008**: Without a ROM argument, unless help was requested, the frontend MUST log the missing ROM argument and exit cleanly with a nonzero status before configuration creation or emulation.
- **FR-009**: Help MUST describe the supported launch inputs and default configuration location, work without a ROM, avoid configuration access or creation, and exit successfully.
- **FR-010**: Existing unreadable or invalid configurations MUST produce a diagnostic identifying the selected path and problem and a clean nonzero exit before emulation. They MUST NOT be overwritten with defaults or trigger fallback to another configuration.
- **FR-011**: Each BIOS setting MUST be a nonempty path string. Missing required settings and wrong value types MUST be reported as configuration errors. BIOS readability and validity checks MUST apply to the configured files and preserve clean failure behavior without sourcing a different BIOS file automatically.
- **FR-012**: Custom configuration paths MUST support absolute and relative paths, including spaces; relative custom paths MUST resolve against the launch working directory.

- **FR-013**: The JSON configuration MUST group settings into `visuals`, `firmware`, `user`, and `emulation` objects. CPU backend, RTC mode, and save format MUST NOT be configurable or selectable by launch arguments; the frontend MUST always use recompiler/JIT, current system real time, and raw `.sav` cartridge saves. Failure to enable a required policy MUST produce a logged clean startup failure rather than silently selecting another mode.
- **FR-014**: `visuals` MUST expose the core-supported 3D threading settings (threaded 3D and worker count) and edge/render toggles (`disable_edge_marking`, `fix_main_screen`, and `auto_trim`). Values MUST be validated against supported core types and ranges before emulation. Generated visual defaults MUST preserve the pinned core's default behavior; determining exact supported worker-count values and default values is a planning prerequisite, not permission to invent unsupported choices.
- **FR-015**: `firmware` MUST also contain a firmware image path defaulting to an empty string. Empty means no external firmware image; a nonempty path MUST load the selected readable, valid image or fail cleanly with a diagnostic. This feature does not add a firmware boot-mode selector.
- **FR-016**: `user` MUST contain nickname, separate numeric integer fields `birthday_month` and `birthday_day`, and favorite color. Defaults MUST be `SEEDLESS`, `birthday_month: 3`, `birthday_day: 2`, and numeric color `0` (the first DS color). Birthday month MUST be from 1 through 12 and day MUST be valid for that month; February 29 is valid because no year is stored. Both birthday fields are required; strings, booleans, and fractional numbers MUST be rejected. A combined birthday date string MUST NOT be used. Nickname MUST be a nonempty string fitting the DS limit of 10 UTF-16 code units; invalid text and overlong values MUST be rejected without truncation. Favorite color MUST be an integer identifying one of the 16 DS colors, 0 through 15. The birthday year MUST NOT be stored or preserved; JSON and the DS profile MUST contain only month and day.
- **FR-017**: `emulation` MUST contain a save-directory path defaulting to an empty string, meaning the directory of the launched `.nds` file. Nonempty values MUST select that directory directly, without an implicit `backup` subdirectory. Cartridge save filenames MUST use the ROM basename with its `.nds` extension replaced by `.sav`, and the same effective path MUST be used to load and write saves. Savestate configuration is outside this feature's scope.
- **FR-018**: The frontend MUST accept one optional save-directory launch argument, including an empty value to select the ROM directory. Launch arguments MUST always override corresponding configuration values for the current launch without rewriting the configuration. Effective save-directory precedence MUST be launch override, then configured value, then the ROM directory when the chosen value is empty. An invalid override MUST cause a diagnostic and clean failure rather than fallback.
- **FR-019**: Save-directory paths MUST support spaces, absolute paths, and paths relative to the launch working directory. Missing directories MUST be created before emulation; unusable destinations MUST cause a logged clean nonzero exit. Runtime save failures MUST also be logged and cause a clean nonzero exit; saves MUST NOT be silently redirected or written as `.dsv`.

### Key Entities *(include if feature involves data)*

- **Frontend configuration**: The selected JSON file, containing the `visuals`, `firmware`, `user`, and `emulation` setting groups. Its location is either the default home-based path or the explicit launch override.
- **BIOS path setting**: A nonempty filesystem path identifying the image for one processor. ARM7 and ARM9 values are independent.
- **Launch request**: One ROM path, an optional configuration override, an optional `--frames N` limit, an optional save-directory override, or a help request, with no BIOS path overrides or other operational settings. Launch overrides take precedence over corresponding file settings.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In all default and custom configuration acceptance scenarios, users launch with the selected BIOS settings across two consecutive launches without repeating BIOS paths or losing edits.
- **SC-002**: In all missing-file scenarios with a writable destination, one launch attempt creates a usable configuration at the selected path with both correct default BIOS paths and a diagnostic identifying the missing file.
- **SC-003**: In every custom configuration scenario, there are zero fallback reads or writes of the default configuration.
- **SC-004**: Every tested configuration access, validation, and creation failure ends before emulation with a readable diagnostic and a nonzero exit, without a crash or overwrite of an existing configuration.
- **SC-005**: Every missing-ROM and unsupported-argument scenario exits cleanly with a diagnostic; help succeeds without a ROM or configuration and lists only the five permitted launch input categories, including `--frames`.

- **SC-006**: Valid bounded launches stop successfully after exactly the requested number of emulation scheduler boundaries with either default or custom configuration; launches without a frame limit retain interruption-based shutdown.

- **SC-007**: All profile boundary tests accept valid dates and colors and nicknames within the DS limit, and reject invalid dates, out-of-range colors, and overlong nicknames with a setting-specific diagnostic.
- **SC-008**: Across default, configured, and overridden save-directory scenarios, raw saves are written to and reloaded from exactly the effective directory, with zero writes to lower-priority destinations and zero configuration rewrites.
- **SC-009**: All valid launch scenarios use the selected visual and firmware settings while retaining JIT execution, real-time RTC, and raw saves; generated configuration contains all four requested groups and every specified default.

## Assumptions

- `$HOME` denotes the launching user's resolved home directory. The requested default location is fixed under that home; `XDG_CONFIG_HOME` does not redirect it in this feature.
- A missing file is distinct from an existing but unreadable or invalid file. Only a missing file is automatically created.
- Argument validation, including missing-ROM handling, precedes configuration creation. A standalone help request takes precedence over the need for a ROM or configuration.
- BIOS paths saved by users are absolute paths. Expansion of shell variables or tilde in edited JSON values and configuration-relative BIOS paths are outside this feature's scope.
- JSON object names are fixed as requested; remaining property names and custom configuration/save-directory flag spellings will be settled during planning; `--frames` keeps its existing spelling and semantics; the required file format, paths, input categories, and observable behavior are fixed here.
- This feature applies to the existing Linux frontend launch interface, replacing the current operational argument surface while retaining `--frames`. Updating dependent local launch scripts and documentation will need to be accounted for during planning.
- Existing ROM validation and BIOS image validation remain dependencies. This feature neither supplies BIOS images nor adds graphics presentation, input, or unrelated user-data settings. Requested visual, firmware, profile, save, and fixed execution policies are in scope.

- Supplied firmware does not override the explicitly configured DS profile. BIOS and nonempty firmware image paths are absolute; no shell-variable expansion is performed.
