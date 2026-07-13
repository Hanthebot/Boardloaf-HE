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

#include "quantum.h"
#include "he_switch_matrix.h"

void eeconfig_init_kb(void) {
    eeprom_he_config.actuation_mode = DEFAULT_ACTUATION_MODE;
    for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            eeprom_he_config.bottoming_reading[row][col]                   = DEFAULT_BOTTOMING_READING;
            eeprom_he_config.per_key_mode_0_actuation_threshold[row][col] = DEFAULT_MODE_0_ACTUATION_LEVEL;
            eeprom_he_config.per_key_mode_0_release_threshold[row][col]   = DEFAULT_MODE_0_RELEASE_LEVEL;
            eeprom_he_config.per_key_mode_1_initial_deadzone[row][col]     = DEFAULT_MODE_1_INITIAL_DEADZONE_OFFSET;
            eeprom_he_config.per_key_mode_1_actuation_offset[row][col]    = DEFAULT_MODE_1_ACTUATION_OFFSET;
            eeprom_he_config.per_key_mode_1_release_offset[row][col]      = DEFAULT_MODE_1_RELEASE_OFFSET;
            eeprom_he_config.per_key_bottom_deadzone[row][col]            = DEFAULT_BOTTOM_DEADZONE;
        }
    }
    eeconfig_update_kb_datablock(&eeprom_he_config, 0, EECONFIG_KB_DATA_SIZE);
    eeconfig_init_user();
}

void keyboard_post_init_kb(void) {
    eeconfig_read_kb_datablock(&eeprom_he_config, 0, EECONFIG_KB_DATA_SIZE);

    he_config.actuation_mode = eeprom_he_config.actuation_mode;
    he_config.bottoming_calibration = false;

    for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            he_config.bottoming_calibration_starter[row][col]               = true;
            he_config.per_key_mode_0_actuation_threshold[row][col]         = eeprom_he_config.per_key_mode_0_actuation_threshold[row][col];
            he_config.per_key_mode_0_release_threshold[row][col]            = eeprom_he_config.per_key_mode_0_release_threshold[row][col];
            he_config.per_key_mode_1_initial_deadzone[row][col]            = eeprom_he_config.per_key_mode_1_initial_deadzone[row][col];
            he_config.per_key_mode_1_actuation_offset[row][col]            = eeprom_he_config.per_key_mode_1_actuation_offset[row][col];
            he_config.per_key_mode_1_release_offset[row][col]              = eeprom_he_config.per_key_mode_1_release_offset[row][col];
            he_config.per_key_bottom_deadzone[row][col]                    = eeprom_he_config.per_key_bottom_deadzone[row][col];
            he_config.extremum[row][col]                                   = DEFAULT_EXTREMUM;
            // If bottoming not calibrated (reading=0), assume ~150 count travel
            // so rescaled thresholds are reasonable out of the box.
            if (eeprom_he_config.bottoming_reading[row][col] == 0) {
                uint16_t assumed_bottom = (he_config.noise_ceiling[row][col] > 150) ? (he_config.noise_ceiling[row][col] - 150) : 0;
                he_config.bottoming_reading[row][col] = assumed_bottom;
            } else {
                he_config.bottoming_reading[row][col] = eeprom_he_config.bottoming_reading[row][col];
            }
            he_config.rescaled_mode_0_actuation_threshold[row][col]     = rescale(he_config.per_key_mode_0_actuation_threshold[row][col], 0, 1023, he_config.noise_ceiling[row][col], he_config.bottoming_reading[row][col]);
            he_config.rescaled_mode_0_release_threshold[row][col]       = rescale(he_config.per_key_mode_0_release_threshold[row][col], 0, 1023, he_config.noise_ceiling[row][col], he_config.bottoming_reading[row][col]);
            he_config.rescaled_mode_1_initial_deadzone_offset[row][col] = rescale(he_config.per_key_mode_1_initial_deadzone[row][col], 0, 1023, he_config.noise_ceiling[row][col], he_config.bottoming_reading[row][col]);
            he_config.rescaled_bottom_deadzone[row][col]                = rescale(he_config.per_key_bottom_deadzone[row][col], 0, 1023, he_config.noise_ceiling[row][col], he_config.bottoming_reading[row][col]);
        }
    }

    keyboard_post_init_user();
}