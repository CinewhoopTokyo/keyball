/*
This is the c configuration file for the keymap

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

#pragma once

#ifdef RGBLIGHT_ENABLE
//#    define RGBLIGHT_EFFECT_BREATHING
//#    define RGBLIGHT_EFFECT_RAINBOW_MOOD
//#    define RGBLIGHT_EFFECT_RAINBOW_SWIRL
//#    define RGBLIGHT_EFFECT_SNAKE
//#    define RGBLIGHT_EFFECT_KNIGHT
//#    define RGBLIGHT_EFFECT_CHRISTMAS
#    define RGBLIGHT_EFFECT_STATIC_GRADIENT
//#    define RGBLIGHT_EFFECT_RGB_TEST
//#    define RGBLIGHT_EFFECT_ALTERNATING
//#    define RGBLIGHT_EFFECT_TWINKLE
#endif

#define TAP_CODE_DELAY 5

#define POINTING_DEVICE_AUTO_MOUSE_ENABLE
#define AUTO_MOUSE_DEFAULT_LAYER 1

// --- タップ/ホールド判定の調整 ---------------------------------------------
// TAPPING_TERM: タップ/ホールドを分ける時間[ms]。大きいほど「タップ」と判定されやすい
#define TAPPING_TERM 220
// PERMISSIVE_HOLD: ホールド中に他キーをタップしたら即ホールド確定
//                  （ホームローmodを確実に効かせる）
#define PERMISSIVE_HOLD
// RETRO_TAPPING: ホールド時間を超えても、他キーを押さずに離せばタップを送る
//                （英数/かな が「押しが長引いて出ない」問題への直接対策）
#define RETRO_TAPPING

// --- センサーの実装角度の補正 -----------------------------------------------
// 低背化のため PMW3360 を基板上で -90° 回して実装し、さらに B.Cu 側に載せて
// レンズが窓を貫通してボールを見る構成にしてある。回転に加えてミラーが1回
// 入るため、幾何から素直に予想した 90 ではなく 270 が正解になる。
//   実測: ボール右→カーソル上 / ボール上→カーソル左  ⇒ ROTATE_270 で一致
// move と scroll の両方に同じ回転が掛かる（lib/keyball/keyball.c）。
#define KEYBALL_SENSOR_ROTATE_270
