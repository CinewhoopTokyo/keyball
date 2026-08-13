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
#    define INRT_CMD       0xB6   // 慣性の調整値の読み書き
#    define KBSTAT_VERSION 6   // 5: 再発動を修正  6: スクロールを別パラメータに分離

#    ifdef POINTING_DEVICE_ENABLE
bool keyball_inertia_is_enabled(void);            // 実体は下の「トラックボールの慣性」ブロック
void keyball_inertia_command(uint8_t *d, uint8_t len);
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
#    ifdef POINTING_DEVICE_ENABLE
    if (length > 0 && data[0] == INRT_CMD) {
        keyball_inertia_command(data, length);
        raw_hid_send(data, length);
        return true;
    }
#    endif
    return false;
}
#endif  // RAW_ENABLE && VIA_ENABLE
// --------------------------------------------------------------------------

// --- トラックボールの慣性 --------------------------------------------------
//
// 弾いたあと滑る。ゆっくり動かしているときは滑らないので、精密操作は据え置き。
//
// ★ 第1版は「動きが止まった瞬間の速度」で判定していたが、これは間違いだった。
//   実測すると、指を離したあともボールが物理的に空転して、
//   センサは 13 → … → 2 → 1 → 0 となだらかに減速した値を出し続ける。
//   止まった瞬間の速度はほぼ 1 なので発動しない。
//   逆に指で急に押さえて止めるとその瞬間の速度が高く、狙いと真逆に滑ってしまう。
//   → 「直近のピーク速度」を覚えておき、そこから滑らせる方式に変えた。
//
// 実測値（自作Choc基板・CPI700・keyball-speed）
//   中央値 5 / 上位10% 11 / 最大 13
//   → ピークのしきい値は 9 前後が妥当。第1版の 18 は一度も届いていなかった。
//
// なぜ pointing_device_task_user() を使うか
//   keyball_on_apply_motion_to_mouse_move() は「動きがあるとき」の変換用で、
//   しかも自分側/相手側で2回呼ばれてレポートを上書きし合う。
//   慣性は「動きが無いtick」にも値を出す必要があるので使えない。
//
// なぜ固定小数点か
//   ATmega32U4 に FPU が無い。速度を 1/256 単位の整数で持つ。
//
// 調整用のコマンド 0xB6 を用意してある。焼き直さずに数値を変えられる。
//   ClaudeCode/keyball/tools の keyball-inertia を使う。

#ifdef POINTING_DEVICE_ENABLE

#    include <stdlib.h>   // labs()。quantum.h 経由で入る保証が無いので明示する

#    define INRT_SHIFT   8                  // 1.0 = 256
#    define INRT_ONE     (1L << INRT_SHIFT)
#    define INRT_TICK_MS KEYBALL_REPORTMOUSE_INTERVAL  // 8ms

// ここから下は 0xB6 で書き換えられる。既定値は実測に基づく。
static uint8_t inrt_peak_min  = 6;    // ポインタ: このピーク速度以上で滑らせる（実測: 小移動2 / フリック6〜17）
static uint8_t inrt_kick      = 140;  // ピークの何割から滑り出すか（/256）0.55
static uint8_t inrt_decay     = 240;  // 減衰 /256。240≒0.938。246でもまだ滑りすぎだった
static uint8_t inrt_scrl_peak = 25;   // スクロール: ピーク 0.25段/tick 以上
// ★ スクロールはポインタと桁が違う。分周後の速度は 0.3〜0.5段/コマしかない。
//   ポインタと同じ kick/decay/停止しきい値を使うと、滑り出す前に
//   停止条件（0.5）を満たしてしまい、一度も動かない。実際そうなっていた。
static uint8_t inrt_scrl_kick  = 220; // スクロールの滑り出し /256
static uint8_t inrt_scrl_decay = 249; // スクロールの減衰 /256
static bool    inrt_enabled   = true;

// ピークを忘れる速さ。指を離してから空転が終わるまで覚えていられる長さにする。
#    define INRT_PEAK_DECAY 252   // /256 ≒ 0.984。300msで約1/100

typedef struct {
    int32_t vx, vy;      // いまの速度
    int32_t px, py;      // ピーク時の速度（向きも保つ）
    int32_t peak;        // ピークの大きさ |px|+|py|
    int32_t rx, ry;      // 整数に落とし切れなかった端数
    bool    coasting;
} inrt_t;

static inrt_t   inrt_move, inrt_scrl;
static uint32_t inrt_last = 0;

static void inrt_reset(inrt_t *s) {
    s->vx = s->vy = s->px = s->py = s->peak = s->rx = s->ry = 0;
    s->coasting = false;
}

bool keyball_inertia_is_enabled(void) {
    return inrt_enabled;
}

static void inrt_stop_all(void) {
    inrt_reset(&inrt_move);
    inrt_reset(&inrt_scrl);
}

/// ピークを更新し、少しずつ忘れる。
/// 「止まった瞬間」ではなく「直近で一番速かったとき」を基準にするための記憶。
static void inrt_peak_update(inrt_t *s) {
    int32_t mag = labs(s->vx) + labs(s->vy);
    if (mag > s->peak) {
        s->peak = mag;
        s->px   = s->vx;
        s->py   = s->vy;
    } else {
        s->peak = (s->peak * INRT_PEAK_DECAY) >> INRT_SHIFT;
        s->px   = (s->px * INRT_PEAK_DECAY) >> INRT_SHIFT;
        s->py   = (s->py * INRT_PEAK_DECAY) >> INRT_SHIFT;
    }
}

/// ポインタ用。重み1/2。減速にも素早く追従する。
static void inrt_track(inrt_t *s, int16_t dx, int16_t dy) {
    // ★ 滑走中にボールが動いた＝利用者が掴んで止めにきた。
    //   ここでピークごと捨てないと、手を離した瞬間に同じピークで
    //   また滑り出して「止めても止まらない」ことになる。実際そうなった。
    if (s->coasting) {
        inrt_reset(s);
    }
    s->vx = (s->vx + ((int32_t)dx << INRT_SHIFT)) / 2;
    s->vy = (s->vy + ((int32_t)dy << INRT_SHIFT)) / 2;
    inrt_peak_update(s);
    s->coasting = false;
    s->rx = s->ry = 0;
}

/// スクロール用。「1コマあたり何段出たか」の割合。
/// ★ 段が出なかったコマ（0）も必ず入れること。分周後は0か1しか出ないので、
///   出たコマだけ平均するとゆっくり回しても弾いても同じ1になる。
static void inrt_rate(inrt_t *s, int8_t h, int8_t v) {
    if (s->coasting && (h != 0 || v != 0)) {
        inrt_reset(s);   // 滑走中に触られたら捨てる（inrt_track と同じ理由）
    }
    s->vx = (s->vx * 15 + ((int32_t)h << INRT_SHIFT)) / 16;
    s->vy = (s->vy * 15 + ((int32_t)v << INRT_SHIFT)) / 16;
    inrt_peak_update(s);
}

/// 滑走を1コマ進める。出す量を *ox,*oy に返す。まだ滑るなら true。
static bool inrt_step(inrt_t *s, int32_t peak_min, uint8_t kick, uint8_t decay,
                      int32_t stop_at, int8_t *ox, int8_t *oy) {
    if (!s->coasting) {
        // ★ 止まった瞬間の速度ではなくピークで判定する。
        //   空転で 1 まで落ちていても、直前に速ければ滑らせる。
        if (!inrt_enabled || s->peak < peak_min) {
            inrt_reset(s);
            return false;
        }
        // ピークの何割かから始める。ボールの空転が終わったところを引き継ぐので、
        // ピークそのままだと速すぎて跳ねて見える。
        s->vx = (s->px * kick) >> INRT_SHIFT;
        s->vy = (s->py * kick) >> INRT_SHIFT;
        s->rx = s->ry = 0;
        s->coasting = true;
        // ピークは使い切る。1回の弾きで滑るのは1回だけ。
        s->peak = 0;
        s->px = s->py = 0;
    }
    s->vx = (s->vx * decay) >> INRT_SHIFT;
    s->vy = (s->vy * decay) >> INRT_SHIFT;
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
    //   この関数は 1ms ごとに呼ばれるが、keyball はセンサ値を 8ms に1回しか
    //   出さない。その合間の「値が0のコマ」を指を離したと誤認すると、
    //   ボールを動かしている最中に滑り出してしまう。
    uint32_t now    = timer_read32();
    bool     moving = scroll ? (r.h != 0 || r.v != 0) : (r.x != 0 || r.y != 0);
    if (!moving && TIMER_DIFF_32(now, inrt_last) < INRT_TICK_MS) {
        return r;
    }
    inrt_last = now;

    int8_t ox = 0, oy = 0;

    if (scroll) {
        // 滑走中は自分の出した値を食べ直さないよう更新しない
        if (!inrt_scrl.coasting) {
            inrt_rate(&inrt_scrl, r.h, r.v);
        }
        if (moving) {
            inrt_scrl.coasting = false;
            inrt_scrl.rx = inrt_scrl.ry = 0;
            return r;
        }
        // 停止しきい値もスクロール用に下げる。ここが 0.5 のままだと一度も動かない。
        if (inrt_step(&inrt_scrl, ((int32_t)inrt_scrl_peak * INRT_ONE) / 100,
                      inrt_scrl_kick, inrt_scrl_decay, INRT_ONE / 16, &ox, &oy)) {
            r.h = ox;
            r.v = oy;
        }
        return r;
    }

    if (moving) {
        inrt_track(&inrt_move, r.x, r.y);
        return r;
    }
    if (inrt_step(&inrt_move, (int32_t)inrt_peak_min * INRT_ONE,
                  inrt_kick, inrt_decay, INRT_ONE / 2, &ox, &oy)) {
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

/// 0xB6 で調整値を読み書きする。実機でしか詰められない値なので、
/// 焼き直さずに変えられるようにしてある。RAMのみ＝電源を切ると既定に戻る。
///   要求: B6 00  → 現在値を返す
///        B6 01 <peak> <kick> <decay> <scrlPeak> <enabled> <scrlKick> <scrlDecay>
///        （0 は「変えない」の意味）
/// 返事: B6 <peak> <kick> <decay> <scrlPeak> <enabled> <scrlKick> <scrlDecay>
void keyball_inertia_command(uint8_t *d, uint8_t len) {
    if (len >= 7 && d[1] == 1) {
        if (d[2] > 0) inrt_peak_min = d[2];
        if (d[3] > 0) inrt_kick     = d[3];
        if (d[4] > 0) inrt_decay    = d[4] < 255 ? d[4] : 254;  // 255だと減衰しない
        if (d[5] > 0) inrt_scrl_peak = d[5];
        inrt_enabled = d[6] != 0;
        if (len >= 9) {
            if (d[7] > 0) inrt_scrl_kick  = d[7];
            if (d[8] > 0) inrt_scrl_decay = d[8] < 255 ? d[8] : 254;
        }
        inrt_stop_all();
    }
    d[0] = 0xB6;
    d[1] = inrt_peak_min;
    d[2] = inrt_kick;
    d[3] = inrt_decay;
    d[4] = inrt_scrl_peak;
    d[5] = inrt_enabled ? 1 : 0;
    d[6] = inrt_scrl_kick;
    d[7] = inrt_scrl_decay;
    for (uint8_t i = 8; i < len; i++) d[i] = 0;
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
