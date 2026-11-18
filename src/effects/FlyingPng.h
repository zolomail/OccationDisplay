#pragma once

#include "Effect.h"
#include <vector>
#include <cstdint>

class FlyingPng : public Effect
{
public:
    // Construct from a LittleFS file path (e.g. "/bitmaps/ufo.png" or "/fullscreen/golfer.png")
    // With optional initial position (XPOS_CENTER, etc.), velocities (dx, dy in 1/16th pixels),
    // and transparency control (pass false to render fully opaque, avoiding the need for ClearScreen).
    // Construct from a LittleFS file path (e.g. "/bitmaps/ufo.png" or "/fullscreen/golfer.png")
    // With optional initial position (XPOS_CENTER, etc.), velocities (dx, dy in 1/16th pixels),
    // transparency control (use_transparency), and static idle (idles after painting front+back double buffers).
    FlyingPng(
        Adafruit_GFX &matrix,
        const char *filepath,
        int16_t x = XPOS_CENTER,
        int16_t y = YPOS_CENTER,
        int16_t dx = 0,
        int16_t dy = 0,
        bool use_transparency = true,
        bool static_idle = false
    );

    // Construct from an in-memory PNG byte buffer (e.g. PROGMEM array)
    FlyingPng(
        Adafruit_GFX &matrix,
        const uint8_t *png_data,
        size_t png_size,
        int16_t x = XPOS_CENTER,
        int16_t y = YPOS_CENTER,
        int16_t dx = 0,
        int16_t dy = 0,
        bool use_transparency = true,
        bool static_idle = false
    );

    ~FlyingPng() override = default;

    void render() override;

    bool isValid() const { return m_valid; }
    int16_t width() const { return m_width; }
    int16_t height() const { return m_height; }
    bool usesTransparency() const { return m_use_transparency; }
    void setUseTransparency(bool enable) { m_use_transparency = enable; }
    bool isStaticIdle() const { return m_static_idle; }
    void setStaticIdle(bool enable) { m_static_idle = enable; }

    // Direct static helpers to decode & draw a PNG onto any Adafruit_GFX display at (x, y)
    static bool draw(Adafruit_GFX &matrix, const char *filepath, int16_t x, int16_t y, bool use_transparency = true);
    static bool draw(Adafruit_GFX &matrix, const uint8_t *png_data, size_t png_size, int16_t x, int16_t y, bool use_transparency = true);

private:
    bool decodeFromFs(const char *filepath);
    bool decodeFromRam(const uint8_t *data, size_t size);

    int16_t m_x{0};
    int16_t m_y{0};
    int16_t m_dx{0};
    int16_t m_dy{0};
    int16_t m_width{0};
    int16_t m_height{0};
    bool m_valid{false};
    bool m_has_alpha{false};
    bool m_use_transparency{true};
    bool m_static_idle{false};
    uint8_t m_frames_rendered{0};

    std::vector<uint16_t> m_rgb;
    std::vector<uint8_t>  m_mask;
};

// Alias for backwards compatibility
using PngEffect = FlyingPng;
