# Analog Features Verification Report

Audit of `ANALOG_FEATURES.md` against each branch's actual implementation.

## Summary — All Discrepancies Resolved

The audit identified 14 doc inaccuracies, 2 code bugs, and 1 cross-branch
inconsistency. All have been fixed across the relevant branches.

## Fixes Applied

### Code Fixes

| # | Branch | Issue | Fix |
|---|--------|-------|-----|
| A | mousekey | `compute_speed_fraction` compared `depth` (0-1000) against `deadzone_pct` (0-100) without scaling → default 50 gave 5%, not 50% | `deadzone = deadzone_pct * 10`; default 50→10 (10% → 100/1000) |
| B | multi-stage | `split_pct`/`hysteresis` compared directly against 0-1000 depth → default split=50 gave 5% travel | `split_scaled = split * 10`; `hyst_scaled = hyst * 10` |

### VIA Enum Consistency

The mousekey branch enum was renamed (id_am_curve etc.) and IDs shifted by 1,
breaking cross-branch merge compatibility. Fixed to use base branch naming
and ID numbering:

- IDs 17–24: use base enum names (`id_analog_mouse_enable`, `id_analog_mouse_curve`, …)
- Mousekey-specific IDs at 25 (`id_am_scroll_max`) and 26 (`id_am_interval_ms`)
- IDs 17, 22–24 remain in enum (no-op handlers) for other branches to implement

### Doc Fixes

| # | Fix |
|---|-----|
| 1 | "0x5F00 + N" → "SAFE_RANGE" |
| 2 | VIA table now shows correct base enum names and all IDs 17–26 |
| 3 | "(planned)" labels removed (already done) |
| 4 | "VIA IDs 17-22 reserved (no-op handlers)" → table now accurate |
| 5–10 | Line counts updated to actual: mousekey .c 115, .h 22; multi-stage .h 22, .c 66; repeat .h 23, .c 67 |
| 11–12 | Checklists all checked ([x]) |
| 13 | State diagram: added IDLE→SHALLOW transition on AM_MULTI press |
| 14 | "+N" clarified as approximate added lines vs net delta |

## Build Status

All three feature branches build clean:
- `make boardloaf_he:vial` — mousekey: OK (UF2)
- `make boardloaf_he:vial` — multi-stage: OK (UF2)
- `make boardloaf_he:vial` — repeat: OK (UF2)
