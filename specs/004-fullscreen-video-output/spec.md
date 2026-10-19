# Feature Specification: Fullscreen Video Output

**Feature Branch**: No branch created; feature directory is independent of the current branch.

**Created**: 2026-10-19

**Status**: Draft

**Input**: User description: Replace headless-only operation with always-fullscreen video output. Configure display mode as auto (default), dual screen, or single screen; auto uses two displays when available and one otherwise, and dual screen also falls back to one display. Allow screen swapping across displays or within a single-display layout. Prioritize dual-screen devices; have the planner research modest single-screen layout options against SeedlessDS-Core capabilities. Use sharp integer scaling and SDL, with the planner choosing SDL2 or SDL3 for compatibility.

## Clarifications

### Session 2026-10-19

- Q: Where must all video output configuration options added by this feature be stored? → A: All must be within the top-level `video` object in the configuration JSON, including display mode, screen swapping, and single-display layout options.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Play fullscreen on a dual-screen device (Priority: P1)

A player launches a game on their dual-screen Linux handheld and sees the DS top and bottom screens on separate physical displays, each fullscreen and sharply scaled, without configuring a display mode first.

**Why this priority**: Dual-screen hardware is the primary target, and visible gameplay replaces the frontend's headless-only behavior.

**Independent Test**: Launch a ROM with distinguishable, changing top and bottom screen content on a dual-screen ARM64 Linux device with display settings omitted. Observe both outputs and verify sharp scaling and fullscreen coverage of each selected display.

**Acceptance Scenarios**:

1. **Given** two usable physical displays and no display settings, **When** a game starts, **Then** auto mode places the DS top screen on the primary/top device display and the DS bottom screen on the secondary/bottom device display, with both outputs fullscreen.
2. **Given** either auto or dual-screen mode on a dual-screen device, **When** the game changes either DS screen, **Then** the corresponding physical display updates throughout the session.
3. **Given** a display whose dimensions are not exact multiples of the source screen, **When** a frame is presented, **Then** the complete image is centered at the largest fitting uniform positive integer scale with black borders in unused space, without cropping, stretching, or smoothing.
4. **Given** two selected displays with different resolutions, **When** the game runs, **Then** each DS screen uses the largest integer scale fitting its own display.

---

### User Story 2 - Choose display behavior with predictable fallback (Priority: P1)

A player chooses auto, dual-screen, or single-screen mode in persistent configuration and can use the same configuration on devices with one or two displays.

**Why this priority**: Explicit choices and a safe single-display fallback make launches predictable across devices.

**Independent Test**: Launch each configured mode with one and two usable displays and compare the output against the mode table below. Repeat with an older valid configuration that omits the new settings.

**Acceptance Scenarios**:

1. **Given** one usable display and either auto or dual-screen selected, **When** the game launches, **Then** both DS screens appear in the configured single-display layout, fullscreen, without a missing-display error.
2. **Given** two usable displays and single-screen selected, **When** the game launches, **Then** both DS screens appear on the primary display in one fullscreen output and the frontend creates no output on the other display.
3. **Given** a new configuration, **When** defaults are generated, **Then** it includes a top-level `video` object containing all video output settings introduced by this feature, including display mode, swap, and single-display layout, with auto mode and swapping disabled.
4. **Given** an existing otherwise valid configuration without the `video` object or without some of its settings, **When** it is loaded, **Then** the same defaults apply without rewriting the file.
5. **Given** invalid display-mode, swap, or layout values, **When** the configuration is loaded, **Then** a diagnostic identifies the setting and startup exits cleanly with a nonzero status before emulation.
6. **Given** different display preferences in default and explicitly selected custom configurations, **When** the custom configuration is selected, **Then** only its preferences apply, under the existing configuration-selection rules.

| Configured mode | One usable physical display | Two usable physical displays |
| --- | --- | --- |
| Auto (default) | Both DS screens in one fullscreen layout | One DS screen per fullscreen display |
| Dual screen | Both DS screens in one fullscreen layout | One DS screen per fullscreen display |
| Single screen | Both DS screens in one fullscreen layout | Both DS screens on the primary display in one fullscreen layout |

---

### User Story 3 - Swap the DS screens (Priority: P2)

A player reverses where the DS screens appear to suit the device's physical orientation or their preferred arrangement.

**Why this priority**: Swapping makes both dual-display and single-display presentation usable when the default placement is inconvenient.

**Independent Test**: Compare otherwise identical launches with swapping disabled and enabled, first using two physical displays and then using a single-display layout with two distinct positions.

**Acceptance Scenarios**:

1. **Given** effective dual-screen output, **When** swapping is enabled, **Then** the DS top screen appears on the display previously assigned the DS bottom screen and vice versa, with scaling recalculated for each destination.
2. **Given** effective single-screen output, including dual-screen fallback, **When** swapping is enabled, **Then** the DS top and bottom screens exchange layout positions while retaining the selected arrangement.
3. **Given** either output mode with swapping disabled, **When** a game launches, **Then** the default top/bottom screen assignment is used consistently across launches with the same display setup.

---

### User Story 4 - Play with a basic single-display layout (Priority: P3)

A player using one physical display can select a small set of supported arrangements for viewing both DS screens, while dual-screen quality remains the delivery priority.

**Why this priority**: A usable fallback is required, but extensive layout customization is secondary to the target dual-screen device.

**Independent Test**: For every layout selected during planning, launch on a single-display device and verify both complete DS images, documented positions, swap behavior, and integer scaling.

**Acceptance Scenarios**:

1. **Given** effective single-screen mode, **When** no layout preference is supplied, **Then** both DS screens are visible in the documented default arrangement.
2. **Given** any supported configured layout, **When** the game launches, **Then** both DS screens occupy their documented positions without overlap or cropping, at the largest uniform integer scale that fits the whole arrangement.
3. **Given** a layout setting and effective dual-screen output, **When** the game launches, **Then** the layout preference is retained but does not change the one-screen-per-display presentation.

### Edge Cases

- With no usable physical display or failed video initialization, startup reports the video failure and exits cleanly with a nonzero status; it does not silently continue headless.
- If a display/layout cannot fit both required source dimensions even at 1x, report the unsupported size and exit cleanly rather than crop, blur, or apply fractional scaling.
- An explicitly requested dual-screen mode on a single-display device uses the same single-display defaults, layout preference, and swap behavior as auto mode, while retaining the configured mode value.
- A swap on mismatched displays recalculates scale for the new destinations; a screen's old scale must not cause clipping on its new display.
- With more than two usable displays, select a documented deterministic pair for dual-screen output; leave other displays without frontend output.
- Loss of a selected display during play must cause either a fullscreen fallback satisfying these rules or a diagnostic and clean shutdown; seamless hot-plug recovery is not required.
- Bounded runs and interruption-based shutdown must release all video outputs cleanly. Help and invalid-argument exits must not require video initialization.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Normal ROM launches MUST present changing SeedlessDS-Core video output throughout the session, including both DS screens; headless-only gameplay MUST cease to be the default behavior.
- **FR-002**: Every active frontend video output MUST be fullscreen from first game presentation until shutdown. Windowed gameplay MUST NOT be offered in this feature.
- **FR-003**: Persistent configuration MUST expose three distinct accepted display-mode choices: auto, dual screen, and single screen. Auto MUST be the default. Effective behavior MUST match the mode table.
- **FR-004**: Display selection MUST depend on usable physical displays, rather than the number of DS screen images. In effective dual-screen mode each DS screen MUST occupy a separate physical display. Selection and normal top/bottom assignment MUST be deterministic and documented, including the primary target device's physical arrangement.
- **FR-005**: Single-screen mode MUST place both DS screens on one selected primary display even when two displays are available. Auto and dual-screen MUST fall back to that presentation when only one usable display exists, without changing the saved mode preference.
- **FR-006**: Configuration MUST provide a boolean swap preference, defaulting to disabled. It MUST exchange physical display assignments in effective dual-screen mode and exchange screen positions in effective single-screen mode, including fallback.
- **FR-007**: Configuration MUST provide a single-display layout preference with a documented default and a small, explicitly enumerated set of choices established during planning. Every offered layout MUST show both complete DS screens; a usable default is mandatory, while additional layouts depend on planner research.
- **FR-008**: Effective dual-screen mode MUST ignore the single-display layout preference for presentation without discarding it from configuration.
- **FR-009**: Scaling MUST preserve source aspect ratio and pixel boundaries using the largest fitting uniform positive integer factor, independently per destination in dual-screen mode and for the whole arrangement in single-screen mode. Pixel blending, fractional scaling, stretching, and cropping MUST NOT occur. Unused space MUST be black with the image or arrangement centered.
- **FR-010**: All video output configuration options introduced by this feature MUST reside within the top-level `video` JSON object, including display mode, swap, and every single-display layout option. Display preferences MUST use the existing frontend configuration selection and validation behavior. New configurations MUST include the `video` object and its defaults; older valid configurations omitting that object or any of its settings MUST use the corresponding defaults without being rewritten. Invalid values MUST yield a setting-specific diagnostic and clean startup failure.
- **FR-011**: Video initialization failures, no usable displays, and insufficient space for 1x presentation MUST produce a readable diagnostic and clean nonzero exit, rather than an invisible game session. Display loss during play MUST follow the edge-case policy above.
- **FR-012**: Video presentation MUST preserve existing ROM execution, configured core visual behavior, save handling, bounded-run semantics, and SIGINT/SIGTERM shutdown behavior. Video resources MUST be released on completion, failure, or interruption; help and usage validation MUST remain usable without displays.

### Key Entities *(include if feature involves data)*

- **Display preferences**: All video output settings added by this feature, persisted within the top-level `video` JSON object, including requested mode, swap flag, and single-display layout. Missing values use documented defaults; requested mode remains distinct from effective output mode.
- **Physical display setup**: Available usable displays, their dimensions, and deterministic ordering/physical roles for selecting output destinations.
- **Effective presentation**: Resolved one-display or two-display output, assignments of DS top/bottom images, layout positions, and integer scaling based on display setup and preferences.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All six combinations of the three configured modes and one/two usable displays produce exactly the screen placement defined in the mode table, with zero windowed game outputs.
- **SC-002**: All mode/display combinations pass with swapping both disabled and enabled; enabling it exchanges exactly the two screen destinations or positions without losing either image.
- **SC-003**: For every supported single-display layout and representative matching, nonmatching, and mismatched dual-display resolutions, the full image fits at the maximum permitted integer factor, with zero blended pixels, zero stretching, zero cropped source pixels, and black unused areas.
- **SC-004**: On real ARM64 Linux hardware, both a dual-display session and a single-display session show changing content from both DS screens for a continuous five-minute game run and exit cleanly on interruption. A bounded run also completes at the existing requested scheduler-boundary count and releases its outputs.
- **SC-005**: All tests for missing preferences, new configuration defaults, custom configuration selection, and invalid preferences match the specified defaults and failure behavior, with zero unintended configuration rewrites. Generated configurations place 100% of the video output options added by this feature within the top-level `video` object.
- **SC-006**: Every tested unavailable-video, absent-display, too-small-display, and display-loss failure produces either the explicitly permitted fullscreen fallback or a diagnostic and clean exit, with zero crashes or silent headless continuation.

## Assumptions

- This feature targets the existing Linux frontend and builds on the configuration behavior defined in `specs/003-frontend-json-config/spec.md`. The top-level grouping is fixed as `video` for every video output option introduced by this feature; property names within that object will be settled during planning, preserving existing conventions. Existing core visual settings remain in their current group. No new configuration UI or live settings editor is required.
- Display preferences are applied at launch. Runtime toggles and seamless display hot-plug handling are outside the required scope.
- “Fill the screen” means maximize whole-number enlargement while keeping the full image and original proportions. Black borders are expected when integer scaling cannot cover the full physical display.
- Single-display output uses a common scale for both images in this initial feature. At least one both-screens-visible arrangement is required; side-by-side and vertically stacked arrangements are research candidates rather than promises of unsupported core capabilities.
- Audio output, new gameplay input/touch controls, ROM browsing, and extensive layout editing are outside this feature. Swapping here changes video placement; it does not introduce a new input system.
- Final display validation requires real ARM64 Linux hardware, prioritizing the dual-screen target device and covering usable single-screen operation as required by the constitution. The specification's completion does not claim this implementation validation has occurred.

### User-Mandated Planning Constraints and Research

- **PC-001 — SDL compatibility**: Video presentation MUST use SDL. The planner MUST research SDL2 versus SDL3 and document the selected version with evidence about Linux ARM64 availability, target-device graphics/display environments, fullscreen and multi-display behavior, and native/cross-build compatibility. The version is intentionally not selected by this specification.
- **PC-002 — Core-supported layouts**: The planner MUST inspect the pinned local SeedlessDS-Core's supported video interfaces and layout capabilities, then enumerate the initial single-display choices, exact default, swap positions, and any limitations. Evaluate side-by-side and vertical arrangements; prioritize reliable dual-screen output over a broad layout feature set. Keep frontend responsibilities separate from core behavior.
- **PC-003 — Device validation**: The plan MUST define deterministic physical display mapping on the primary dual-screen device and the hardware checks needed for fullscreen, integer scaling, swapping, and single-display fallback. Document dependency and runtime compatibility for ARM64 Linux.
- **PC-004 — Permitted research**: Use locally available core sources and permitted primary documentation for research. All agent work remains subject to constitution Principle VI: no GitHub actions, commits, or remote Git operations.
