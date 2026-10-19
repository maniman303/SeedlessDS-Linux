# Data Model: Fullscreen Video Output

## Video preferences — persisted

Owned by the existing frontend configuration model. All new video output preferences reside in the top-level `video` object; existing `visuals` fields remain where they are.

| JSON path | Type | Values | Missing-value default |
| --- | --- | --- | --- |
| `video` | object | Members below | All member defaults |
| `video.display_mode` | string enum | `auto`, `dual_screen`, `single_screen` | `auto` |
| `video.swap_screens` | boolean | `false`, `true` | `false` |
| `video.single_screen_layout` | string enum | `vertical`, `horizontal` | `vertical` |

Missing is distinct from null. Null/wrong types and unsupported enum values are configuration errors identifying the full field path. An empty object is valid. New default files contain all three members. Existing files are not rewritten on load. Preserve requested values even when the effective mode falls back. Unknown-member behavior follows the common feature 003 loader; this feature does not add a new general JSON unknown-key policy. The overlay schema deliberately permits unrelated existing properties.

Fullscreen and sharp integer fit are fixed presentation invariants, not persisted toggles. No display index, scale, filtering, rotation, gap, audio, or live-toggle setting is introduced.

## Physical display inventory — per session

| Field | Meaning |
| --- | --- |
| `sdl_index` | SDL index valid for the current enumeration; never persisted as stable device identity |
| `name` | Diagnostic OS/SDL name, not a unique key |
| `bounds` / `desktop_mode` | OS display geometry and current mode used for enumeration diagnostics |
| `selected_role` | Primary (index 0), secondary (index 1), or unused |
| `renderer_width_px`, `renderer_height_px` | Actual drawable pixel dimensions after fullscreen setup |
| `backend`, `renderer_driver` | Runtime diagnostic identifiers |

Use only successful physical output enumeration; reject SDL dummy/offscreen as normal gameplay backends. A test-only software/dummy path can verify data behavior, not physical acceptance. If two displays enumerate but cannot host independent fullscreen outputs, initialization fails and hardware dual-screen acceptance remains unsatisfied.

## Effective presentation — immutable until geometry refresh or termination

Relationships: preferences + display inventory → effective mode → output surfaces → DS image assignments and destination rectangles.

| Input | Effective mode | Selected displays |
| --- | --- | --- |
| Any requested mode, 1 display | Single | Index 0 |
| `auto` / `dual_screen`, ≥2 displays | Dual | Indices 0 and 1 |
| `single_screen`, ≥2 displays | Single | Index 0 |
| Any mode, 0 displays | Startup failure | None |

`auto` and `dual_screen` are separate persisted enum values with identical initial resolution behavior. Dual mode ignores the layout for drawing. Additional displays remain unused.

Without swap, top maps to primary display/first layout slot and bottom to secondary display/second layout slot. With swap, exchange the two image assignments. DS screen identity remains top/bottom throughout; only destinations change. In dual mode each destination's scale is recomputed from its dimensions.

## Integer geometry

Source image: width `w=256`, height `h=192`, RGBA pitch `1024`.

For output size `W×H` and required canvas `Cw×Ch`:

```text
k = min(W / Cw, H / Ch) using integer floor division
if k < 1: reject this presentation
origin_x = (W - Cw*k) / 2 using integer floor division
origin_y = (H - Ch*k) / 2 using integer floor division
```

Odd leftover pixels place the extra border pixel on the right/bottom. Use checked arithmetic and reject invalid/nonpositive dimensions or overflow.

| Presentation | Canvas | Slot 0 rectangle before scale/centering | Slot 1 rectangle |
| --- | --- | --- | --- |
| Dual (per output) | 256×192 | `(0,0,256,192)` on display 0 | `(0,0,256,192)` on display 1 |
| Vertical | 256×384 | `(0,0,256,192)` | `(0,192,256,192)` |
| Horizontal | 512×192 | `(0,0,256,192)` | `(256,0,256,192)` |

Single mode uses one common `k` for the entire canvas. No gap or overlap. Actual rectangles multiply slot coordinates/sizes by `k` and add the origin. Dual mode applies the formula independently on each display. Rectangle math operates in renderer pixels with renderer scale 1 and no logical-size transform.

Examples:

- Dual 640×480: `k=2`, each destination `(64,48,512,384)`.
- Vertical on 640×480: `k=1`, top `(192,48,256,192)`, bottom `(192,240,256,192)`.
- Horizontal on 640×480: `k=1`, top `(64,144,256,192)`, bottom `(320,144,256,192)`.
- Dual displays 1024×768 and 640×480: factors 4 and 2; swapping changes which image gets each factor.
- Vertical on 256×383: rejected; no fractional fallback.

## Frame pair and mailbox — ephemeral

**Frame pair**: two frontend-owned arrays, 196,608 bytes each, RGBA32, plus a monotonic publication sequence. Both images must come from the same completed core page flip. The sequence identifies publication; it is not `nds_control_status.frames` and does not drive bounded execution.

**Mailbox**: one latest published pair, validity/sequence, short copy mutex, producer staging pair and consumer-owned pair. Total image storage is three pairs/1.125 MiB, independent of runtime. Replacing an unread pair is allowed. Producer never waits for display acknowledgment and main thread never renders from live core pages. Do not retain or expose callback pointers after the hook returns.

**Lock order**: core page mutex → brief mailbox publication lock. Consumer copies under mailbox lock, releases it, then calls SDL. No path acquires the core lock while holding the mailbox lock. Hook detach is serialized under the core mutex before core buffer destruction. Keep storage alive until producer threads have quiesced.

## Session lifecycle and resources

```text
CLI/config validated
  → SDL enumerated
  → fullscreen outputs ready and geometry valid
  → core initialized, mailbox/hook attached
  → load/emulation worker active; main thread presents
  → stop requested / bounded completion / failure
  → worker joined; hook detached under page lock
  → core destroyed; mailbox released
  → textures/renderers/windows destroyed; SDL quit
```

Each allocation/initialization has an ownership flag so partial startup can unwind. Loss of a selected display or unrecoverable fullscreen/geometry change terminates through the same cleanup path. A conversion/publication error sets a thread-safe failure flag for the main loop; callbacks do not destroy SDL/core resources. Existing core failure and signal policies remain authoritative; video errors use the new diagnostic operation described in the output contract.

## Validation boundaries

Configuration validation precedes SDL/core activity. Display/rectangle validation precedes emulation. Paired frame validity is checked at the adapter boundary. Runtime errors request cleanup on the main thread. Physical sharpness and panel placement require hardware evidence beyond the data model.
