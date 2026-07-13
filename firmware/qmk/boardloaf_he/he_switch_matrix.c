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

#include "he_switch_matrix.h"
#include "analog.h"
#include "print.h"
#include "wait.h"
#include "keyboard.h"

#if defined(__AVR__)
#    error "AVR platforms not supported due to a variety of reasons. Among them there are limited memory, limited number of pins and ADC not being able to give satisfactory results."
#endif

eeprom_he_config_t eeprom_he_config;
he_config_t        he_config;
bool he_live_stream_enabled = false;

const pin_t row_pins[]                               = MATRIX_ROW_PINS;
const pin_t amux_sel_pins[]                          = AMUX_SEL_PINS;
const pin_t amux_n_col_channels[AMUX_MAX_COLS_COUNT] = AMUX_COL_CHANNELS;
const uint8_t rows_per_col[]                         = ROWS_PER_COL;

#define AMUX_SEL_PINS_COUNT ARRAY_SIZE(amux_sel_pins)

static uint16_t sw_value[HE_MATRIX_ROWS][MATRIX_COLS];
static adc_mux adcMux[HE_MATRIX_ROWS];

void init_row(void) {
    for (uint8_t idx = 0; idx < HE_MATRIX_ROWS; ++idx) {
        palSetLineMode(row_pins[idx], PAL_MODE_INPUT_ANALOG);
        adcMux[idx] = pinToMux(row_pins[idx]);
    }
}

void init_amux(void) {
    for (uint8_t idx = 0; idx < AMUX_SEL_PINS_COUNT; ++idx) {
        gpio_set_pin_output(amux_sel_pins[idx]);
    }
}

void select_amux(uint8_t col) {
    uint8_t ch = amux_n_col_channels[col];
    for (uint8_t idx = 0; idx < AMUX_SEL_PINS_COUNT; ++idx) {
        gpio_write_pin(amux_sel_pins[idx], ch & (1 << idx));
    }
}

int he_init(void) {
    init_row();
    init_amux();
    return 0;
}

void he_noise_ceiling(void) {
    for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            he_config.noise_ceiling[row][col] = 0;
        }
    }
    for (uint8_t i = 0; i < DEFAULT_NOISE_CEILING_SAMPLING_COUNT; i++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            select_amux(col);
            for (uint8_t row = 0; row < rows_per_col[col]; row++) {
                he_config.noise_ceiling[row][col] += he_readkey_raw(row);
            }
        }
        wait_ms(5);
    }
    for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            he_config.noise_ceiling[row][col] /= DEFAULT_NOISE_CEILING_SAMPLING_COUNT;
        }
    }
}

bool he_matrix_scan(matrix_row_t current_matrix[]) {
    bool updated = false;

    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        select_amux(col);
        for (uint8_t row = 0; row < rows_per_col[col]; row++) {
            sw_value[row][col] = he_readkey_raw(row);
            if (he_config.bottoming_calibration) {
                if (he_config.bottoming_calibration_starter[row][col]) {
                    he_config.bottoming_reading[row][col] = sw_value[row][col];
                    he_config.bottoming_calibration_starter[row][col] = false;
                } else if (sw_value[row][col] < he_config.bottoming_reading[row][col]) {
                    he_config.bottoming_reading[row][col] = sw_value[row][col];
                }
            } else {
                updated |= he_update_key(&current_matrix[row], row, col, sw_value[row][col]);
            }
        }
    }

    return he_config.bottoming_calibration ? false : updated;
}

uint16_t he_readkey_raw(uint8_t row) {
    uint16_t sw_value = 0;
    sw_value = adc_read(adcMux[row]);
    return sw_value;
}

bool he_update_key(matrix_row_t* current_row, uint8_t row, uint8_t col, uint16_t sw_value) {
    bool current_state = (*current_row >> col) & 1;

    // Real Time Noise Floor Calibration
    if ((he_config.noise_ceiling[row][col] + NOISE_CEILING_THRESHOLD) < sw_value) {
        he_config.noise_ceiling[row][col]                           = sw_value;
        he_config.rescaled_mode_0_actuation_threshold[row][col]     = rescale(he_config.per_key_mode_0_actuation_threshold[row][col], 0, 1023, he_config.noise_ceiling[row][col], he_config.bottoming_reading[row][col]);
        he_config.rescaled_mode_0_release_threshold[row][col]       = rescale(he_config.per_key_mode_0_release_threshold[row][col], 0, 1023, he_config.noise_ceiling[row][col], he_config.bottoming_reading[row][col]);
        he_config.rescaled_mode_1_initial_deadzone_offset[row][col] = rescale(he_config.per_key_mode_1_initial_deadzone[row][col], 0, 1023, he_config.noise_ceiling[row][col], he_config.bottoming_reading[row][col]);
        he_config.rescaled_bottom_deadzone[row][col]                = rescale(he_config.per_key_bottom_deadzone[row][col], 0, 1023, he_config.noise_ceiling[row][col], he_config.bottoming_reading[row][col]);
    }

    // Bottom deadzone: if key is very close to bottoming, force pressed
    // regardless of mode. Prevents chatter when fully bottomed out.
    if (he_config.per_key_bottom_deadzone[row][col] > 0 && sw_value < he_config.rescaled_bottom_deadzone[row][col]) {
        if (sw_value < he_config.extremum[row][col]) {
            he_config.extremum[row][col] = sw_value;
        }
        if (!current_state) {
            *current_row |= (1 << col);
            return true;
        }
        return false;
    }

    if (he_config.actuation_mode == 0) {
        // APC mode
        if (current_state && he_config.rescaled_mode_0_release_threshold[row][col] < sw_value) {
            *current_row &= ~(1 << col);
            return true;
        }
        if ((!current_state) && sw_value < he_config.rescaled_mode_0_actuation_threshold[row][col]) {
            *current_row |= (1 << col);
            return true;
        }
    } else if (he_config.actuation_mode == 1) {
        // Rapid Trigger mode (per-key offsets)
        uint8_t act_off = he_config.per_key_mode_1_actuation_offset[row][col];
        uint8_t rel_off = he_config.per_key_mode_1_release_offset[row][col];
        if (sw_value < he_config.rescaled_mode_1_initial_deadzone_offset[row][col]) {
            if (current_state) {
                if (sw_value < he_config.extremum[row][col]) {
                    he_config.extremum[row][col] = sw_value;
                } else if (he_config.extremum[row][col] + rel_off < sw_value) {
                    he_config.extremum[row][col] = sw_value;
                    *current_row &= ~(1 << col);
                    return true;
                }
            } else {
                if (he_config.extremum[row][col] < sw_value) {
                    he_config.extremum[row][col] = sw_value;
                } else if (sw_value < he_config.extremum[row][col] - act_off) {
                    he_config.extremum[row][col] = sw_value;
                    *current_row |= (1 << col);
                    return true;
                }
            }
        } else {
            if (he_config.extremum[row][col] < sw_value) {
                he_config.extremum[row][col] = sw_value;
                *current_row &= ~(1 << col);
                return true;
            }
        }
    }
    return false;
}

void he_print_matrix(void) {
    uint8_t row_offset = is_keyboard_left() ? 0 : HE_MATRIX_ROWS;
    uprintf("MATRIX_STREAM:\n");
    for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            if (row < rows_per_col[col]) {
                uprintf("%d,", sw_value[row][col]);
            } else {
                uprintf("-1,");
            }
        }
        uprintf("\n");
    }
    uprintf("MATRIX_STATE:\n");
    for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
        matrix_row_t state = matrix_get_row(row + row_offset);
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            uprintf("%d,", (state & (1 << col)) ? 1 : 0);
        }
        uprintf("\n");
    }
}

uint16_t rescale(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void he_rescale_values(uint8_t item) {
    switch (item) {
        case 0:
            for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
                for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                    he_config.rescaled_mode_0_actuation_threshold[row][col] = rescale(he_config.per_key_mode_0_actuation_threshold[row][col], 0, 1023, he_config.noise_ceiling[row][col], he_config.bottoming_reading[row][col]);
                }
            }
            break;
        case 1:
            for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
                for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                    he_config.rescaled_mode_0_release_threshold[row][col] = rescale(he_config.per_key_mode_0_release_threshold[row][col], 0, 1023, he_config.noise_ceiling[row][col], he_config.bottoming_reading[row][col]);
                }
            }
            break;
        case 2:
            for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
                for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                    he_config.rescaled_mode_1_initial_deadzone_offset[row][col] = rescale(he_config.per_key_mode_1_initial_deadzone[row][col], 0, 1023, he_config.noise_ceiling[row][col], he_config.bottoming_reading[row][col]);
                }
            }
            break;
        case 3:
            for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
                for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                    he_config.rescaled_bottom_deadzone[row][col] = rescale(he_config.per_key_bottom_deadzone[row][col], 0, 1023, he_config.noise_ceiling[row][col], he_config.bottoming_reading[row][col]);
                }
            }
            break;
        default:
            break;
    }
}

void he_apply_key_to_all(uint8_t row, uint8_t col) {
    uint16_t m0_act = he_config.per_key_mode_0_actuation_threshold[row][col];
    uint16_t m0_rel = he_config.per_key_mode_0_release_threshold[row][col];
    uint16_t m1_dz  = he_config.per_key_mode_1_initial_deadzone[row][col];
    uint8_t  m1_act = he_config.per_key_mode_1_actuation_offset[row][col];
    uint8_t  m1_rel = he_config.per_key_mode_1_release_offset[row][col];
    uint16_t bdz   = he_config.per_key_bottom_deadzone[row][col];
    for (uint8_t r = 0; r < HE_MATRIX_ROWS; r++) {
        for (uint8_t c = 0; c < MATRIX_COLS; c++) {
            he_config.per_key_mode_0_actuation_threshold[r][c] = m0_act;
            he_config.per_key_mode_0_release_threshold[r][c] = m0_rel;
            he_config.per_key_mode_1_initial_deadzone[r][c] = m1_dz;
            he_config.per_key_mode_1_actuation_offset[r][c] = m1_act;
            he_config.per_key_mode_1_release_offset[r][c] = m1_rel;
            he_config.per_key_bottom_deadzone[r][c] = bdz;
        }
    }
    he_rescale_values(0);
    he_rescale_values(1);
    he_rescale_values(2);
    he_rescale_values(3);
    uprintf("Apply key (%d,%d) to all keys\n", row, col);
}