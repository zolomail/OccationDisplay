#pragma once

#include "Effect.h"
#include "bitmaps/SdfAtlases.h"
#include <vector>
#include <cstdint>

class SDFEffect : public Effect
{
public:
    SDFEffect(
        Adafruit_GFX &matrix,
        const SdfAtlas &atlas,
        uint16_t color = 0xFFFF,
        int16_t x = XPOS_CENTER,
        int16_t y = YPOS_CENTER,
        float scale = 1.0f,
        float angle_deg = 0.0f,
        float angular_vel_deg = 0.0f,
        int16_t dx = 0,
        int16_t dy = 0,
        float outline_px = 1.5f
    );

    ~SDFEffect() override = default;

    void render() override;

    void setAtlas(const SdfAtlas &atlas) { m_atlas = atlas; }
    void setColor(uint16_t color) { m_color = color; }
    void setPosition(int16_t x, int16_t y);
    void setScale(float scale) { m_scale = (scale > 0.05f) ? scale : 0.05f; }
    void setAngle(float angle_deg) { m_angle_deg = angle_deg; }
    void setAngularVelocity(float vel_deg) { m_angular_vel_deg = vel_deg; }
    void setOutlineWidth(float outline_px) { m_outline_px = (outline_px > 0.0f) ? outline_px : 0.0f; }

private:
    SdfAtlas m_atlas;
    uint16_t m_color{0xFFFF};
    int16_t  m_x{0};
    int16_t  m_y{0};
    int16_t  m_dx{0};
    int16_t  m_dy{0};
    float    m_scale{1.0f};
    float    m_angle_deg{0.0f};
    float    m_angular_vel_deg{0.0f};
    float    m_outline_px{1.5f};

    std::vector<uint16_t> m_scratch_rgb;
    std::vector<uint8_t>  m_scratch_mask;
};
