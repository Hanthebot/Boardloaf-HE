#include QMK_KEYBOARD_H

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT_split_3x5_3(
        KC_Q,               KC_W,           KC_E,           KC_R,           KC_T,
        KC_Y,               KC_U,           KC_I,           KC_O,           KC_P,
        LSFT_T(KC_A),       KC_S,           KC_D,           KC_F,           KC_G,
        KC_H,               KC_J,           KC_K,           KC_L,           LSFT_T(KC_SCLN),
        LT(4, KC_Z),        KC_X,           KC_C,           KC_V,           KC_B,
        KC_N,               KC_M,           KC_COMM,       KC_DOT,         LT(4, KC_SLSH),
        KC_LGUI,            LT(1, KC_TAB),  LSFT_T(KC_SPC),
        LT(2, KC_BSPC),   LCTL_T(KC_ENT), LALT_T(KC_ENT)
    ),
    [1] = LAYOUT_split_3x5_3(
        KC_TRNS,            KC_TRNS,        KC_MUTE,        KC_MPLY,        KC_MPRV,
        KC_DEL,          KC_END,         KC_PGUP,        KC_PGDN,      KC_HOME,
        KC_BRID,            KC_BRIU,        KC_VOLD,        KC_VOLU,        KC_MNXT,
        KC_TRNS,            KC_RIGHT,       KC_UP,          KC_DOWN,        KC_LEFT,
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_TRNS,            KC_NO,          KC_TRNS,
        KC_RALT,            KC_ENT,       KC_BSPC
    ),
    [2] = LAYOUT_split_3x5_3(
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        LSFT(KC_7),         LSFT(KC_MINS), KC_RBRC,    KC_LBRC,    KC_GRV,
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_QUOT,           KC_EQL,       LSFT(KC_0),     LSFT(KC_9),     KC_BSLS,
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_LSFT,          LSFT(KC_DOT),   LSFT(KC_COMM),  LSFT(KC_RBRC),  LSFT(KC_LBRC),
        KC_LCTL,           LSFT_T(KC_0),   KC_0,
        KC_TRNS,            KC_LSFT,      KC_NO
    ),
    [3] = LAYOUT_split_3x5_3(
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_LCTL,           KC_WH_R,        KC_WH_U,        KC_WH_D,        KC_WH_L,
        KC_TRNS,            KC_TRNS,        KC_LSFT,      KC_LCTL,       KC_TRNS,
        KC_ACL0,            KC_MS_R,       KC_MS_U,        KC_MS_D,        KC_MS_L,
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_ACL1,            KC_BTN3,        KC_BTN2,        KC_BTN1,        KC_BTN3,
        KC_BTN3,            KC_BTN1,        KC_BTN2,
        KC_TRNS,            KC_LCTL,       KC_NO
    ),
    [4] = LAYOUT_split_3x5_3(
        KC_TRNS,            KC_F7,          KC_F8,          KC_F9,          KC_F10,
        LSFT(KC_MINS),     KC_9,           KC_8,           KC_7,           KC_KP_ASTERISK,
        KC_TRNS,            KC_F4,          KC_F5,          KC_F6,          KC_F11,
        KC_DOT,             KC_6,           KC_5,           KC_4,           KC_KP_PLUS,
        KC_TRNS,            KC_F1,          KC_F2,          KC_F3,          KC_F12,
        KC_SLSH,           KC_3,           KC_2,           KC_1,           KC_MINS,
        KC_LSFT,          KC_NO,          KC_LSFT,
        KC_TRNS,            LCTL_T(KC_0),   LSFT_T(KC_0)
    ),
    [5] = LAYOUT_split_3x5_3(
        KC_F5,              KC_HOME,        KC_PGUP,        KC_PGDN,      KC_END,
        KC_TRNS,            KC_RIGHT,       KC_UP,          KC_DOWN,        KC_LEFT,
        KC_A,               KC_S,           KC_D,           KC_F,           KC_G,
        KC_SCLN,          KC_L,           KC_K,           KC_J,           KC_H,
        KC_BRID,            KC_BRIU,        KC_VOLD,        KC_VOLU,        KC_TRNS,
        KC_TRNS,            KC_MS_R,        KC_MS_U,        KC_MS_D,        KC_MS_L,
        KC_ESC,          KC_SPC,       KC_LSFT,
        KC_BSPC,          KC_ENT,       KC_RSFT
    ),
    [6] = LAYOUT_split_3x5_3(
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_TRNS,            KC_TRNS,        KC_TRNS,
        KC_TRNS,            KC_TRNS,        KC_TRNS
    ),
    [7] = LAYOUT_split_3x5_3(
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_TRNS,            KC_TRNS,        KC_TRNS,        KC_TRNS,        KC_TRNS,
        KC_TRNS,            KC_TRNS,        KC_TRNS,
        KC_TRNS,            KC_TRNS,        KC_TRNS
    )
};