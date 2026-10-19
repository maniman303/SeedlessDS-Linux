# Updating SeedlessDS-Core safely

The project consumes `external/SeedlessDS-Core` at the commit recorded in `patches/seedlessds-core/manifest.json`. Never edit, reset or clean that checkout as part of an update. Inspect its Git status and candidate commit first, then review every source addition/removal and Android/presentation dependency change.

Update the manifest's pinned commit, nested submodule commits and expected C/assembly source inventory together. Rebuild the staged-source patch series against the new upstream context; keep patches ordered, hash-pinned and narrowly scoped. Review each platform adapter replacement, especially shared memory, fatal jump storage, file roots, audio/microphone discard and presentation removal. Run `scripts/prepare-core.sh` twice for both presets and verify the upstream checkout remains clean.

Build from clean native and cross directories. Review the target ABI assertions and regenerate `build/validation/abi-report.txt`; check fixed machine offsets, page mapping assumptions, pthread adapter capacity, legacy stat fields and jump-buffer storage. Inspect `runtime-inventory.json` for application libraries, dynamic dependencies and GLIBC symbol versions. Run the CLI failure checks and, on real ARM64 hardware with user-supplied assets, the 1-frame, 120-frame, repeated bounded, unlimited-interrupt and SIGTERM workloads. Record the ROM checksum and runtime environment without storing ROMs, BIOS images, credentials or private device endpoints in the repository.

A core update is ready only when the staged build and runtime evidence describe the new commit. Leave hardware-dependent acceptance marked outstanding when the device or legal test assets are unavailable.
