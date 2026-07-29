#include "he_analog_mousekey.h"
#include "he_switch_matrix.h"
#include "analog_common.h"
#include "report.h"
#include "host.h"
#include "timer.h"
#include "action.h"

uint8_t analog_key_row[10] = {0};
uint8_t analog_key_col[10] = {0};

#define am_deadzone()     (he_config.an_deadzone_pct ? he_config.an_deadzone_pct : 50)
#define am_curve()        (he_config.an_curve_exponent ? he_config.an_curve_exponent : 2)
#define am_max_speed()    (he_config.an_max_speed ? he_config.an_max_speed : 10)
#define am_snipe_div()    (he_config.an_snipe_divisor ? he_config.an_snipe_divisor : 10)
#define am_scroll_max()   (he_config.an_scroll_max ? he_config.an_scroll_max : 5)
#define am_interval()     (he_config.an_interval_ms ? he_config.an_interval_ms : 10)

bool analog_mousekey_process_record(uint16_t keycode, keyrecord_t *record) {
    if (keycode >= AM_MS_UP && keycode <= AM_MULTI) {
        uint8_t idx = keycode - AM_MS_UP;
        if (record->event.pressed) {
            analog_key_row[idx] = record->event.key.row;
            analog_key_col[idx] = record->event.key.col;
            analog_activity_mask |= (1 << idx);
        } else {
            analog_activity_mask &= ~(1 << idx);
        }
        return true;
    }
    return false;
}

static uint32_t compute_speed_fraction(uint16_t depth, uint8_t exponent, uint16_t deadzone) {
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
    if (row >= HE_MATRIX_ROWS) return 1000;
    return he_depth[row][col];
}

void analog_mousekey_task(void) {
    uint8_t  interval   = am_interval();
    uint8_t  curve      = am_curve();
    uint8_t  max_speed  = am_max_speed();
    uint8_t  deadzone   = am_deadzone();
    uint8_t  snipe_div  = am_snipe_div();
    uint8_t  scroll_max = am_scroll_max();

    static uint16_t last_mouse_time = 0;
    if (timer_elapsed(last_mouse_time) < interval) return;
    last_mouse_time = timer_read();

    uint16_t cursor_mask = analog_activity_mask & 0x0F;
    uint16_t scroll_mask = (analog_activity_mask >> 4) & 0x0F;
    if (!cursor_mask && !scroll_mask) return;

    report_mouse_t report = {0};
    int16_t dx = 0, dy = 0, dv = 0, dh = 0;

    static const int8_t dir_dx[4] = {0, 0, -1, 1};
    static const int8_t dir_dy[4] = {-1, 1, 0, 0};

    for (uint8_t i = 0; i < 4; i++) {
        if (cursor_mask & (1 << i)) {
            uint8_t idx = i;
            uint16_t depth = get_key_depth(analog_key_row[idx], analog_key_col[idx]);
            uint32_t fraction = compute_speed_fraction(depth, curve, deadzone);
            int16_t speed = (int16_t)((fraction * max_speed) / 1000);
            if (speed < 1 && depth > deadzone) speed = 1;
            dx += dir_dx[i] * speed;
            dy += dir_dy[i] * speed;
        }
    }

    for (uint8_t i = 0; i < 4; i++) {
        if (scroll_mask & (1 << i)) {
            uint8_t idx = 4 + i;
            uint16_t depth = get_key_depth(analog_key_row[idx], analog_key_col[idx]);
            uint32_t fraction = compute_speed_fraction(depth, curve, deadzone);
            int16_t speed = (int16_t)((fraction * scroll_max) / 1000);
            if (speed < 1 && depth > deadzone) speed = 1;
            if (i < 2) {
                dv += (i == 0 ? speed : -speed);
            } else {
                dh += (i == 2 ? -speed : speed);
            }
        }
    }

    if (analog_activity_mask & AM_SNIPE_BIT) {
        dx /= snipe_div;
        dy /= snipe_div;
    }

    report.x = (int8_t)(dx > 127 ? 127 : dx < -127 ? -127 : dx);
    report.y = (int8_t)(dy > 127 ? 127 : dy < -127 ? -127 : dy);
    report.v = (int8_t)(dv > 127 ? 127 : dv < -127 ? -127 : dv);
    report.h = (int8_t)(dh > 127 ? 127 : dh < -127 ? -127 : dh);

    if (report.x || report.y || report.v || report.h) {
        host_mouse_send(&report);
    }
}
