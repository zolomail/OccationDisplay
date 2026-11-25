#pragma once

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

struct PanelCalibration {
    uint8_t r{100};
    uint8_t g{100};
    uint8_t b{100};
};

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

    // Fast row-buffered direct DMA blitter for 16-bit RGB565 bitmaps with 8-bit per-pixel alpha (0..255).
    static void drawRGBABitmap(
        MatrixPanel_I2S_DMA &panel,
        int16_t x,
        int16_t y,
        const uint16_t *bitmap,
        const uint8_t *alpha,
        int16_t w,
        int16_t h
    );

    // Apply per-panel R/G/B gain (0..100%) in-place on the DMA back buffer.
    // Skips any panel where r == 100 && g == 100 && b == 100.
    static void applyPanelCalibration(
        MatrixPanel_I2S_DMA &panel,
        const PanelCalibration *cal,
        size_t panel_count,
        uint16_t panel_res_x = 64
    );
};
