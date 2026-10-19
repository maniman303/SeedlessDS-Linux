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
