#include "FlyingPng.h"
#include "FastHub75.h"
#include <LittleFS.h>
#include <PNGdec.h>
#include <cstring>
#include <memory>

namespace {

constexpr uint8_t PNG_SIG[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

struct DecodeContext {
    int width;
    int height;
    std::vector<uint16_t> *rgb;
    std::vector<uint8_t>  *alpha; // 8-bit alpha per pixel (nullptr if not needed)
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
    const bool build_alpha = (ctx->alpha != nullptr);

    if (pDraw->iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA) {
        const uint8_t *src = pDraw->pPixels;
        for (int x = 0; x < w && x < ctx->width; ++x) {
            const uint8_t r = src[0];
            const uint8_t g = src[1];
            const uint8_t b = src[2];
            const uint8_t a = src[3];
            src += 4;

            const size_t pixel_idx = static_cast<size_t>(y) * ctx->width + x;
            (*ctx->rgb)[pixel_idx] = color565(r, g, b);

            if (build_alpha) {
                (*ctx->alpha)[pixel_idx] = a;
                if (a < 255) {
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

            const size_t pixel_idx = static_cast<size_t>(y) * ctx->width + x;
            (*ctx->rgb)[pixel_idx] = color565(r, g, b);

            if (build_alpha) {
                (*ctx->alpha)[pixel_idx] = 255;
            }
        }
    } else {
        // Indexed, grayscale, etc.
        std::vector<uint16_t> line_buffer(w);
        ctx->png->getLineAsRGB565(pDraw, line_buffer.data(), PNG_RGB565_LITTLE_ENDIAN, 0x0000);

        bool has_trans = false;
        std::vector<uint8_t> alpha_mask;
        if (build_alpha) {
            alpha_mask.assign(row_bytes, 0);
            has_trans = (ctx->png->getAlphaMask(pDraw, alpha_mask.data(), 128) != 0);
            if (has_trans) {
                ctx->has_alpha = true;
            }
        }

        for (int x = 0; x < w && x < ctx->width; ++x) {
            const size_t pixel_idx = static_cast<size_t>(y) * ctx->width + x;
            (*ctx->rgb)[pixel_idx] = line_buffer[x];

            if (build_alpha) {
                const bool opaque = !has_trans || (((alpha_mask[x / 8] >> (7 - (x % 8))) & 1) != 0);
                (*ctx->alpha)[pixel_idx] = opaque ? 255 : 0;
            }
        }
    }
}

uint16_t readBE16(const uint8_t *p)
{
    return (static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]);
}

uint32_t readBE32(const uint8_t *p)
{
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
            static_cast<uint32_t>(p[3]);
}

void writeBE32(uint8_t *p, uint32_t v)
{
    p[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 8)  & 0xFF);
    p[3] = static_cast<uint8_t>(v & 0xFF);
}

uint32_t crc32Update(uint32_t crc, const uint8_t *buf, size_t len)
{
    crc = ~crc;
    for (size_t i = 0; i < len; ++i) {
        crc ^= buf[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
        }
    }
    return ~crc;
}

void appendChunk(std::vector<uint8_t> &out, const char type[4], const uint8_t *data, uint32_t len)
{
    const size_t base = out.size();
    out.resize(base + 8 + len + 4);
    writeBE32(&out[base], len);
    std::memcpy(&out[base + 4], type, 4);
    if (len > 0 && data) {
        std::memcpy(&out[base + 8], data, len);
    }
    uint32_t crc = crc32Update(0, reinterpret_cast<const uint8_t *>(type), 4);
    if (len > 0 && data) {
        crc = crc32Update(crc, data, len);
    }
    writeBE32(&out[base + 8 + len], crc);
}

bool decodeSinglePngBuffer(
    const uint8_t *data,
    size_t size,
    int &out_w,
    int &out_h,
    std::vector<uint16_t> &out_rgb,
    std::vector<uint8_t> *out_alpha,
    bool &out_has_alpha)
{
    auto png = std::make_unique<PNG>();
    const int rc = png->openRAM(const_cast<uint8_t *>(data), static_cast<int>(size), pngDrawCallback);
    if (rc != PNG_SUCCESS) {
        Serial.printf("FlyingPng: failed to open RAM png, rc=%d\n", rc);
        return false;
    }

    out_w = png->getWidth();
    out_h = png->getHeight();
    if (out_w <= 0 || out_h <= 0 || out_w > 512 || out_h > 512) {
        png->close();
        return false;
    }

    const size_t total_pixels = static_cast<size_t>(out_w) * out_h;
    out_rgb.assign(total_pixels, 0);
    if (out_alpha) {
        out_alpha->assign(total_pixels, 0);
    }

    DecodeContext ctx{out_w, out_h, &out_rgb, out_alpha, false, png.get()};
    const int dec_rc = png->decode(&ctx, 0);
    png->close();

    if (dec_rc != PNG_SUCCESS) {
        Serial.printf("FlyingPng: decode RAM error, rc=%d\n", dec_rc);
        return false;
    }

    out_has_alpha = ctx.has_alpha;
    return true;
}

inline uint16_t blendRgb565(uint16_t fg, uint16_t bg, uint8_t a)
{
    const uint16_t inv_a = 255 - a;
    const uint32_t fr = (fg >> 11) & 0x1F, fg_c = (fg >> 5) & 0x3F, fb = fg & 0x1F;
    const uint32_t br = (bg >> 11) & 0x1F, bg_c = (bg >> 5) & 0x3F, bb = bg & 0x1F;
    const uint32_t r = (fr * a + br * inv_a + 127) / 255;
    const uint32_t g = (fg_c * a + bg_c * inv_a + 127) / 255;
    const uint32_t b = (fb * a + bb * inv_a + 127) / 255;
    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

struct ChunkSlice {
    uint32_t offset;
    uint32_t length;
    bool     is_fdat;
};

struct ApngFrameDesc {
    uint32_t width{0};
    uint32_t height{0};
    uint32_t x_offset{0};
    uint32_t y_offset{0};
    uint32_t delay_ms{100};
    uint8_t  dispose_op{0};
    uint8_t  blend_op{0};
    std::vector<ChunkSlice> data_chunks;
};

void advanceAxis(int16_t &pos, int16_t &vel, int16_t item_size, int16_t display_size, bool wrap_around)
{
    if (vel == 0) return;

    if (wrap_around) {
        pos += vel;
        const int16_t px = pos >> 4;
        if (vel > 0 && px >= display_size) {
            pos = static_cast<int16_t>(-item_size) << 4;
        } else if (vel < 0 && px <= -item_size) {
            pos = display_size << 4;
        }
    } else {
        const int16_t next_px = (pos + vel) >> 4;
        if (next_px + item_size > display_size || next_px < 0) {
            vel = -vel;
        }
        pos += vel;
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
    bool static_idle,
    bool wrap_around
) :
    Effect(matrix),
    m_x(x),
    m_y(y),
    m_dx(dx),
    m_dy(dy),
    m_use_transparency(use_transparency),
    m_static_idle(static_idle),
    m_wrap_around(wrap_around)
{
    if (decodeFromFs(filepath)) {
        m_x = resolveX(m_x, m_width) << 4;
        m_y = resolveY(m_y, m_height) << 4;
        m_last_frame_ms = millis();
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
    bool static_idle,
    bool wrap_around
) :
    Effect(matrix),
    m_x(x),
    m_y(y),
    m_dx(dx),
    m_dy(dy),
    m_use_transparency(use_transparency),
    m_static_idle(static_idle),
    m_wrap_around(wrap_around)
{
    if (decodeFromRam(png_data, png_size)) {
        m_x = resolveX(m_x, m_width) << 4;
        m_y = resolveY(m_y, m_height) << 4;
        m_last_frame_ms = millis();
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
    if (sz == 0 || sz > 262144) {
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
    if (!data || size < 8) return false;

    if (std::memcmp(data, PNG_SIG, 8) == 0 && decodeApngFromRam(data, size)) {
        return true;
    }
    return decodeStaticFromRam(data, size);
}

bool FlyingPng::decodeStaticFromRam(const uint8_t *data, size_t size)
{
    m_frames.clear();
    Frame frame;
    int w = 0, h = 0;
    bool has_alpha = false;
    if (!decodeSinglePngBuffer(data, size, w, h, frame.rgb, m_use_transparency ? &frame.alpha : nullptr, has_alpha)) {
        return false;
    }

    m_width = static_cast<int16_t>(w);
    m_height = static_cast<int16_t>(h);
    m_has_alpha = m_use_transparency && has_alpha;
    m_num_plays = 0;
    m_frames.push_back(std::move(frame));
    return true;
}

bool FlyingPng::decodeApngFromRam(const uint8_t *data, size_t size)
{
    // Scan PNG chunks to check for acTL + fcTL
    uint32_t ihdr_off = 0;
    bool has_actl = false;
    uint32_t num_plays = 0;
    std::vector<ChunkSlice> shared_chunks; // PLTE, tRNS
    std::vector<ApngFrameDesc> frame_descs;
    bool seen_idat = false;

    size_t pos = 8;
    while (pos + 12 <= size) {
        const uint32_t len = readBE32(data + pos);
        if (pos + 12 + len > size) break;

        const char *type = reinterpret_cast<const char *>(data + pos + 4);
        const uint32_t payload_off = static_cast<uint32_t>(pos + 8);

        if (std::memcmp(type, "IHDR", 4) == 0 && len == 13) {
            ihdr_off = payload_off;
        } else if (std::memcmp(type, "PLTE", 4) == 0 || std::memcmp(type, "tRNS", 4) == 0) {
            shared_chunks.push_back({payload_off, len, false});
        } else if (std::memcmp(type, "acTL", 4) == 0 && len >= 8) {
            has_actl = true;
            num_plays = readBE32(data + payload_off + 4);
        } else if (std::memcmp(type, "fcTL", 4) == 0 && len >= 26) {
            ApngFrameDesc fd;
            fd.width      = readBE32(data + payload_off + 4);
            fd.height     = readBE32(data + payload_off + 8);
            fd.x_offset   = readBE32(data + payload_off + 12);
            fd.y_offset   = readBE32(data + payload_off + 16);
            const uint16_t delay_num = readBE16(data + payload_off + 20);
            const uint16_t delay_den = readBE16(data + payload_off + 22);
            fd.dispose_op = data[payload_off + 24];
            fd.blend_op   = data[payload_off + 25];

            const uint32_t den = (delay_den == 0) ? 100 : delay_den;
            fd.delay_ms = (delay_num == 0) ? 10 : ((static_cast<uint32_t>(delay_num) * 1000UL + (den / 2)) / den);
            frame_descs.push_back(std::move(fd));
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            seen_idat = true;
            // IDAT only belongs to frame 0 if an fcTL preceded it
            if (!frame_descs.empty() && frame_descs.size() == 1) {
                frame_descs.back().data_chunks.push_back({payload_off, len, false});
            }
        } else if (std::memcmp(type, "fdAT", 4) == 0 && len > 4) {
            if (!frame_descs.empty()) {
                frame_descs.back().data_chunks.push_back({payload_off + 4, len - 4, true});
            }
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }

        pos += 12 + len;
    }

    (void)seen_idat;
    if (!has_actl || ihdr_off == 0 || frame_descs.size() <= 1) {
        return false; // Fall back to static PNG decoder
    }

    const int canvas_w = static_cast<int>(readBE32(data + ihdr_off));
    const int canvas_h = static_cast<int>(readBE32(data + ihdr_off + 4));
    if (canvas_w <= 0 || canvas_h <= 0 || canvas_w > 512 || canvas_h > 512) {
        return false;
    }

    const size_t canvas_pixels = static_cast<size_t>(canvas_w) * canvas_h;

    std::vector<uint16_t> canvas_rgb(canvas_pixels, 0);
    std::vector<uint8_t>  canvas_alpha(canvas_pixels, 0);
    std::vector<uint16_t> prev_rgb;
    std::vector<uint8_t>  prev_alpha;

    m_frames.clear();
    m_frames.reserve(frame_descs.size());
    bool any_alpha = false;

    std::vector<uint8_t> single_png;
    for (const auto &fd : frame_descs) {
        if (fd.width == 0 || fd.height == 0 || fd.data_chunks.empty()) {
            continue;
        }
        if (fd.x_offset + fd.width > static_cast<uint32_t>(canvas_w) ||
            fd.y_offset + fd.height > static_cast<uint32_t>(canvas_h)) {
            continue;
        }

        // Build a minimal single-frame PNG in memory
        single_png.clear();
        single_png.insert(single_png.end(), PNG_SIG, PNG_SIG + 8);

        uint8_t ihdr[13];
        std::memcpy(ihdr, data + ihdr_off, 13);
        writeBE32(ihdr, fd.width);
        writeBE32(ihdr + 4, fd.height);
        appendChunk(single_png, "IHDR", ihdr, 13);

        for (const auto &sc : shared_chunks) {
            const char *sc_type = reinterpret_cast<const char *>(data + sc.offset - 4);
            appendChunk(single_png, sc_type, data + sc.offset, sc.length);
        }
        for (const auto &dc : fd.data_chunks) {
            appendChunk(single_png, "IDAT", data + dc.offset, dc.length);
        }
        appendChunk(single_png, "IEND", nullptr, 0);

        int fw = 0, fh = 0;
        std::vector<uint16_t> frame_rgb;
        std::vector<uint8_t>  frame_alpha;
        bool frame_has_alpha = false;
        if (!decodeSinglePngBuffer(single_png.data(), single_png.size(), fw, fh, frame_rgb, &frame_alpha, frame_has_alpha)) {
            m_frames.clear();
            return false;
        }
        if (frame_has_alpha || fw < canvas_w || fh < canvas_h) any_alpha = true;

        if (fd.dispose_op == 2) {
            prev_rgb = canvas_rgb;
            prev_alpha = canvas_alpha;
        }

        // Composite onto canvas
        const int x0 = static_cast<int>(fd.x_offset);
        const int y0 = static_cast<int>(fd.y_offset);
        for (int fy = 0; fy < fh; ++fy) {
            for (int fx = 0; fx < fw; ++fx) {
                const size_t fidx = static_cast<size_t>(fy) * fw + fx;
                const uint8_t fa = frame_alpha[fidx];
                const size_t cidx = static_cast<size_t>(y0 + fy) * canvas_w + (x0 + fx);
                if (fd.blend_op == 1) {
                    if (fa == 0) {
                        continue;
                    } else if (fa == 255) {
                        canvas_rgb[cidx] = frame_rgb[fidx];
                        canvas_alpha[cidx] = 255;
                    } else {
                        canvas_rgb[cidx] = blendRgb565(frame_rgb[fidx], canvas_rgb[cidx], fa);
                        canvas_alpha[cidx] = static_cast<uint8_t>(
                            fa + (canvas_alpha[cidx] * (255 - fa) + 127) / 255);
                    }
                } else {
                    canvas_rgb[cidx] = frame_rgb[fidx];
                    canvas_alpha[cidx] = fa;
                }
            }
        }

        // Snapshot composed frame
        Frame out_frame;
        out_frame.rgb = canvas_rgb;
        out_frame.delay_ms = fd.delay_ms;
        if (m_use_transparency) {
            out_frame.alpha = canvas_alpha;
        }
        m_frames.push_back(std::move(out_frame));

        // Apply dispose_op for the next frame
        if (fd.dispose_op == 1) {
            // BACKGROUND: clear frame region to transparent black
            any_alpha = true;
            for (int fy = 0; fy < fh; ++fy) {
                for (int fx = 0; fx < fw; ++fx) {
                    const size_t cidx = static_cast<size_t>(y0 + fy) * canvas_w + (x0 + fx);
                    canvas_rgb[cidx] = 0;
                    canvas_alpha[cidx] = 0;
                }
            }
        } else if (fd.dispose_op == 2) {
            // PREVIOUS: restore canvas before this frame
            canvas_rgb = std::move(prev_rgb);
            canvas_alpha = std::move(prev_alpha);
        }
    }

    if (m_frames.empty()) {
        return false;
    }

    m_width = static_cast<int16_t>(canvas_w);
    m_height = static_cast<int16_t>(canvas_h);
    m_has_alpha = m_use_transparency && any_alpha;
    m_num_plays = num_plays;
    m_frame_idx = 0;
    m_plays_done = 0;
    return true;
}

void FlyingPng::setPosition(int16_t x, int16_t y)
{
    m_x = resolveX(x, m_width) << 4;
    m_y = resolveY(y, m_height) << 4;
}

void FlyingPng::render()
{
    if (!m_valid || m_frames.empty()) return;

    if (m_frames.size() == 1 && m_static_idle && m_dx == 0 && m_dy == 0) {
        if (m_frames_rendered >= 2) {
            return; // Front & back double-buffers are already painted!
        }
        ++m_frames_rendered;
    }

    // Advance animation frame based on elapsed time
    if (m_frames.size() > 1) {
        const uint32_t now = millis();
        const uint32_t delay = m_frames[m_frame_idx].delay_ms;
        if (now - m_last_frame_ms >= delay) {
            m_last_frame_ms = now;
            if (m_frame_idx + 1 < m_frames.size()) {
                ++m_frame_idx;
            } else if (m_num_plays == 0 || (m_plays_done + 1) < m_num_plays) {
                ++m_plays_done;
                m_frame_idx = 0;
            }
        }
    }

    const Frame &frame = m_frames[m_frame_idx];
    const int16_t cur_x = m_x >> 4;
    const int16_t cur_y = m_y >> 4;

    auto *hub75 = static_cast<MatrixPanel_I2S_DMA *>(&m_matrix);
    if (m_use_transparency && m_has_alpha && !frame.alpha.empty()) {
        FastHub75::drawRGBABitmap(
            *hub75,
            cur_x,
            cur_y,
            frame.rgb.data(),
            frame.alpha.data(),
            m_width,
            m_height
        );
    } else {
        FastHub75::drawRGBBitmap(
            *hub75,
            cur_x,
            cur_y,
            frame.rgb.data(),
            nullptr,
            m_width,
            m_height,
            false
        );
    }

    // Handle bouncing or wrap-around movement if dx/dy non-zero
    advanceAxis(m_x, m_dx, m_width, m_matrix.width(), m_wrap_around);
    advanceAxis(m_y, m_dy, m_height, m_matrix.height(), m_wrap_around);
}

bool FlyingPng::draw(Adafruit_GFX &matrix, const char *filepath, int16_t x, int16_t y, bool use_transparency)
{
    FlyingPng temp(matrix, filepath, x, y, 0, 0, use_transparency);
    if (!temp.isValid()) return false;
    temp.render();
    return true;
}

bool FlyingPng::draw(Adafruit_GFX &matrix, const uint8_t *png_data, size_t png_size, int16_t x, int16_t y, bool use_transparency)
{
    FlyingPng temp(matrix, png_data, png_size, x, y, 0, 0, use_transparency);
    if (!temp.isValid()) return false;
    temp.render();
    return true;
}
