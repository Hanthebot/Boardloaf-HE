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

// Runtime values are sanitised here so a stale/incompatible EEPROM (or a
// bogus VIA write) can never disable analog movement completely.
// Speed and acceleration come from QMK's own mousekey settings (mk_* /
// MOUSEKEY_*), so the analog feel matches the standard QMK mouse keys.
#define am_deadzone()     ((he_config.an_deadzone_pct <= 100) ? he_config.an_deadzone_pct : AM_DEADZONE_PCT)
#define am_curve()        ((he_config.an_curve_exponent >= 1 && he_config.an_curve_exponent <= 5) ? he_config.an_curve_exponent : AM_CURVE_EXPONENT)
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

bool analog_mousekey_process_record(uint16_t keycode, keyrecord_t *record) {
    int8_t idx = analog_mousekey_slot(keycode);
    if (idx < 0) {
        return false;
    }

    // AM_MULTI belongs to the multi-stage feature.  Do not swallow it here.
    if (idx == AM_MULTI_IDX) {
        return false;
    }

    if (idx == AM_SNIPE_IDX) {
        if (record->event.pressed) {
            analog_activity_mask |= AM_SNIPE_BIT;
        } else {
            analog_activity_mask &= ~AM_SNIPE_BIT;
        }
        return true;
    }

    if (record->event.pressed) {
        analog_key_row[idx]       = record->event.key.row;
        analog_key_col[idx]       = record->event.key.col;
        analog_key_press_time[idx] = timer_read32();
        analog_activity_mask |= (1 << idx);
    } else {
        analog_activity_mask &= ~(1 << idx);
        analog_key_press_time[idx] = 0;
    }
    return true;
}

static uint32_t compute_speed_fraction(uint16_t depth, uint8_t exponent, uint16_t deadzone_pct) {
    uint16_t deadzone = deadzone_pct * 10;
    if (deadzone > 999) deadzone = 999;
    if (depth <= deadzone) return 0;
    uint16_t effective = depth - deadzone;
    uint16_t max_range = 1000 - deadzone;
    if (max_range == 0) return 0;
    uint32_t result = effective;
    for (uint8_t i = 1; i < exponent; i++) {
        result = (result * effective) / max_range;
    }
    return result > 1000 ? 1000 : result;
}

static uint16_t get_key_depth(uint8_t row, uint8_t col) {
    // Only the local half has depth data.  On the remote half (rows >=
    // HE_MATRIX_ROWS) fall back to full speed; the half-duplex V1 split
    // transport does not carry analog values.
    if (row >= HE_MATRIX_ROWS || col >= MATRIX_COLS) return 1000;
    return he_depth[row][col];
}

// QMK's per-event unit at the analog multiplier's default (10 / 5 = 1x).
static uint16_t analog_base_unit(uint32_t delta, uint8_t qmk_max, uint8_t analog_mult, uint8_t analog_default, uint16_t limit) {
    uint32_t unit = delta * qmk_max * analog_mult / analog_default;
    if (unit > limit) unit = limit;
    if (unit == 0) unit = 1;
    return (uint16_t)unit;
}

// Power-curve time acceleration.  QMK's delay/time-to-max define the
// window; AM_TIME_EXP makes the ramp back-loaded instead of linear.
static uint32_t time_mult_q8(uint32_t hold_ms, bool snipe_active, uint32_t delay_ms, uint32_t ramp_ms) {
    if (snipe_active || ramp_ms == 0 || hold_ms <= delay_ms) return 256;

    uint32_t x = hold_ms - delay_ms;
    if (x > ramp_ms) x = ramp_ms;

    uint32_t t_q8   = x * 256 / ramp_ms;
    uint32_t pow_q8 = t_q8;
    for (uint8_t i = 1; i < AM_TIME_EXP; i++) {
        pow_q8 = pow_q8 * t_q8 / 256;
    }

    uint32_t max_q8 = AM_TIME_FULL_PCT * 256 / 100;
    return 256 + (max_q8 - 256) * pow_q8 / 256;
}

void analog_mousekey_task(void) {
    uint8_t  interval  = am_interval();
    uint8_t  curve     = am_curve();
    uint8_t  deadzone  = am_deadzone();
    uint8_t  snipe_div = am_snipe_div();

    // QMK's default max unit is the reference; 50% press is centred on it.
    uint16_t cursor_base = analog_base_unit(QS_mousekey_move_delta, mk_max_speed, am_max_speed(), 10, MOUSEKEY_MOVE_MAX);
    uint16_t scroll_base = analog_base_unit(MOUSEKEY_WHEEL_DELTA, mk_wheel_max_speed, am_scroll_max(), 5, MOUSEKEY_WHEEL_MAX);

    uint32_t mid_fraction = compute_speed_fraction(AM_PRESS_REF_PCT * 10, curve, deadzone);
    uint32_t press_max_q8 = AM_PRESS_FULL_PCT * 256 / 100;
    uint32_t cursor_delay = (uint32_t)mk_delay * 10;
    uint32_t cursor_ramp  = (uint32_t)mk_time_to_max * (mk_interval ? mk_interval : 1);
    uint32_t scroll_delay = (uint32_t)mk_wheel_delay * 10;
    uint32_t scroll_ramp  = (uint32_t)mk_wheel_time_to_max * (mk_wheel_interval ? mk_wheel_interval : 1);
    if (cursor_ramp > AM_TIME_RAMP_MAX_MS) cursor_ramp = AM_TIME_RAMP_MAX_MS;
    if (scroll_ramp > AM_TIME_RAMP_MAX_MS) scroll_ramp = AM_TIME_RAMP_MAX_MS;

    static uint16_t last_mouse_time = 0;
    if (timer_elapsed(last_mouse_time) < interval) return;
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
            uint32_t fraction = compute_speed_fraction(depth, curve, deadzone);
            uint32_t press_q8 = mid_fraction ? (fraction * 256 / mid_fraction) : 0;
            if (press_q8 > press_max_q8) press_q8 = press_max_q8;

            uint32_t hold_ms  = snipe_active ? 0 : timer_elapsed32(analog_key_press_time[idx]);
            uint32_t time_q8  = time_mult_q8(hold_ms, snipe_active, cursor_delay, cursor_ramp);
            uint64_t speed_q8 = (uint64_t)cursor_base * press_q8 * time_q8 * interval / (256ULL * (mk_interval ? mk_interval : 1));

            dx_q8 += (int32_t)speed_q8 * dir_dx[i];
            dy_q8 += (int32_t)speed_q8 * dir_dy[i];
        }
    }

    for (uint8_t i = 0; i < 4; i++) {
        if (scroll_mask & (1 << i)) {
            uint8_t  idx   = 4 + i;
            uint16_t depth = get_key_depth(analog_key_row[idx], analog_key_col[idx]);
            uint32_t fraction = compute_speed_fraction(depth, curve, deadzone);
            uint32_t press_q8 = mid_fraction ? (fraction * 256 / mid_fraction) : 0;
            if (press_q8 > press_max_q8) press_q8 = press_max_q8;

            uint32_t hold_ms  = snipe_active ? 0 : timer_elapsed32(analog_key_press_time[idx]);
            uint32_t time_q8  = time_mult_q8(hold_ms, snipe_active, scroll_delay, scroll_ramp);
            uint64_t speed_q8 = (uint64_t)scroll_base * press_q8 * time_q8 * interval / (256ULL * (mk_wheel_interval ? mk_wheel_interval : 1));

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
