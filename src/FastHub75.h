#pragma once

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

class FastHub75
{
public:
    // Fast row-buffered direct DMA blitter for 16-bit RGB565 bitmaps.
    // Pre-resolves DMA row bitplane pointers once per row, eliminating per-pixel virtual calls, bounds checks,
    // and redundant row structures. Supports both opaque images and 1-bit transparency masks with 2D clipping.
    static void drawRGBBitmap(
        MatrixPanel_I2S_DMA &panel,
        int16_t x,
        int16_t y,
        const uint16_t *bitmap,
        const uint8_t *mask,
        int16_t w,
        int16_t h,
        bool use_transparency
    );
};
