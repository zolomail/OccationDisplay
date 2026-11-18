#include "PngEffect.h"
#include <LittleFS.h>
#include <PNGdec.h>
#include <memory>

namespace {

struct DecodeContext {
    int width;
    int height;
    std::vector<uint16_t> *rgb;
    std::vector<uint8_t>  *mask;
    bool has_alpha;
    PNG *png;
};

void *pngOpenCallback(const char *szFilename, int32_t *pFileSize)
{
    File f = LittleFS.open(szFilename, "r");
    if (!f || f.isDirectory()) {
        return nullptr;
    }
    *pFileSize = static_cast<int32_t>(f.size());
    return new File(f);
}

void pngCloseCallback(void *pHandle)
{
    auto *f = static_cast<File *>(pHandle);
    if (f) {
        f->close();
        delete f;
    }
}

int32_t pngReadCallback(PNGFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
    auto *f = static_cast<File *>(pFile->fHandle);
    if (!f) return 0;
    return static_cast<int32_t>(f->read(pBuf, iLen));
}

int32_t pngSeekCallback(PNGFILE *pFile, int32_t iPosition)
{
    auto *f = static_cast<File *>(pFile->fHandle);
    if (!f) return 0;
    return f->seek(iPosition) ? iPosition : -1;
}

void pngDrawCallback(PNGDRAW *pDraw)
{
    auto *ctx = static_cast<DecodeContext *>(pDraw->pUser);
    const int y = pDraw->y;
    if (y < 0 || y >= ctx->height) return;

    const int w = pDraw->iWidth;
    const int row_bytes = (ctx->width + 7) / 8;

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

            if (a >= 32) {
                const size_t mask_byte_idx = y * row_bytes + (x / 8);
                (*ctx->mask)[mask_byte_idx] |= (0x80 >> (x % 8));
            } else {
                ctx->has_alpha = true;
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

            const size_t mask_byte_idx = y * row_bytes + (x / 8);
            (*ctx->mask)[mask_byte_idx] |= (0x80 >> (x % 8));
        }
    } else {
        // Indexed, grayscale, etc.
        std::vector<uint16_t> line_buffer(w);
        ctx->png->getLineAsRGB565(pDraw, line_buffer.data(), PNG_RGB565_LITTLE_ENDIAN, 0x0000);
        std::vector<uint8_t> alpha_mask(row_bytes, 0);
        const uint8_t has_trans = ctx->png->getAlphaMask(pDraw, alpha_mask.data(), 32);
        if (has_trans) {
            ctx->has_alpha = true;
        }

        for (int x = 0; x < w && x < ctx->width; ++x) {
            const size_t pixel_idx = y * ctx->width + x;
            (*ctx->rgb)[pixel_idx] = line_buffer[x];

            const size_t mask_byte_idx = y * row_bytes + (x / 8);
            if (!has_trans || ((alpha_mask[x / 8] >> (7 - (x % 8))) & 1)) {
                (*ctx->mask)[mask_byte_idx] |= (0x80 >> (x % 8));
            }
        }
    }
}

} // namespace

PngEffect::PngEffect(
    Adafruit_GFX &matrix,
    const char *filepath,
    int16_t x,
    int16_t y,
    int16_t dx,
    int16_t dy
) :
    Effect(matrix),
    m_x(x),
    m_y(y),
    m_dx(dx),
    m_dy(dy)
{
    if (decodeFromFs(filepath)) {
        m_x = resolveX(m_x, m_width) << 4;
        m_y = resolveY(m_y, m_height) << 4;
        m_valid = true;
    }
}

PngEffect::PngEffect(
    Adafruit_GFX &matrix,
    const uint8_t *png_data,
    size_t png_size,
    int16_t x,
    int16_t y,
    int16_t dx,
    int16_t dy
) :
    Effect(matrix),
    m_x(x),
    m_y(y),
    m_dx(dx),
    m_dy(dy)
{
    if (decodeFromRam(png_data, png_size)) {
        m_x = resolveX(m_x, m_width) << 4;
        m_y = resolveY(m_y, m_height) << 4;
        m_valid = true;
    }
}

bool PngEffect::decodeFromFs(const char *filepath)
{
    auto png = std::make_unique<PNG>();
    const int rc = png->open(
        filepath,
        pngOpenCallback,
        pngCloseCallback,
        pngReadCallback,
        pngSeekCallback,
        pngDrawCallback
    );
    if (rc != PNG_SUCCESS) {
        Serial.printf("PngEffect: failed to open '%s', rc=%d\n", filepath, rc);
        return false;
    }

    m_width = png->getWidth();
    m_height = png->getHeight();
    if (m_width <= 0 || m_height <= 0 || m_width > 512 || m_height > 512) {
        png->close();
        return false;
    }

    const size_t total_pixels = m_width * m_height;
    const size_t row_bytes = (m_width + 7) / 8;
    const size_t total_mask_bytes = row_bytes * m_height;

    m_rgb.assign(total_pixels, 0);
    m_mask.assign(total_mask_bytes, 0);

    DecodeContext ctx{m_width, m_height, &m_rgb, &m_mask, false, png.get()};
    const int dec_rc = png->decode(&ctx, 0);
    png->close();

    if (dec_rc != PNG_SUCCESS) {
        Serial.printf("PngEffect: decode error '%s', rc=%d\n", filepath, dec_rc);
        return false;
    }

    m_has_alpha = ctx.has_alpha;
    return true;
}

bool PngEffect::decodeFromRam(const uint8_t *data, size_t size)
{
    if (!data || size == 0) return false;

    auto png = std::make_unique<PNG>();
    const int rc = png->openRAM(const_cast<uint8_t *>(data), static_cast<int>(size), pngDrawCallback);
    if (rc != PNG_SUCCESS) {
        Serial.printf("PngEffect: failed to open RAM png, rc=%d\n", rc);
        return false;
    }

    m_width = png->getWidth();
    m_height = png->getHeight();
    if (m_width <= 0 || m_height <= 0 || m_width > 512 || m_height > 512) {
        png->close();
        return false;
    }

    const size_t total_pixels = m_width * m_height;
    const size_t row_bytes = (m_width + 7) / 8;
    const size_t total_mask_bytes = row_bytes * m_height;

    m_rgb.assign(total_pixels, 0);
    m_mask.assign(total_mask_bytes, 0);

    DecodeContext ctx{m_width, m_height, &m_rgb, &m_mask, false, png.get()};
    const int dec_rc = png->decode(&ctx, 0);
    png->close();

    if (dec_rc != PNG_SUCCESS) {
        Serial.printf("PngEffect: decode RAM error, rc=%d\n", dec_rc);
        return false;
    }

    m_has_alpha = ctx.has_alpha;
    return true;
}

void PngEffect::render()
{
    if (!m_valid || m_rgb.empty()) return;

    const int16_t cur_x = m_x >> 4;
    const int16_t cur_y = m_y >> 4;

    if (m_has_alpha && !m_mask.empty()) {
        m_matrix.drawRGBBitmap(cur_x, cur_y, m_rgb.data(), m_mask.data(), m_width, m_height);
    } else {
        m_matrix.drawRGBBitmap(cur_x, cur_y, m_rgb.data(), m_width, m_height);
    }

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

bool PngEffect::draw(Adafruit_GFX &matrix, const char *filepath, int16_t x, int16_t y)
{
    PngEffect temp(matrix, filepath, x, y);
    if (!temp.isValid()) return false;
    temp.render();
    return true;
}

bool PngEffect::draw(Adafruit_GFX &matrix, const uint8_t *png_data, size_t png_size, int16_t x, int16_t y)
{
    PngEffect temp(matrix, png_data, png_size, x, y);
    if (!temp.isValid()) return false;
    temp.render();
    return true;
}
