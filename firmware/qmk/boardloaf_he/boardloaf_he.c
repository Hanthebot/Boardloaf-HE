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

#include <string.h>

#include "quantum.h"
#include "he_switch_matrix.h"
#include "analog_common.h"
#include "he_analog_mousekey.h"
#include "eeprom.h"
#include "nvm/eeprom/nvm_eeprom_eeconfig_internal.h"

// EEPROM datablock size before the analog fields were appended.  QMK uses
// the datablock size as its version, so a size change alone would make the
// old datablock invalid and reset every per-key calibration value.
#define LEGACY_EEPROM_KB_DATA_SIZE 217

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
    eeprom_he_config.an_deadzone_pct    = AM_DEADZONE_PCT;
    eeprom_he_config.an_curve_exponent  = AM_CURVE_EXPONENT;
    eeprom_he_config.an_max_speed       = AM_MAX_SPEED;
    eeprom_he_config.an_snipe_divisor   = AM_SNIPE_DIVISOR;
    eeprom_he_config.an_scroll_max      = AM_SCROLL_MAX;
    eeprom_he_config.an_interval_ms     = AM_INTERVAL_MS;
    for (uint8_t i = 0; i < 8; i++) eeprom_he_config.an_reserved[i] = 0;
    eeprom_he_config.an_reserved[0] = AM_EEPROM_VERSION;
    eeconfig_update_kb_datablock(&eeprom_he_config, 0, EECONFIG_KB_DATA_SIZE);
    eeconfig_init_user();
}

void keyboard_post_init_kb(void) {
    bool     am_config_migrated = false;
    uint32_t stored_version     = eeprom_read_dword(EECONFIG_KEYBOARD);

    if (stored_version == LEGACY_EEPROM_KB_DATA_SIZE) {
        // The old 217-byte layout is a strict prefix of the new one.  Read
        // it raw (the public API would reject it as version-mismatched) so
        // existing per-key calibration and bottoming data survive the
        // upgrade instead of being reset to zero.
        eeprom_read_block(&eeprom_he_config, EECONFIG_KB_DATABLOCK, LEGACY_EEPROM_KB_DATA_SIZE);
        memset((uint8_t *)&eeprom_he_config + LEGACY_EEPROM_KB_DATA_SIZE, 0, sizeof(eeprom_he_config) - LEGACY_EEPROM_KB_DATA_SIZE);
        am_config_migrated = true;
    } else if (eeconfig_is_kb_datablock_valid()) {
        eeconfig_read_kb_datablock(&eeprom_he_config, 0, EECONFIG_KB_DATA_SIZE);
    } else {
        // Blank or unknown datablock: initialize the keyboard defaults.
        eeconfig_init_kb();
        eeconfig_read_kb_datablock(&eeprom_he_config, 0, EECONFIG_KB_DATA_SIZE);
    }

    // Migrate analog EEPROM fields.  Keep custom values when possible.
    // Version rule: only bump AM_EEPROM_VERSION when stored field semantics
    // change.  For default-only changes, preserve user values and replace
    // only values that still equal the old default.
    if (eeprom_he_config.an_reserved[0] != AM_EEPROM_VERSION) {
        if (eeprom_he_config.an_reserved[0] == AM_LEGACY_EEPROM_VERSION) {
            // 0xA5 -> 0xA6: deadzone default 10 -> 2, curve default 2 -> 1.
            if (eeprom_he_config.an_deadzone_pct == 10) {
                eeprom_he_config.an_deadzone_pct = AM_DEADZONE_PCT;
            }
            if (eeprom_he_config.an_curve_exponent == 2) {
                eeprom_he_config.an_curve_exponent = AM_CURVE_EXPONENT;
            }
        } else {
            // Older/blank EEPROM: initialize all analog fields.
            eeprom_he_config.an_deadzone_pct   = AM_DEADZONE_PCT;
            eeprom_he_config.an_curve_exponent = AM_CURVE_EXPONENT;
            eeprom_he_config.an_max_speed      = AM_MAX_SPEED;
            eeprom_he_config.an_snipe_divisor  = AM_SNIPE_DIVISOR;
            eeprom_he_config.an_scroll_max     = AM_SCROLL_MAX;
            eeprom_he_config.an_interval_ms    = AM_INTERVAL_MS;
        }
        for (uint8_t i = 0; i < 8; i++) eeprom_he_config.an_reserved[i] = 0;
        eeprom_he_config.an_reserved[0] = AM_EEPROM_VERSION;
        am_config_migrated              = true;
    }

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
    he_config.an_deadzone_pct   = eeprom_he_config.an_deadzone_pct;
    he_config.an_curve_exponent = eeprom_he_config.an_curve_exponent;
    he_config.an_max_speed      = eeprom_he_config.an_max_speed;
    he_config.an_snipe_divisor  = eeprom_he_config.an_snipe_divisor;
    he_config.an_scroll_max     = eeprom_he_config.an_scroll_max;
    he_config.an_interval_ms    = eeprom_he_config.an_interval_ms;

    if (am_config_migrated) {
        eeconfig_update_kb_datablock(&eeprom_he_config, 0, EECONFIG_KB_DATA_SIZE);
    }

    keyboard_post_init_user();
}

uint16_t analog_activity_mask = 0;

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
#if ANALOG_MOUSEKEY_ENABLE
    if (analog_mousekey_process_record(keycode, record)) return false;
#endif
    return process_record_user(keycode, record);
}

void housekeeping_task_kb(void) {
#if ANALOG_MOUSEKEY_ENABLE
    analog_mousekey_task();
#endif
    housekeeping_task_user();
}
