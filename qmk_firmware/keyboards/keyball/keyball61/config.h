/*
Copyright 2021 @Yowkees
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

// Key matrix parameters (Keyball61 is duplex matrix)
#define MATRIX_ROWS         (5 * 2)  // split keyboard
#define MATRIX_COLS         (4 * 2)  // duplex matrix
#define MATRIX_ROW_PINS     { D4, C6, D7, E6, B4 }
#define MATRIX_COL_PINS     { F4, F5, F6, F7 }
#define MATRIX_MASKED
#define DEBOUNCE            5

// Split parameters
#define SOFT_SERIAL_PIN         D2
#define SPLIT_HAND_MATRIX_GRID  F7, D7
#define SPLIT_USB_DETECT
#ifdef OLED_ENABLE
#    define SPLIT_OLED_ENABLE
#endif

// If your PC does not recognize Keyball, try setting this macro. This macro
// increases the firmware size by 200 bytes, so it is disabled by default, but
// it has been reported to work well in such cases.
//#define SPLIT_WATCHDOG_ENABLE

#define SPLIT_TRANSACTION_IDS_KB KEYBALL_GET_INFO, KEYBALL_GET_MOTION, KEYBALL_SET_CPI

// RGB LED settings
#define WS2812_DI_PIN       D3
#ifdef RGBLIGHT_ENABLE
#    define RGBLED_NUM      74
#    define RGBLED_SPLIT    { 37, 37 }
#    ifndef RGBLIGHT_LIMIT_VAL
#        define RGBLIGHT_LIMIT_VAL  120 // limitated for power consumption
#    endif
#    ifndef RGBLIGHT_VAL_STEP
#        define RGBLIGHT_VAL_STEP   12
#    endif
#    ifndef RGBLIGHT_HUE_STEP
#        define RGBLIGHT_HUE_STEP   17
#    endif
#    ifndef RGBLIGHT_SAT_STEP
#        define RGBLIGHT_SAT_STEP   17
#    endif
#endif
#ifdef RGB_MATRIX_ENABLE
#    define RGB_MATRIX_SPLIT        { 37, 37 }
#    define RGB_MATRIX_LED_COUNT    74
#    define SPLIT_TRANSPORT_MIRROR

// 電力制限（USBバスパワー保護）
#    define RGB_MATRIX_MAXIMUM_BRIGHTNESS 120

// 押鍵反応を使うために必要
#    define RGB_MATRIX_KEYREACTIVE_ENABLED
#    define RGB_MATRIX_KEYPRESSES

// 標準エフェクトは全て無効（サイズ節約）。カスタムのみ使う
#    define RGB_MATRIX_CUSTOM_USER

// カスタムエフェクト GLOW_REACTIVE のパラメータ
#    define GLOW_BASE_VAL   25    // 常時点灯の明るさ
#    define GLOW_HIT_BOOST  200   // 押鍵時の上乗せ
#    define GLOW_SPREAD     60    // 光が広がる距離

// 起動時の設定
#    define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_CUSTOM_GLOW_REACTIVE
#    define RGB_MATRIX_DEFAULT_HUE  170   // 青系
#    define RGB_MATRIX_DEFAULT_SAT  255
#    define RGB_MATRIX_DEFAULT_VAL  120   // 押鍵時の最大輝度
#    define RGB_MATRIX_DEFAULT_SPD  90    // 減衰速度（大きいほど速く戻る）

// 無操作での消灯はしない（常時点灯させるため）
#endif

#ifndef OLED_FONT_H
#    define OLED_FONT_H "keyboards/keyball/lib/logofont/logofont.c"
#    define OLED_FONT_START 32
#    define OLED_FONT_END 195
#endif

#if !defined(LAYER_STATE_8BIT) && !defined(LAYER_STATE_16BIT) && !defined(LAYER_STATE_32BIT)
#    define LAYER_STATE_8BIT
#endif

// To squeeze firmware size
#undef LOCKING_SUPPORT_ENABLE
#undef LOCKING_RESYNC_ENABLE
