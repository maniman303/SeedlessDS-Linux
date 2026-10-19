# CLI Contract

```text
seedlessds-headless --help
seedlessds-headless [--frames N] [--data-dir DIR] ROM
```

`--help` succeeds without a ROM or initialized core. One ROM is required for execution; use `--` before a path beginning with a dash. `--frames` accepts decimal integers from 1 through UINT64_MAX; omission runs until interrupted. `--data-dir` defaults to the current working directory and selects the core data root; ROM resolution remains relative to the invocation directory. Reject duplicate options, unknown options, extra operands and invalid values with usage diagnostics.

Core data paths and any required BIOS/firmware assets must be documented after actual boot validation. No runtime assets are downloaded or generated implicitly by the runner. Existing POSIX file bridge uses `SEEDLESS_ROOT`; runner sets it to the selected absolute data root before core initialization and handles write errors explicitly.

Diagnostics go to stderr. Each started execution produces one final stdout record:

```text
result=completed frames=120 requested=120
result=interrupted frames=42 signal=SIGINT
result=failed frames=0 operation=load
```

Counts are illustrative. Early CLI validation failures need only stderr diagnostics. No record may claim completed unless the exact target was reached through genuine scheduler boundaries, the core stopped normally, and no detected runtime error occurred.

Exit codes: 0 for help or bounded success; 2 for usage/input validation; 1 for initialization/load/runtime failure; 130 for SIGINT; 143 for SIGTERM. Handle interruption outside signal context, including shutdown from an acknowledged target pause. No windows, audio playback or microphone capture. A fatal process signal remains a failure, never converted to success.
