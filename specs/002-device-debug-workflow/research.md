# Phase 0 Research: Device Debug Workflow

All design unknowns are resolved below. Repository scripts are the authority for project behavior; device-specific facts remain runtime observations rather than invented requirements.

## Configuration format and creation

- Decision: JSON with exactly `ip_address`, `root_password`, `install_dir`, and `rom_dir`; tracked `.device.example.json`, ignored `.device.local`. Use Python JSON parsing, ipaddress validation, and POSIX path checks. Create atomically with mode 0600 after the implementer explicitly requests the three connection/install values and the ROM directory; never overwrite existing settings silently. If `--rom` is omitted, select the first readable `.nds` in `rom_dir`.
- Rationale: Existing `.gitignore` already excludes `.device.local` and its suffixes. JSON preserves spaces, quotes and shell metacharacters without sourcing executable shell assignments. Ubuntu setup already provides Python.
- Alternatives considered: Sourced shell config permits execution and fragile escaping; a new configuration library adds no needed capability. Secret vaults would change the requested local-password workflow.
- Restriction: Password is nonempty, nonplaceholder, no NUL/CR/LF; line-based sshpass input cannot faithfully carry line terminators. Preserve all other characters exactly. Reject unsupported input without printing it.

## SSH and literal values

- Decision: OpenSSH with root password authentication, sshpass `-d` on a fresh anonymous pipe per subprocess, no password argv/environment, no local shell. Disable SSH config surprises with explicit options, no agent forwarding or multiplexing, password authentication only, and strict known-host checking.
- Rationale: Avoid new Python SSH dependencies and target SFTP assumptions. Register the verified host key interactively before automation. Unknown/changed keys stop with an actionable host-verification stage error. Never disable verification or silently replace keys.
- Alternatives considered: sshpass `-p` exposes argv; `-e` exposes environment; Paramiko adds dependencies; key-only authentication does not meet FR-005. `accept-new` is possible but explicit verified enrollment provides a clearer prerequisite.
- Sources: [OpenSSH ssh manual](https://man.openbsd.org/ssh.1), [OpenSSH configuration manual](https://man.openbsd.org/ssh_config.5), and [sshpass source](https://github.com/kevinburke/sshpass/blob/master/main.c). These establish remote-shell command behavior, host verification controls, and password file-descriptor support. Quote every remote token with POSIX `shlex.quote`; local argv lists alone do not protect a remote shell.

## Deployment and compatibility

- Decision: Inspect actual local ELF and runtime inventory, then check target Linux/aarch64, loader, libraries and glibc requirements. Verify the inventory against the current binary rather than trusting a stale absolute artifact path. Stream binary bytes over SSH stdin to a unique same-directory temporary file, verify SHA-256, chmod, and atomically rename to the final basename. Invoke only after verification and promotion.
- Rationale: One executable is deployed because application libraries are statically embedded. SSH stdin avoids scp/SFTP availability and remote filename syntax differences. Same-filesystem rename prevents partial final executables. Authentication has a separate password fd so binary stdin is preserved.
- Alternatives considered: Direct overwrite risks executing truncation; uploading a whole build tree adds unrelated files and private paths. Automatic library installation or libc replacement is outside scope.
- Evidence: `CMakeLists.txt`, `scripts/audit-runtime.py`, `cmake/toolchains/aarch64-linux-gnu.cmake` define the binary, inventory and sysroot. Do not hardcode the README's reference GLIBC 2.34 claim as every future build's minimum. Unknown ABI checks fail with an actionable compatibility outcome; successful `--help` is an additional launch check, not ROM acceptance.

## Runtime inputs and outcomes

- Decision: Require existing device-local absolute ROM and writable data-root paths, supplied separately as CLI arguments. Default to 120 frames and a bounded remote timeout, preserving runner output and exit status. Do not upload or generate runtime assets implicitly. Verify final result record matches requested frames and zero runner exit before reporting success.
- Rationale: Existing runner supports these inputs and deterministic scheduler bounds. A transport failure and application failure must remain distinguishable; obtain a separate remote wrapper exit marker, and treat absent marker/disconnect as unconfirmed execution.
- Alternatives considered: An unbounded default risks unattended execution; a ROM directory in the device config avoids repeating a target path while leaving per-run `--rom` selection available. Existing `validate-video.sh` is optional deeper validation, requiring Bash, ripgrep and timeout on the target; it is not required by this lightweight deployment path.

## Build documentation baseline

- Decision: Retain setup script's Ubuntu 24.04/26.04 baseline and native/cross mode validation; document Windows x86-64 via WSL2 Ubuntu. Add sshpass and explicit SSH prerequisite checks to setup. Run installer before `--check`; CMake automatically runs prepare-core, and the post-build audit generates runtime inventory.
- Rationale: Source inspection confirms CMake >=3.28, Ninja, ARM64 compiler, and `SEEDLESS_TARGET_SYSROOT`; no additional frontend dependencies are needed. Dependency fetches require network access on a cold build; changing sysroot requires a clean preset build directory/reconfiguration.
- Alternatives considered: Rewriting setup or inventing another build path would duplicate maintained capabilities. Manual preparation can be documented diagnostically, but is not a mandatory extra step before CMake.

## Validation and evidence

- Decision: Focused unittest tests with a fake SSH executable cover literal values, no secret exposure, strict stage ordering, interrupted/corrupt transfer, and exit/result handling. Build the intended preset and then perform real hardware acceptance during implementation; review sanitized evidence into README Debug.
- Rationale: Mock tests validate failure paths without claiming hardware success. Hardware availability and assets are explicit external dependencies. Record blocked or failed attempts accurately, leaving successful acceptance outstanding.
- Alternatives considered: A cross build or fake transport alone cannot meet FR-009. Publishing raw logs risks private values; evidence must pass a streaming password/private-value redactor before persistence or display, including chunk-boundary matches.
