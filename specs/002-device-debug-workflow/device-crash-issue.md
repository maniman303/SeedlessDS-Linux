# ARM64 runner faults to an unmapped PC during ROM startup on RG DS Plus

## Summary

The Linux ARM64 runner crashes during ROM startup on an Anbernic RG DS Plus instead of completing even a one-frame bounded run. The failure reproduces with two different ROMs. This blocks real-device acceptance for this feature (FR-009/SC-002).

## Environment

- Device: Anbernic RG DS Plus
- OS/kernel: Linux, 7.2.7
- Architecture/runtime: AArch64, GNU libc 2.44
- Artifact: instrumented `linux-arm64-cross` build, SHA-256 `9a45b74d9ee8653ebbf840ce6361e1bf9820c40d60cbf401367dc9533373eeb5`
- Build host: Ubuntu 26.04 amd64; cross build and target compatibility/deployment checks passed

## Reproduction

Deploy through `scripts/debug-device.py run` with an existing device-local ROM and writable data directory, using `--frames 1 --timeout 30`. The same failure was also observed with 120 frames and with a second ROM. SSH transport succeeds and the runner exits 135/139 before writing the required `result=completed ...` record.

## Captured failure

```text
seedlessds[info]: core initialization completed
seedlessds[info]: ROM load started; bounded_frames=enabled
seedlessds[fatal]: signal=SIGBUS code=1 code_name=BUS_ADRALN ... phase=rom-load ... pc_mod4=3 ... pc_outside_main_image=1 pc_map=unmapped_or_unlisted
```

The PC equals the fault address and is unaligned, outside the main executable and unmapped. This is consistent with an invalid indirect control-flow target during ROM startup, likely in the ARM64 JIT/dispatcher path. The precise corruption source is not yet proven. The device kernel reported failed `systemd-coredump` pipe attempts, so no core dump was available. Sanitized local run evidence is in ignored `build/validation/device/20261019T143135Z-7ddd2b34.json`.

## Expected

The runner loads the ROM, runs the requested scheduler boundaries, and emits exactly `result=completed frames=1 requested=1` with exit 0.

## Follow-up

- Reproduce under an available AArch64 debugger or add narrowly scoped JIT/dispatcher diagnostics to identify the invalid branch target.
- Identify and fix the core/JIT defect (or document a verified target-specific workaround).
- Rebuild and verify the fix on hardware with both a one-frame diagnostic and the full 120-frame acceptance run.
- Remove or reduce temporary fatal-signal diagnostics if they are not appropriate for the production runner.

No credentials, device address, ROM filename, or private runtime paths are included.

## Resolution (2026-10-19)

**Fixed and hardware validated on 2026-10-19.** Missing BIOS files caused machine initialization to call `longjmp` before its jump buffer had been initialized. The Linux compatibility patch now establishes the startup failure target before initialization and calls `setjmp` directly so the compiler observes its returns-twice semantics. Missing or incorrectly sized BIOS files report the required filenames and sizes and exit 1 instead of crashing. BIOS files belong under `--data-dir/system/`; the previous `SeedlessDS/` guidance was incorrect for the POSIX adapter.

The final cross build (`/tmp/seedlessds-device-cross/bin/seedlessds-headless`, SHA-256 `2445062962015cd08214e50735e5aeb3aa155555245ca19d3f97adc81a7f5ff7`) passed verified deployment and exact 1-frame and 120-frame runs with two ROMs on the RG DS Plus (Linux 7.2.7, AArch64, GNU libc 2.44). A separate empty-data-root regression returned `result=failed frames=0 operation=load`, exit 1, with the BIOS diagnostic. Existing device BIOS images were copied into the isolated validation data root; no ROM or BIOS content is distributed. Sanitized session records are in ignored `build/validation/device/`. Bounded hardware acceptance FR-009/SC-002 now passes. No commits or remote Git operations were performed.
