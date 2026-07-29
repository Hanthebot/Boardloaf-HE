#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "action.h"
#include "matrix.h"

#define MS_KEY_COUNT (HE_MATRIX_ROWS * MATRIX_COLS)

typedef struct PACKED {
    uint16_t shallow_kc;
    uint16_t deep_kc;
    uint8_t  split_pct;
    uint8_t  hysteresis;
} ms_key_config_t;

extern ms_key_config_t ms_config[MS_KEY_COUNT];
extern uint8_t         ms_stage[MS_KEY_COUNT];

void   multistage_init(void);
bool   multistage_process_record(uint16_t keycode, keyrecord_t *record);
void   multistage_task(void);
