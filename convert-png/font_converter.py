#!/usr/bin/env python3
"""
PNG sprite-sheet font to Adafruit_GFX header converter.

Each glyph in the PNG is separated by a 1-pixel-wide vertical column whose top
pixel (y = 0) is pure red (255, 0, 0). Leading empty slots before 0x20 (space)
are trimmed, and identical/overlapping glyph bitmaps are deduplicated.

Usage:
    python font_converter.py TinyWide.png -o ../src/fonts/TinyWide.h
    python font_converter.py TinyWide.png SmallWide.png ZoloMicro.png wide.png --out-dir ../src/fonts
"""

import argparse
import glob
import os
from typing import List, Optional
from PIL import Image


class Char:
    def __init__(self, ascii_num: int):
        self.ascii = ascii_num
        self.bitmapOffset = 0
        self.width = 0
        self.height = 0
        self.xAdvance = 0
        self.xMax = 0
        self.yMax = 0
        self.xOffset = 99999
        self.yOffset = 99999
        self.data: List[int] = []

    def isEmpty(self) -> bool:
        return self.xAdvance == 0

    def getData(self) -> bytes:
        xrs = self.xAdvance - self.xMax - 1
        total_data = 0
        for line_data in self.data[self.yOffset : self.yOffset + self.height]:
            line_data >>= xrs
            total_data <<= self.width
            total_data |= line_data
        total_data <<= (8 - (self.width * self.height % 8)) % 8
        return total_data.to_bytes((self.width * self.height + 7) // 8, byteorder="big")

    def getMask(self) -> bytes:
        total_data = (2 ** (self.width * self.height)) - 1
        total_data <<= (8 - (self.width * self.height % 8)) % 8
        return total_data.to_bytes((self.width * self.height + 7) // 8, byteorder="big")


def find_masked(all_data: bytearray, all_mask: bytearray, data: bytes, mask: bytes) -> Optional[int]:
    for i in range(len(all_data) - len(data) + 1):
        for n, j in enumerate(data):
            overlap = all_mask[i + n] & mask[n]
            if (j & overlap) != (all_data[i + n] & overlap):
                break
        else:
            return i
    return None


def convert_font(fontfilepath: str, out_path: str, symbol_name: Optional[str] = None) -> None:
    fontname = symbol_name if symbol_name else os.path.splitext(os.path.basename(fontfilepath))[0]
    image = Image.open(fontfilepath).convert("RGBA")
    width_px, height_px = image.size
    pixels = image.load()

    curr_char = Char(0)
    font_chars: List[Char] = []
    for x in range(width_px):
        if pixels[x, 0][0] == 255 and pixels[x, 0][1] == 0 and pixels[x, 0][2] == 0:
            for y in range(height_px):
                line_data = 0
                for xx in range(curr_char.xAdvance):
                    line_data <<= 1
                    if pixels[x + xx - curr_char.xAdvance, y][3] > 128:
                        curr_char.yOffset = min(curr_char.yOffset, y)
                        curr_char.yMax = max(curr_char.yMax, y)
                        curr_char.xOffset = min(curr_char.xOffset, xx)
                        curr_char.xMax = max(curr_char.xMax, xx)
                        line_data |= 1
                curr_char.data.append(line_data)
            curr_char.xOffset = 1 if curr_char.xOffset == 99999 else curr_char.xOffset
            curr_char.yOffset = 1 if curr_char.yOffset == 99999 else curr_char.yOffset
            curr_char.width = curr_char.xMax + 1 - curr_char.xOffset
            curr_char.height = curr_char.yMax + 1 - curr_char.yOffset
            font_chars.append(curr_char)
            curr_char = Char(len(font_chars))
        else:
            curr_char.xAdvance += 1

    first_char = next(c for c in font_chars if not c.isEmpty())
    font_chars = font_chars[first_char.ascii :]

    all_data = bytearray()
    all_mask = bytearray()
    compression = 0
    for c in font_chars:
        data = c.getData()
        mask = c.getMask()
        pos = find_masked(all_data, all_mask, data, mask)
        if pos is not None:
            for k in range(len(data)):
                all_data[pos + k] = (all_data[pos + k] & ~mask[k]) | (data[k] & mask[k])
                all_mask[pos + k] |= mask[k]
            c.bitmapOffset = pos
            compression += len(data)
            continue
        c.bitmapOffset = len(all_data)
        all_data += data
        all_mask += mask

    lines = [
        f"const uint8_t {fontname}Bitmaps[] PROGMEM = {{",
        "\t" + ",".join([f"0x{d:02x}" for d in all_data] + [""]) + "\t",
        "};",
        "",
        "/* {bitmapOffset, width, height, xAdvance, xOffset, yOffset} */",
        f"const GFXglyph {fontname}Glyphs[] PROGMEM = {{",
    ]
    for c in font_chars:
        ch = chr(c.ascii) if 31 < c.ascii < 0x7F else " "
        lines.append(
            f"\t{{ {c.bitmapOffset}, {c.width}, {c.height}, {c.xAdvance}, {c.xOffset}, {c.yOffset} }},  // {ch} : 0x{c.ascii:02x}"
        )
    lines.extend([
        "};",
        "",
        f"const GFXfont {fontname} PROGMEM = {{",
        f"\t(uint8_t  *){fontname}Bitmaps,",
        f"\t(GFXglyph *){fontname}Glyphs,",
        f"\t0x{font_chars[0].ascii:02x},",
        f"\t0x{font_chars[-1].ascii:02x},",
        f"\t{height_px}",
        "};",
        "",
    ])

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))

    print(
        f"{fontname} ({width_px}x{height_px}): 0x{font_chars[0].ascii:02x}..0x{font_chars[-1].ascii:02x} "
        f"({len(font_chars)} glyphs), {len(all_data)} bytes (saved {compression} bytes) -> {out_path}"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert PNG font sheet(s) to Adafruit_GFX C++ header(s).")
    parser.add_argument("inputs", nargs="+", help="Input PNG file(s) (globs supported)")
    parser.add_argument("-o", "--output", help="Output header path (single input only)")
    parser.add_argument("--out-dir", help="Output directory when converting one or more PNGs")
    parser.add_argument("-n", "--name", help="C++ font symbol name (single input only)")
    args = parser.parse_args()

    expanded: List[str] = []
    for pat in args.inputs:
        matches = sorted(glob.glob(pat))
        expanded.extend(matches if matches else [pat])

    if (args.output or args.name) and len(expanded) > 1:
        parser.error("--output and --name can only be used with a single input PNG")

    for path in expanded:
        base = os.path.splitext(os.path.basename(path))[0]
        if args.output:
            out_path = args.output
        elif args.out_dir:
            # Preserve capitalized header filename (e.g. wide.png -> Wide.h)
            hdr_name = base[0].upper() + base[1:] + ".h"
            out_path = os.path.join(args.out_dir, hdr_name)
        else:
            out_path = os.path.join(os.path.dirname(os.path.abspath(path)), "..", "src", "fonts", base[0].upper() + base[1:] + ".h")
        convert_font(path, out_path, symbol_name=args.name)


if __name__ == "__main__":
    main()
