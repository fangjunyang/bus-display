/*
 * hzk16.h
 * Placeholder header with STOP_BITMAPS used by main.c
 * The arrays below are placeholders (all zeros) to allow compilation and testing of framework.
 * Replace these with real 16x16 bitmaps (32 bytes per char) generated from a HZK16 file.
 * Use tools/hzk16_convert.py to convert an hzk16 binary into this header with only required characters.
 */

#ifndef HZK16_H
#define HZK16_H

// number of stop bitmaps (we allocate 16 for future extension)
#define STOP_COUNT 16

// Each bitmap is 32 bytes (16 rows x 16 columns packed). Format: for each column: top 8 bits, bottom 8 bits (2 bytes per column)
const unsigned char STOP_BITMAPS[STOP_COUNT][32] = {
    // 0: 起点站 (placeholder)
    {0},
    // 1: 人民广场
    {0},
    // 2: 文化宫
    {0},
    // 3: 火车站
    {0},
    // 4: 汽车站
    {0},
    // 5: 人民医院
    {0},
    // 6: 市政府
    {0},
    // 7: 终点站
    {0},
    // 8: 科技馆
    {0},
    // 9: 图书馆
    {0},
    // 10: 体育中心
    {0},
    // 11: 商业街
    {0},
    // 12: 大学城
    {0},
    // 13: 森林公园
    {0},
    // 14: (reserved)
    {0},
    // 15: (reserved)
    {0}
};

#endif
