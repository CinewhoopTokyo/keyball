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
#    define KBSTAT_VERSION 8   // 8: スクロールを均してから渡す（掴み検出の誤爆を修正）

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

#    define INRT_SHIFT   8
#    define INRT_ONE     (1L << INRT_SHIFT)
#    define INRT_TICK_MS KEYBALL_REPORTMOUSE_INTERVAL  // 8ms

// 0xB6 で書き換えられる。既定値は実測に基づく。
static uint8_t inrt_peak_min   = 6;   // この速度を超えたら慣性を許可（実測: 小移動2 / フリック6〜17）
static uint8_t inrt_kick       = 250; // 引き継ぐ割合 /256。256に近いほど段差が消える
static uint8_t inrt_decay      = 238; // 減衰 /256。238≒0.930
static uint8_t inrt_scrl_peak  = 25;  // スクロール: 0.25段/コマ
static uint8_t inrt_scrl_kick  = 250;
static uint8_t inrt_scrl_decay = 250;  // 250≒0.977。246だと約7段と物足りなかった
static bool    inrt_enabled    = true;

// 掴んで止めたと判定する、直前のボール速度。これ未満から0になっただけなら
// 自然に回り終わっただけとみなす。
#    define INRT_GRAB      (3 * INRT_ONE)

typedef struct {
    int32_t cx, cy;    // 慣性の速度。これを出す
    int32_t rx, ry;    // 整数に落とし切れなかった端数
    int32_t lastMag;   // 直前のボール速度の大きさ
    bool    armed;     // 十分速く動かされた＝慣性を出してよい
} inrt_t;

static inrt_t inrt_move, inrt_scrl;
static uint32_t inrt_last = 0;

// スクロールを均すための入れ物。理由は使う場所のコメント参照。
static int32_t inrt_scrl_rate_x = 0, inrt_scrl_rate_y = 0;

static void inrt_reset(inrt_t *s) {
    s->cx = s->cy = s->rx = s->ry = s->lastMag = 0;
    s->armed = false;
}

bool keyball_inertia_is_enabled(void) { return inrt_enabled; }

static void inrt_stop_all(void) {
    inrt_reset(&inrt_move);
    inrt_reset(&inrt_scrl);
    inrt_scrl_rate_x = inrt_scrl_rate_y = 0;
}

/// 1コマ分の処理。ボールの値 (bx,by) を受け取り、出す値を (*ox,*oy) に返す。
/// 戻り値 true なら慣性が上書きした。
///
/// ★ 第2版までは「ボールが完全に止まってから滑り出す」方式で、2つ問題があった。
///   ① 止まった瞬間に速度が 1 から 9 へ跳ね、カクッと段差ができる
///   ② 滑走中はボールが止まっているので、指を置いても信号が出ず、止められない
///   → ボールの空転と慣性を重ね、大きい方を出す方式に変えた。
///     段差が消え、滑走中もボールが回っているので掴めば急停止として検出できる。
static bool inrt_run(inrt_t *s, int32_t bx, int32_t by,
                     int32_t arm_min, uint8_t kick, uint8_t decay,
                     int32_t stop_at, int32_t grab_at, int8_t *ox, int8_t *oy) {
    int32_t bmag = labs(bx) + labs(by);
    int32_t cmag = labs(s->cx) + labs(s->cy);

    if (bmag >= cmag) {
        // ボールの方が速い＝まだ回されている。慣性の速度をボールに合わせる。
        s->cx = (bx * kick) >> INRT_SHIFT;
        s->cy = (by * kick) >> INRT_SHIFT;
        s->rx = s->ry = 0;
        if (bmag >= arm_min) s->armed = true;
        s->lastMag = bmag;
        return false;                     // ボールの値をそのまま通す
    }

    if (!inrt_enabled || !s->armed) {
        inrt_reset(s);
        return false;
    }
    // ★ 掴んで止められた。回っていたボールが急に0になったら指が触れたとみなす。
    //   自然に回り終わる場合は 3→2→1→0 と落ちるので lastMag が小さく、ここに来ない。
    if (bmag == 0 && s->lastMag >= grab_at) {
        inrt_reset(s);
        return false;
    }
    s->lastMag = bmag;

    s->cx = (s->cx * decay) >> INRT_SHIFT;
    s->cy = (s->cy * decay) >> INRT_SHIFT;
    if (labs(s->cx) + labs(s->cy) < stop_at) {
        inrt_reset(s);
        return false;
    }
    s->rx += s->cx;
    s->ry += s->cy;
    // 負数の算術右シフトは床関数。端数を引き戻すので誤差は溜まらない。
    int32_t px = s->rx >> INRT_SHIFT;
    int32_t py = s->ry >> INRT_SHIFT;
    s->rx -= px << INRT_SHIFT;
    s->ry -= py << INRT_SHIFT;
    *ox = px > 127 ? 127 : (px < -127 ? -127 : (int8_t)px);
    *oy = py > 127 ? 127 : (py < -127 ? -127 : (int8_t)py);
    return true;
}

report_mouse_t pointing_device_task_user(report_mouse_t r) {
    bool scroll = keyball_get_scroll_mode();

    static bool last_scroll = false;
    if (scroll != last_scroll) {   // x/y と h/v をまたいで滑らせない
        last_scroll = scroll;
        inrt_stop_all();
    }
    if (r.buttons) {               // ドラッグ中に勝手に動くと困る
        inrt_stop_all();
        return r;
    }

    // 8ms のコマ送り。keyball はセンサ値を 8ms に1回しか出さないので、
    // その合間の「値が0のコマ」を数えると空転を見誤る。
    uint32_t now    = timer_read32();
    bool     moving = scroll ? (r.h != 0 || r.v != 0) : (r.x != 0 || r.y != 0);
    if (!moving && TIMER_DIFF_32(now, inrt_last) < INRT_TICK_MS) {
        return r;
    }
    inrt_last = now;

    int8_t ox = 0, oy = 0;
    if (scroll) {
        // ★ 分周後の値は 0 か 1 しか出ない。飛び飛びなので、そのまま速度として
        //   渡すと「段が出ないコマ」が毎回“急に0になった＝掴まれた”に見えて、
        //   滑走が始まった次のコマで必ず打ち切られる。実際そうなっていた。
        //   「1コマあたり何段出たか」に均してから渡す。0のコマも入れるのが肝で、
        //   出たコマだけ平均するとゆっくり回しても弾いても同じ 1 になる。
        //   時定数は16コマ≒128ms。
        inrt_scrl_rate_x = (inrt_scrl_rate_x * 15 + ((int32_t)r.h << INRT_SHIFT)) / 16;
        inrt_scrl_rate_y = (inrt_scrl_rate_y * 15 + ((int32_t)r.v << INRT_SHIFT)) / 16;
        // 均した値は急に0にならないので、掴み検出は使えない（しきい値を届かない値に）。
        // スクロールの滑走は0.5秒ほどで自然に止まる。途中で止めたいならキーを押す。
        if (inrt_run(&inrt_scrl, inrt_scrl_rate_x, inrt_scrl_rate_y,
                     ((int32_t)inrt_scrl_peak * INRT_ONE) / 100,
                     inrt_scrl_kick, inrt_scrl_decay,
                     INRT_ONE / 16, 0x7FFFFFFFL, &ox, &oy)) {
            r.h = ox;
            r.v = oy;
        }
        return r;
    }
    if (inrt_run(&inrt_move, (int32_t)r.x << INRT_SHIFT, (int32_t)r.y << INRT_SHIFT,
                 (int32_t)inrt_peak_min * INRT_ONE,
                 inrt_kick, inrt_decay,
                 INRT_ONE / 2, INRT_GRAB, &ox, &oy)) {
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
