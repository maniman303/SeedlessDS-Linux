<!--
Sync Impact Report
Version change: 1.1.0 → 1.2.0 (new non-negotiable agent Git and GitHub restrictions)
Modified principles: None; existing principles preserved.
Added sections: VI. Local Git Only; No Agent Commits or GitHub Actions.
Modified sections: Development Workflow and Governance (restriction enforcement).
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
over SSH where supported. A developer-local, git-ignored configuration MAY hold device addresses
and connection settings. Credentials MUST NOT be committed, logged, or stored as plaintext in
tracked files; SSH keys or an SSH agent SHOULD be used for authentication. Hardware-specific
issues MUST be recorded with enough device and environment detail to reproduce them.

### VI. Local Git Only; No Agent Commits or GitHub Actions
All agents MUST operate locally and MUST NOT attempt any GitHub action, including read-only
GitHub access. This prohibition includes GitHub websites, APIs, CLI commands, connectors, and
integrations; issues, pull requests, comments, reviews, releases, repository settings, and GitHub
Actions workflows MUST NOT be accessed, created, modified, triggered, or otherwise operated by
agents.

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
skipped for the prohibited action, and the agent MUST report the conflict. No routine exception
or general approval overrides this principle; changing it requires an explicit amendment to this
constitution.

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

Changes MUST preserve the principles above. Each change MUST be built in its intended Linux build
environment before it is considered ready. Changes to display handling MUST be checked for both
dual-screen and single-screen behavior. Device-dependent behavior MUST be validated on real
hardware over SSH when applicable. Reviews MUST identify any exception to these constraints and
document its reason and scope.

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

**Version**: 1.2.0 | **Ratified**: TODO(RATIFICATION_DATE): original adoption date is unknown | **Last Amended**: 2026-10-19
