<!--
Sync Impact Report
Version change: 1.4.0 → 1.5.0 (require network-enabled execution on the first attempt for commands that need network access)
Modified principles: None.
Added sections: None.
Modified sections: Development Workflow (require network-enabled execution for permitted network-dependent commands from the first attempt).
Removed sections: None
Follow-up TODOs: Confirm the original ratification date when project history provides it.
-->
# SeedlessDS-Linux Constitution

## Core Principles

### I. Linux and ARM64 First
The frontend MUST run on ARM64 Linux devices, including the RG DS Plus target class. Linux
ARM64 is a primary deployment target and MUST remain a first-class consideration in architecture,
dependency selection, and release decisions. Rationale: the project exists to provide a usable
Linux frontend for SeedlessDS-Core on these devices.

### II. Portable Frontend and Display Support
The frontend MUST use broadly available, portable Linux graphics and input libraries. SDL2 or
SDL3 MUST be preferred for cross-device windowing, display, and input support unless a documented
technical constraint requires another choice. The design MUST support dual-screen devices as a
primary use case and provide a usable single-screen mode. Device-specific behavior MUST be isolated
so it does not prevent operation on other compatible Linux devices.

### III. SeedlessDS-Core Integration Boundary
The project MUST act as a Linux frontend for SeedlessDS-Core and MUST preserve a clear boundary
between frontend responsibilities and core functionality. Core behavior MUST use the core's
supported interfaces rather than being reimplemented in the frontend. Integration changes MUST
account for the core's build and runtime requirements on ARM64 Linux.

### IV. Linux Build and Cross-Compilation
The project MUST be compilable in a Linux environment and MUST support producing ARM64 Linux
builds with compilers and toolchains that target ARM64. Development on Windows x86-64 MUST use
WSL2 with Ubuntu as the Linux build environment. Build instructions and dependencies MUST make
the native Linux and WSL2 workflows reproducible. Rationale: matching the target operating system
and architecture reduces host-specific build failures.

### V. Hardware-Based Debugging and Credential Safety
Final device debugging and validation MUST be performed on real ARM64 Linux hardware, accessed
over SSH where supported. Agents MUST build the current worktree locally in its intended Linux
environment before device validation; on an x86-64 host, use the ARM64 cross-build. Agents MUST
transfer the resulting executable to the device over SSH and run/debug that binary there, and
MUST NOT build the project on the device or transfer source for a remote build. A developer-local,
git-ignored configuration MAY hold device addresses and connection settings. Credentials MUST NOT
be committed, logged, or stored as plaintext in tracked files; SSH keys or an SSH agent SHOULD be
used for authentication. Hardware-specific issues MUST be recorded with enough device and
environment detail to reproduce them. SSH device access is for artifact transfer, application
execution, diagnostics, and necessary asset checks; it does not permit remote Git or GitHub
operations, commits, pushes, or sudo operations.

### VI. Local Git Only; No Agent Commits or GitHub Collaboration Actions
All agents MUST operate locally and MUST NOT perform GitHub collaboration or repository-management
actions. This prohibition includes issues, pull requests, comments, reviews, repository settings,
GitHub Actions workflows, and other account or repository operations.

Agents MAY download source archives, dependency archives, or release assets required by the
repository's documented local build and test workflows, including from GitHub-hosted URLs. Such
downloads MUST use project-declared or otherwise pinned inputs and MUST verify the declared
checksum when available. This exception is limited to obtaining local build/test inputs; it does
not permit Git operations, collaboration actions, or unrelated GitHub browsing.

Agents MUST NOT create any Git commit, including local commits, amended commits, or commits
created indirectly by merge, rebase, cherry-pick, scripts, or other commands. Agents MUST NOT
push anything to `origin` or any other remote, including branches, tags, commits, or other refs.
Agents MUST NOT perform other remote Git operations, including clone, fetch, or pull. Local Git
inspection, diffs, staging, and branch or worktree operations MAY be used only when they do not
create commits, contact remotes, or perform a GitHub action. Rationale: the user retains exclusive
control over commits and all remote repository activity.

These restrictions apply to every agent, delegated agent, script, hook, skill, tool, connector,
and automated workflow acting on an agent's behalf. Agents MUST NOT attempt a prohibited action,
request tool approval for it, or bypass the restriction through another tool, transport, account,
remote name, or intermediary. A conflicting task, template, skill, hook, or workflow MUST be
skipped for the prohibited action, and the agent MUST report the conflict. The permitted local
build/test input downloads above are the only exception to the prohibition on GitHub access.

## Platform and Build Constraints

The supported baseline is Linux on ARM64. SDL2 and SDL3 are the preferred portable platform
libraries. The application MUST provide useful operation on both dual-screen and single-screen
devices. Build and packaging decisions MUST account for the target device's available Linux
libraries and ARM64 toolchain; optional device-specific integrations MUST NOT be required for the
basic single-screen frontend to launch.

## Development Workflow

Before invoking a tool, script, hook, or workflow, agents MUST check that it cannot perform an
action prohibited by Principle VI. Prohibited steps MUST be skipped and reported; permitted local
work MUST continue where independent of those steps.

Commands that require network access MUST use network-enabled execution from the first attempt
whenever the execution environment provides that option. Agents MUST NOT first try such a command
inside a network-restricted sandbox and retry only after it fails. This applies especially to SSH
device deployment, execution, and diagnostics, and to permitted downloads of pinned build/test
inputs. Network-enabled execution does not permit remote Git operations or GitHub collaboration
actions prohibited by Principle VI, and does not permit bypassing the sudo handoff.

Changes MUST preserve the principles above. Each change MUST be built in its intended Linux build
environment before it is considered ready. For device run/debug work, agents MUST first build
locally, then transfer the executable over SSH and run/debug it on the device; remote compilation
is prohibited. Where available, use the repository deployment helper to retain binary-integrity
checks, host verification, and diagnostic capture. Reports MUST distinguish local build and
deployment success from successful application or emulation execution, and state when missing
device inputs prevent a full run. Changes to display handling MUST be checked for both dual-screen
and single-screen behavior. Device-dependent behavior MUST be validated on real hardware over SSH
when applicable. Reviews MUST identify any exception to these constraints and document its reason
and scope.

Builds MUST use the canonical CMake preset directories: `build/linux-arm64-cross` for ARM64
cross-builds and `build/linux-arm64-native` for native ARM64 builds. Agents MUST NOT create
suffixed or task-specific alternatives. Both presets MUST place the application at
`build/<preset>/bin/seedlessds` and the runtime inventory at
`build/<preset>/runtime-inventory.json`. The supported executable name is `seedlessds`; stale
`seedlessds-headless` artifacts from older builds MUST be removed from the active preset output.
The standard configure/build/test entry point is `scripts/build.sh`; agents SHOULD use it unless
the task requires a specific CMake operation outside the wrapper's options.

All tasks, agents, scripts, and automated workflows MUST stop before an operation that requires
`sudo`. The agent or workflow MUST explain why elevated privileges are needed, provide the exact
command for the user to run manually, and ask the user to run it. Agents and automation MUST NOT
execute the privileged operation themselves or bypass this handoff through another elevation
mechanism. Work MUST resume only after the user explicitly confirms successful completion;
elapsed time or approval to proceed does not count as confirmation that the command was run.
If the user reports failure, the workflow MUST resolve the failure before continuing dependent
steps. Rationale: privileged changes remain under the user's direct control.

Linux terminal commands presented to the user MUST assume an existing Linux terminal, including
Ubuntu in WSL. Commands MUST be directly runnable in that terminal without a `bash`, `bash -c`,
`wsl`, or equivalent shell-launch wrapper. Instructions MUST NOT tell the user to open or invoke
Bash before running Linux commands. Shell code fences MUST use `sh` or omit the language label
instead of using `bash`. Rationale: the user's WSL Ubuntu terminal already provides the shell.

## Governance

This constitution governs project architecture and development decisions. Amendments MUST be made
in this document, include a version and date update, and explain their impact in a Sync Impact
Report during review. Version numbers use semantic versioning: MAJOR for incompatible changes to
governance, MINOR for new or materially expanded principles or sections, and PATCH for wording or
clarity changes that do not change requirements. The constitution MUST be reviewed when a change
affects platform support, core integration, build workflows, display support, or hardware access.
Contributors and reviewers MUST check proposed work against these principles and document any
approved exception and its rationale. Principle VI permits no exceptions under this process;
its restrictions require an explicit constitutional amendment to change. Compliance reviews MUST
check agent tool use and Git operations against Principle VI.

**Version**: 1.5.0 | **Ratified**: TODO(RATIFICATION_DATE): original adoption date is unknown | **Last Amended**: 2026-10-19
