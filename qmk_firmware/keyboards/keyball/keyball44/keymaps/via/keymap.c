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

// --- 状態をPCへ送る（OLEDエミュレータ用） ----------------------------------
// 実機にOLEDを付けていないので、OLEDに出るはずの情報を Raw HID で PC へ流す。
// VIA_ENABLE=yes により RAW_ENABLE も有効なので、送る口は既にある。
//
// 受け側: ClaudeCode/keyball/tools/pointermon
//
// 注意: Remap(WebHID) を開いたままだと Raw HID の取り合いになる。同時に使わないこと。
//       止めたいときは KBSTAT_TG（Remap: USER00 / 0x7E40）を任意のキーに割り当てて押す。
#ifdef RAW_ENABLE
#    include "raw_hid.h"
#    include <string.h>

#    ifndef RAW_EPSIZE
#        define RAW_EPSIZE 32
#    endif

enum custom_keycodes {
    KBSTAT_TG = KEYBALL_SAFE_RANGE,   // 送信の入/切  = QK_USER_0 = 0x7E40（Remap: USER00）
};

#    define KBSTAT_MAGIC    0xB5
#    define KBSTAT_VERSION  1
#    define KBSTAT_INTERVAL 100        // 送信間隔[ms]

static bool     kbstat_enable = true;
static uint32_t kbstat_last   = 0;

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (keycode == KBSTAT_TG) {
        if (record->event.pressed) kbstat_enable = !kbstat_enable;
        return false;
    }
    return true;
}

static void kbstat_send(void) {
    uint8_t b[RAW_EPSIZE];
    memset(b, 0, sizeof(b));

    b[0] = KBSTAT_MAGIC;
    b[1] = KBSTAT_VERSION;

    b[2] = keyball_get_cpi();                    // 実CPI = この値 × 100
    b[3] = keyball_get_scroll_div();
    b[4] = keyball_get_scroll_mode() ? 0x01 : 0x00;
#    if KEYBALL_SCROLLSNAP_ENABLE == 2
    // 0 = スナップ機能なし、1..3 = VERTICAL / HORIZONTAL / FREE
    b[4] |= (uint8_t)((keyball_get_scrollsnap_mode() + 1) << 1);
#    endif

    uint32_t ls = layer_state;
    b[5] = (uint8_t)(ls);
    b[6] = (uint8_t)(ls >> 8);
    b[7] = (uint8_t)(ls >> 16);
    b[8] = (uint8_t)(ls >> 24);

    // MOUSE_EXTENDED_REPORT の有無で x/y の型が変わるので、必ず int16 に広げてから詰める
    int16_t mx = (int16_t)keyball.last_mouse.x;
    int16_t my = (int16_t)keyball.last_mouse.y;
    b[9]  = (uint8_t)(mx);
    b[10] = (uint8_t)(mx >> 8);
    b[11] = (uint8_t)(my);
    b[12] = (uint8_t)(my >> 8);
    b[13] = (uint8_t)(keyball.last_mouse.h);
    b[14] = (uint8_t)(keyball.last_mouse.v);

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    b[15] = get_auto_mouse_enable() ? 1 : 0;
    uint16_t amt = get_auto_mouse_timeout();
    b[16] = (uint8_t)(amt);
    b[17] = (uint8_t)(amt >> 8);
#    endif

    b[18] = keyball.last_pos.row;
    b[19] = keyball.last_pos.col;
    b[20] = (uint8_t)(keyball.last_kc);
    b[21] = (uint8_t)(keyball.last_kc >> 8);
    b[22] = keyball.this_have_ball ? 1 : 0;
    b[23] = keyball.that_have_ball ? 1 : 0;

    raw_hid_send(b, sizeof(b));
}

void housekeeping_task_user(void) {
    // 送るのはUSBに繋がっている側だけ。従側は raw_hid を持たない。
    if (!kbstat_enable || !is_keyboard_master()) return;
    uint32_t now = timer_read32();
    if (TIMER_DIFF_32(now, kbstat_last) < KBSTAT_INTERVAL) return;
    kbstat_last = now;
    kbstat_send();
}
#endif  // RAW_ENABLE
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
