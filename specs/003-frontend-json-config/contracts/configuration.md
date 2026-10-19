# JSON Configuration Contract

The four top-level groups and all listed fields are required. Existing partial or invalid files fail rather than being overwritten or silently filled. Unknown keys, duplicate keys at any level, and attempted fixed-policy settings are rejected with a dotted-key diagnostic. JSON is strict UTF-8: no comments, trailing commas, nonfinite numbers, trailing data, invalid escapes, unpaired surrogates, or embedded NUL in strings. Numeric integer fields reject booleans and fractional representations.

## Shape and defaults

BIOS strings below illustrate a home of `/home/alex`; generated files use the actual resolved home, not literal shell expressions.

```json
{
  "visuals": {
    "threaded_3d": false,
    "thread_count": 0,
    "disable_edge_marking": false,
    "fix_main_screen": false,
    "auto_trim": false
  },
  "firmware": {
    "bios_arm7": "/home/alex/.config/seedlessds/bios/drastic_bios_arm7.bin",
    "bios_arm9": "/home/alex/.config/seedlessds/bios/drastic_bios_arm9.bin",
    "firmware_path": ""
  },
  "user": {
    "nickname": "SEEDLESS",
    "birthday_month": 3,
    "birthday_day": 2,
    "color": 0
  },
  "emulation": {
    "save_dir": ""
  }
}
```

Visual defaults preserve the current zero-config-word behavior. `thread_count` accepts integers 0–15; 0 requests the core default (effectively one worker). Actual parallelism is subject to available workers, internal scale, and the core scheduling rules described in research.md.

## Validation and lifecycle

- BIOS paths: nonempty absolute strings, readable regular files, ARM7 16384 bytes and ARM9 4096 bytes. Use exactly these paths; no original/replacement filename search.
- Firmware: empty disables external image lookup. A nonempty absolute path must identify a readable, structurally supported image; core validation is authoritative, failures are surfaced. Do not accept merely because it exists. Profile fields override image profile data.
- Nickname: nonempty valid UTF-8 converting losslessly into at most 10 UTF-16 code units; reject overflow rather than truncate.
- Birthday: integer month 1–12 and integer day 1–month maximum; allow February 29. No year or combined birthday string.
- Color: integer 0–15.
- Rendering toggles: JSON booleans. Thread count: integer 0–15; reject values outside the encoded range rather than clamp. Core runtime scheduling may use fewer workers than requested.
- Save directory: string; empty means canonical ROM parent. Relative nonempty values resolve against launch working directory. CLI override wins even when empty. Create the effective directory if absent and check usability; never append `backup`.
- Cartridge filename: ROM basename with `.nds` replaced by `.sav`; retain basename case, and use the identical path for read/write. No `.dsv` fallback or automatic conversion.
- Fixed policies are absent from JSON: always JIT, real-time system RTC, raw cartridge saves.

Open selected config as a nonblocking regular file; only ENOENT is missing, while EACCES, directories, invalid symlinks, and other errors are failures. Missing parents may be created. For missing config, log the exact selected path; write and sync a complete default document to a same-directory temporary file, then publish without replacing an existing file (atomic no-replace link/rename). If another process wins, load its file. Check write/close/publish errors and remove only this process's temporary file. Never truncate an existing file. Config reads and creation must handle EINTR and short IO. Bound file size to 64 KiB, nesting to 8, and reject paths that exceed the core's destination buffer capacity (including terminator).

No configuration rewriting on launch, override application, or failed validation. HOME is required only when resolving the default config or generating default BIOS paths; a complete custom config can work without HOME. XDG_CONFIG_HOME does not redirect the requested location.
