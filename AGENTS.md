# Agent Instructions

These instructions apply to every agent working in this repository, including delegated agents
and scripts, hooks, skills, tools, connectors, or automated workflows acting on an agent's behalf.
Read and follow `.specify/memory/constitution.md` before starting work.

## Local Git Only; No Commits or GitHub Collaboration Actions

- Agents MUST NOT perform GitHub collaboration or repository-management actions. This includes
  issues, pull requests, comments, reviews, repository settings, GitHub Actions workflows, and
  other account or repository operations.
- Agents MAY download source archives, dependency archives, or release assets required by the
  repository's documented local build and test workflows, including from GitHub-hosted URLs.
  Use pinned/project-declared inputs and verify their declared checksums when available. This
  permission is limited to obtaining build/test inputs and does not permit Git operations,
  collaboration actions, or unrelated GitHub browsing.
- Agents MUST NOT create any Git commit, including local commits, amended commits, or commits
  created indirectly by merge, rebase, cherry-pick, scripts, or other commands.
- Agents MUST NOT push anything to `origin` or any other remote, including branches, tags,
  commits, or other refs. Other remote Git operations, including clone, fetch, and pull, are
  also prohibited.
- Local Git inspection, diffs, staging, and branch or worktree operations are allowed only when
  they do not create commits, contact remotes, or perform a GitHub action.
- Before invoking a tool, script, hook, or workflow, agents MUST check for prohibited actions.
  Conflicting steps MUST be skipped and reported; independent permitted local work MUST continue.
- Agents MUST NOT request tool approval for prohibited actions or bypass these restrictions
  through another tool, transport, account, remote name, or intermediary.
- The build/test download permission above is the only exception to the GitHub access restriction.
  Other changes to these restrictions require an explicit amendment to Principle VI of the
  constitution and an update to this file.

The user retains exclusive control over commits and all remote repository activity.

## Local Build, SSH Device Validation

- Commands that require network access MUST be invoked with network-enabled execution from the
  first attempt whenever the environment offers that option. Do not first try them in a restricted
  sandbox. This applies especially to SSH device deployment, execution, and diagnostics, and to
  permitted downloads of pinned build/test inputs. Network access MUST NOT be used for prohibited
  remote Git or GitHub collaboration operations, and MUST NOT be used to bypass the sudo handoff.

- Use the canonical CMake preset build directory: `build/linux-arm64-cross` for ARM64
  cross-builds and `build/linux-arm64-native` for native ARM64 builds. Do not create suffixed
  alternatives such as `-current`, `-new`, or task-specific build directories. Both presets
  place the application at `build/<preset>/bin/seedlessds` and its runtime inventory at
  `build/<preset>/runtime-inventory.json`.
- Use `scripts/build.sh` as the standard configure/build/test entry point. It selects the preset
  from host architecture by default; use `--cross` or `--native` to select it explicitly.
- Reconfigure and rebuild the canonical preset in place when source changes. Remove a stale
  `seedlessds-headless` output left by an older build; the frontend is always graphical and the
  supported executable name is `seedlessds`.


- For device run or debug requests, agents MUST build the current worktree locally in its
  intended Linux environment first. On an x86-64 host, use the ARM64 cross-build; do not try to
  compile the project on the device or transfer the source tree for a remote build.
- After a successful local build, agents MUST transfer the built executable to the ARM64
  device over SSH and run/debug that executable on the device. Use the repository's deployment
  helper when available so binary integrity, host verification, and diagnostics are preserved.
- SSH MAY be used for executable transfer, launching the frontend, collecting runtime
  diagnostics, and checking required device assets. This does not permit remote Git operations,
  GitHub actions, commits, pushes, or sudo operations; all existing restrictions remain in
  force.
- Agents MUST report local build failures or missing device inputs (such as a ROM) plainly and
  MUST distinguish a successful build/deployment from a successful emulation run.
