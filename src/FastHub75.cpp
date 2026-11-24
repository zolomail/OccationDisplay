#include "FastHub75.h"
#include <algorithm>

// Access private member `fb` of MatrixPanel_I2S_DMA
template<typename Tag>
struct AccessTag {
    typedef typename Tag::type type;
    static type ptr;
};
template<typename Tag>
typename AccessTag<Tag>::type AccessTag<Tag>::ptr;

template<typename Tag, typename Tag::type p>
struct AccessRob : AccessTag<Tag> {
    struct Filler {
        Filler() { AccessTag<Tag>::ptr = p; }
    };
    static Filler filler;
};
template<typename Tag, typename Tag::type p>
typename AccessRob<Tag, p>::Filler AccessRob<Tag, p>::filler;

struct MatrixPanel_fb { typedef frameStruct* MatrixPanel_I2S_DMA::*type; };
template struct AccessRob<MatrixPanel_fb, &MatrixPanel_I2S_DMA::fb>;

void FastHub75::drawRGBBitmap(
    MatrixPanel_I2S_DMA &panel,
    int16_t x,
    int16_t y,
    const uint16_t *bitmap,
    const uint8_t *mask,
    int16_t w,
    int16_t h,
    bool use_transparency
) {
    if (!bitmap || w <= 0 || h <= 0) return;

    frameStruct *fb = panel.*AccessTag<MatrixPanel_fb>::ptr;
    if (!fb) return;

    const int16_t panel_w = panel.width();
    const int16_t panel_h = panel.height();
    const uint8_t rows_per_frame = panel_h / 2; // 16 for a 32-high matrix
    const uint8_t color_depth = 8;

    // 2D Screen Clipping
    const int16_t start_y = std::max<int16_t>(0, y);
    const int16_t end_y   = std::min<int16_t>(panel_h, y + h);
    if (start_y >= end_y) return;

    const int16_t start_x = std::max<int16_t>(0, x);
    const int16_t end_x   = std::min<int16_t>(panel_w, x + w);
    if (start_x >= end_x) return;

    const int16_t mask_stride = (w + 7) / 8;

    for (int16_t py = start_y; py < end_y; ++py) {
        const int16_t img_y = py - y;
        const uint16_t *bitmap_row = bitmap + (img_y * w);
        const uint8_t  *mask_row   = mask ? (mask + (img_y * mask_stride)) : nullptr;

        const uint8_t sub_row = (py >= rows_per_frame) ? (py - rows_per_frame) : py;
        const uint16_t bit_offset = (py >= rows_per_frame) ? BITS_RGB2_OFFSET : 0;
        const uint16_t clear_mask = (py >= rows_per_frame) ? BITMASK_RGB2_CLEAR : BITMASK_RGB1_CLEAR;

        auto &rowStruct = fb->rowBits[sub_row];
        ESP32_I2S_DMA_STORAGE_TYPE *base_ptr = rowStruct->data.get();
        const uint16_t stride = rowStruct->width;

        // Fetch row pointers for all 8 bitplanes ONCE per row
        ESP32_I2S_DMA_STORAGE_TYPE *plane_ptrs[8];
        #pragma GCC unroll 8
        for (uint8_t d = 0; d < color_depth; ++d) {
            plane_ptrs[d] = base_ptr + (d * stride);
        }

        if (use_transparency && mask_row) {
            int16_t px = start_x;
            while (px < end_x) {
                const int16_t img_x = px - x;

                // Byte-level skip: if 8 pixels are fully transparent, skip them at once
                if ((img_x & 7) == 0 && (px + 8 <= end_x)) {
                    const uint8_t mask_byte = mask_row[img_x >> 3];
                    if (mask_byte == 0x00) {
                        px += 8;
                        continue;
                    }
                }

                // Check 1-bit mask for this pixel
                const uint8_t mask_byte = mask_row[img_x >> 3];
                if ((mask_byte & (0x80 >> (img_x & 7))) == 0) {
                    ++px;
                    continue; // Skip transparent pixel
                }

                const uint16_t color = bitmap_row[img_x];
                uint8_t r, g, b;
                MatrixPanel_I2S_DMA::color565to888(color, r, g, b);
                const uint8_t r_comp = lumConvTab[r];
                const uint8_t g_comp = lumConvTab[g];
                const uint8_t b_comp = lumConvTab[b];

                if ((r_comp | g_comp | b_comp) == 0) {
                    #pragma GCC unroll 8
                    for (uint8_t d = 0; d < color_depth; ++d) {
                        plane_ptrs[d][px] &= clear_mask;
                    }
                } else {
                    #pragma GCC unroll 8
                    for (uint8_t d = 0; d < color_depth; ++d) {
                        const uint16_t rgb_bits = (((b_comp >> d) & 1) << 2) |
                                                  (((g_comp >> d) & 1) << 1) |
                                                   ((r_comp >> d) & 1);
                        plane_ptrs[d][px] = (plane_ptrs[d][px] & clear_mask) | (rgb_bits << bit_offset);
                    }
                }
                ++px;
            }
        } else {
            // Opaque path: no mask tests at all
            for (int16_t px = start_x; px < end_x; ++px) {
                const int16_t img_x = px - x;
                const uint16_t color = bitmap_row[img_x];

                uint8_t r, g, b;
                MatrixPanel_I2S_DMA::color565to888(color, r, g, b);
                const uint8_t r_comp = lumConvTab[r];
                const uint8_t g_comp = lumConvTab[g];
                const uint8_t b_comp = lumConvTab[b];

                if ((r_comp | g_comp | b_comp) == 0) {
                    #pragma GCC unroll 8
                    for (uint8_t d = 0; d < color_depth; ++d) {
                        plane_ptrs[d][px] &= clear_mask;
                    }
                } else {
                    #pragma GCC unroll 8
                    for (uint8_t d = 0; d < color_depth; ++d) {
                        const uint16_t rgb_bits = (((b_comp >> d) & 1) << 2) |
                                                  (((g_comp >> d) & 1) << 1) |
                                                   ((r_comp >> d) & 1);
                        plane_ptrs[d][px] = (plane_ptrs[d][px] & clear_mask) | (rgb_bits << bit_offset);
                    }
                }
            }
        }
    }
}

void FastHub75::applyPanelCalibration(
    MatrixPanel_I2S_DMA &panel,
    const PanelCalibration *cal,
    size_t panel_count,
    uint16_t panel_res_x
) {
    if (!cal || panel_count == 0 || panel_res_x == 0) return;

    bool any_active = false;
    for (size_t i = 0; i < panel_count; ++i) {
        if (cal[i].r < 100 || cal[i].g < 100 || cal[i].b < 100) {
            any_active = true;
            break;
        }
    }
    if (!any_active) return;

    frameStruct *fb = panel.*AccessTag<MatrixPanel_fb>::ptr;
    if (!fb) return;

    const int16_t panel_w = panel.width();
    const uint8_t rows_per_frame = panel.height() / 2;
    const uint8_t color_depth = 8;
    const uint16_t rgb_both_clear = BITMASK_RGB1_CLEAR & BITMASK_RGB2_CLEAR;

    for (size_t p_idx = 0; p_idx < panel_count; ++p_idx) {
        const PanelCalibration &pc = cal[p_idx];
        if (pc.r >= 100 && pc.g >= 100 && pc.b >= 100) continue;

        const int16_t x_start = static_cast<int16_t>(p_idx * panel_res_x);
        const int16_t x_end   = std::min<int16_t>(panel_w, static_cast<int16_t>((p_idx + 1) * panel_res_x));
        if (x_start >= x_end) break;

        uint8_t r_lut[256], g_lut[256], b_lut[256];
        for (int v = 0; v < 256; ++v) {
            r_lut[v] = static_cast<uint8_t>((v * pc.r + 50) / 100);
            g_lut[v] = static_cast<uint8_t>((v * pc.g + 50) / 100);
            b_lut[v] = static_cast<uint8_t>((v * pc.b + 50) / 100);
        }

        for (uint8_t sub_row = 0; sub_row < rows_per_frame; ++sub_row) {
            auto &rowStruct = fb->rowBits[sub_row];
            ESP32_I2S_DMA_STORAGE_TYPE *base_ptr = rowStruct->data.get();
            const uint16_t stride = rowStruct->width;

            ESP32_I2S_DMA_STORAGE_TYPE *plane_ptrs[8];
            #pragma GCC unroll 8
            for (uint8_t d = 0; d < color_depth; ++d) {
                plane_ptrs[d] = base_ptr + (d * stride);
            }

            for (int16_t px = x_start; px < x_end; ++px) {
                uint8_t r1 = 0, g1 = 0, b1 = 0;
                uint8_t r2 = 0, g2 = 0, b2 = 0;

                #pragma GCC unroll 8
                for (uint8_t d = 0; d < color_depth; ++d) {
                    const uint16_t w = plane_ptrs[d][px];
                    r1 |= ((w >> 0) & 1) << d;
                    g1 |= ((w >> 1) & 1) << d;
                    b1 |= ((w >> 2) & 1) << d;
                    const uint16_t w2 = w >> BITS_RGB2_OFFSET;
                    r2 |= ((w2 >> 0) & 1) << d;
                    g2 |= ((w2 >> 1) & 1) << d;
                    b2 |= ((w2 >> 2) & 1) << d;
                }

                if ((r1 | g1 | b1 | r2 | g2 | b2) == 0) continue;

                const uint8_t nr1 = r_lut[r1], ng1 = g_lut[g1], nb1 = b_lut[b1];
                const uint8_t nr2 = r_lut[r2], ng2 = g_lut[g2], nb2 = b_lut[b2];

                #pragma GCC unroll 8
                for (uint8_t d = 0; d < color_depth; ++d) {
                    const uint16_t rgb1 = (((nb1 >> d) & 1) << 2) |
                                          (((ng1 >> d) & 1) << 1) |
                                           ((nr1 >> d) & 1);
                    const uint16_t rgb2 = (((nb2 >> d) & 1) << 2) |
                                          (((ng2 >> d) & 1) << 1) |
                                           ((nr2 >> d) & 1);
                    plane_ptrs[d][px] = (plane_ptrs[d][px] & rgb_both_clear) | rgb1 | (rgb2 << BITS_RGB2_OFFSET);
                }
            }
        }
    }
}
