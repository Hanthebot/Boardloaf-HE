#include "he_multistage.h"
#include "he_switch_matrix.h"
#include "analog_common.h"
#include "action.h"
#include "quantum.h"

ms_key_config_t ms_config[MS_KEY_COUNT] = {{0}};
uint8_t         ms_stage[MS_KEY_COUNT]  = {0};

static uint8_t  ms_active_row = 0;
static uint8_t  ms_active_col = 0;

void multistage_init(void) {
    for (uint8_t i = 0; i < MS_KEY_COUNT; i++) {
        ms_stage[i] = 0;
    }
}

bool multistage_process_record(uint16_t keycode, keyrecord_t *record) {
    if (keycode != AM_MULTI) return false;

    uint8_t row = record->event.key.row;
    uint8_t col = record->event.key.col;
    uint8_t idx = row * MATRIX_COLS + col;

    if (record->event.pressed) {
        analog_activity_mask |= AM_MULTI_BIT;
        ms_active_row = row;
        ms_active_col = col;
    } else {
        uint8_t stage = ms_stage[idx];
        if (stage == 1) unregister_code16(ms_config[idx].shallow_kc);
        if (stage == 2) unregister_code16(ms_config[idx].deep_kc);
        ms_stage[idx] = 0;
        analog_activity_mask &= ~AM_MULTI_BIT;
    }
    return true;
}

void multistage_task(void) {
    if (!(analog_activity_mask & AM_MULTI_BIT)) return;

    uint8_t row = ms_active_row;
    uint8_t col = ms_active_col;

    if (row >= HE_MATRIX_ROWS) return;

    uint8_t  idx        = row * MATRIX_COLS + col;
    uint16_t depth      = he_depth[row][col];
    uint8_t  split      = ms_config[idx].split_pct;
    uint8_t  hyst       = ms_config[idx].hysteresis;
    uint8_t  stage      = ms_stage[idx];

    uint16_t shallow_kc = ms_config[idx].shallow_kc;
    uint16_t deep_kc    = ms_config[idx].deep_kc;

    if (depth > split && stage != 2) {
        if (stage == 1) unregister_code16(shallow_kc);
        register_code16(deep_kc);
        ms_stage[idx] = 2;
    } else if (depth < (split - hyst) && stage != 1) {
        if (stage == 2) unregister_code16(deep_kc);
        register_code16(shallow_kc);
        ms_stage[idx] = 1;
    }
}
