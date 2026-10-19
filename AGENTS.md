# Agent Instructions

These instructions apply to every agent working in this repository, including delegated agents
and scripts, hooks, skills, tools, connectors, or automated workflows acting on an agent's behalf.
Read and follow `.specify/memory/constitution.md` before starting work.

## Local Git Only; No Commits or GitHub Actions

- Agents MUST NOT attempt any GitHub action, including read-only access through websites, APIs,
  CLI commands, connectors, or integrations. This includes issues, pull requests, comments,
  reviews, releases, repository settings, and GitHub Actions workflows.
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
- Routine exceptions and general approval do not override these restrictions. Changing them
  requires an explicit amendment to Principle VI of the constitution and an update to this file.

The user retains exclusive control over commits and all remote repository activity.

## Local Build, SSH Device Validation

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
