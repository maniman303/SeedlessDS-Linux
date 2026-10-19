# Device Debug CLI Contract

This is the planned implementation interface; commands do not exist yet.

```text
python3 scripts/debug-device.py check [--config PATH]
python3 scripts/debug-device.py run [--config PATH] --binary PATH --inventory PATH [--rom REMOTE_PATH] [--frontend-config REMOTE_PATH] [--save-dir REMOTE_PATH] [--frames N] [--timeout SECONDS]
```

Both commands resolve the default `.device.local` relative to repository root, regardless of caller directory. Explicit local paths resolve from caller directory. `check` validates configuration and host tools only and makes no network connections. `run` performs check, artifact audit, connection, target compatibility/runtime-input checks, staged deployment, verification, promotion, help launch, bounded execution and evidence capture in that order. Help must not require a configuration file. No CLI flag accepts a password.

Configuration template schema:

```json
{
  "ip_address": "<CONSOLE_IP>",
  "root_password": "<ROOT_SSH_PASSWORD>",
  "install_dir": "<ABSOLUTE_BINARY_INSTALL_DIRECTORY>",
  "rom_dir": "<ABSOLUTE_ROM_DIRECTORY>"
}
```

When `--rom` is omitted, `run` selects the first readable `.nds` file in `rom_dir`. Validation and ownership rules are in [data-model.md](../data-model.md). During the implementation configuration task the implementer MUST explicitly request all three actual values before creating/populating `.device.local`. No private values are needed to generate the template, plan, tests or scripts. Existing valid settings are reused; no automatic overwrite. File creation must not put credentials in displayed shell commands/history.

SSH user is root, port 22; IP supports IPv4 and IPv6. Required known-host entry is enrolled by a separate interactive SSH command after the developer verifies the fingerprint through the console or another trusted source. Automated sessions use strict verification and bounded connection attempts. Password auth only; no implicit fallback to agent/key auth. Unknown/changed host key is a host-verification failure with recovery guidance, never an automatic replacement.

Remote token quoting must preserve spaces, quotes, dollar signs and metacharacters literally. Reject unsupported control characters with field-name-only errors. The password travels via a fresh inherited anonymous fd to sshpass, never command arguments/environment. No verbose sshpass, shell tracing, raw config dumps or unsanitized exceptions. Redact password, address and install directory from stdout/stderr and evidence, buffering enough to catch split-chunk and escaped matches.

Remote install directory may be created with `mkdir -p` after connection/compatibility checks and must be a writable real directory. Refuse a symlink final executable or directory where safe staging cannot be established. Stream to a unique temporary file inside that directory; use restrictive permissions, compare remote SHA-256, make executable and rename to `seedlessds` only after complete transfer. Previous final executable is preserved on transfer/hash/promotion failure. Concurrent workflow sessions to the same destination are unsupported and must be rejected with a destination lock; release only the current session's lock. After a crash provide explicit stale-lock inspection/recovery instructions.

Runtime ROM and data paths refer to existing user-supplied target assets. The data root must be writable; no provisioning of assets. Execute unchanged runner with `--data-dir`, `--frames` (default 120), and ROM; default remote timeout 30 seconds, TERM then KILL after 5 seconds. Target timeout support is a preflight requirement. Report runner exit separately from SSH exit; a distinct wrapper marker carries remote completion status. Missing marker means unconfirmed execution. Never classify SSH exit 255 as a confirmed application exit.

CLI exits: 0 successful local check or complete bounded run; 2 local usage/config/artifact validation; 3 connection/authentication/host verification; 4 target compatibility or runtime prerequisites; 5 deployment/transfer/hash/promotion; 6 application failure, timeout, malformed/missing result; 7 interrupted/disconnected or otherwise unconfirmed execution. Emit sanitized `stage=<name> status=<passed|failed|blocked>` diagnostics with an actionable message; include runner exit/result when available. A wrapper completion marker must be parsed separately from runner text and never be inferred from incidental log output.

Each run stores a uniquely named sanitized evidence record in ignored `build/validation/device/`. README Debug is updated from inspected evidence, with device/environment, revision/hash, actual steps and outcomes, failures and workarounds. Failed/blocked attempts remain explicitly unsuccessful. Only exit 0 plus matching `result=completed frames=N requested=N` and verified deployment establishes SC-002.
