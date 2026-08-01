// clang-format off
// カスタムRGBエフェクト: GLOW_REACTIVE
//   ベース輝度で常時点灯しつつ、押鍵したキーとその周辺を明るくする。
//   標準の SOLID_REACTIVE_* は押鍵時のみ点灯するため自作。
RGB_MATRIX_EFFECT(GLOW_REACTIVE)

#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

// 常時点灯のベース輝度（0-255）。ここを上げると普段が明るくなる
#ifndef GLOW_BASE_VAL
#    define GLOW_BASE_VAL 25
#endif
// 押鍵時の輝度上乗せ量
#ifndef GLOW_HIT_BOOST
#    define GLOW_HIT_BOOST 200
#endif
// 光が広がる距離（大きいほど広範囲が反応）
#ifndef GLOW_SPREAD
#    define GLOW_SPREAD 60
#endif

static bool GLOW_REACTIVE(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    HSV hsv = rgb_matrix_config.hsv;
    // ベース輝度は設定値と GLOW_BASE_VAL の小さい方を採用
    uint8_t base = hsv.v < GLOW_BASE_VAL ? hsv.v : GLOW_BASE_VAL;

    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();

        uint16_t boost = 0;
#ifdef RGB_MATRIX_KEYREACTIVE_ENABLED
        // 直近の押鍵からの距離と経過時間で明るさを決める
        for (uint8_t j = 0; j < g_last_hit_tracker.count; j++) {
            int16_t dx = g_led_config.point[i].x - g_last_hit_tracker.x[j];
            int16_t dy = g_led_config.point[i].y - g_last_hit_tracker.y[j];
            uint8_t dist = sqrt16(dx * dx + dy * dy);
            if (dist > GLOW_SPREAD) continue;

            // 経過時間による減衰（rgb_matrix_config.speed が大きいほど速く消える）
            uint16_t tick = scale16by8(g_last_hit_tracker.tick[j], rgb_matrix_config.speed);
            if (tick > 255) continue;

            uint16_t fade_time = 255 - tick;                  // 時間減衰
            uint16_t fade_dist = 255 - (dist * 255 / GLOW_SPREAD); // 距離減衰
            uint16_t add = (uint32_t)GLOW_HIT_BOOST * fade_time / 255 * fade_dist / 255;
            if (add > boost) boost = add;                     // 最も強い反応を採用
        }
#endif

        uint16_t v = base + boost;
        hsv.v = v > 255 ? 255 : (uint8_t)v;
        RGB rgb = rgb_matrix_hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
