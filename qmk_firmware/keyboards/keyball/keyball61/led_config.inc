// clang-format off
// Keyball61 RGB Matrix LED config
// 自動生成: KiCad設計データ(LED/スイッチ座標)より
// LED実装数 左37 右34 = 71 (RGBLED_NUM=74, 右手末尾は未実装)

led_config_t g_led_config = {
    // 1. キーマトリクス位置 → LED index
    {
        {  24,  19,  14, NO_LED,   9,   5,   1, NO_LED },
        {  25,  20,  15, NO_LED,  10,   6,   2, NO_LED },
        {  26,  21,  16, NO_LED,  11,   7,   3, NO_LED },
        {  27,  22,  17, NO_LED,  12,   8,   4,   0 },
        {  28,  23,  18, NO_LED,  13, NO_LED, NO_LED, NO_LED },
        {  44,  49,  54, NO_LED,  58,  62,  66, NO_LED },
        {  45,  50,  55, NO_LED,  59,  63,  67, NO_LED },
        {  46,  51,  56, NO_LED,  60,  64,  68, NO_LED },
        {  47,  52,  57, NO_LED,  61,  65,  69,  70 },
        {  48,  53, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
    },
    // 2. 各LEDの物理座標 {x, y}
    {
        {  0,47}, { 16, 3}, { 16,17}, { 16,31}, { 16,45}, { 33, 1},
        { 33,15}, { 33,29}, { 33,43}, { 49, 0}, { 49,13}, { 49,27},
        { 49,41}, { 49,55}, { 66, 3}, { 66,17}, { 66,31}, { 66,45},
        { 66,58}, { 83, 8}, { 83,22}, { 83,36}, { 83,50}, { 83,64},
        { 99, 8}, { 99,22}, { 99,36}, { 99,50}, { 99,64}, { 38, 5},
        { 74, 6}, {100,26}, { 99,53}, { 66,48}, { 34,48}, { 19,53},
        {  1,53}, {126,53}, {148,44}, {165,43}, {223,52}, {224,26},
        {197, 6}, {163, 5}, {223, 8}, {223,22}, {223,36}, {223,50},
        {223,64}, {207, 8}, {207,22}, {207,36}, {207,50}, {207,64},
        {190, 3}, {190,17}, {190,31}, {190,45}, {173, 0}, {173,13},
        {173,27}, {173,41}, {157, 1}, {157,15}, {157,29}, {157,43},
        {140, 3}, {140,17}, {140,31}, {140,45}, {124,47}, {112,32},
        {112,32}, {112,32},
    },
    // 3. 各LEDのフラグ
    //    LED_FLAG_KEYLIGHT=4: キー直下(反応する)
    //    LED_FLAG_INDICATOR=1: アンダーグロー(常時点灯)
    //    LED_FLAG_NONE=0: 未実装キー(消灯)
    {
        LED_FLAG_NONE, LED_FLAG_NONE, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_NONE, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_NONE, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_NONE, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_NONE,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_NONE,
        LED_FLAG_NONE, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_NONE, LED_FLAG_INDICATOR, LED_FLAG_INDICATOR, LED_FLAG_INDICATOR,
        LED_FLAG_INDICATOR, LED_FLAG_INDICATOR, LED_FLAG_INDICATOR, LED_FLAG_INDICATOR,
        LED_FLAG_INDICATOR, LED_FLAG_INDICATOR, LED_FLAG_INDICATOR, LED_FLAG_INDICATOR,
        LED_FLAG_INDICATOR, LED_FLAG_INDICATOR, LED_FLAG_INDICATOR, LED_FLAG_INDICATOR,
        LED_FLAG_NONE, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_NONE, LED_FLAG_NONE, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_NONE, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_NONE, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_NONE, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_NONE, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_NONE, LED_FLAG_NONE,
        LED_FLAG_NONE, LED_FLAG_NONE,
    }
};
// clang-format on
