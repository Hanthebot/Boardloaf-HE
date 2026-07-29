#include "he_analog_repeat.h"
#include "he_switch_matrix.h"
#include "analog_common.h"
#include "quantum.h"
#include "timer.h"

ar_track_t ar_track[AR_MAX_KEYS] = {{0}};
bool       ar_enabled             = true;

void ar_track_key(uint16_t keycode, keyrecord_t *record) {
    if (keycode >= AM_MS_UP && keycode <= AM_MULTI) return;

    if (record->event.pressed) {
        for (uint8_t i = 0; i < AR_MAX_KEYS; i++) {
            if (!ar_track[i].active) {
                ar_track[i].keycode        = keycode;
                ar_track[i].row            = record->event.key.row;
                ar_track[i].col            = record->event.key.col;
                ar_track[i].active         = true;
                ar_track[i].last_repeat    = timer_read();
                break;
            }
        }
    } else {
        for (uint8_t i = 0; i < AR_MAX_KEYS; i++) {
            if (ar_track[i].active && ar_track[i].row == record->event.key.row && ar_track[i].col == record->event.key.col) {
                ar_track[i].active = false;
                break;
            }
        }
    }
}

static uint16_t ar_depth_to_interval(uint16_t depth) {
    if (depth >= 1000) return AR_MIN_INTERVAL;
    uint16_t interval = AR_MAX_INTERVAL - ((uint32_t)(AR_MAX_INTERVAL - AR_MIN_INTERVAL) * depth / 1000);
    if (interval < AR_MIN_INTERVAL) return AR_MIN_INTERVAL;
    if (interval > AR_MAX_INTERVAL) return AR_MAX_INTERVAL;
    return interval;
}

static void ar_repeat_tap(uint16_t keycode, uint8_t row, uint8_t col) {
    tap_code16(keycode);
    if (matrix_get_row(row) & (1 << col)) {
        register_code16(keycode);
    }
}

void ar_task(void) {
    if (!ar_enabled) return;

    for (uint8_t i = 0; i < AR_MAX_KEYS; i++) {
        if (!ar_track[i].active) continue;

        uint8_t r = ar_track[i].row;
        uint8_t c = ar_track[i].col;
        if (r >= HE_MATRIX_ROWS) continue;

        uint16_t depth   = he_depth[r][c];
        uint16_t interval = ar_depth_to_interval(depth);

        if (timer_elapsed(ar_track[i].last_repeat) >= interval) {
            ar_track[i].last_repeat = timer_read();
            ar_repeat_tap(ar_track[i].keycode, r, c);
        }
    }
}
