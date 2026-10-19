# Linux Core Configuration Bridge Contract

## Frontend module boundary

- `config.h`: typed persisted settings, init/free, selected-file load-or-create, schema validation, and effective path resolution with structured errors. No public JSON-node types or core-private structs.
- `launch_options.h`: argv parsing, help and explicit override-presence flags. No filesystem writes or core calls.
- `core_config.h`: build config bits, convert profile, initialize/copy runtime paths through the compatibility API, inspect startup/save status, release runtime resources. No JSON parsing.
- `runner.c`: call these APIs, preserve frame/signal control and worker lifecycle, join and flush before deciding the final outcome, then free owned state. Remove superseded argument/BIOS helper implementations rather than leave duplicate policy.

## Proposed supported Linux compatibility API

Add a separate public `seedlessds/linux_config.h` through an ordered compatibility patch. Define a versioned options structure with `struct_size`, exact ARM7/ARM9 paths, optional firmware path, exact raw cartridge-save path, and independent private-data root. Expose a setup function returning a structured success/error code and a status accessor reporting latched asset/save failures. Names can follow existing public API conventions; no private struct addresses or packed offsets cross this interface. Implementation copies strings, rejects overflow before mutation, and forbids reconfiguration while running. Settings live for one active session and reset on teardown to prevent reuse of a previous ROM's paths.

Apply explicit BIOS paths before core initialization can read images. Apply firmware and profile before firmware mount/ROM start. Support no external firmware when empty, and prohibit fallback to modified/external images from earlier sessions in that case. Nonempty firmware is parsed and validated through existing core mechanisms, with failures propagated. Preserve generated/modified firmware under the independent private-data layout; never let it replace explicitly configured startup input or profile.

Raw cartridge saves use the exact provided `.sav` path for both mounting and flushing, bypassing hardcoded `backup/` only for this explicit Linux override. Preserve other caller behavior when no override is installed. Do not change nds_t offsets or existing ABI structs; adding a separate structure avoids positional-initializer/layout breakage. Maintain existing ABI assertions and add coverage for the new bridge contract.

Validate BIOS as regular readable files with existing size requirements before core startup. Retain core save mechanics and error checks; surface failed reads, writes, flushes and close operations via latched thread-safe status through teardown. Missing saves are normal; malformed/unreadable existing saves fail without overwriting. Runner must check final-flush status before printing `result=completed`.

## Config-bit translation

| Setting | Translation |
|---|---|
| threaded_3d | bit 28 |
| thread_count | bits 16–19, 0–15 |
| auto_trim | bit 36 |
| fix_main_screen | bit 35 |
| disable_edge_marking | bit 40 |
| RTC real time | bit 39 always set |
| raw cartridge saves | bit 50 always set |

Keep unrelated existing bits unchanged. JIT remains enabled by core initialization and is checked by adapter/runtime verification, not exposed as a user choice. Public profile packing uses English 1, color in byte 1, month minus 1 in byte 2, and day in byte 3. See research.md for local source evidence.

Compatibility patch requirements: deterministic application to the manifest-pinned source, checksum registration, old API behavior preserved without overrides, strict no-truncation paths, no emulation algorithm replacement, and direct runtime observation of effective settings in tests. The patch and frontend are an inseparable delivery unit for FR-003 and FR-013–FR-019.
