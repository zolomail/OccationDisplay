#include "SDFEffect.h"
#include "FastHub75.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr float DEG_TO_RAD_F = 0.017453292519943295f;

inline uint16_t scaleRgb565(uint16_t c, uint8_t alpha)
{
    const uint32_t r = (((c >> 11) & 0x1F) * alpha) >> 8;
    const uint32_t g = (((c >> 5)  & 0x3F) * alpha) >> 8;
    const uint32_t b = (( c        & 0x1F) * alpha) >> 8;
    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

inline int32_t sampleSdfBilinearQ4(
    const int8_t *data,
    int32_t w,
    int32_t h,
    int32_t u_16,
    int32_t v_16)
{
    const int32_t max_u = ((w - 2) << 16) | 0xFFFF;
    const int32_t max_v = ((h - 2) << 16) | 0xFFFF;
    if (u_16 < 0 || u_16 > max_u || v_16 < 0 || v_16 > max_v) {
        return 127; // Outside atlas bounds: never smear edge texels
    }

    const int32_t ix = u_16 >> 16;
    const int32_t iy = v_16 >> 16;
    const int32_t fx = (u_16 >> 8) & 0xFF;
    const int32_t fy = (v_16 >> 8) & 0xFF;

    const int8_t *p = data + iy * w + ix;
    const int32_t d00 = p[0];
    const int32_t d10 = p[1];
    const int32_t d01 = p[w];
    const int32_t d11 = p[w + 1];

    const int32_t top = d00 * (256 - fx) + d10 * fx;
    const int32_t bot = d01 * (256 - fx) + d11 * fx;
    return (top * (256 - fy) + bot * fy) >> 16;
}

} // namespace

SDFEffect::SDFEffect(
    Adafruit_GFX &matrix,
    const SdfAtlas &atlas,
    uint16_t color,
    int16_t x,
    int16_t y,
    float scale,
    float angle_deg,
    float angular_vel_deg,
    int16_t dx,
    int16_t dy,
    float outline_px
) :
    Effect(matrix),
    m_atlas(atlas),
    m_color(color),
    m_dx(dx),
    m_dy(dy),
    m_scale((scale > 0.05f) ? scale : 0.05f),
    m_angle_deg(angle_deg),
    m_angular_vel_deg(angular_vel_deg),
    m_outline_px((outline_px > 0.0f) ? outline_px : 0.0f)
{
    setPosition(x, y);
}

void SDFEffect::setPosition(int16_t x, int16_t y)
{
    m_x = resolveX(x, 0) << 4;
    m_y = resolveY(y, 0) << 4;
}

void SDFEffect::render()
{
    if (!m_atlas.data || m_atlas.width < 2 || m_atlas.height < 2) {
        return;
    }

    const int16_t panel_w = m_matrix.width();
    const int16_t panel_h = m_matrix.height();
    const int32_t aw      = m_atlas.width;
    const int32_t ah      = m_atlas.height;

    const float cx_scr = static_cast<float>(m_x) * (1.0f / 16.0f);
    const float cy_scr = static_cast<float>(m_y) * (1.0f / 16.0f);
    const float cx_tex = (static_cast<float>(aw) - 1.0f) * 0.5f;
    const float cy_tex = (static_cast<float>(ah) - 1.0f) * 0.5f;

    const float rad   = m_angle_deg * DEG_TO_RAD_F;
    const float cos_a = cosf(rad);
    const float sin_a = sinf(rad);
    const float inv_s = 1.0f / m_scale;

    // Tight screen-space bounding box around rotated/scaled atlas
    const float half_bw_f = (fabsf(cos_a) * aw + fabsf(sin_a) * ah) * 0.5f * m_scale + 2.0f;
    const float half_bh_f = (fabsf(sin_a) * aw + fabsf(cos_a) * ah) * 0.5f * m_scale + 2.0f;

    const int16_t x0 = std::max<int16_t>(0, static_cast<int16_t>(floorf(cx_scr - half_bw_f)));
    const int16_t y0 = std::max<int16_t>(0, static_cast<int16_t>(floorf(cy_scr - half_bh_f)));
    const int16_t x1 = std::min<int16_t>(panel_w, static_cast<int16_t>(ceilf(cx_scr + half_bw_f)));
    const int16_t y1 = std::min<int16_t>(panel_h, static_cast<int16_t>(ceilf(cy_scr + half_bh_f)));

    if (x0 < x1 && y0 < y1) {
        const int16_t bw = x1 - x0;
        const int16_t bh = y1 - y0;
        const size_t row_bytes = (bw + 7) / 8;

        m_scratch_rgb.resize(static_cast<size_t>(bw) * bh);
        m_scratch_mask.assign(row_bytes * bh, 0);

        // Inverse affine matrix in 16.16 fixed point: screen -> atlas
        const float c_inv = cos_a * inv_s;
        const float s_inv = sin_a * inv_s;
        const int32_t du_dx = static_cast<int32_t>(lroundf( c_inv * 65536.0f));
        const int32_t dv_dx = static_cast<int32_t>(lroundf(-s_inv * 65536.0f));
        const int32_t du_dy = static_cast<int32_t>(lroundf( s_inv * 65536.0f));
        const int32_t dv_dy = static_cast<int32_t>(lroundf( c_inv * 65536.0f));

        const float dx0 = static_cast<float>(x0) - cx_scr;
        const float dy0 = static_cast<float>(y0) - cy_scr;
        const int32_t u_origin = static_cast<int32_t>(lroundf((cx_tex + c_inv * dx0 + s_inv * dy0) * 65536.0f));
        const int32_t v_origin = static_cast<int32_t>(lroundf((cy_tex - s_inv * dx0 + c_inv * dy0) * 65536.0f));

        // 4x4 tile skipping thresholds in Q4 SDF units
        const bool    is_outline     = (m_outline_px > 0.0f);
        const int32_t stroke_half_q4 = is_outline
            ? std::max<int32_t>(2, static_cast<int32_t>(lroundf(0.5f * m_outline_px * inv_s * m_atlas.scale_q4)))
            : 0;
        const int32_t aa_half_q4     = std::max<int32_t>(4, static_cast<int32_t>(lroundf(0.75f * inv_s * m_atlas.scale_q4)));
        const int32_t aa_span_q4     = aa_half_q4 * 2;
        const int32_t tile_rad_q4    = static_cast<int32_t>(lroundf(2.15f * inv_s * m_atlas.scale_q4)) + 2;
        const int32_t outer_skip     = tile_rad_q4 + stroke_half_q4 + aa_half_q4;
        const int32_t solid_thresh   = is_outline
            ? (stroke_half_q4 - tile_rad_q4 - aa_half_q4)
            : -(tile_rad_q4 + aa_half_q4);

        // 1.5px offset from tile top-left to tile center in fixed point
        const int32_t center_du = (du_dx * 3 + du_dy * 3) >> 1;
        const int32_t center_dv = (dv_dx * 3 + dv_dy * 3) >> 1;

        for (int16_t ly = 0; ly < bh; ly += 4) {
            const int16_t th = std::min<int16_t>(4, bh - ly);
            const int32_t u_tile_row = u_origin + ly * du_dy;
            const int32_t v_tile_row = v_origin + ly * dv_dy;

            for (int16_t lx = 0; lx < bw; lx += 4) {
                const int16_t tw = std::min<int16_t>(4, bw - lx);
                const int32_t u_tile = u_tile_row + lx * du_dx;
                const int32_t v_tile = v_tile_row + lx * dv_dx;

                const int32_t d_center = sampleSdfBilinearQ4(
                    m_atlas.data, aw, ah, u_tile + center_du, v_tile + center_dv);

                if (d_center > outer_skip || (is_outline && d_center < -outer_skip)) {
                    continue; // Entire 4x4 tile is outside stroke (or hollow interior)
                }

                const bool tile_solid = is_outline
                    ? (solid_thresh > 0 && std::abs(d_center) < solid_thresh)
                    : (d_center < solid_thresh);
                if (tile_solid) {
                    // Entire 4x4 tile is solid inside: fast fill without bilinear/AA
                    for (int16_t r = 0; r < th; ++r) {
                        const size_t py = static_cast<size_t>(ly + r);
                        uint16_t *dst_rgb = &m_scratch_rgb[py * bw + lx];
                        uint8_t  *dst_msk = &m_scratch_mask[py * row_bytes];
                        for (int16_t c = 0; c < tw; ++c) {
                            dst_rgb[c] = m_color;
                            const int16_t px = lx + c;
                            dst_msk[px >> 3] |= static_cast<uint8_t>(0x80 >> (px & 7));
                        }
                    }
                    continue;
                }

                // Boundary tile: affine scanline blitter with bilinear SDF + AA
                int32_t u_row = u_tile;
                int32_t v_row = v_tile;
                for (int16_t r = 0; r < th; ++r) {
                    const size_t py = static_cast<size_t>(ly + r);
                    uint16_t *dst_rgb = &m_scratch_rgb[py * bw + lx];
                    uint8_t  *dst_msk = &m_scratch_mask[py * row_bytes];
                    int32_t u = u_row;
                    int32_t v = v_row;

                    for (int16_t c = 0; c < tw; ++c) {
                        const int32_t d_raw = sampleSdfBilinearQ4(m_atlas.data, aw, ah, u, v);
                        const int32_t d_q4  = is_outline ? (std::abs(d_raw) - stroke_half_q4) : d_raw;
                        if (d_q4 <= -aa_half_q4) {
                            dst_rgb[c] = m_color;
                            const int16_t px = lx + c;
                            dst_msk[px >> 3] |= static_cast<uint8_t>(0x80 >> (px & 7));
                        } else if (d_q4 < aa_half_q4) {
                            const int32_t cov = ((aa_half_q4 - d_q4) * 255) / aa_span_q4;
                            if (cov >= 16) {
                                dst_rgb[c] = scaleRgb565(m_color, static_cast<uint8_t>(cov));
                                const int16_t px = lx + c;
                                dst_msk[px >> 3] |= static_cast<uint8_t>(0x80 >> (px & 7));
                            }
                        }
                        u += du_dx;
                        v += dv_dx;
                    }
                    u_row += du_dy;
                    v_row += dv_dy;
                }
            }
        }

        auto *hub75 = static_cast<MatrixPanel_I2S_DMA *>(&m_matrix);
        FastHub75::drawRGBBitmap(
            *hub75, x0, y0, m_scratch_rgb.data(), m_scratch_mask.data(), bw, bh, true);
    }

    // Update rotation & optional linear motion
    if (m_angular_vel_deg != 0.0f) {
        m_angle_deg += m_angular_vel_deg;
        if (m_angle_deg >= 360.0f) m_angle_deg -= 360.0f;
        else if (m_angle_deg < 0.0f) m_angle_deg += 360.0f;
    }

    if (m_dx != 0) {
        const int16_t nx = (m_x + m_dx) >> 4;
        if (nx < 0 || nx >= panel_w) m_dx = -m_dx;
        m_x += m_dx;
    }
    if (m_dy != 0) {
        const int16_t ny = (m_y + m_dy) >> 4;
        if (ny < 0 || ny >= panel_h) m_dy = -m_dy;
        m_y += m_dy;
    }
}
