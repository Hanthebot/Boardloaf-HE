#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "action.h"

#define AR_MAX_KEYS 16
#define AR_MIN_INTERVAL 15
#define AR_MAX_INTERVAL 200

typedef struct {
    uint16_t keycode;
    uint8_t  row;
    uint8_t  col;
    bool     active;
    uint16_t last_repeat;
} ar_track_t;

extern ar_track_t ar_track[AR_MAX_KEYS];
extern bool       ar_enabled;

void ar_track_key(uint16_t keycode, keyrecord_t *record);
void ar_task(void);
