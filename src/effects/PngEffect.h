#pragma once

#include "Effect.h"
#include <vector>
#include <cstdint>

class PngEffect : public Effect
{
public:
    // Construct from a LittleFS file path (e.g. "/bitmaps/ufo.png" or "/weather/sunny_day.png")
    PngEffect(
        Adafruit_GFX &matrix,
        const char *filepath,
        int16_t x = XPOS_CENTER,
        int16_t y = YPOS_CENTER,
        int16_t dx = 0,
        int16_t dy = 0
    );

    // Construct from an in-memory PNG byte buffer (e.g. PROGMEM array)
    PngEffect(
        Adafruit_GFX &matrix,
        const uint8_t *png_data,
        size_t png_size,
        int16_t x = XPOS_CENTER,
        int16_t y = YPOS_CENTER,
        int16_t dx = 0,
        int16_t dy = 0
    );

    ~PngEffect() override = default;

    void render() override;

    bool isValid() const { return m_valid; }
    int16_t width() const { return m_width; }
    int16_t height() const { return m_height; }

    // Direct static helpers to decode & draw a PNG onto any Adafruit_GFX display at (x, y)
    static bool draw(Adafruit_GFX &matrix, const char *filepath, int16_t x, int16_t y);
    static bool draw(Adafruit_GFX &matrix, const uint8_t *png_data, size_t png_size, int16_t x, int16_t y);

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

    std::vector<uint16_t> m_rgb;
    std::vector<uint8_t>  m_mask;
};
