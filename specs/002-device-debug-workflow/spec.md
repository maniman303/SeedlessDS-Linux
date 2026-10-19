# Feature Specification: Device Debug Workflow and Build Documentation

**Feature Branch**: `main` (existing branch; no branch hook configured)

**Created**: 2026-10-19

**Status**: Draft

**Input**: User description: "Define a debug process using a git-ignored real-console configuration containing its IP address, root SSH password, and binary installation directory; provide a tracked template; ask for these values during the configuration task; run the process on hardware and document findings in README Debug; document the build environment, prerequisite scripts and build commands in README Build."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Configure a Real Console Privately (Priority: P1)

A developer prepares a local configuration for their console from a shared template so the debug process knows where to connect and install binaries without sharing private connection details.

**Why this priority**: Real-device deployment depends on accurate connection settings, and the root password must remain private.

**Independent Test**: Starting with only the tracked template, request the developer's three values, create the local configuration, and verify that Git excludes it while retaining the template.

**Acceptance Scenarios**:

1. **Given** the configuration-creation task is being executed, **When** console details are needed, **Then** the implementer explicitly asks the user for the console IP address, root SSH password, and absolute binary installation directory before populating the local file; no value is invented or copied from shared examples.
2. **Given** the supplied values, **When** configuration is created, **Then** all three are available to the debug workflow, the local file is ignored by Git, and the template remains eligible for tracking with placeholders only.
3. **Given** missing, malformed, or placeholder values, **When** debugging is requested, **Then** the workflow reports the invalid field and stops before connecting or copying binaries without displaying the password.

---

### User Story 2 - Run and Verify Debugging on Hardware (Priority: P1)

A developer runs a documented process that connects to the configured real ARM64 console as root over SSH, copies the intended build into the configured directory, and executes it to observe its behavior and diagnostics.

**Why this priority**: A successful build alone does not demonstrate that the application runs on the console.

**Independent Test**: With a valid configuration, a compatible built artifact, and required user-provided runtime inputs, perform one complete connection, deployment, and execution on real hardware and record the outcome.

**Acceptance Scenarios**:

1. **Given** reachable compatible hardware and valid configuration, **When** the debug process runs, **Then** it authenticates as root using the configured password, deploys the intended binaries to the configured directory, executes the application, and exposes the resulting diagnostics and exit outcome without exposing credentials.
2. **Given** a completed hardware run, **When** documentation is updated, **Then** the README Debug section records the tested device and environment, steps actually performed, observed results, issues, and any workarounds with private values removed.
3. **Given** unavailable hardware, failed authentication, a failed transfer, or a failed application run, **When** validation is reported, **Then** the failing stage is identified, subsequent dependent stages stop, and the README distinguishes failed or blocked validation from successful validation.
4. **Given** an existing local configuration, **When** a subsequent debug session starts, **Then** it reuses those settings without requiring edits to tracked files.

---

### User Story 3 - Build and Debug from the README (Priority: P2)

A developer follows the README Build section to prepare a supported environment and produce a target-compatible artifact, then follows its Debug section to deploy and run it on the console.

**Why this priority**: Reproducible instructions let contributors use the workflow without reconstructing setup steps from source files.

**Independent Test**: Follow the README from an environment satisfying its stated baseline through prerequisite setup, build, and the documented hardware workflow, recording any missing steps.

**Acceptance Scenarios**:

1. **Given** a supported Linux host or Windows x86-64 host using WSL2 Ubuntu, **When** the developer reads Build, **Then** they can identify supported host versions, required tools and dependencies, environment variables, target compatibility requirements, and working directories.
2. **Given** a supported host, **When** prerequisite preparation is followed, **Then** Build explains which setup and verification scripts to run, their arguments and order, and any permissions required before configuration and compilation.
3. **Given** completed preparation, **When** the developer follows Build, **Then** explicit commands cover configuration and compilation for native ARM64 and cross builds, identify the output binaries, and explain where those binaries can run.
4. **Given** the built artifact, **When** the developer reads Debug, **Then** instructions cover template-to-local configuration setup, required connection data, runtime inputs, deployment, execution, diagnostic interpretation, and the actual hardware findings.

### Edge Cases

- Local configuration is absent, incomplete, or still contains template placeholders.
- Passwords contain spaces, quotes, or other special characters; values must be preserved and never interpreted as commands.
- The IP address is invalid or the console is unreachable; authentication or host verification fails.
- The destination is relative, absent, unwritable, or contains spaces; invalid paths fail before deployment, and a valid missing directory is created or receives an actionable prerequisite instruction.
- The binary is missing or incompatible with the console architecture or runtime libraries.
- Transfer or execution fails partway through; an incomplete deployment must not be reported as successful or executed as if complete.
- Required ROMs or other runtime assets are unavailable; validation remains explicitly blocked until supplied by the user.
- Shared logs or README examples could expose a password or device-specific values; recorded findings use redacted values or placeholders.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The feature MUST provide a developer-local debug configuration containing the real console IP address, root SSH password, and absolute remote directory where binaries are installed or copied.
- **FR-002**: The local configuration MUST be ignored by Git; a matching template MUST be included in version control with documented fields and placeholders, without real credentials or private console settings.
- **FR-003**: The later implementation task that creates and populates the local configuration MUST explicitly ask the user for all three values before writing them. Specification and planning MUST NOT require those private values.
- **FR-004**: The debug process MUST load and validate the local configuration before remote actions, identify missing or invalid fields, and preserve supplied values literally, including special characters.
- **FR-005**: The debug process MUST connect to the configured console as root over SSH using its configured password and deploy the intended build binaries into the configured directory without requiring changes to tracked connection settings.
- **FR-006**: The debug process MUST run the deployed application on real ARM64 Linux hardware with the required user-provided runtime inputs and provide diagnostics and an observable execution outcome. Debugging here means deployment, execution, and diagnostic inspection; interactive breakpoint debugging is outside this feature's required scope.
- **FR-007**: The workflow MUST identify connection, authentication, transfer, compatibility, and execution failures with actionable messages; failed prerequisite stages MUST prevent dependent execution, and incomplete transfers MUST not be treated as successful deployments.
- **FR-008**: Real passwords MUST NOT appear in tracked files, documentation, displayed commands, diagnostic logs, or recorded validation evidence. The local credential file MUST be restricted to its owner where filesystem permissions support that restriction.
- **FR-009**: Implementation validation MUST include actually running the complete debug process on the user's real console. A simulated session, cross-compilation result, or instructions alone MUST NOT satisfy hardware acceptance.
- **FR-010**: Following the hardware attempt, the README MUST contain a Debug section describing prerequisites, local configuration setup, deployment and execution commands, runtime inputs, expected diagnostics, and findings grounded in that attempt, including tested device/environment, observed outcome, issues, and workarounds. Failed or blocked attempts MUST be stated accurately; successful hardware acceptance remains outstanding until a complete run succeeds.
- **FR-011**: The README MUST contain a Build section describing supported native Linux and WSL2 Ubuntu environments, dependencies and toolchains, required environment settings, setup and verification scripts with arguments and order, and configuration and build commands for native ARM64 and cross builds.
- **FR-012**: Build documentation MUST state command working directories, resulting artifact locations, target runtime compatibility constraints, and which host or device can execute each artifact.
- **FR-013**: The documented workflow MUST reuse the project's existing build capabilities and runner behavior; unrelated frontend features, changes to core emulation, distribution of ROMs, and automated device provisioning are outside scope.

### Key Entities

- **Local Debug Configuration**: Private developer settings identifying one real console, its root authentication password, and the binary destination directory.
- **Configuration Template**: Shared placeholder-only description of the fields required to create a local configuration.
- **Build Artifact**: A built executable intended for the console, associated with its architecture and runtime requirements.
- **Hardware Validation Record**: Sanitized account of a real attempt, including device/environment, executed workflow, outcome, issues, and workarounds, published in README Debug.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can complete configuration from the template using exactly the three requested console values, with zero real passwords or private console settings in tracked changes.
- **SC-002**: At least one complete real-console session successfully connects, deploys the intended artifact to the selected destination, and executes it with an observable result; all stages are backed by recorded observations.
- **SC-003**: A developer following only the README and its explicitly linked references can prepare a supported environment, produce the intended artifact, and complete a hardware session without undocumented prerequisite steps.
- **SC-004**: Every hardware outcome claimed in README Debug is traceable to the actual validation attempt, with zero untested claims presented as observed successes.
- **SC-005**: Missing configuration, invalid configuration, authentication failure, transfer failure, and application failure each produce an identifiable outcome without revealing the password or falsely reporting success.

## Assumptions

- The target is a user-controlled ARM64 Linux console that permits root SSH password authentication and is reachable from the development environment; changing console authentication policies is outside scope.
- The user will provide connection values during the implementation configuration task and make the console available for validation. The password is retained only in the ignored local file as requested; shared artifacts contain placeholders.
- The current headless runner is the application to deploy and execute. Required legal ROMs and runtime assets are supplied by the user and are not distributed by this feature.
- Existing setup scripts and native/cross build presets remain the baseline; documentation must verify their current behavior rather than assume previously documented steps still work.
- Hardware access and compatible runtime libraries are dependencies for successful end-to-end acceptance. A blocked attempt can be documented but does not complete that acceptance.
