# Data Model

These are local files and in-memory workflow records; no database or migration is required.

| Entity | Fields and validation | Relationships |
|---|---|---|
| LocalDebugConfiguration | `ip_address`: literal IPv4/IPv6, no hostname/port; `root_password`: nonempty nonplaceholder string, no NUL/CR/LF, preserve whitespace and special characters; `install_dir`: absolute POSIX directory, no control characters, reject `/` and traversal components | One console per invocation; loaded from owner-only ignored `.device.local` |
| ConfigurationTemplate | Same three keys with explicit placeholder strings, no real device values | Tracked `.device.example.json`; never accepted as valid config |
| BuildArtifact | Local regular readable ELF path, AArch64 machine, SHA-256, size, interpreter, shared dependencies, required GLIBC versions, matching inventory path | Exactly one artifact per session, tied to current bytes through audit/hash |
| RuntimeInputs | Device-local absolute ROM path and existing writable data directory; frames integer 1..UINT64_MAX; timeout positive seconds | Passed to unchanged runner CLI, never embedded in tracked config |
| Deployment | Session id, temporary remote filename, final filename, expected and received SHA-256, promotion status | Artifact deployed within configured install directory |
| HardwareValidationRecord | Timestamp, revision, artifact hash/preset, sanitized device model/OS/architecture/libc, stage statuses, runner exit, transport exit, result record, observations, issues/workarounds | Ignored evidence for a session; manually reviewed summary in README Debug |

Reject malformed JSON, duplicate/unknown/missing keys, wrong types and placeholders without echoing values or parser excerpts. On POSIX require owner match, regular nonsymlink local config and no group/other permissions; create with 0600 and restrictive umask. WSL2 validation runs in the Linux filesystem where these restrictions work.

State flow: `unconfigured → configured → locally_validated → connected → compatible → staged → verified → promoted → executed → recorded`. Invalid local inputs make no remote connection. Any remote-stage error stops dependent stages. Cleanup removes only the current session's partial temporary file; it never removes a previously installed executable or user assets. An execution failure retains the verified deployed executable for inspection. A disconnect after promotion/execution yields an unconfirmed outcome unless explicit completion was received.

Validation records use `passed`, `failed`, `blocked`, or `not_run` per stage. Overall success requires matching hash, completed transfer and promotion, runner exit 0, and an exact bounded completion record. An existing binary, help output, failed ROM load, timeout or missing runtime assets cannot produce successful hardware acceptance. Raw passwords/IP/destination must not enter shared evidence; redact configured private values before output and persistence, including escaped representations. Raw connection settings are not fields in the published record.
