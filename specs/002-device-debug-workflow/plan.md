# Implementation Plan: Device Debug Workflow and Build Documentation

**Branch**: `main` | **Date**: 2026-10-19 | **Spec**: [spec.md](spec.md)

**Input**: `specs/002-device-debug-workflow/spec.md`

## Summary

Add a developer-local, password-authenticated SSH deployment and execution workflow for the existing ARM64 headless runner. Use a Python standard-library CLI, a tracked JSON template and ignored owner-only local JSON file, OpenSSH plus sshpass with an anonymous password pipe, staged verified deployment, bounded execution, and sanitized evidence. Expand README Build and Debug using existing setup scripts, CMake presets, and observations from an actual user-console attempt during implementation.

Planning stops after design. No device configuration is populated, credentials requested, application implementation changed, or hardware success asserted here. Setup reports feature identifier `002-device-debug-workflow`; actual Git branch remains `main`, matching the spec. PowerShell is unavailable; the repository's Bash setup-plan twin was used.

## Technical Context

**Language/Version**: Python 3.12+ for new workflow and unittest tests (Ubuntu 24.04 baseline); existing Bash and C11 runner unchanged.

**Primary Dependencies**: Python standard library; OpenSSH client; sshpass (new debug prerequisite); existing CMake >=3.28, Ninja, GCC/aarch64-linux-gnu toolchain and readelf. Target: POSIX shell, uname, mkdir, cat, chmod, mv, rm, sha256sum, timeout, and compatible glibc loader/libc. No target Python or SFTP required.

**Storage**: `.device.local` JSON (ignored, mode 0600); `.device.example.json` tracked template; ignored `build/validation/device/` sanitized session evidence; manually reviewed README findings.

**Testing**: Python unittest with fake subprocess/transport and temporary files; setup script syntax/ShellCheck if modified; existing cross build and runtime audit; real console bounded run. Native CTest only on ARM64.

**Target Platform**: Ubuntu 24.04/26.04 ARM64 native or amd64 cross hosts; Windows x86-64 via WSL2 Ubuntu; real ARM64 Linux console with root SSH password authentication and matching libc ABI.

**Project Type**: Developer CLI and build/debug documentation around existing headless executable.

**Performance Goals**: One device and artifact per invocation; default 120 scheduler boundaries; connect timeout 10 seconds, transfer timeout 120 seconds, execution timeout 30 seconds plus 5-second termination grace. No emulation performance change or benchmark requirement.

**Constraints**: Ask for all three private configuration values only during implementation configuration task. Never evaluate config as code; never expose password in argv, environment, diagnostics or evidence. Preserve SSH host verification. Do not execute incomplete transfers. No ROM distribution, device provisioning, core changes or frontend work.

**Scale/Scope**: One template, one Python CLI, focused tests, small setup-prerequisite adjustment, README Build/Debug, and one real-console acceptance record.

## Constitution Check

*Initial gate before research and final gate after design: PASS.*

| Principle | Initial assessment | Post-design assessment |
|---|---|---|
| I. Linux and ARM64 first | Deploy only the existing Linux ARM64 artifact | ELF and target checks precede execution; real hardware required |
| II. Portable display/input | No display/input changes | Headless workflow leaves dual/single-screen work outside scope |
| III. Core boundary | Reuse runner CLI and build manifest | No upstream or emulation changes |
| IV. Linux builds and cross-compilation | Existing native/cross presets remain baseline | README covers Ubuntu and WSL2, sysroot, output and execution constraints; intended build required during implementation |
| V. Hardware and credential safety | Owner-only ignored settings and actual console acceptance | Password requested explicitly by feature; sshpass receives it through an anonymous pipe; host verification retained; sanitized observations only |

Password authentication departs from the constitution's SHOULD preference for SSH keys because FR-001/005 explicitly require the configured root password. This is a scoped rationale, not a violation of a MUST. A failed or blocked hardware attempt can be recorded but does not satisfy FR-009/SC-002. No unresolved design clarification or unjustified gate failure remains; device identity, ABI, and runtime assets are implementation inputs to discover on the actual console.

## Project Structure

### Documentation (this feature)

```text
specs/002-device-debug-workflow/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
└── contracts/
    └── device-debug.md
```

`tasks.md` is a subsequent speckit-tasks output and is not generated here.

### Source Code (repository root)

```text
.device.example.json               # new placeholder template
.device.local                      # private ignored file, created during implementation
.gitignore                         # existing .device.local ignore rules retained
README.md                          # Build and Debug sections
scripts/
├── debug-device.py                 # new host CLI
├── setup-ubuntu.sh                 # add/check debug prerequisites
├── prepare-core.sh                 # existing automatic configure prerequisite
├── audit-runtime.py                # existing ELF audit
└── validate-headless.sh            # existing optional deeper native acceptance
tests/
└── device-debug/
    └── test_debug_device.py         # new credential, quoting, failure and staging checks
build/validation/device/            # ignored per-run evidence
```

**Structure Decision**: Keep host tooling in `scripts/`; use no additional service, package manager, SSH library, or target agent. Existing CMake/source paths are unchanged. Document interfaces in [contracts/device-debug.md](contracts/device-debug.md), entities in [data-model.md](data-model.md), decisions in [research.md](research.md), and validation in [quickstart.md](quickstart.md).

## Complexity Tracking

No MUST violations or additional architectural layers require justification.
