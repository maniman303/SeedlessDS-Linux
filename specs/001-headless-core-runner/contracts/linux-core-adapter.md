# Linux Core Adapter Contract

The CLI includes public core headers only. Frontend-owned compatibility code may include internal headers to translate platform responsibilities, with a revision manifest tying it to the inspected layout.

| Boundary | Required semantics |
| --- | --- |
| Platform registration | `nds_create(platform, ...)` must honor a live platform context, with well-defined null/default behavior. Keep context valid through worker teardown. |
| Files | Real POSIX descriptors, ownership and failure handling; no JVM. Reject truncated paths; preserve absolute ROM paths and selected data root. Convert legacy stat layout explicitly when needed. |
| Threads | Real mutex/condition/thread operations; verify embedded sizes/alignment or use owned indirection. No fake locks or unconditional wake-success stubs. |
| Time | Monotonic elapsed clock, wall clock for RTC; no unintended fixed clock in default runs. |
| Memory | Real memfd/ftruncate shared backing, correct alias mappings/protection/unmapping and cleanup; verify page-size constraints and JIT instruction-cache maintenance. Failure propagates. |
| Audio/mic | Retain emulated SPU behavior and required state; discard generated host output, provide neutral microphone behavior, never touch OpenSL vtables or devices. |
| Video | Retain software rasterization/buffers and core-required synchronization; discard presentation. No display context, GL driver, or blocked waits for absent presentation consumers. |
| Legacy logging | Translate `__android_log_print` to bounded, variadic-safe stderr logging. |
| Execution | Load/dispatch on one worker, real scheduler control boundaries, target armed before worker, quit/wake/join cleanup. No synthetic frame stepping. |
| ABI | Compile assertions/probes for jump, pthread, stat, page and alignment assumptions; typed or explicitly translated boundaries on mismatch. Do not shift fixed offsets without auditing consumers. |

Patch manifest entries contain base commit, patch order/name/hash, reason, upstream source context and acceptance checks. Apply patches only to staged sources; fail on drift. Broad symbol interposition, blanket no-op GL/OpenSL implementations, suppressing undefined symbols, and fake success for required services are prohibited. Unused public presentation features may be excluded or explicitly unsupported in this milestone.
