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
#include "action.h"
#include "print.h"
#include "via.h"

#ifdef VIA_ENABLE

enum via_enums {
    // clang-format off
    id_actuation_mode = 1,
    id_mode_0_actuation_threshold = 2,
    id_mode_0_release_threshold = 3,
    id_save_threshold_data = 4,
    id_mode_1_initial_deadzone_offset = 5,
    id_mode_1_actuation_offset = 6,
    id_mode_1_release_offset = 7,
    id_bottoming_calibration = 8,
    id_noise_ceiling_calibration = 9,
    id_show_calibration_data = 10,
    id_clear_bottoming_calibration_data = 11,
    id_toggle_live_stream = 12,
    id_apply_key_to_all = 13,
    id_get_all_per_key = 14,
    id_get_key_calib = 15,
    id_bottom_deadzone = 16
    // clang-format on
};

// Set custom value. value_data layout: [row, col, payload...]
void via_config_set_value(uint8_t *data) {
    uint8_t *value_id   = &(data[0]);
    uint8_t *value_data = &(data[1]);

    switch (*value_id) {
        case id_actuation_mode: {
            he_config.actuation_mode = value_data[0];
            eeprom_he_config.actuation_mode = value_data[0];
            eeconfig_update_kb_datablock_field(eeprom_he_config, actuation_mode);
            if (he_config.actuation_mode == 0) {
                uprintf("#########################\n");
                uprintf("#  Actuation Mode: APC  #\n");
                uprintf("#########################\n");
            } else if (he_config.actuation_mode == 1) {
                uprintf("#################################\n");
                uprintf("# Actuation Mode: Rapid Trigger #\n");
                uprintf("#################################\n");
            }
            break;
        }
        case id_mode_0_actuation_threshold: {
            uint8_t row = value_data[0];
            uint8_t col = value_data[1];
            uint16_t v = value_data[2] | (value_data[3] << 8);
            he_config.per_key_mode_0_actuation_threshold[row][col] = v;
            he_config.rescaled_mode_0_actuation_threshold[row][col] = rescale(v, 0, 1023, he_config.noise_ceiling[row][col], he_config.bottoming_reading[row][col]);
            uprintf("Set M0 actuation [%d,%d]=%d (rescaled=%d)\n", row, col, v, he_config.rescaled_mode_0_actuation_threshold[row][col]);
            break;
        }
        case id_mode_0_release_threshold: {
            uint8_t row = value_data[0];
            uint8_t col = value_data[1];
            uint16_t v = value_data[2] | (value_data[3] << 8);
            he_config.per_key_mode_0_release_threshold[row][col] = v;
            he_config.rescaled_mode_0_release_threshold[row][col] = rescale(v, 0, 1023, he_config.noise_ceiling[row][col], he_config.bottoming_reading[row][col]);
            uprintf("Set M0 release [%d,%d]=%d (rescaled=%d)\n", row, col, v, he_config.rescaled_mode_0_release_threshold[row][col]);
            break;
        }
        case id_mode_1_initial_deadzone_offset: {
            uint8_t row = value_data[0];
            uint8_t col = value_data[1];
            uint16_t v = value_data[2] | (value_data[3] << 8);
            he_config.per_key_mode_1_initial_deadzone[row][col] = v;
            he_config.rescaled_mode_1_initial_deadzone_offset[row][col] = rescale(v, 0, 1023, he_config.noise_ceiling[row][col], he_config.bottoming_reading[row][col]);
            uprintf("Set M1 deadzone [%d,%d]=%d (rescaled=%d)\n", row, col, v, he_config.rescaled_mode_1_initial_deadzone_offset[row][col]);
            break;
        }
        case id_mode_1_actuation_offset: {
            uint8_t row = value_data[0];
            uint8_t col = value_data[1];
            uint8_t v = value_data[2];
            he_config.per_key_mode_1_actuation_offset[row][col] = v;
            uprintf("Set M1 act_off [%d,%d]=%d\n", row, col, v);
            break;
        }
        case id_mode_1_release_offset: {
            uint8_t row = value_data[0];
            uint8_t col = value_data[1];
            uint8_t v = value_data[2];
            he_config.per_key_mode_1_release_offset[row][col] = v;
            uprintf("Set M1 rel_off [%d,%d]=%d\n", row, col, v);
            break;
        }
        case id_bottom_deadzone: {
            uint8_t row = value_data[0];
            uint8_t col = value_data[1];
            uint16_t v = value_data[2] | (value_data[3] << 8);
            he_config.per_key_bottom_deadzone[row][col] = v;
            he_config.rescaled_bottom_deadzone[row][col] = rescale(v, 0, 1023, he_config.noise_ceiling[row][col], he_config.bottoming_reading[row][col]);
            break;
        }
        case id_save_threshold_data: {
            for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
                for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                    eeprom_he_config.per_key_mode_0_actuation_threshold[row][col] = he_config.per_key_mode_0_actuation_threshold[row][col];
                    eeprom_he_config.per_key_mode_0_release_threshold[row][col]   = he_config.per_key_mode_0_release_threshold[row][col];
                    eeprom_he_config.per_key_mode_1_initial_deadzone[row][col]     = he_config.per_key_mode_1_initial_deadzone[row][col];
                    eeprom_he_config.per_key_mode_1_actuation_offset[row][col]    = he_config.per_key_mode_1_actuation_offset[row][col];
                    eeprom_he_config.per_key_mode_1_release_offset[row][col]      = he_config.per_key_mode_1_release_offset[row][col];
                    eeprom_he_config.per_key_bottom_deadzone[row][col]            = he_config.per_key_bottom_deadzone[row][col];
                }
            }
            eeconfig_update_kb_datablock(&eeprom_he_config, 0, EECONFIG_KB_DATA_SIZE);
            he_rescale_values(0);
            he_rescale_values(1);
            he_rescale_values(2);
            he_rescale_values(3);
            uprintf("####################################\n");
            uprintf("# Per-key thresholds applied+saved #\n");
            uprintf("####################################\n");
            break;
        }
        case id_bottoming_calibration: {
            if (value_data[0] == 1) {
                he_config.bottoming_calibration = true;
                uprintf("##############################\n");
                uprintf("# Bottoming calibration mode #\n");
                uprintf("##############################\n");
            } else {
                he_config.bottoming_calibration = false;
                for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
                    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                        if ((he_config.noise_ceiling[row][col] - BOTTOMING_CALIBRATION_THRESHOLD) < he_config.bottoming_reading[row][col]) {
                            eeprom_he_config.bottoming_reading[row][col] = 0;
                        } else {
                            eeprom_he_config.bottoming_reading[row][col] = he_config.bottoming_reading[row][col];
                        }
                    }
                }
                he_rescale_values(0);
                he_rescale_values(1);
                he_rescale_values(2);
                eeconfig_update_kb_datablock(&eeprom_he_config, 0, EECONFIG_KB_DATA_SIZE);
                uprintf("## Bottoming calibration done ##\n");
            }
            break;
        }
        case id_noise_ceiling_calibration: {
            if (value_data[0] == 0) {
                he_noise_ceiling();
                he_rescale_values(0);
                he_rescale_values(1);
                he_rescale_values(2);
                uprintf("#############################\n");
                uprintf("# Noise floor data acquired #\n");
                uprintf("#############################\n");
            }
            break;
        }
        case id_show_calibration_data: {
            if (value_data[0] == 0) {
                uprintf("\n###############\n");
                uprintf("# Noise Ceiling #\n");
                uprintf("###############\n");
                for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
                    for (uint8_t col = 0; col < MATRIX_COLS - 1; col++) {
                        uprintf("%4d,", he_config.noise_ceiling[row][col]);
                    }
                    uprintf("%4d\n", he_config.noise_ceiling[row][MATRIX_COLS - 1]);
                }
                uprintf("\n######################\n");
                uprintf("# Bottoming Readings #\n");
                uprintf("######################\n");
                for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
                    for (uint8_t col = 0; col < MATRIX_COLS - 1; col++) {
                        uprintf("%4d,", eeprom_he_config.bottoming_reading[row][col]);
                    }
                    uprintf("%4d\n", eeprom_he_config.bottoming_reading[row][MATRIX_COLS - 1]);
                }
                uprintf("\n######################################\n");
                uprintf("# APC actuation (raw, per-key)      #\n");
                uprintf("######################################\n");
                for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
                    for (uint8_t col = 0; col < MATRIX_COLS - 1; col++) {
                        uprintf("%4d,", he_config.per_key_mode_0_actuation_threshold[row][col]);
                    }
                    uprintf("%4d\n", he_config.per_key_mode_0_actuation_threshold[row][MATRIX_COLS - 1]);
                }
                uprintf("\n######################################\n");
                uprintf("# APC release (raw, per-key)        #\n");
                uprintf("######################################\n");
                for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
                    for (uint8_t col = 0; col < MATRIX_COLS - 1; col++) {
                        uprintf("%4d,", he_config.per_key_mode_0_release_threshold[row][col]);
                    }
                    uprintf("%4d\n", he_config.per_key_mode_0_release_threshold[row][MATRIX_COLS - 1]);
                }
                uprintf("\n######################################\n");
                uprintf("# RT deadzone (raw, per-key)         #\n");
                uprintf("######################################\n");
                for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
                    for (uint8_t col = 0; col < MATRIX_COLS - 1; col++) {
                        uprintf("%4d,", he_config.per_key_mode_1_initial_deadzone[row][col]);
                    }
                    uprintf("%4d\n", he_config.per_key_mode_1_initial_deadzone[row][MATRIX_COLS - 1]);
                }
                uprintf("\n######################################\n");
                uprintf("# RT act_off (per-key)              #\n");
                uprintf("######################################\n");
                for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
                    for (uint8_t col = 0; col < MATRIX_COLS - 1; col++) {
                        uprintf("%4d,", he_config.per_key_mode_1_actuation_offset[row][col]);
                    }
                    uprintf("%4d\n", he_config.per_key_mode_1_actuation_offset[row][MATRIX_COLS - 1]);
                }
                uprintf("\n######################################\n");
                uprintf("# RT rel_off (per-key)              #\n");
                uprintf("######################################\n");
                for (uint8_t row = 0; row < HE_MATRIX_ROWS; row++) {
                    for (uint8_t col = 0; col < MATRIX_COLS - 1; col++) {
                        uprintf("%4d,", he_config.per_key_mode_1_release_offset[row][col]);
                    }
                    uprintf("%4d\n", he_config.per_key_mode_1_release_offset[row][MATRIX_COLS - 1]);
                }
                print("\n");
            }
            break;
        }
        case id_clear_bottoming_calibration_data: {
            if (value_data[0] == 0) {
                eeconfig_init_kb();
                keyboard_post_init_kb();
                uprintf("######################################\n");
                uprintf("# Bottoming calibration data cleared #\n");
                uprintf("######################################\n");
            }
            break;
        }
        case id_toggle_live_stream: {
            he_live_stream_enabled = (value_data[0] == 1);
            uprintf(he_live_stream_enabled ? "Live stream ON\n" : "Live stream OFF\n");
            break;
        }
        case id_apply_key_to_all: {
            uint8_t row = value_data[0];
            uint8_t col = value_data[1];
            he_apply_key_to_all(row, col);
            break;
        }
        default: {
            break;
        }
    }
}

// Get custom value. value_data layout for per-key get: [row, col, payload...]
void via_config_get_value(uint8_t *data) {
    uint8_t *value_id   = &(data[0]);
    uint8_t *value_data = &(data[1]);

    switch (*value_id) {
        case id_actuation_mode: {
            value_data[0] = he_config.actuation_mode;
            break;
        }
        case id_mode_0_actuation_threshold: {
            uint8_t row = value_data[0];
            uint8_t col = value_data[1];
            uint16_t v = he_config.per_key_mode_0_actuation_threshold[row][col];
            value_data[2] = v & 0xFF;
            value_data[3] = v >> 8;
            break;
        }
        case id_mode_0_release_threshold: {
            uint8_t row = value_data[0];
            uint8_t col = value_data[1];
            uint16_t v = he_config.per_key_mode_0_release_threshold[row][col];
            value_data[2] = v & 0xFF;
            value_data[3] = v >> 8;
            break;
        }
        case id_mode_1_initial_deadzone_offset: {
            uint8_t row = value_data[0];
            uint8_t col = value_data[1];
            uint16_t v = he_config.per_key_mode_1_initial_deadzone[row][col];
            value_data[2] = v & 0xFF;
            value_data[3] = v >> 8;
            break;
        }
        case id_mode_1_actuation_offset: {
            uint8_t row = value_data[0];
            uint8_t col = value_data[1];
            value_data[2] = he_config.per_key_mode_1_actuation_offset[row][col];
            break;
        }
        case id_mode_1_release_offset: {
            uint8_t row = value_data[0];
            uint8_t col = value_data[1];
            value_data[2] = he_config.per_key_mode_1_release_offset[row][col];
            break;
        }
        case id_bottom_deadzone: {
            uint8_t row = value_data[0];
            uint8_t col = value_data[1];
            uint16_t v = he_config.per_key_bottom_deadzone[row][col];
            value_data[2] = v & 0xFF;
            value_data[3] = v >> 8;
            break;
        }
        case id_get_all_per_key: {
            // Returns a compact dump: for each key (3 rows x 6 cols), 7 bytes:
            // m0_act_lo, m0_act_hi, m0_rel_lo, m0_rel_hi, m1_dz_lo, m1_dz_hi,
            // a single packed byte: (act_off << 4) | (rel_off & 0x0f)
            // Total = 3 * 6 * 7 = 126 bytes. value_data has 28 bytes max, so
            // caller must paginate via value_data[0]=offset byte.
            // Each page carries 4 keys (28 bytes), so 5 pages cover 18 keys (last partial).
            // For simplicity: pack 4 keys per page.
            uint8_t offset = value_data[0];
            uint8_t *out = &value_data[1];
            // 4 keys per page * 7 bytes = 28 bytes payload under offset + value_id overhead
            for (uint8_t i = 0; i < 4; i++) {
                uint8_t kidx = offset * 4 + i;
                if (kidx >= HE_MATRIX_ROWS * MATRIX_COLS) {
                    break;
                }
                uint8_t r = kidx / MATRIX_COLS;
                uint8_t c = kidx % MATRIX_COLS;
                uint16_t m0a = he_config.per_key_mode_0_actuation_threshold[r][c];
                uint16_t m0r = he_config.per_key_mode_0_release_threshold[r][c];
                uint16_t m1d = he_config.per_key_mode_1_initial_deadzone[r][c];
                uint8_t  m1a = he_config.per_key_mode_1_actuation_offset[r][c];
                uint8_t  m1rl = he_config.per_key_mode_1_release_offset[r][c];
                uint8_t base = i * 7;
                out[base + 0] = m0a & 0xFF;
                out[base + 1] = m0a >> 8;
                out[base + 2] = m0r & 0xFF;
                out[base + 3] = m0r >> 8;
                out[base + 4] = m1d & 0xFF;
                out[base + 5] = m1d >> 8;
                out[base + 6] = (m1a << 4) | (m1rl & 0x0F);
            }
            break;
        }
        case id_get_key_calib: {
            // Returns per-key noise_ceiling and bottoming_reading so the webapp
            // can compute true travel percentage for RT offsets.
            // value_data IN:  [row, col]
            // value_data OUT: [row, col, noise_lo, noise_hi, bottom_lo, bottom_hi]
            uint8_t row = value_data[0];
            uint8_t col = value_data[1];
            uint16_t noise  = he_config.noise_ceiling[row][col];
            uint16_t bottom = he_config.bottoming_reading[row][col];
            value_data[2] = noise & 0xFF;
            value_data[3] = noise >> 8;
            value_data[4] = bottom & 0xFF;
            value_data[5] = bottom >> 8;
            break;
        }
        default: {
            break;
        }
    }
}

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    uint8_t *command_id        = &(data[0]);
    uint8_t *channel_id        = &(data[1]);
    uint8_t *value_id_and_data = &(data[2]);

    if (*channel_id == id_custom_channel) {
        switch (*command_id) {
            case id_custom_set_value: {
                via_config_set_value(value_id_and_data);
                break;
            }
            case id_custom_get_value: {
                via_config_get_value(value_id_and_data);
                break;
            }
            case id_custom_save: {
                // Bypass the save function in favor of pinpointed saves
                break;
            }
            default: {
                *command_id = id_unhandled;
                break;
            }
        }
        return;
    }

    *command_id = id_unhandled;
}

void raw_hid_receive_kb(uint8_t *data, uint8_t length) {
    via_custom_value_command_kb(data, length);
}

#endif // VIA_ENABLE