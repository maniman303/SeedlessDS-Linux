# Frontend CLI Contract

```text
seedlessds-headless [--config PATH] [--save-dir DIR] [--frames N] [--] ROM
seedlessds-headless --help
```

- `--config` selects the sole JSON source/creation destination. One occurrence; nonempty path.
- `--save-dir` selects the cartridge-save directory for this run. One occurrence; an explicitly empty argument is valid and means the ROM directory. Presence must be stored independently of string length.
- `--frames` retains a decimal integer from 1 through UINT64_MAX, exact scheduler-boundary stop behavior, and duplicate/overflow rejection. Without it, interruption stops the runner.
- Exactly one readable regular ROM is required except for standalone help. `--` ends option parsing. Values are separate argv elements; `--option=value` and short aliases are not introduced.
- No BIOS/firmware/profile/visual/backend/RTC/save-format launch overrides. `--data-dir` is removed.
- CLI values always take precedence over corresponding valid JSON settings. Overrides never rewrite JSON. Schema/type validation still applies to the selected JSON; filesystem checks apply to the effective save destination, not an overridden directory.
- Exit 0: help or completed bounded run. Exit 2: invalid invocation/missing or unreadable ROM. Exit 1: config, asset, initialization, runtime, or save failure. Existing signal exits remain 130/143, except a persistent save failure takes precedence as exit 1.
- Diagnostic stderr includes the config path and offending key or filesystem operation as applicable. Existing stdout `result=... frames=...` records remain for automation; extend operation names with `config`, `firmware`, and `save` without removing existing categories.

Startup order: standalone help; parse/validate arguments and ROM; select/load-or-create JSON; validate types/ranges; apply launch overrides; resolve/check effective paths; initialize bridge/core and fixed policies; load ROM; run; join worker; flush saves; inspect persistent save error; destroy/free.

A missing ROM exits before creating configuration. A supplied custom config never causes default config access, regardless of failure. A newly created config is complete and valid even if subsequent BIOS checks fail.
