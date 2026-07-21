/*
Copyright 2022 @Yowkees
Copyright 2022 MURAOKA Taro (aka KoRoN, @kaoriya)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include QMK_KEYBOARD_H

#include "quantum.h"

// --- 押下中だけ低CPI（精密モード / サニパー） --------------------------------
// Remapで USER00 (0x7E40) を好きなキーに割り当てて使う。押している間だけ低CPI、
// 離すと元のCPIに戻る（現在CPIを退避するので普段CPIを上げていてもOK）。
enum custom_keycodes {
    CPI_PREC = KEYBALL_SAFE_RANGE,   // = QK_USER_0 = 0x7E40（Remap: USER00）
};

#define PRECISION_CPI 2              // 押下中のCPI（×100 → 2 = 200CPI）。1=100 / 3=300 で調整可

static uint8_t  saved_cpi   = 0;      // 精密モード前のCPI
static bool     prec_active = false;  // 精密モード中か
static keypos_t prec_key;             // 押した「物理キー位置」を記憶

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // --- 保険：精密モード中に「記憶した物理キー」が離されたら必ず復帰 ---
    // AML等でレイヤーが変わると、離した時のキーコードが CPI_PREC 以外に解決され、
    // 復帰処理が走らず低CPIのまま固着する。キーコードではなく物理位置で判定して防ぐ。
    if (prec_active && !record->event.pressed
        && record->event.key.row == prec_key.row
        && record->event.key.col == prec_key.col) {
        keyball_set_cpi(saved_cpi);
        prec_active = false;
        return false;
    }

    switch (keycode) {
        case CPI_PREC:
            if (record->event.pressed) {
                if (!prec_active) {                 // 二重押しで退避値を潰さない
                    saved_cpi   = keyball_get_cpi();
                    prec_key    = record->event.key;
                    prec_active = true;
                }
                keyball_set_cpi(PRECISION_CPI);     // 精密モードへ
            } else {
                if (prec_active) {
                    keyball_set_cpi(saved_cpi);     // 離したら元のCPIへ復帰
                    prec_active = false;
                }
            }
            return false;
    }
    return true;
}
// --------------------------------------------------------------------------

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  // keymap for default (VIA)
  [0] = LAYOUT_universal(
    KC_ESC   , KC_Q     , KC_W     , KC_E     , KC_R     , KC_T     ,                                        KC_Y     , KC_U     , KC_I     , KC_O     , KC_P     , KC_DEL   ,
    KC_TAB   , KC_A     , KC_S     , KC_D     , KC_F     , KC_G     ,                                        KC_H     , KC_J     , KC_K     , KC_L     , KC_SCLN  , S(KC_7)  ,
    KC_LSFT  , KC_Z     , KC_X     , KC_C     , KC_V     , KC_B     ,                                        KC_N     , KC_M     , KC_COMM  , KC_DOT   , KC_SLSH  , KC_INT1  ,
              KC_LALT,KC_LGUI,LCTL_T(KC_LNG2)     ,LT(1,KC_SPC),LT(3,KC_LNG1),                  KC_BSPC,LT(2,KC_ENT), RCTL_T(KC_LNG2),     KC_RALT  , KC_PSCR
  ),

  [1] = LAYOUT_universal(
    SSNP_FRE ,  KC_F1   , KC_F2    , KC_F3   , KC_F4    , KC_F5    ,                                         KC_F6    , KC_F7    , KC_F8    , KC_F9    , KC_F10   , KC_F11   ,
    SSNP_VRT ,  _______ , _______  , KC_UP   , KC_ENT   , KC_DEL   ,                                         KC_PGUP  , KC_BTN1  , KC_UP    , KC_BTN2  , KC_BTN3  , KC_F12   ,
    SSNP_HOR ,  _______ , KC_LEFT  , KC_DOWN , KC_RGHT  , KC_BSPC  ,                                         KC_PGDN  , KC_LEFT  , KC_DOWN  , KC_RGHT  , _______  , _______  ,
                  _______  , _______ , _______  ,         _______  , _______  ,                   _______  , _______  , _______       , _______  , _______
  ),

  [2] = LAYOUT_universal(
    _______  ,S(KC_QUOT), KC_7     , KC_8    , KC_9     , S(KC_8)  ,                                         S(KC_9)  , S(KC_1)  , S(KC_6)  , KC_LBRC  , S(KC_4)  , _______  ,
    _______  ,S(KC_SCLN), KC_4     , KC_5    , KC_6     , KC_RBRC  ,                                         KC_NUHS  , KC_MINS  , S(KC_EQL), S(KC_3)  , KC_QUOT  , S(KC_2)  ,
    _______  ,S(KC_MINS), KC_1     , KC_2    , KC_3     ,S(KC_RBRC),                                        S(KC_NUHS),S(KC_INT1), KC_EQL   ,S(KC_LBRC),S(KC_SLSH),S(KC_INT3),
                  KC_0     , KC_DOT  , _______  ,         _______  , _______  ,                   KC_DEL   , _______  , _______       , _______  , _______
  ),

  [3] = LAYOUT_universal(
    RGB_TOG  , AML_TO   , AML_I50  , AML_D50  , _______  , _______  ,                                        RGB_M_P  , RGB_M_B  , RGB_M_R  , RGB_M_SW , RGB_M_SN , RGB_M_K  ,
    RGB_MOD  , RGB_HUI  , RGB_SAI  , RGB_VAI  , _______  , SCRL_DVI ,                                        RGB_M_X  , RGB_M_G  , RGB_M_T  , RGB_M_TW , _______  , _______  ,
    RGB_RMOD , RGB_HUD  , RGB_SAD  , RGB_VAD  , _______  , SCRL_DVD ,                                        CPI_D1K  , CPI_D100 , CPI_I100 , CPI_I1K  , _______  , KBC_SAVE ,
                  QK_BOOT  , KBC_RST  , _______  ,        _______  , _______  ,                   _______  , _______  , _______       , KBC_RST  , QK_BOOT
  ),
};
// clang-format on

layer_state_t layer_state_set_user(layer_state_t state) {
    // Auto enable scroll mode when the highest layer is 3
    keyball_set_scroll_mode(get_highest_layer(state) == 3);
    return state;
}

#ifdef OLED_ENABLE

#    include "lib/oledkit/oledkit.h"

void oledkit_render_info_user(void) {
    keyball_oled_render_keyinfo();
    keyball_oled_render_ballinfo();
    keyball_oled_render_layerinfo();
}
#endif
