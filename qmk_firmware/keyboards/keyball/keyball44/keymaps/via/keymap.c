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

// --- 状態をPCへ返す（OLEDエミュレータ用） ----------------------------------
// 実機にOLEDを付けていないので、OLEDに出るはずの情報を Raw HID で PC へ返す。
//
// ★勝手に送らない。PCから要求されたときだけ返す。
//   最初は100msごとに送りつける実装にしたが、Remap(WebHID)は
//   「コマンドを送って返事を待つ」方式のため、こちらの無関係なパケットを
//   返事と誤解して接続できなくなった。要求応答にすれば干渉しない。
//
// 受け側: ClaudeCode/keyball/tools/pointermon
#if defined(RAW_ENABLE) && defined(VIA_ENABLE)
#    include "raw_hid.h"
#    include "via.h"
#    include <string.h>

// VIAのコマンドIDは 0x00〜0x0E 付近と 0xFF。衝突しない値を選ぶ。
#    define KBSTAT_CMD     0xB5
#    define KBSTAT_VERSION 3   // 3: b[24] に慣性のON/OFFを追加

#    ifdef POINTING_DEVICE_ENABLE
bool keyball_inertia_is_enabled(void);   // 実体は下の「トラックボールの慣性」ブロック
#    endif

static void kbstat_fill(uint8_t *b, uint8_t len) {
    memset(b, 0, len);

    b[0] = KBSTAT_CMD;
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
#    ifdef POINTING_DEVICE_ENABLE
    if (len > 24) {
        b[24] = keyball_inertia_is_enabled() ? 1 : 0;
    }
#    endif
}

// VIAが自前で処理する前に呼ばれる。true を返すとVIAの処理を止める。
// 自分のコマンド以外は false を返し、Remapの動作を一切邪魔しない。
bool via_command_kb(uint8_t *data, uint8_t length) {
    if (length > 0 && data[0] == KBSTAT_CMD) {
        kbstat_fill(data, length);
        raw_hid_send(data, length);
        return true;
    }
    return false;
}
#endif  // RAW_ENABLE && VIA_ENABLE
// --------------------------------------------------------------------------

// --- トラックボールの慣性 --------------------------------------------------
//
// フリックしたあと滑る。ゆっくり動かしているときは滑らないので、
// 精密な位置合わせは今までどおりできる。ポインタとスクロールの両方に効く。
//
// なぜ pointing_device_task_user() を使うか
//   keyball_on_apply_motion_to_mouse_move() は「動きがあるとき」の変換用で、
//   しかも自分側/相手側で2回呼ばれてレポートを上書きし合う。
//   慣性は「動きが無いtick」にも値を出す必要があるので使えない。
//   pointing_device_task_user() なら最終レポートを1回だけ触れる。
//   keyball 側は pointing_device_task_kb() を上書きしていないので素通しで届く。
//
// なぜ固定小数点か
//   ATmega32U4 に FPU が無い。速度を 1/256 単位の整数で持つ。
//
// なぜ時間で駆動するか
//   pointing_device_task の呼ばれる間隔は環境で変わる。tick数で減衰させると
//   滑り方が状況で変わってしまう。keyball のレポート間隔と同じ 8ms(125Hz)で
//   1コマ進める。

#ifdef POINTING_DEVICE_ENABLE

#    include <stdlib.h>   // labs()。quantum.h 経由で入る保証が無いので明示する

#    define INRT_SHIFT   8                  // 1.0 = 256
#    define INRT_ONE     (1L << INRT_SHIFT)
#    define INRT_TICK_MS KEYBALL_REPORTMOUSE_INTERVAL  // 8ms
#    define INRT_DECAY   246                // 246/256 ≒ 0.961 /コマ。約0.7秒で止まる

// ポインタ用。CPI700での移動量は普段 2〜16、強く弾くと40前後（pointermon実測）。
// 18 にすると「普段の速さで動かして止めた」では滑らず、意図して弾いたときだけ滑る。
// 机上シミュレーションでの滑走距離: ピーク24→558カウント / ピーク40→950カウント。
#    define INRT_MOVE_MIN  (18 * INRT_ONE)  // これ未満の速度では滑らせない
#    define INRT_MOVE_STOP (1 * INRT_ONE)

// スクロール用。単位は「1コマあたり何段出たか」。0.60段/tick 以上で滑らせる。
// DIV6(÷32)ならボール20カウント/レポートあたりが境目。
// 滑走はボール20→7段 / 28→13段 / 44→24段。
#    define INRT_SCRL_MIN  ((60 * INRT_ONE) / 100)
#    define INRT_SCRL_STOP (INRT_ONE / 4)

typedef struct {
    int32_t vx, vy;   // 速度
    int32_t rx, ry;   // 整数に落とし切れなかった端数の持ち越し
    bool    coasting;
} inrt_t;

static inrt_t   inrt_move, inrt_scrl;
static uint32_t inrt_last    = 0;
static bool     inrt_enabled = true;

static void inrt_reset(inrt_t *s) {
    s->vx = s->vy = s->rx = s->ry = 0;
    s->coasting = false;
}

bool keyball_inertia_is_enabled(void) {
    return inrt_enabled;
}

static void inrt_stop_all(void) {
    inrt_reset(&inrt_move);
    inrt_reset(&inrt_scrl);
}

/// ポインタ用。入力がある間は速度を追いかける。
///
/// 重みは 1/2。1/4 にすると追従が遅く、5レポート分のフリックで入力の76%までしか
/// 上がらなかった（机上で確認）。1/2 なら立ち上がりが速いだけでなく、
/// 「狙って減速して止める」ときに速度も素早く落ちるので、
/// 精密に止めたつもりが滑り出す事故が起きない。
/// 実際、ピーク40から4レポート(32ms)減速するだけで滑走はキャンセルされる。
static void inrt_track(inrt_t *s, int16_t dx, int16_t dy) {
    s->vx = (s->vx + ((int32_t)dx << INRT_SHIFT)) / 2;
    s->vy = (s->vy + ((int32_t)dy << INRT_SHIFT)) / 2;
    s->coasting = false;
    s->rx = s->ry = 0;
}

/// スクロール用。「1コマあたり何段出たか」の割合を測る。
///
/// ★ 段が出なかったコマ（0）も必ず入れること。
///   分周後の値は 0 か 1 しか出ないので、出たコマだけ平均すると
///   ゆっくり回しても弾いても同じ 1 になり、区別できなくなる。
///   時定数は 16コマ ≒ 128ms。
static int32_t inrt_rate(int32_t v, int8_t n) {
    return (v * 15 + ((int32_t)n << INRT_SHIFT)) / 16;
}

/// 滑走を1コマ進める。出す量を *ox,*oy に返す。まだ滑るなら true。
static bool inrt_step(inrt_t *s, int32_t start_min, int32_t stop_at, int8_t *ox, int8_t *oy) {
    if (!s->coasting) {
        // 滑り出しの判定。ゆっくり動かして止めただけなら滑らせない
        if (!inrt_enabled || labs(s->vx) + labs(s->vy) < start_min) {
            inrt_reset(s);
            return false;
        }
        s->coasting = true;
    }
    s->vx = (s->vx * INRT_DECAY) >> INRT_SHIFT;
    s->vy = (s->vy * INRT_DECAY) >> INRT_SHIFT;
    if (labs(s->vx) + labs(s->vy) < stop_at) {
        inrt_reset(s);
        return false;
    }
    s->rx += s->vx;
    s->ry += s->vy;
    // 負数の算術右シフトは床関数になる。端数を引き戻すので誤差は溜まらない。
    int32_t px = s->rx >> INRT_SHIFT;
    int32_t py = s->ry >> INRT_SHIFT;
    s->rx -= px << INRT_SHIFT;
    s->ry -= py << INRT_SHIFT;
    // MOUSE_EXTENDED_REPORT は無効なので int8 に収める
    *ox = px > 127 ? 127 : (px < -127 ? -127 : (int8_t)px);
    *oy = py > 127 ? 127 : (py < -127 ? -127 : (int8_t)py);
    return true;
}

report_mouse_t pointing_device_task_user(report_mouse_t r) {
    bool scroll = keyball_get_scroll_mode();

    // モードが変わったら滑走は打ち切る。x/y と h/v をまたいで滑らせない
    static bool last_scroll = false;
    if (scroll != last_scroll) {
        last_scroll = scroll;
        inrt_stop_all();
    }

    // ボタンを押している間は滑らせない。ドラッグ中に勝手に動くと困る
    if (r.buttons) {
        inrt_stop_all();
        return r;
    }

    // ★ 8ms のコマ送り。動きがあったコマは必ず処理し、無かったコマは
    //   前回から 8ms 経ってから処理する。
    //   この関数は 1ms ごと（POINTING_DEVICE_TASK_THROTTLE_MS の既定）に呼ばれるが、
    //   keyball はセンサ値を 8ms に1回しか出さない（KEYBALL_REPORTMOUSE_INTERVAL）。
    //   その合間の「値が0のコマ」を指を離したと誤認すると、
    //   ボールを動かしている最中に滑り出してしまう。
    uint32_t now    = timer_read32();
    bool     moving = scroll ? (r.h != 0 || r.v != 0) : (r.x != 0 || r.y != 0);
    if (!moving && TIMER_DIFF_32(now, inrt_last) < INRT_TICK_MS) {
        return r;
    }
    inrt_last = now;

    int8_t ox = 0, oy = 0;

    if (scroll) {
        // 段が出なかったコマも割合に入れる（inrt_rate のコメント参照）。
        // 滑走中は自分の出した値を食べ直さないよう更新しない。
        if (!inrt_scrl.coasting) {
            inrt_scrl.vx = inrt_rate(inrt_scrl.vx, r.h);
            inrt_scrl.vy = inrt_rate(inrt_scrl.vy, r.v);
        }
        if (moving) {
            inrt_scrl.coasting = false;
            inrt_scrl.rx = inrt_scrl.ry = 0;
            return r;
        }
        if (inrt_step(&inrt_scrl, INRT_SCRL_MIN, INRT_SCRL_STOP, &ox, &oy)) {
            r.h = ox;
            r.v = oy;
        }
        return r;
    }

    if (moving) {
        inrt_track(&inrt_move, r.x, r.y);
        return r;
    }
    if (inrt_step(&inrt_move, INRT_MOVE_MIN, INRT_MOVE_STOP, &ox, &oy)) {
        r.x = ox;
        r.y = oy;
    }
    return r;
}

// キーを押したら滑走は即やめる。惰性で動いている最中に打ち始めたときに
// カーソルが逃げていくのを防ぐ。
// QK_USER_0 は慣性のON/OFF。Remapでは "User 0" として割り当てられる。
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        if (keycode == QK_USER_0) {
            inrt_enabled = !inrt_enabled;
            inrt_stop_all();
            return false;
        }
        inrt_stop_all();
    }
    return true;
}

#endif  // POINTING_DEVICE_ENABLE
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
