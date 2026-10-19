# Tasks: Device Debug Workflow and Build Documentation

**Input**: Design documents in `specs/002-device-debug-workflow/`: plan.md, spec.md, research.md, data-model.md, contracts/device-debug.md, quickstart.md; governance in `.specify/memory/constitution.md`.

**Organization**: Setup → foundation → US1 (P1) → US2 (P1) → US3 (P2) → polish. Paths are relative to repository root. This is an existing repository; do not reinitialize it or change the core, runner, or frontend.

**Tests**: Acceptance and failure verification below implement the specification's scenarios and success criteria. No separate automated test-writing tasks are added because the specification does not explicitly request an automated suite or TDD; the plan's proposed unittest suite at `tests/device-debug/test_debug_device.py` remains a design recommendation. Fake sessions never satisfy hardware acceptance.

**Format**: `- [ ] TNNN [P?] [USn?] Description with exact file paths`. `[P]` means work in distinct files can proceed together once listed prerequisites are complete. Story labels occur only in story phases.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish prerequisites without collecting private device settings.

- [ ] T001 Review `scripts/setup-ubuntu.sh`, `CMakePresets.json`, `CMakeLists.txt`, `scripts/audit-runtime.py`, and `src/` runner argument/result handling against `specs/002-device-debug-workflow/contracts/device-debug.md`; confirm existing paths, inventory fields, result syntax and repository-root assumptions before adding the CLI.
- [ ] T002 Add sshpass to installed packages and explicit ssh/sshpass checks in `scripts/setup-ubuntu.sh`, retaining native/cross architecture gates, Ubuntu 24.04/26.04 support, apt privilege behavior, dry-run and check modes; verify Python 3.12+ availability for the new workflow.

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish the shared CLI and private diagnostic boundary before story implementation.

- [ ] T003 Create the Python-standard-library CLI skeleton in `scripts/debug-device.py` with `check [--config PATH]` and `run [--config PATH] --binary PATH --inventory PATH --rom REMOTE_PATH --data-dir REMOTE_PATH [--frames N] [--timeout SECONDS]`; resolve default `.device.local` from repository root and explicit paths from caller directory, support help without config, and expose no password flag.
- [ ] T004 Implement shared stage/error and redaction infrastructure in `scripts/debug-device.py`: sanitize password/IP/install directory and escaped representations before any stdout/stderr or evidence write, handle split-chunk matches and exceptions, emit `stage=<name> status=<passed|failed|blocked>`, and reserve contract exits 0–7 with runner and transport outcomes distinct.

**Checkpoint**: Shared foundation is ready; no private settings have been requested or written.

## Phase 3: User Story 1 — Configure a Real Console Privately (Priority: P1) — MVP

**Goal**: Create reusable owner-only settings from a shared placeholder template and reject unsafe inputs before remote work.

**Independent Test**: Starting with the template, explicitly request exactly the three settings during T008, create the ignored file, run local-only `check`, and verify Git exclusion and invalid-field rejection without displaying values or connecting.

- [ ] T005 [P] [US1] Create `.device.example.json` with exactly `ip_address`, `root_password`, `install_dir` and contract placeholders `<CONSOLE_IP>`, `<ROOT_SSH_PASSWORD>`, `<ABSOLUTE_BINARY_INSTALL_DIRECTORY>`; preserve `.gitignore` exclusions for `.device.local` and `.device.local.*` and ensure the template is not ignored.
- [ ] T006 [P] [US1] Implement JSON/schema and file validation in `scripts/debug-device.py` using these verbatim `data-model.md` constraints: `ip_address`: "literal IPv4/IPv6, no hostname/port"; `root_password`: "nonempty nonplaceholder string, no NUL/CR/LF, preserve whitespace and special characters"; `install_dir`: "absolute POSIX directory, no control characters, reject `/` and traversal components". Reject malformed JSON, duplicate/unknown/missing keys, wrong types and placeholders with field-name-only errors and no parser excerpts; on POSIX require owner match, regular nonsymlink config and no group/other permissions.
- [ ] T007 [US1] Complete local-only `check` in `scripts/debug-device.py` after T006: validate configuration, Python and host ssh/sshpass/readelf prerequisites, return 0 on success or 2 for local validation errors, and make no network calls; reuse existing valid settings without rewriting them.
- [ ] T008 [US1] During implementation explicitly ask the user for all three real values—console IP address, root SSH password and absolute binary installation directory—before creating/populating `.device.local`; create atomically with restrictive umask and mode 0600 without credentials in displayed commands/history, preserve literal values, and never silently overwrite existing settings. If inputs are unavailable, leave this task pending and continue only independent work.
- [ ] T009 [US1] Verify `.device.local` with local-only `check` and `git check-ignore`, verify `.device.example.json` remains eligible for tracking, and exercise missing/malformed/placeholder/duplicate/unknown-key/wrong-type/permissions/symlink/IPv6/special-character cases using temporary files with `scripts/debug-device.py`; confirm invalid inputs cause no remote actions and no value exposure.

**Checkpoint**: US1 is complete only when the user-supplied local configuration exists and passes the independent checks. No hardware execution is claimed.

## Phase 4: User Story 2 — Run and Verify Debugging on Hardware (Priority: P1)

**Goal**: Connect as root with strict host verification, deploy only a verified compatible artifact, run bounded emulation, and record actual observations safely.

**Independent Test**: Use valid settings, a fresh matching artifact/inventory and user-supplied target assets to complete one real-console session with verified deployed SHA-256, runner exit 0, and exact `result=completed frames=N requested=N`. Repeat to establish configuration reuse. Failures must stop dependent stages and remain unsuccessful.

- [ ] T010 [US2] Implement artifact and runtime-argument preflight in `scripts/debug-device.py`: enforce BuildArtifact "Local regular readable ELF path, AArch64 machine, SHA-256, size, interpreter, shared dependencies, required GLIBC versions, matching inventory path" and RuntimeInputs "Device-local absolute ROM path and existing writable data directory; frames integer 1..UINT64_MAX; timeout positive seconds". Check path syntax locally, defer asset existence/writability to target preflight, default frames to 120 and timeout to 30 seconds, rerun/reuse `scripts/audit-runtime.py` inspection against current bytes, compare supplied inventory/path with fresh findings, and stop stale/missing/incompatible artifacts before connection.
- [ ] T011 [US2] Implement OpenSSH/sshpass transport in `scripts/debug-device.py`: root/port 22 with literal IPv4/IPv6, fresh anonymous inherited password fd per subprocess, binary stdin preserved, no password argv/environment or local shell, explicit password-only authentication, strict known-host verification, disabled forwarding/multiplexing/config surprises, 10-second connect bound and actionable connection/auth/host-key failures (exit 3); quote each remote token with POSIX shlex quoting.
- [ ] T012 [US2] Add remote compatibility/runtime preflight in `scripts/debug-device.py` after T010–T011: require Linux/AArch64, compatible loader/libc/shared dependencies and every required GLIBC symbol version, POSIX shell and contract utilities including usable timeout, readable existing ROM and writable existing data directory; unknown compatibility or missing assets/tools exits 4 before deployment without provisioning assets or libraries.
- [ ] T013 [US2] Implement destination locking and staged deployment in `scripts/debug-device.py`: safely create/check the writable real install directory, reject symlink final executable/unsafe staging and concurrent destination sessions, stream to a restrictive unique same-directory temporary file with a 120-second transfer bound, compare received SHA-256, chmod and atomically promote to `seedlessds-headless` only after full verification; retain old executable on transfer/hash/promotion failure, clean only this session's temporary file/lock, report deployment exit 5 and stale-lock inspection/recovery guidance.
- [ ] T014 [US2] Implement post-promotion help launch and bounded runner execution in `scripts/debug-device.py` using unchanged `--data-dir`, `--frames` and ROM arguments, TERM then KILL after 5-second grace, a separately parsed wrapper completion marker and exact runner result matching; distinguish application/timeout/malformed-result exit 6 from interrupted/disconnected/unconfirmed exit 7, never interpret SSH 255 as a confirmed runner exit, retain verified executable after execution failure, and report success only with verified deployment plus runner exit 0 and exact bounded completion.
- [ ] T015 [US2] Persist one uniquely named sanitized HardwareValidationRecord per invocation in ignored `build/validation/device/` via `scripts/debug-device.py`, including "Timestamp, revision, artifact hash/preset, sanitized device model/OS/architecture/libc, stage statuses, runner exit, transport exit, result record, observations, issues/workarounds"; use stage values "`passed`, `failed`, `blocked`, or `not_run`", retain no raw settings, record early failures where possible, and mark disconnect after promotion/execution unconfirmed unless explicit completion arrived.
- [ ] T016 [US2] Exercise `scripts/debug-device.py` with controlled fake subprocess/transport inputs for secret/chunk/escape redaction, literal remote metacharacters, auth/host failures, ABI/assets rejection, corrupt/interrupted transfer, old-binary preservation, lock ownership/concurrency, timeout, nonzero runner exit, incidental/forged/missing completion output and disconnect; inspect sanitized records in `build/validation/device/` and verify every failed stage prevents dependent actions and contract exits remain distinct.
- [ ] T017 [US2] Build and audit the intended artifact using `scripts/setup-ubuntu.sh` install/check and the appropriate `CMakePresets.json` preset from repository root; inspect `build/<preset>/bin/seedlessds-headless` and `build/<preset>/runtime-inventory.json`, use `SEEDLESS_TARGET_SYSROOT` before a clean configure if required, run native CTest only on ARM64, and retain sanitized build/audit observations under `build/validation/device/` without running ARM64 binaries on amd64.
- [ ] T018 [US2] After T008–T017, obtain user-supplied device-local ROM/data paths and console availability, verify/enroll the SSH fingerprint through a trusted source using interactive SSH, then execute the complete `scripts/debug-device.py run` workflow on the actual console and repeat with reused settings; inspect `build/validation/device/` for hash agreement, stage results, runner/SSH exit and exact requested-frame completion. Missing credentials/hardware/assets or failed execution leaves successful FR-009/SC-002 acceptance pending; do not substitute fake transport or help output.
- [ ] T019 [US2] Add actual hardware findings to `README.md` Debug from manually inspected `build/validation/device/` evidence: attempt date, sanitized model/OS/kernel/architecture/libc, revision/artifact hash/preset, actual sanitized steps, outcomes/exits, issues and observed workarounds; explicitly distinguish failed/blocked attempts from successful bounded execution and leave T018 outstanding until it succeeds.

**Checkpoint**: US2 requires a successful actual bounded run. A truthful failed/blocked README entry is useful progress but does not complete hardware acceptance.

## Phase 5: User Story 3 — Build and Debug from the README (Priority: P2)

**Goal**: Make the existing native/cross build and private real-device workflow reproducible from the README.

**Independent Test**: Follow README from its stated Ubuntu/WSL2 baseline through preparation, intended build, configuration and real-device execution without undocumented steps; compare the recorded hardware claims with the actual evidence.

- [ ] T020 [US3] Expand `README.md` Build with repository-root working directory, Ubuntu 24.04/26.04 ARM64 native and amd64 cross/Windows x86-64 WSL2 environments, Linux-filesystem recommendation, Python/OpenSSH/sshpass/CMake >=3.28/Ninja/toolchain dependencies, submodule initialization, network requirements, setup install then check commands for both modes and apt permissions; explain automatic `scripts/prepare-core.sh` and runtime-audit order, sysroot environment and clean reconfiguration.
- [ ] T021 [US3] Add explicit native and cross configure/build/inspection commands and native-only CTest in `README.md` Build; identify both executable and inventory output paths, compatible ARM64 Linux execution environments, actual ELF-derived loader/libc requirements rather than a universal fixed GLIBC baseline, and prevent examples implying execution on amd64.
- [ ] T022 [US3] Complete `README.md` Debug around T019 findings: describe all three template/local fields and explicit user-input step, safe owner-only JSON creation without inline credentials, Git ignore checks, local `check`, trusted fingerprint enrollment, target tool/ABI prerequisites, device-local ROM/data/BIOS layout, full run commands/defaults, literal path handling, stage/exit/result interpretation, evidence location, locking/stale-lock recovery and actionable failure remediation; link existing optional `scripts/validate-headless.sh` with its additional target prerequisites.
- [ ] T023 [US3] Follow the final `README.md` Build and Debug commands against `specs/002-device-debug-workflow/quickstart.md`, verify no required step depends on undocumented context, and reconcile instructions with inspected hardware evidence in `build/validation/device/`; correct documentation gaps and report any blocked baseline/native/hardware checks accurately without asserting unperformed validation.

**Checkpoint**: README reproduces the supported build path and real hardware workflow, with observations matching evidence.

## Phase 6: Polish & Cross-Cutting Concerns

- [ ] T024 [P] Run Bash syntax and ShellCheck for modified `scripts/setup-ubuntu.sh`, inspect dry-run/check behavior on supported available hosts, and verify `scripts/debug-device.py --help`, subcommand help and default/explicit configuration resolution from both repository and another working directory.
- [ ] T025 [P] Review `README.md`, `.device.example.json`, changed tracked files, `.gitignore` behavior and `build/validation/device/` evidence for raw/escaped private settings, ensure `.device.local` remains owner-only and untracked, and verify no ROMs/assets or raw logs are introduced into shared changes.
- [ ] T026 Reconcile `specs/002-device-debug-workflow/quickstart.md` with the implemented CLI and README, record completed versus blocked acceptance checks and requirement coverage there, and confirm `specs/002-device-debug-workflow/tasks.md` does not mark T008/T018 or dependent acceptance complete without actual user inputs and successful hardware evidence.

## Dependencies & Execution Order

```text
T001 → T002 → T003 → T004 → foundation ready
foundation → US1: (T005 || T006) → T007 → T008 → T009
foundation + T006/T007 → US2: T010 → T011 → T012 → T013 → T014 → T015 → T016
T002 → T017 (build can overlap US1/US2 development)
US1 complete + T016 + T017 → T018 → T019
US2 findings + T020 → T021 → T022 → T023 → US3 complete
all stories → (T024 || T025) → T026
```

US1 is independently testable without hardware execution. US2 integrates US1 validation and requires user configuration plus hardware/assets for acceptance; its CLI work can proceed while those inputs are pending. US3 Build drafting can start after setup review, but final Debug and acceptance depend on US2 observations. Do not present these integrated stories as having no dependencies.

Within the single `scripts/debug-device.py`, execute edits sequentially to avoid conflicts. T010–T016 do not require actual private credentials. T017 can proceed independently after T002 and before real configuration. T019 may record a failed/blocked attempt early, but successful hardware acceptance remains open.

## Parallel Opportunities and Examples

- **US1**: After foundation, T005 template/ignore work and T006 CLI validation touch distinct files and can run together. Finish both before configuration acceptance.
- **US2**: After T002 and T007, T017 intended build/audit can run alongside T010–T016 CLI work; artifacts must be ready before T018. CLI tasks themselves are sequential because they share one file. Hardware validation cannot overlap unfinished deployment/execution logic.
- **US3**: After T001/T002, T020–T021 Build documentation can be drafted while US2 CLI work proceeds in `scripts/debug-device.py`. Serialize all `README.md` edits with T019 and T022; final walkthrough waits for final documentation and actual hardware findings.
- **Polish**: T024 tool checks and T025 privacy review can run together after story work; T026 consumes both results.

## Implementation Strategy

1. Deliver setup/foundation and US1 as the MVP: placeholder template, local-only validation, explicitly requested owner-only ignored settings. Stop its acceptance at T009; this MVP makes no claim of hardware success.
2. Implement verified deployment and bounded execution incrementally, validate controlled failures, build the intended artifact and complete the real-console gate. Preserve failed/blocked evidence truthfully until the full session succeeds.
3. Complete README Build/Debug using observed behavior, follow its instructions and finish cross-cutting checks. Keep hardware-dependent acceptance pending when external inputs are unavailable while continuing independent documentation/tool work.

## Completeness and Requirement Mapping

| Requirements | Tasks |
|---|---|
| FR-001–004; SC-001 | T005–T009 |
| FR-005–007; SC-005 | T010–T016, T018 |
| FR-008 | T004–T006, T008–T009, T011, T015–T016, T025 |
| FR-009; SC-002 | T017–T018 (real success required) |
| FR-010; SC-004 | T015, T018–T019, T022–T023 |
| FR-011–012; SC-003 | T001–T002, T017, T020–T023 |
| FR-013; constitution | T001, T010–T014, T017, T026 |

All four entities and RuntimeInputs/Deployment supporting records are assigned to US1 or US2. No database, service, device provisioning, core change or ROM distribution is required.
