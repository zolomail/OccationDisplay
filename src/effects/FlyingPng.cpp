#include "FlyingPng.h"
#include "FastHub75.h"
#include <LittleFS.h>
#include <PNGdec.h>
#include <memory>

namespace {

struct DecodeContext {
    int width;
    int height;
    std::vector<uint16_t> *rgb;
    std::vector<uint8_t>  *mask; // Can be nullptr if use_transparency is false
    bool has_alpha;
    PNG *png;
};

void pngDrawCallback(PNGDRAW *pDraw)
{
    auto *ctx = static_cast<DecodeContext *>(pDraw->pUser);
    const int y = pDraw->y;
    if (y < 0 || y >= ctx->height) return;

    const int w = pDraw->iWidth;
    const int row_bytes = (ctx->width + 7) / 8;
    const bool build_mask = (ctx->mask != nullptr);

    if (pDraw->iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA) {
        const uint8_t *src = pDraw->pPixels;
        for (int x = 0; x < w && x < ctx->width; ++x) {
            const uint8_t r = src[0];
            const uint8_t g = src[1];
            const uint8_t b = src[2];
            const uint8_t a = src[3];
            src += 4;

            const size_t pixel_idx = y * ctx->width + x;
            (*ctx->rgb)[pixel_idx] = color565(r, g, b);

            if (build_mask) {
                if (a >= 32) {
                    const size_t mask_byte_idx = y * row_bytes + (x / 8);
                    (*ctx->mask)[mask_byte_idx] |= (0x80 >> (x % 8));
                } else {
                    ctx->has_alpha = true;
                }
            }
        }
    } else if (pDraw->iPixelType == PNG_PIXEL_TRUECOLOR) {
        const uint8_t *src = pDraw->pPixels;
        for (int x = 0; x < w && x < ctx->width; ++x) {
            const uint8_t r = src[0];
            const uint8_t g = src[1];
            const uint8_t b = src[2];
            src += 3;

            const size_t pixel_idx = y * ctx->width + x;
            (*ctx->rgb)[pixel_idx] = color565(r, g, b);

            if (build_mask) {
                const size_t mask_byte_idx = y * row_bytes + (x / 8);
                (*ctx->mask)[mask_byte_idx] |= (0x80 >> (x % 8));
            }
        }
    } else {
        // Indexed, grayscale, etc.
        std::vector<uint16_t> line_buffer(w);
        ctx->png->getLineAsRGB565(pDraw, line_buffer.data(), PNG_RGB565_LITTLE_ENDIAN, 0x0000);

        bool has_trans = false;
        std::vector<uint8_t> alpha_mask;
        if (build_mask) {
            alpha_mask.assign(row_bytes, 0);
            has_trans = (ctx->png->getAlphaMask(pDraw, alpha_mask.data(), 32) != 0);
            if (has_trans) {
                ctx->has_alpha = true;
            }
        }

        for (int x = 0; x < w && x < ctx->width; ++x) {
            const size_t pixel_idx = y * ctx->width + x;
            (*ctx->rgb)[pixel_idx] = line_buffer[x];

            if (build_mask) {
                const size_t mask_byte_idx = y * row_bytes + (x / 8);
                if (!has_trans || ((alpha_mask[x / 8] >> (7 - (x % 8))) & 1)) {
                    (*ctx->mask)[mask_byte_idx] |= (0x80 >> (x % 8));
                }
            }
        }
    }
}

} // namespace

FlyingPng::FlyingPng(
    Adafruit_GFX &matrix,
    const char *filepath,
    int16_t x,
    int16_t y,
    int16_t dx,
    int16_t dy,
    bool use_transparency,
    bool static_idle
) :
    Effect(matrix),
    m_x(x),
    m_y(y),
    m_dx(dx),
    m_dy(dy),
    m_use_transparency(use_transparency),
    m_static_idle(static_idle)
{
    if (decodeFromFs(filepath)) {
        m_x = resolveX(m_x, m_width) << 4;
        m_y = resolveY(m_y, m_height) << 4;
        m_valid = true;
    }
}

FlyingPng::FlyingPng(
    Adafruit_GFX &matrix,
    const uint8_t *png_data,
    size_t png_size,
    int16_t x,
    int16_t y,
    int16_t dx,
    int16_t dy,
    bool use_transparency,
    bool static_idle
) :
    Effect(matrix),
    m_x(x),
    m_y(y),
    m_dx(dx),
    m_dy(dy),
    m_use_transparency(use_transparency),
    m_static_idle(static_idle)
{
    if (decodeFromRam(png_data, png_size)) {
        m_x = resolveX(m_x, m_width) << 4;
        m_y = resolveY(m_y, m_height) << 4;
        m_valid = true;
    }
}

bool FlyingPng::decodeFromFs(const char *filepath)
{
    if (!LittleFS.exists(filepath)) {
        Serial.printf("FlyingPng: file not found on LittleFS: '%s'\n", filepath);
        return false;
    }
    File f = LittleFS.open(filepath, "r");
    if (!f || f.isDirectory()) {
        Serial.printf("FlyingPng: failed to open file '%s'\n", filepath);
        return false;
    }
    const size_t sz = f.size();
    if (sz == 0 || sz > 65536) {
        f.close();
        return false;
    }
    std::vector<uint8_t> raw(sz);
    const size_t nread = f.read(raw.data(), sz);
    f.close();
    if (nread != sz) {
        return false;
    }
    return decodeFromRam(raw.data(), sz);
}

bool FlyingPng::decodeFromRam(const uint8_t *data, size_t size)
{
    if (!data || size == 0) return false;

    auto png = std::make_unique<PNG>();
    const int rc = png->openRAM(const_cast<uint8_t *>(data), static_cast<int>(size), pngDrawCallback);
    if (rc != PNG_SUCCESS) {
        Serial.printf("FlyingPng: failed to open RAM png, rc=%d\n", rc);
        return false;
    }

    m_width = png->getWidth();
    m_height = png->getHeight();
    if (m_width <= 0 || m_height <= 0 || m_width > 512 || m_height > 512) {
        png->close();
        return false;
    }

    const size_t total_pixels = m_width * m_height;
    m_rgb.assign(total_pixels, 0);

    if (m_use_transparency) {
        const size_t row_bytes = (m_width + 7) / 8;
        const size_t total_mask_bytes = row_bytes * m_height;
        m_mask.assign(total_mask_bytes, 0);
    } else {
        m_mask.clear();
    }

    DecodeContext ctx{m_width, m_height, &m_rgb, m_use_transparency ? &m_mask : nullptr, false, png.get()};
    const int dec_rc = png->decode(&ctx, 0);
    png->close();

    if (dec_rc != PNG_SUCCESS) {
        Serial.printf("FlyingPng: decode RAM error, rc=%d\n", dec_rc);
        return false;
    }

    m_has_alpha = m_use_transparency && ctx.has_alpha;
    return true;
}

void FlyingPng::render()
{
    if (!m_valid || m_rgb.empty()) return;

    if (m_static_idle && m_dx == 0 && m_dy == 0) {
        if (m_frames_rendered >= 2) {
            return; // Front & back double-buffers are already painted!
        }
        ++m_frames_rendered;
    }

    const int16_t cur_x = m_x >> 4;
    const int16_t cur_y = m_y >> 4;

    auto *hub75 = static_cast<MatrixPanel_I2S_DMA *>(&m_matrix);
    FastHub75::drawRGBBitmap(
        *hub75,
        cur_x,
        cur_y,
        m_rgb.data(),
        (m_use_transparency && m_has_alpha && !m_mask.empty()) ? m_mask.data() : nullptr,
        m_width,
        m_height,
        m_use_transparency && m_has_alpha
    );

    // Handle bouncing physics if dx/dy non-zero
    if (m_dx != 0) {
        if (((m_x + m_dx) >> 4) + m_width > m_matrix.width() || ((m_x + m_dx) >> 4) < 0) {
            m_dx = -m_dx;
        }
        m_x += m_dx;
    }

    if (m_dy != 0) {
        if (((m_y + m_dy) >> 4) + m_height > m_matrix.height() || ((m_y + m_dy) >> 4) < 0) {
            m_dy = -m_dy;
        }
        m_y += m_dy;
    }
}

bool FlyingPng::draw(Adafruit_GFX &matrix, const char *filepath, int16_t x, int16_t y, bool use_transparency)
{
    FlyingPng temp(matrix, filepath, x, y, use_transparency);
    if (!temp.isValid()) return false;
    temp.render();
    return true;
}

bool FlyingPng::draw(Adafruit_GFX &matrix, const uint8_t *png_data, size_t png_size, int16_t x, int16_t y, bool use_transparency)
{
    FlyingPng temp(matrix, png_data, png_size, x, y, use_transparency);
    if (!temp.isValid()) return false;
    temp.render();
    return true;
}
