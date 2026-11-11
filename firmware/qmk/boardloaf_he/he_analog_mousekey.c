#include "quantum.h"
#include "he_analog_mousekey.h"
#include "he_switch_matrix.h"
#include "analog_common.h"
#include "report.h"
#include "host.h"
#include "timer.h"
#include "action.h"
#include "mousekey.h"
#include "qmk_settings.h"

uint8_t analog_key_row[10] = {0};
uint8_t analog_key_col[10] = {0};

// Press timestamp per analog slot, used for QMK-compatible speed ramping.
static uint32_t analog_key_press_time[10] = {0};

// Q8.8 residual per axis so sub-unit speeds survive between reports.
static int32_t cursor_res_x = 0, cursor_res_y = 0;
static int32_t scroll_res_v = 0, scroll_res_h = 0;

// Force one immediate update on the first press instead of waiting for the
// next report interval.  This keeps first-movement latency close to QMK.
static bool analog_force_update = false;

// Runtime values are sanitised here so a stale/incompatible EEPROM (or a
// bogus VIA write) can never disable analog movement completely.
// Speed and acceleration come from QMK's own mousekey settings (mk_* /
// MOUSEKEY_*), so the analog feel matches the standard QMK mouse keys.
#define am_max_speed()    ((he_config.an_max_speed >= 1 && he_config.an_max_speed <= 127) ? he_config.an_max_speed : AM_MAX_SPEED)
#define am_snipe_div()    ((he_config.an_snipe_divisor >= 1 && he_config.an_snipe_divisor <= 100) ? he_config.an_snipe_divisor : AM_SNIPE_DIVISOR)
#define am_scroll_max()   ((he_config.an_scroll_max >= 1 && he_config.an_scroll_max <= 127) ? he_config.an_scroll_max : AM_SCROLL_MAX)
#define am_interval()     ((he_config.an_interval_ms >= 1 && he_config.an_interval_ms <= 100) ? he_config.an_interval_ms : AM_INTERVAL_MS)

// Map both the stock QMK mouse keycodes and the optional AM_* custom
// keycodes onto the same internal slots.  The stock keycodes are the
// important ones: they are always present in Vial's keycode picker, so
// analog mouse keys can be used without any custom keycode support or
// webapp configuration.
static int8_t analog_mousekey_slot(uint16_t keycode) {
    switch (keycode) {
        case KC_MS_U:
            return AM_MS_UP_IDX;
        case KC_MS_D:
            return AM_MS_DOWN_IDX;
        case KC_MS_L:
            return AM_MS_LEFT_IDX;
        case KC_MS_R:
            return AM_MS_RIGHT_IDX;
        case KC_WH_U:
            return AM_WH_UP_IDX;
        case KC_WH_D:
            return AM_WH_DOWN_IDX;
        case KC_WH_L:
            return AM_WH_LEFT_IDX;
        case KC_WH_R:
            return AM_WH_RIGHT_IDX;
        case KC_ACL0: // "mouse accel 0" is repurposed as the snipe key
            return AM_SNIPE_IDX;
        default:
            if (keycode >= AM_MS_UP && keycode <= AM_MULTI) {
                return keycode - AM_MS_UP;
            }
            return -1;
    }
}

static bool analog_key_is_local(uint8_t row) {
    return is_keyboard_left() ? (row < HE_MATRIX_ROWS) : (row >= HE_MATRIX_ROWS);
}

bool analog_mousekey_process_record(uint16_t keycode, keyrecord_t *record) {
    int8_t idx = analog_mousekey_slot(keycode);
    if (idx < 0) {
        return false;
    }

    // AM_MULTI belongs to the multi-stage feature.  Do not swallow it here.
    if (idx == AM_MULTI_IDX) {
        return false;
    }

    // Remote-half keys have no analog depth.  Let QMK handle them as normal
    // mousekeys instead of pretending they are fully pressed.
    if (idx != AM_SNIPE_IDX && !analog_key_is_local(record->event.key.row)) {
        return false;
    }

    if (idx == AM_SNIPE_IDX) {
        if (record->event.pressed) {
            analog_activity_mask |= AM_SNIPE_BIT;
        } else {
            analog_activity_mask &= ~AM_SNIPE_BIT;
        }
        // Let stock KC_ACL0 reach QMK too, so snipe also slows remote-half
        // keys that fall through to QMK's native mousekey handling.
        return false;
    }

    if (record->event.pressed) {
        analog_key_row[idx]       = record->event.key.row;
        analog_key_col[idx]       = record->event.key.col;
        analog_key_press_time[idx] = timer_read32();
        analog_activity_mask |= (1 << idx);
        analog_force_update = true;
    } else {
        analog_activity_mask &= ~(1 << idx);
        analog_key_press_time[idx] = 0;
    }
    return true;
}

// Piecewise press curve: depth (0..1000) -> % of QMK base speed.
// Separate shallow / regular / deep anchors make shallow presses visible
// without making the regular range jump too quickly.
typedef struct {
    uint16_t depth;
    uint8_t  pct;
} press_anchor_t;

static const press_anchor_t press_curve[] = {
    {0, AM_MIN_PRESS_PCT},
    {200, 8},
    {500, 20},
    {800, 50},
    {1000, 100},
};

static uint32_t press_coeff_q8(uint16_t depth) {
    uint16_t n = depth > 1000 ? 1000 : depth;

    for (uint8_t i = 0; i < ARRAY_SIZE(press_curve) - 1; i++) {
        const press_anchor_t *a = &press_curve[i];
        const press_anchor_t *b = &press_curve[i + 1];
        if (n <= b->depth) {
            uint32_t pct = a->pct + (uint32_t)(n - a->depth) * (b->pct - a->pct) / (b->depth - a->depth);
            return pct * 256 / 100;
        }
    }
    return 256;
}

static uint16_t get_key_depth(uint8_t row, uint8_t col) {
    // The local half always stores depth in local rows 0..HE_MATRIX_ROWS-1.
    // On a left master the right half's global rows are 3..5; on a right
    // master the left half's global rows are 0..2.  Remote keys are not
    // tracked by the analog layer any more, so this fallback is only a
    // safety net for malformed positions.
    if (col >= MATRIX_COLS) return 1000;

    if (is_keyboard_left()) {
        if (row >= HE_MATRIX_ROWS) return 1000;
        return he_depth[row][col];
    }

    if (row < HE_MATRIX_ROWS) return 1000;
    return he_depth[row - HE_MATRIX_ROWS][col];
}

// Time coefficient in Q8.8: starts at AM_TIME_START_PCT and ramps to
// AM_TIME_MAX_PCT with a power curve over the QMK-derived window.
static uint32_t time_coeff_q8(uint32_t hold_ms, bool snipe_active, uint32_t ramp_ms) {
    uint32_t start = AM_TIME_START_PCT * 256 / 100;
    uint32_t max   = AM_TIME_MAX_PCT * 256 / 100;

    if (snipe_active) return 256;
    if (ramp_ms == 0) return start;

    uint32_t x = hold_ms > ramp_ms ? ramp_ms : hold_ms;
    uint32_t t = x * 256 / ramp_ms;
    uint32_t p = t;
    for (uint8_t i = 1; i < AM_TIME_EXP; i++) {
        p = p * t / 256;
    }

    return start + (max - start) * p / 256;
}

void analog_mousekey_task(void) {
    uint8_t interval  = am_interval();
    uint8_t snipe_div = am_snipe_div();

    // QMK base unit = step size * max speed.  The analog max fields are
    // relative multipliers (10 / 5 = 1x).
    uint16_t cursor_base = (uint16_t)((uint32_t)QS_mousekey_move_delta * mk_max_speed * am_max_speed() / 10);
    uint16_t scroll_base = (uint16_t)((uint32_t)MOUSEKEY_WHEEL_DELTA * mk_wheel_max_speed * am_scroll_max() / 5);
    if (cursor_base > MOUSEKEY_MOVE_MAX) cursor_base = MOUSEKEY_MOVE_MAX;
    if (scroll_base > MOUSEKEY_WHEEL_MAX) scroll_base = MOUSEKEY_WHEEL_MAX;
    if (cursor_base == 0) cursor_base = 1;
    if (scroll_base == 0) scroll_base = 1;

    uint32_t cursor_ramp = (uint32_t)mk_time_to_max * (mk_interval ? mk_interval : 1);
    uint32_t scroll_ramp = (uint32_t)mk_wheel_time_to_max * (mk_wheel_interval ? mk_wheel_interval : 1);
    if (cursor_ramp > AM_TIME_RAMP_MAX_MS) cursor_ramp = AM_TIME_RAMP_MAX_MS;
    if (scroll_ramp > AM_TIME_RAMP_MAX_MS) scroll_ramp = AM_TIME_RAMP_MAX_MS;

    static uint16_t last_mouse_time = 0;
    if (!analog_force_update && timer_elapsed(last_mouse_time) < interval) return;
    analog_force_update = false;
    last_mouse_time = timer_read();

    uint16_t cursor_mask = analog_activity_mask & 0x0F;
    uint16_t scroll_mask = (analog_activity_mask >> 4) & 0x0F;
    if (!cursor_mask && !scroll_mask) {
        cursor_res_x = cursor_res_y = 0;
        scroll_res_v = scroll_res_h = 0;
        return;
    }

    // Preserve the button state owned by QMK's mousekey subsystem; sending
    // a zero-button report here would release a held mouse button.
    report_mouse_t report = mousekey_get_report();
    report.x = 0;
    report.y = 0;
    report.v = 0;
    report.h = 0;

    bool snipe_active = (analog_activity_mask & AM_SNIPE_BIT) != 0;

    if (!cursor_mask) {
        cursor_res_x = cursor_res_y = 0;
    }
    if (!scroll_mask) {
        scroll_res_v = scroll_res_h = 0;
    }

    int32_t dx_q8 = cursor_res_x;
    int32_t dy_q8 = cursor_res_y;
    int32_t dv_q8 = scroll_res_v;
    int32_t dh_q8 = scroll_res_h;

    static const int8_t dir_dx[4] = {0, 0, -1, 1};
    static const int8_t dir_dy[4] = {-1, 1, 0, 0};

    for (uint8_t i = 0; i < 4; i++) {
        if (cursor_mask & (1 << i)) {
            uint8_t  idx   = i;
            uint16_t depth = get_key_depth(analog_key_row[idx], analog_key_col[idx]);
            uint32_t c_press = press_coeff_q8(depth);
            uint32_t hold_ms = snipe_active ? 0 : timer_elapsed32(analog_key_press_time[idx]);
            uint32_t c_time  = time_coeff_q8(hold_ms, snipe_active, cursor_ramp);
            uint64_t speed_q8 = (uint64_t)cursor_base * c_press * c_time * interval / (256ULL * (mk_interval ? mk_interval : 1));

            dx_q8 += (int32_t)speed_q8 * dir_dx[i];
            dy_q8 += (int32_t)speed_q8 * dir_dy[i];
        }
    }

    for (uint8_t i = 0; i < 4; i++) {
        if (scroll_mask & (1 << i)) {
            uint8_t  idx   = 4 + i;
            uint16_t depth = get_key_depth(analog_key_row[idx], analog_key_col[idx]);
            uint32_t c_press = press_coeff_q8(depth);
            uint32_t hold_ms = snipe_active ? 0 : timer_elapsed32(analog_key_press_time[idx]);
            uint32_t c_time  = time_coeff_q8(hold_ms, snipe_active, scroll_ramp);
            uint64_t speed_q8 = (uint64_t)scroll_base * c_press * c_time * interval / (256ULL * (mk_wheel_interval ? mk_wheel_interval : 1));

            if (i < 2) {
                dv_q8 += (i == 0) ? (int32_t)speed_q8 : -(int32_t)speed_q8;
            } else {
                dh_q8 += (i == 2) ? -(int32_t)speed_q8 : (int32_t)speed_q8;
            }
        }
    }

    int16_t dx = (int16_t)(dx_q8 / 256);
    int16_t dy = (int16_t)(dy_q8 / 256);
    int16_t dv = (int16_t)(dv_q8 / 256);
    int16_t dh = (int16_t)(dh_q8 / 256);

    cursor_res_x = dx_q8 - (int32_t)dx * 256;
    cursor_res_y = dy_q8 - (int32_t)dy * 256;
    scroll_res_v = dv_q8 - (int32_t)dv * 256;
    scroll_res_h = dh_q8 - (int32_t)dh * 256;

    if (snipe_active) {
        int16_t pre_dx = dx;
        int16_t pre_dy = dy;
        dx /= snipe_div;
        dy /= snipe_div;
        // Keep at least one unit of movement in each active direction;
        // otherwise a fully pressed key (speed < divisor) would stop moving.
        if (pre_dx && !dx) dx = (pre_dx > 0) ? 1 : -1;
        if (pre_dy && !dy) dy = (pre_dy > 0) ? 1 : -1;
    }

    report.x = (int8_t)(dx > 127 ? 127 : dx < -127 ? -127 : dx);
    report.y = (int8_t)(dy > 127 ? 127 : dy < -127 ? -127 : dy);
    report.v = (int8_t)(dv > 127 ? 127 : dv < -127 ? -127 : dv);
    report.h = (int8_t)(dh > 127 ? 127 : dh < -127 ? -127 : dh);

    if (report.x || report.y || report.v || report.h) {
        host_mouse_send(&report);
    }
}
