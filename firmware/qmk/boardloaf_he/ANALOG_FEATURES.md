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

## Multi-Stage Keys (`feature/multi-stage`)

### Design

Each physical key assigned `AM_MULTI` in VIAL acts as two keycodes: a *shallow* keycode (light press, above split threshold) and a *deep* keycode (deep press, below split threshold). State machine in `he_multistage.c`:

```
IDLE ──depth>split──► DEEP (register deep_kc via register_code16)
DEEP ──depth<split-hyst──► SHALLOW (unregister deep_kc, register shallow_kc)
SHALLOW ──depth>split──► DEEP (unregister shallow_kc, register deep_kc)
On key release → unregister whichever stage is active
```

**2G (staged layers):** Zero extra code. User sets `shallow_kc = MO(3)`, `deep_kc = MO(4)` in the webapp. `register_code16` handles layer on/off automatically.

### VIA / Webapp Split

- **VIAL**: user assigns `AM_MULTI` keycode to each physical key
- **Webapp** (VIA ID 24): per-position config — `shallow_kc` (uint16), `deep_kc` (uint16), `split_pct` (uint8, 0–100), `hysteresis` (uint8, 0–50). SET immediately persists to EEPROM.

### EEPROM Layout

```c
struct PACKED {
    uint16_t shallow_kc;
    uint16_t deep_kc;
    uint8_t  split_pct;
    uint8_t  hysteresis;
} ms_config[18];   // 18 × 6 = 108 bytes, offset 217–324
```

Total EEPROM: 325 bytes (was 217).

### File Inventory

| File | Lines | Role |
|------|-------|------|
| `he_multistage.h` | 26 | API, per-key config struct |
| `he_multistage.c` | 72 | State machine, process_record hook, housekeeping task |
| `boardloaf_he.c` | +7 | Hook calls + EEPROM init/load |
| `he_switch_matrix.h` | +7 | EEPROM struct extension |
| `via_he.c` (×2) | +16 | VIA ID 24 set/get |

### Build Verification

```bash
make boardloaf_he:vial   # 325 bytes EEPROM, clean
```

---

## Dynamic Key Repeat (`feature/analog-repeat`)

### Design

Tracks ALL non-analog key presses in a circular tracking array (`ar_track[16]`). In `housekeeping_task_kb()`, for each still-held key where `he_depth[row][col]` is available (local half):

```
interval = map(depth, 0 → 200ms, 1000 → 15ms)  // linear
if timer_elapsed(last_repeat) >= interval:
    tap_code16(keycode)           // sends press+release (5ms TAP_CODE_DELAY)
    if key still physically held: // matrix_get_row check
        register_code16(keycode)  // re-hold
```

The `tap_code16` + re-register pattern produces a clean press+release+repress for the host, triggering an additional auto-repeat character. The 5ms gap is imperceptible. If the user releases the physical key during the gap, the `matrix_get_row` check prevents a stuck key.

### VIA Config

VIA ID 23 (`id_an_repeat_enable`): toggle `ar_enabled` at runtime. Default on (enabled). No EEPROM persistence (resets to enabled on boot).

### File Inventory

| File | Lines | Role |
|------|-------|------|
| `he_analog_repeat.h` | 20 | API, tracking struct |
| `he_analog_repeat.c` | 68 | Key tracking, depth→interval mapping, repeat tap |
| `boardloaf_he.c` | +2 | Track + task hooks |
| `rules.mk` | +1 | SRC entry |
| `via_he.c` (×2) | +8 | VIA ID 23 toggle |

### Build Verification

```bash
make boardloaf_he:vial   # No EEPROM change, clean
```

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

### Multi-Stage Branch
- [x] `AM_MULTI` keycode tracked with row/col position
- [x] Per-position config stored in EEPROM and loaded at boot
- [x] State machine transitions with hysteresis (5% default gap)
- [x] Shallow/deep keycodes register/unregister via register_code16
- [x] Layer MO( ) keycodes work as deep_kc (staged layers / 2G)
- [x] VIA ID 24: set/get per-position config, persisted to EEPROM

### Analog Repeat Branch
- [x] All non-analog key presses tracked in `ar_track[16]`
- [x] Depth→interval linear mapping (15–200 ms)
- [x] `tap_code16` + re-register with physical hold check prevents stuck keys
- [x] VIA ID 23: toggle on/off at runtime
