#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "quantum_keycodes.h"

enum analog_keycodes {
    AM_MS_UP = SAFE_RANGE,
    AM_MS_DOWN,
    AM_MS_LEFT,
    AM_MS_RIGHT,
    AM_WH_UP,
    AM_WH_DOWN,
    AM_WH_LEFT,
    AM_WH_RIGHT,
    AM_SNIPE,
    AM_MULTI,
};

// Vial's keycode picker only knows the stock QMK keycode list, so it cannot
// assign the AM_* keycodes above.  The analog mousekey module also accepts
// the stock KC_MS_*/KC_WH_*/KC_ACL0 keycodes for Vial-only use; see
// he_analog_mousekey.c and ANALOG_FEATURES.md.

#define AM_MS_UP_BIT    (1 << 0)
#define AM_MS_DOWN_BIT  (1 << 1)
#define AM_MS_LEFT_BIT  (1 << 2)
#define AM_MS_RIGHT_BIT (1 << 3)
#define AM_WH_UP_BIT    (1 << 4)
#define AM_WH_DOWN_BIT  (1 << 5)
#define AM_WH_LEFT_BIT  (1 << 6)
#define AM_WH_RIGHT_BIT (1 << 7)
#define AM_SNIPE_BIT    (1 << 8)
#define AM_MULTI_BIT    (1 << 9)

extern uint16_t analog_activity_mask;
