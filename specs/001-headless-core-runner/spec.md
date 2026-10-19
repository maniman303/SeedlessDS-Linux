# Feature Specification: Headless Core Runner and Ubuntu Build Setup

**Feature Branch**: `main` (existing branch; no branch hook configured)

**Created**: 2026-10-19

**Status**: Draft

**Input**: User description: "we need a barebone frontend for the SeedlesDS-Core from /external/SeedlessDS-Core, that will just run, without even any video or audio output, just run, with core code connected. And we will need a working build system for everything, alongside proper tooling installation in ubuntu"

## Clarifications

### Session 2026-10-19

- Q: What linking tradeoff should guide the build? → A: Include as many libraries as practical in the executable; output size is not a concern, and compatibility takes priority over forcing full static linking. (User-provided clarification; no follow-up question required.)

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Run the Core Headlessly (Priority: P1)

A developer launches a minimal command-line frontend with a local Nintendo DS ROM and verifies actual core execution on ARM64 Linux without display or sound devices.

**Why this priority**: Real execution provides the foundation for later frontend work.

**Independent Test**: Run a known-good test ROM for 120 frames on ARM64 Linux without display or audio services and inspect completion output.

**Acceptance Scenarios**:

1. **Given** a built runner and supported test ROM, **When** 120 frames are requested, **Then** the real core loads the ROM, completes exactly 120 frame executions, reports that count, and exits successfully without video or audio output.
2. **Given** a valid ROM and no frame limit, **When** the developer launches and later interrupts execution, **Then** execution continues until interrupted and shuts down within five seconds with an interruption outcome.
3. **Given** missing, unreadable, or rejected ROM input, **When** execution is requested, **Then** the runner identifies the failing input or operation and exits unsuccessfully without claiming successful execution.

---

### User Story 2 - Build the Complete Application (Priority: P1)

A developer builds the frontend, core, and required dependencies from the repository and obtains a runnable ARM64 Linux artifact.

**Why this priority**: The integrated program must be reproducibly buildable.

**Independent Test**: Follow the native ARM64 Ubuntu and Ubuntu x86-64 cross-build workflows from clean checkouts and run each artifact on real ARM64 Linux hardware.

**Acceptance Scenarios**:

1. **Given** a clean checkout and installed native prerequisites, **When** the documented ARM64 Ubuntu build is followed, **Then** the complete application builds from source without manual source edits or Android tooling.
2. **Given** Ubuntu x86-64, including WSL2, **When** the cross-build instructions are followed, **Then** an ARM64 Linux artifact is produced and passes the bounded-run scenario on target hardware.
3. **Given** missing prerequisites or an unsupported target architecture, **When** building is attempted, **Then** the build fails with an actionable diagnosis rather than presenting an unusable artifact as successful.
4. **Given** a target with the documented operating-system runtime but no separately installed application libraries, **When** the developer transfers and runs the built executable with required ROM/data assets, **Then** it completes the bounded-run scenario without installing those application libraries; every remaining shared-library requirement has a documented compatibility reason.

---

### User Story 3 - Prepare Ubuntu Tooling (Priority: P2)

A contributor prepares a clean Ubuntu environment using repository instructions and proceeds to building and running the application.

**Why this priority**: Repeatable setup removes undocumented machine-specific prerequisites.

**Independent Test**: Follow setup on clean supported Ubuntu, verify prerequisites, and repeat setup successfully.

**Acceptance Scenarios**:

1. **Given** clean supported Ubuntu and installation privileges, **When** setup is followed, **Then** all tools and development dependencies for the selected native or cross-build workflow are installed and verified.
2. **Given** a prepared environment, **When** setup is repeated, **Then** required prerequisites remain usable and verification passes.
3. **Given** insufficient privileges or failed downloads, **When** setup cannot complete, **Then** it names the failed prerequisite and recovery action without reporting success.

### Edge Cases

- Missing nested dependencies and interrupted downloads produce actionable failures.
- ROM paths containing spaces work; directories, absent files, and rejected ROMs fail clearly.
- Zero, negative, malformed, or unrepresentable frame limits fail before execution.
- Absent display, audio, and microphone services do not cause initialization failures or device-related waits.
- Initialization and runtime failures release acquired resources and report unsuccessful outcomes.
- Termination requests are handled while core worker threads are active.
- Unwritable runtime data directories are reported when required by the core.
- Cross-builds reject accidental use of incompatible host dependencies.
- An application dependency lacking a compatible static build must trigger a clear build failure unless an explicit, documented shared-library exception is selected.
- A build using a newer operating-system runtime must not be presented as compatible with older targets merely because application dependencies are embedded.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The project MUST provide a command-line runner that initializes the real SeedlessDS-Core, loads a user-selected ROM, executes frames, and releases the core on shutdown.
- **FR-002**: The runner MUST operate without creating windows, presenting video, playing sound, capturing microphone input, or requiring display and sound devices or services. Internal computation needed for correct emulation remains permitted.
- **FR-003**: The runner MUST support a positive frame limit and unlimited execution when the limit is omitted. Bounded execution MUST complete exactly the requested number of frames.
- **FR-004**: The runner MUST provide usage help, validate inputs, and report initialization, ROM loading, completed frame count, completion, and failures through text output.
- **FR-005**: Bounded completion MUST return success; invalid input, initialization failure, ROM rejection, and detected runtime failure MUST return failure. Interruption MUST be distinguishable from bounded completion.
- **FR-006**: Ordinary termination requests MUST stop execution and release acquired resources within five seconds under the supported test workload.
- **FR-007**: The frontend MUST use the core's supported integration boundary, provide required Linux platform services, and supply neutral game input. Emulation behavior MUST remain the core's responsibility.
- **FR-008**: The repository MUST provide one documented build entry point covering frontend, core, and necessary dependencies from source, with native ARM64 Linux and Ubuntu x86-64 to ARM64 Linux configurations.
- **FR-009**: The build workflow MUST prepare required nested dependencies, identify network prerequisites, keep generated artifacts separate from tracked sources, and diagnose missing prerequisites and unsupported targets.
- **FR-010**: Linux build and execution MUST work without an Android application, SDK, NDK, original DraStic binary, or prebuilt Android core library.
- **FR-011**: The repository MUST provide repeatable Ubuntu tooling installation and verification instructions or automation, identifying the supported release baseline, privileges, and native and cross-build prerequisites.
- **FR-012**: Documentation MUST cover clean checkout, dependency preparation, tooling setup, build, bounded smoke run, and result interpretation, including WSL2 guidance for Windows developers.
- **FR-013**: Acceptance verification MUST demonstrate actual execution using an identified legally usable test ROM on real ARM64 Linux hardware and record core revision, build environment, device details, requested and completed frames, and outcome. SSH settings, when needed, MUST remain developer-local; credentials MUST not enter tracked files or logs.

- **FR-014**: Distributed builds MUST embed the core and application dependency libraries into the executable wherever compatible static versions are available. Binary size MUST NOT be a reason to leave those libraries external.
- **FR-015**: Compatibility and correct execution MUST take precedence over full static linking. Each retained shared-library dependency MUST be documented with its purpose, compatibility reason, and supported runtime baseline; the build MUST NOT silently fall back from static application dependencies to shared ones.
- **FR-016**: Release verification MUST inventory remaining runtime library dependencies and demonstrate the bounded-run scenario on a target containing only the documented operating-system runtime and user-provided ROM/data assets, with no separately installed application libraries except explicitly documented exceptions.

### Key Entities

- **Run request**: ROM path and optional positive frame limit.
- **Run result**: Outcome, completed frame count, and diagnostics distinguishing completion, interruption, and failure.
- **Build configuration**: Host environment, ARM64 Linux target, prerequisites, and artifact location.
- **Verification record**: Core revision, ROM identification, environment, target device, frame counts, and outcome.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A contributor using only repository instructions prepares clean supported Ubuntu and produces the complete ARM64 Linux runner with zero manual source edits or undocumented prerequisites.
- **SC-002**: Artifacts from both build workflows each complete three consecutive 120-frame runs on real ARM64 Linux hardware, reporting exactly 120 completed frames and successful termination every time.
- **SC-003**: All bounded acceptance runs succeed without display or audio services and produce zero windows, audible output, or microphone capture sessions.
- **SC-004**: Every documented invalid-input, missing-prerequisite, and ROM-rejection check reports failure and names the failed input or operation.
- **SC-005**: Three consecutive unlimited runs stop within five seconds of termination requests and leave no runner-owned processes executing.
- **SC-006**: Two consecutive executions of Ubuntu setup on the same supported environment each leave all prerequisite checks passing.

- **SC-007**: Both build configurations produce an executable with zero external core or application dependency libraries except explicitly documented compatibility exceptions, and each passes the 120-frame acceptance run without installing additional application libraries on the target.
- **SC-008**: Every remaining shared-library dependency is listed in the build's runtime inventory with its compatibility reason and runtime baseline; no undeclared runtime library is needed for acceptance runs.

## Assumptions

- The user's `/external/SeedlessDS-Core` means the existing repository-relative `external/SeedlessDS-Core` checkout.
- This is a headless integration milestone. Display layouts, menus, interactive controls, audio playback, and save-state controls are deferred. The constitution's eventual portable single-screen and dual-screen requirements remain applicable to later display work.
- ARM64 Linux is the runtime baseline because the supplied core includes AArch64 assembly. Ubuntu x86-64 and WSL2 are build hosts; native x86-64 execution is outside this milestone.
- Planning will select the supported Ubuntu LTS baseline and exact tools based on core requirements; this specification does not prescribe build technology.
- Contributors have network access for initial dependency acquisition and can obtain installation privileges.
- A legally usable known-good test ROM and real ARM64 Linux hardware will be available for acceptance. Commercial ROMs and proprietary BIOS or firmware images will not be bundled; any required user-provided runtime assets must be documented.
- Frame counts demonstrate actual execution rather than game compatibility or real-time speed. No throughput target is imposed.

- Static linking is a portability preference rather than a requirement to replace the existing operating-system runtime or eliminate required external ROM/data assets. A fully static executable is optional only if it preserves the supported compatibility and correctness guarantees.
