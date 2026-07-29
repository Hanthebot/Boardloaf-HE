# Analog Hall Effect Features — Implementation Guide

## Overview

Extends Boardloaf-HE's Hall Effect sensing beyond binary key press detection (APC/Rapid Trigger) into proportional/analog features driven by per-key press depth. Built incrementally from a common base, each feature in its own branch.

## Branch Structure

```
main ── feature/analog-base ── feature/analog-mousekey (this branch)
                              ├── feature/multi-stage    (planned)
                              └── feature/analog-repeat  (planned)
```

---

## Base Infrastructure (`feature/analog-base`)

### Press Depth Table

**File:** `he_switch_matrix.c:39` / `he_switch_matrix.h:65`

```c
extern uint16_t he_depth[HE_MATRIX_ROWS][MATRIX_COLS];
```

Computed once per `he_matrix_scan()` cycle, right after the raw ADC read:

```c
range = noise_ceiling[r][c] - bottoming_reading[r][c]
depth = ((noise_ceiling - sw_value) * 1000) / range  // 0 (resting) … 1000 (bottomed)
```

- 0 = resting (key not touched, or at noise_ceiling)
- 1000 = fully pressed (at or past bottoming_reading)
- 0.1 % resolution per count
- Zero overhead on existing logic (`he_update_key()` uses `sw_value` directly, not `he_depth`)
- Negative temperature drift handled by the existing real-time noise floor recalibration in `he_update_key()`; depth will reflect the adjusted range next cycle

### Custom Keycodes

**File:** `analog_common.h`

| Keycode     | Bit        | Index | Function              |
|-------------|------------|-------|-----------------------|
| `AM_MS_UP`  | bit 0      | 0     | Analog cursor up      |
| `AM_MS_DOWN`| bit 1      | 1     | Analog cursor down    |
| `AM_MS_LEFT`| bit 2      | 2     | Analog cursor left    |
| `AM_MS_RIGHT`| bit 3     | 3     | Analog cursor right   |
| `AM_WH_UP`  | bit 4      | 4     | Analog scroll up      |
| `AM_WH_DOWN`| bit 5      | 5     | Analog scroll down    |
| `AM_WH_LEFT`| bit 6      | 6     | Analog scroll left    |
| `AM_WH_RIGHT`| bit 7     | 7     | Analog scroll right   |
| `AM_SNIPE`  | bit 8      | 8     | Sniper modifier       |
| `AM_MULTI`  | bit 9      | 9     | Multi-stage trigger   |

Keycodes are defined at `0x5F00 + N` (QMK SAFE_RANGE). Included via `analog_common.h` in both the dev and vial keymaps — VIAL's preprocessor discovers them and they appear in the keycode picker.

### Activity Tracking

**File:** `boardloaf_he.c:72`

```c
uint16_t analog_activity_mask = 0;
```

- Each bit tracks one analog keycode's press/release state
- `process_record_kb()` in the base branch sets/clears bits inline
- Child branches move this to dedicated handlers

### Hooks

**File:** `boardloaf_he.c`

```c
bool process_record_kb(uint16_t keycode, keyrecord_t *record);
void housekeeping_task_kb(void);
```

- `process_record_kb`: delegates to `return process_record_user()` for non-analog keycodes
- `housekeeping_task_kb`: calls `housekeeping_task_user()`
- Child branches override these to inject analog processing

### VIA Reserved IDs

**File:** `via_he.c` (both dev and vial)

| ID  | Name                | Used By         |
|-----|---------------------|-----------------|
| 17  | `id_am_curve`       | analog-mousekey |
| 18  | `id_am_deadzone`    | analog-mousekey |
| 19  | `id_am_max_speed`   | analog-mousekey |
| 20  | `id_an_snipe_divisor`| analog-mousekey |
| 21  | `id_am_scroll_max`  | analog-mousekey |
| 22  | `id_am_interval_ms` | analog-mousekey |

---

## Analog Mousekey (`feature/analog-mousekey`)

### Architecture

```
process_record_kb()         ──► analog_mousekey_process_record()
                                     │
                              stores row/col per keycode idx
                              sets bit in analog_activity_mask

housekeeping_task_kb()      ──► analog_mousekey_task()
                                     │
                              1. rate-limit check (default 10 ms)
                              2. for each active cursor bit:
                                   row = analog_key_row[idx]
                                   col = analog_key_col[idx]
                                   depth = he_depth[row][col]
                                   fraction = compute_speed_fraction(depth, curve, deadzone)
                                   speed = fraction * max_speed / 1000
                                   accumulate into dx/dy
                              3. same for scroll bits → dv/dh
                              4. apply snipe divisor if AM_SNIPE active
                              5. clamp to int8
                              6. host_mouse_send(&report)
```

### Speed Curve

`compute_speed_fraction()` applies an **exponential curve**:

```c
effective = depth - deadzone
max_range = 1000 - deadzone
fraction = effective                          // for exponent=1 (linear)
fraction = effective² / max_range             // for exponent=2 (quadratic, default)
fraction = effective³ / max_range²            // for exponent=3 (cubic)
...
```

Implemented as iterative multiplication with no lookup table (~50 instructions per key).

Exponent 1–5: `1` = linear/reactive, `2` = quadratic (balance), `3+` = cubic+ (precision at shallow depth, speed at bottom).

Deadzone can be set 0–100 (% of travel). Depth below deadzone yields zero speed. This prevents cursor drift from resting fingers.

### Key Position Tracking

**File:** `he_analog_mousekey.c`

```c
uint8_t analog_key_row[10];
uint8_t analog_key_col[10];
```

When an analog keycode is pressed, `analog_mousekey_process_record()` captures the key's matrix position (row, col). The task reads `he_depth[row][col]` for the local half. For keys on the **remote half** (row >= HE_MATRIX_ROWS), depth defaults to 1000 (full speed) — the split transport does not yet sync analog values.

### Sniper Key

`AM_SNIPE` acts as a modifier: when held, all cursor movement is divided by `an_snipe_divisor` (default 10). Assignable to any key in VIAL. Works for both local and remote half keys.

### EEPROM Config

**File:** `he_switch_matrix.h` (extended)

New fields in `eeprom_he_config_t`:

| Field              | Bytes | Default | Range |
|--------------------|-------|---------|-------|
| `an_deadzone_pct`  | 1     | 50      | 0–100 |
| `an_curve_exponent`| 1     | 2       | 1–5   |
| `an_max_speed`     | 1     | 10      | 1–127 |
| `an_snipe_divisor` | 1     | 10      | 1–100 |
| `an_scroll_max`    | 1     | 5       | 1–127 |
| `an_interval_ms`   | 1     | 10      | 1–100 |
| `an_reserved[8]`   | 8     | 0       | —     |

Total EEPROM: 231 bytes (was 217).

Runtime counterparts in `he_config_t` are loaded from EEPROM in `keyboard_post_init_kb()` and updated live via VIA (saved to EEPROM on set).

### VIA Configuration

All six parameters are accessible via VIA raw HID (custom IDs 17–22). Each is a single byte — get/set with immediate effect on runtime config, saved to EEPROM on set. A companion webapp update would add slider controls (see webapp/ section in README for existing calibration webapp pattern).

### Split Keyboard Limitation

Analog depth data is computed locally on each half but not transmitted across the split serial link. Therefore:
- **Master half keys**: full analog control (depth from `he_depth`)
- **Remote half keys**: fall back to depth 1000 (full speed, no proportional control)

Workaround: assign analog cursor/scroll keys to the half connected via USB (typically the left half with the default handedness pin assignment). A future enhancement could add depth values to the split transport protocol.

### File Inventory (mousekey branch)

| File                     | Lines | Role |
|--------------------------|-------|------|
| `he_analog_mousekey.c`   | 160   | Tracking, curve math, task loop |
| `he_analog_mousekey.h`   | 30    | API, index constants |
| `boardloaf_he.c`         | +4    | Delegate to module |
| `he_switch_matrix.h`     | +17   | EEPROM + runtime fields |
| `config.h`               | +7    | Defaults + new EEPROM size |
| `rules.mk`               | +1    | SRC entry |
| `via_he.c` (×2)          | +60   | VIA set/get handlers |

### Build Verification

```bash
make boardloaf_he:vial   # Links to 99K UF2, OK
```

UF2 size unchanged from base — the new code fits within existing flash budget.

---

## Multi-Stage Keys (`feature/multi-stage` — planned)

### Design

Each physical key assigned `AM_MULTI` in VIAL has two keycodes configured per-key via webapp: a *shallow* keycode (triggered at light press) and a *deep* keycode (triggered when crossing the split threshold). The threshold and a small hysteresis band prevent flickering.

State machine in `housekeeping_task_kb()` or a `process_record` hook:

```
IDLE ──depth>split──► DEEP (register deep keycode, unregister shallow)
DEEP ──depth<split-hyst──► SHALLOW (register shallow keycode, unregister deep)
SHALLOW ──depth>split──► DEEP
Both: on key release → unregister whatever is active
```

**2G (staged layers):** Trivially achieved — user sets shallow = `MO(3)`, deep = `MO(4)` in the webapp config. No firmware change needed.

### VIA / Webapp Split

- **VIAL**: user assigns `AM_MULTI` keycode to each physical key that should behave as multi-stage
- **Webapp**: per-position configuration (shallow keycode, deep keycode, split %, hysteresis %)

### EEPROM Storage

Per-position struct (up to 18 keys per half):
```c
typedef struct {
    uint16_t shallow_keycode;  // QMK keycode for light press
    uint16_t deep_keycode;     // QMK keycode for deep press
    uint8_t  split_pct;        // 0–100, threshold boundary
    uint8_t  hysteresis;       // 0–50, hysteresis gap in percent
} multi_stage_key_t;
```

Requires additional VIA ID for configuration.

---

## Dynamic Key Repeat (`feature/analog-repeat` — planned)

### Design

For held keys (backspace, arrows, etc.), inject additional key taps at a rate proportional to press depth. Light hold = slow repeat (one char per ~200 ms), deep hold = fast repeat (one char per ~20 ms).

In `housekeeping_task_kb()`:
```
for each held key in "analog repeat" list:
    depth = he_depth[row][col]
    interval = map(depth, 0 → max_interval, 1000 → min_interval)
    if timer has passed interval since last repeat:
        tap_code(keycode)
```

No per-key configuration needed for V1 — enabled globally via `id_an_repeat_enable` (VIA ID 23, reserved in base branch). Future: per-key enable/disable list.

---

## Verification Checklist

Use this to confirm implementation quality across all branches:

### Base Branch
- [ ] `he_depth` computed every matrix scan cycle
- [ ] `he_depth` not used by `he_update_key()` — existing APC/RT unchanged
- [ ] Custom keycodes compile and appear in VIAL keycode picker
- [ ] `process_record_kb()` returns `false` for analog keycodes, `true` for others
- [ ] `housekeeping_task_kb()` available for child branches
- [ ] VIA IDs 17–22 reserved (no-op handlers)
- [ ] UF2 compiles to same size as main

### Analog Mousekey Branch
- [ ] `AM_MS_*` keycodes tracked with row/col position
- [ ] `analog_mousekey_task()` rate-limited via `an_interval_ms`
- [ ] Exponential curve: depth→speed mapping monotonic for exponent 1–5
- [ ] Deadzone correctly suppresses speed for depth ≤ threshold
- [ ] Snipe divisor applied to cursor only (not scroll)
- [ ] Scroll reported via `v`/`h` axes independently
- [ ] Remote half keys fall back to depth 1000
- [ ] VIA set/get round-trips correctly for all 6 parameters
- [ ] EEPROM init + load handles new fields without data corruption
- [ ] No regressions in APC/Rapid Trigger key press detection

### Multi-Stage Branch (planned)
- [ ] `AM_MULTI` keycode tracked with row/col position
- [ ] Per-position config stored and loaded correctly
- [ ] State machine transitions with hysteresis
- [ ] Shallow/deep keycodes register/unregister correctly
- [ ] Layer MO( ) keys work as deep keycodes
- [ ] Webapp can read/write per-position config

### Analog Repeat Branch (planned)
- [ ] Held keys trigger additional taps at depth-dependent rate
- [ ] Rate respects min/max interval bounds
- [ ] Does not interfere with normal key repeat
- [ ] VIA enable/disable flag works
