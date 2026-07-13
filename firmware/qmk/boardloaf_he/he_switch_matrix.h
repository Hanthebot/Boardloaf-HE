/* Copyright 2023 Cipulot, 2024 byungyoonc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "matrix.h"
#include "eeconfig.h"
#include "util.h"

// Per-key Hall Effect configuration. Each of the 18 keys (3 rows x 6 cols
// per half in the split boardloaf layout) gets its own config rather than
// a board-wide default; the webapp "apply to all" button facilitates
// uniform configuration across all keys.
typedef struct PACKED {
    uint8_t  actuation_mode;                                                    // 0: APC, 1: Rapid Trigger
    uint16_t bottoming_reading[HE_MATRIX_ROWS][MATRIX_COLS];                     // per-key bottoming reading (used to rescale)
    uint16_t per_key_mode_0_actuation_threshold[HE_MATRIX_ROWS][MATRIX_COLS];   // per-key APC actuation point (raw 0..1023 scale)
    uint16_t per_key_mode_0_release_threshold[HE_MATRIX_ROWS][MATRIX_COLS];     // per-key APC release point (raw 0..1023 scale)
    uint16_t per_key_mode_1_initial_deadzone[HE_MATRIX_ROWS][MATRIX_COLS];      // per-key RT initial deadzone input value (raw 0..1023 scale)
    uint8_t  per_key_mode_1_actuation_offset[HE_MATRIX_ROWS][MATRIX_COLS];      // per-key RT actuation offset (1..255, raw units)
    uint8_t  per_key_mode_1_release_offset[HE_MATRIX_ROWS][MATRIX_COLS];        // per-key RT release offset (1..255, raw units)
    uint16_t per_key_bottom_deadzone[HE_MATRIX_ROWS][MATRIX_COLS];              // per-key bottom deadzone (raw 0..1023 scale, 0=disabled)
} eeprom_he_config_t;

typedef struct {
    uint8_t  actuation_mode;
    uint16_t rescaled_mode_0_actuation_threshold[HE_MATRIX_ROWS][MATRIX_COLS];
    uint16_t rescaled_mode_0_release_threshold[HE_MATRIX_ROWS][MATRIX_COLS];
    uint16_t rescaled_mode_1_initial_deadzone_offset[HE_MATRIX_ROWS][MATRIX_COLS];
    uint16_t per_key_mode_0_actuation_threshold[HE_MATRIX_ROWS][MATRIX_COLS];
    uint16_t per_key_mode_0_release_threshold[HE_MATRIX_ROWS][MATRIX_COLS];
    uint16_t per_key_mode_1_initial_deadzone[HE_MATRIX_ROWS][MATRIX_COLS];
    uint8_t  per_key_mode_1_actuation_offset[HE_MATRIX_ROWS][MATRIX_COLS];
    uint8_t  per_key_mode_1_release_offset[HE_MATRIX_ROWS][MATRIX_COLS];
    uint16_t per_key_bottom_deadzone[HE_MATRIX_ROWS][MATRIX_COLS];
    uint16_t rescaled_bottom_deadzone[HE_MATRIX_ROWS][MATRIX_COLS];
    uint16_t extremum[HE_MATRIX_ROWS][MATRIX_COLS];
    uint16_t noise_ceiling[HE_MATRIX_ROWS][MATRIX_COLS];
    bool     bottoming_calibration;
    bool     bottoming_calibration_starter[HE_MATRIX_ROWS][MATRIX_COLS];
    uint16_t bottoming_reading[HE_MATRIX_ROWS][MATRIX_COLS];
} he_config_t;

_Static_assert(sizeof(eeprom_he_config_t) == EECONFIG_KB_DATA_SIZE, "Mismatch in keyboard EECONFIG stored data");

extern eeprom_he_config_t eeprom_he_config;
extern he_config_t        he_config;
extern bool he_live_stream_enabled;

void init_row(void);
void init_amux(void);
void select_amux(uint8_t col);

int      he_init(void);
void     he_noise_ceiling(void);
bool     he_matrix_scan(matrix_row_t current_matrix[]);
uint16_t he_readkey_raw(uint8_t row);
bool     he_update_key(matrix_row_t* current_row, uint8_t row, uint8_t col, uint16_t sw_value);
void     he_print_matrix(void);
void     he_rescale_values(uint8_t item);
void     he_apply_key_to_all(uint8_t row, uint8_t col);

uint16_t rescale(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max);