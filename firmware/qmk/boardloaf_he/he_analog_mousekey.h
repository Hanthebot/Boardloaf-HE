#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "action.h"

#define AM_MS_UP_IDX    0
#define AM_MS_DOWN_IDX  1
#define AM_MS_LEFT_IDX  2
#define AM_MS_RIGHT_IDX 3
#define AM_WH_UP_IDX    4
#define AM_WH_DOWN_IDX  5
#define AM_WH_LEFT_IDX  6
#define AM_WH_RIGHT_IDX 7
#define AM_SNIPE_IDX    8
#define AM_MULTI_IDX    9

extern uint8_t analog_key_row[10];
extern uint8_t analog_key_col[10];

bool analog_mousekey_process_record(uint16_t keycode, keyrecord_t *record);
void analog_mousekey_task(void);
