# Data Model and Lifecycle

## RunRequest

- `rom_path`: required canonical readable regular file; resolve relative path before core root translation.
- `frame_limit`: optional unsigned 64-bit decimal integer, range 1 through UINT64_MAX; omitted means unlimited.
- `data_dir`: writable absolute directory, default current working directory; chosen root for core files, never an implicit change to the ROM path.
- `stop_requested`: signal-safe flag, consumed by main outside the handler.

Validation precedes core initialization. Reject unknown/duplicate options, malformed limits, path truncation, and missing values.

## RunSession

One session owns one platform context, the global core instance, one dispatch worker, control tracking, and adapter resources. States: `validated -> initialized -> loading/running -> stopping -> stopped`. Failure can lead from any acquired-resource state through `stopping` to `failed`. The upstream load call blocks through execution, so loading/running is one worker operation; do not assume its return signals a ready-to-step core.

Configure neutral input and control tracking before dispatch. Set target before worker start. Observe control status under its own synchronization; target completion requires the target frame count and acknowledged pause. Normal main-thread quit wakes that pause, joins the dispatch worker, captures final status, ends control tracking and releases remaining resources. Cleanup must account for core resources already released by its unwind path.

## RunResult

- `outcome`: completed, interrupted, invalid-input, initialization-failed, load-failed, runtime-failed.
- `frames`: genuine scheduler boundary count from control status; never run-frame call count.
- `requested_frames`: nullable original target.
- `signal`: optional SIGINT/SIGTERM for interruption.
- `operation` and `diagnostic`: failure context, no credentials.

A load return of true is insufficient for completion: require target count, normal stop cause and no detected error. Frame count is 64-bit and must not use the truncated public frame-info counter.

## BuildConfiguration and CoreCompatibilityManifest

Configuration records preset, host/target triples, compiler/version, sysroot, source/core revisions, dependency versions/hashes, build directory, artifact path and runtime library requirements, static application-library inventory, remaining shared-library names/reasons, minimum runtime symbol versions, and explicitly selected linkage exceptions. Manifest records ordered patch hashes, base core commit, source replacements, and expected context. Changes invalidate staged/build state rather than reusing stale artifacts.

## VerificationRecord

Records timestamp, configuration, device model/kernel/libc/page size, ROM identity and SHA-256, separately supplied runtime assets, requested/completed boundaries, elapsed shutdown, exit status, ELF dependency findings and logs. No private SSH endpoints or credentials in tracked evidence. Relationship: each record references one build and run request; each build requires native/cross and repeat-run acceptance appropriate to its configuration.
