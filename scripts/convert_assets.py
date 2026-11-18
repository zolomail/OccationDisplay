import os
import re
import math
import zlib
import struct

def write_png(filename, width, height, rgba_data):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
    
    raw = bytearray()
    for y in range(height):
        raw.append(0) # Filter byte: None
        raw.extend(rgba_data[y * width * 4 : (y + 1) * width * 4])
        
    png = (
        b'\x89PNG\r\n\x1a\n'
        + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
        + chunk(b'IDAT', zlib.compress(bytes(raw), 9))
        + chunk(b'IEND', b'')
    )
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    with open(filename, 'wb') as f:
        f.write(png)
    print(f"Created {filename} ({width}x{height}, {len(png)} bytes)")

def rgb565_to_rgba(c565, alpha=255):
    r = ((c565 >> 11) & 0x1F) * 255 // 31
    g = ((c565 >> 5) & 0x3F) * 255 // 63
    b = (c565 & 0x1F) * 255 // 31
    return r, g, b, alpha

def parse_c_array(header_path, array_name):
    with open(header_path, 'r', encoding='utf-8') as f:
        content = f.read()
    pattern = rf'{array_name}\s*(\[\])?\s*(\w+)?\s*=\s*\{{([^}}]+)\}}'
    m = re.search(pattern, content, re.DOTALL)
    if not m:
        raise ValueError(f"Array {array_name} not found in {header_path}")
    raw_nums = m.group(3)
    # Strip single-line and multi-line C/C++ comments so comment numbers aren't parsed
    raw_nums = re.sub(r'//.*', '', raw_nums)
    raw_nums = re.sub(r'/\*.*?\*/', '', raw_nums, flags=re.DOTALL)
    values = []
    for token in re.finditer(r'0x[0-9a-fA-F]+|\d+', raw_nums):
        val_str = token.group(0)
        values.append(int(val_str, 16 if val_str.startswith('0x') else 10))
    return values

# 1. Convert UFO (16x10)
ufo_bmp = parse_c_array('src/bitmaps/ufo.h', 'ufo_bitmap')
ufo_mask = parse_c_array('src/bitmaps/ufo.h', 'ufo_mask')
ufo_rgba = bytearray()
w, h = 16, 10
row_bytes = (w + 7) // 8
for y in range(h):
    for x in range(w):
        c565 = ufo_bmp[y * w + x]
        mask_byte = ufo_mask[y * row_bytes + (x // 8)]
        bit = (mask_byte >> (7 - (x % 8))) & 1
        a = 255 if bit else 0
        r, g, b, a = rgb565_to_rgba(c565, a)
        ufo_rgba.extend([r, g, b, a])
write_png('data/bitmaps/ufo.png', w, h, ufo_rgba)

# 2. Convert Pumpkin (19x18)
p_bmp = parse_c_array('src/bitmaps/pumpkin.h', 'pumpkin_bitmap')
p_mask = parse_c_array('src/bitmaps/pumpkin.h', 'pumpkin_mask')
p_rgba = bytearray()
w, h = 19, 18
row_bytes = (w + 7) // 8
for y in range(h):
    for x in range(w):
        c565 = p_bmp[y * w + x]
        mask_byte = p_mask[y * row_bytes + (x // 8)]
        bit = (mask_byte >> (7 - (x % 8))) & 1
        a = 255 if bit else 0
        r, g, b, a = rgb565_to_rgba(c565, a)
        p_rgba.extend([r, g, b, a])
write_png('data/bitmaps/pumpkin.png', w, h, p_rgba)

# 3. Convert Ghost (33x27)
g_bmp = parse_c_array('src/bitmaps/ghost.h', 'ghost_bitmap')
g_mask = parse_c_array('src/bitmaps/ghost.h', 'ghost_mask')
g_rgba = bytearray()
w, h = 33, 27
row_bytes = (w + 7) // 8
for y in range(h):
    for x in range(w):
        c565 = g_bmp[y * w + x]
        mask_byte = g_mask[y * row_bytes + (x // 8)]
        bit = (mask_byte >> (7 - (x % 8))) & 1
        a = 255 if bit else 0
        r, g, b, a = rgb565_to_rgba(c565, a)
        g_rgba.extend([r, g, b, a])
write_png('data/bitmaps/ghost.png', w, h, g_rgba)

# 4. Convert Champagne / New Year (34x32)
c_bmp = parse_c_array('src/bitmaps/champagne.h', 'newyear_bitmap')
c_rgba = bytearray()
w, h = 34, 32
for y in range(h):
    for x in range(w):
        c565 = c_bmp[y * w + x]
        a = 0 if c565 == 0 else 255
        r, g, b, a = rgb565_to_rgba(c565, a)
        c_rgba.extend([r, g, b, a])
write_png('data/bitmaps/champagne.png', w, h, c_rgba)

# 5. WeatherOverlay Icons
# Parse palette from WeatherOverlay.cpp
def c565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

palette = {
    '_': 0x0000,
    'SY': c565(255, 230,  50),
    'SH': c565(255, 249, 191),
    'SO': c565(255, 165,   0),
    'ML': c565(255, 240, 150),
    'MC': c565(205, 190, 115),
    'MD': c565( 28,  36,  64),
    'ME': c565( 65,  80, 125),
    'ST': c565(180, 215, 255),
    'CW': c565(245, 250, 255),
    'CL': c565(143, 191, 250),
    'CB': c565( 95, 150, 225),
    'CN': c565(110, 145, 205),
    'CD': c565( 65,  90, 145),
    'RB': c565( 40, 140, 255),
    'SW': c565(255, 255, 255),
    'SC': c565(130, 225, 255),
}

with open('src/effects/WeatherOverlay.cpp', 'r', encoding='utf-8') as f:
    wo_content = f.read()

icons_to_extract = [
    ('ICON_SUNNY_DAY', 'sunny_day.png'),
    ('ICON_CLOUDY_DAY', 'cloudy_day.png'),
    ('ICON_CLOUDY_NIGHT', 'cloudy_night.png'),
    ('ICON_RAINY_DAY', 'rainy_day.png'),
    ('ICON_RAINY_NIGHT', 'rainy_night.png'),
    ('ICON_SNOW_DAY', 'snow_day.png'),
    ('ICON_SNOW_NIGHT', 'snow_night.png'),
    ('ICON_HAIL_DAY', 'hail_day.png'),
    ('ICON_HAIL_NIGHT', 'hail_night.png'),
]

for var_name, out_file in icons_to_extract:
    pat = rf'{var_name}\s*\[\s*16\s*\*\s*16\s*\]\s*=\s*\{{([^}}]+)\}}'
    m = re.search(pat, wo_content)
    if not m:
        print(f"Warning: {var_name} not found!")
        continue
    tokens = [t.strip() for t in m.group(1).replace('\n', ',').split(',') if t.strip()]
    rgba = bytearray()
    for tok in tokens:
        c565_val = palette.get(tok, 0)
        a = 0 if tok == '_' else 255
        r, g, b, a = rgb565_to_rgba(c565_val, a)
        rgba.extend([r, g, b, a])
    write_png(f'data/weather/{out_file}', 16, 16, rgba)

# 6. Generate 8 Moon Phases (16x16)
moon_names = [
    'moon_new.png',
    'moon_waxing_crescent.png',
    'moon_first_quarter.png',
    'moon_waxing_gibbous.png',
    'moon_full.png',
    'moon_waning_gibbous.png',
    'moon_last_quarter.png',
    'moon_waning_crescent.png'
]

CX = 7.5
CY = 7.5
R = 5.8
R2 = R * R

for phase in range(8):
    # Initialize 16x16 with transparent
    grid = [[0 for _ in range(16)] for _ in range(16)]
    
    # Twinkling stars
    stars = [(1, 2), (14, 2), (13, 13), (2, 12)]
    for sx, sy in stars:
        grid[sy][sx] = palette['ST']
        
    for r in range(1, 15):
        dy = float(r) - CY
        dy2 = dy * dy
        if dy2 >= R2:
            continue
        span = math.sqrt(R2 - dy2)
        
        for c in range(1, 15):
            dx = float(c) - CX
            if dx * dx + dy2 > R2:
                continue
            
            lit = False
            if phase == 0:
                lit = False
            elif phase == 1:
                lit = (dx >= 0.40 * span)
            elif phase == 2:
                lit = (dx >= 0.0)
            elif phase == 3:
                lit = (dx >= -0.45 * span)
            elif phase == 4:
                lit = True
            elif phase == 5:
                lit = (dx <= 0.45 * span)
            elif phase == 6:
                lit = (dx <= 0.0)
            elif phase == 7:
                lit = (dx <= -0.40 * span)
                
            col = palette['MD']
            if lit:
                is_crater = ((5 <= r <= 6 and 9 <= c <= 10) or
                             (9 <= r <= 10 and 5 <= c <= 6) or
                             (r == 11 and c == 9))
                is_highlight = (dy < -1.5 and dx < 1.5)
                col = palette['MC'] if is_crater else (palette['SH'] if is_highlight else palette['ML'])
            elif phase == 0 and (dx * dx + dy2 >= (R - 1.1) * (R - 1.1)):
                col = palette['ME']
                
            grid[r][c] = col
            
    rgba = bytearray()
    for y in range(16):
        for x in range(16):
            c_val = grid[y][x]
            a = 0 if c_val == 0 else 255
            r_val, g_val, b_val, a_val = rgb565_to_rgba(c_val, a)
            rgba.extend([r_val, g_val, b_val, a_val])
            
    write_png(f'data/weather/{moon_names[phase]}', 16, 16, rgba)

print("All bitmaps and weather icons successfully converted to PNG in data/!")
