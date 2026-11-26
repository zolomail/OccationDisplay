#!/usr/bin/env python3
"""
PNG to 8-bit Signed Distance Field (SDF) C++ header converter.

Reads one or more PNG images, computes an exact 2D Euclidean Signed Distance
Field (negative inside, positive outside) at the target atlas resolution
(default 32x32), quantizes distances as int8_t in Q4 texel units (16 = 1 texel),
and writes a C++ header ready for SDFEffect.

Usage:
    python sdf_converter.py input.png -o output.h -n MyShape --size 32
    python sdf_converter.py star.png shamrock.png diamond.png heart.png -o ../src/bitmaps/SdfAtlases.h
"""

import argparse
import glob
import math
import os
from typing import List, Tuple
from PIL import Image


def load_png_mask(path: str) -> Tuple[int, int, List[List[bool]]]:
    """Load PNG via Pillow and return (width, height, inside_mask[y][x])."""
    img = Image.open(path).convert("RGBA")
    width, height = img.size
    px = img.load()
    has_transparent = any(px[x, y][3] < 255 for y in range(height) for x in range(width))

    mask: List[List[bool]] = [[False] * width for _ in range(height)]
    for y in range(height):
        for x in range(width):
            r, g, b, a = px[x, y]
            mask[y][x] = (a >= 128) if has_transparent else ((r + g + b) >= 128)
    return width, height, mask


def _edt_1d(f: List[float], n: int) -> List[float]:
    """Felzenszwalb & Huttenlocher O(n) 1D squared Euclidean distance transform."""
    inf = 1e18
    d = [0.0] * n
    v = [0] * n
    z = [0.0] * (n + 1)
    k = 0
    v[0] = 0
    z[0] = -inf
    z[1] = inf

    for q in range(1, n):
        while True:
            r = v[k]
            s = ((f[q] + q * q) - (f[r] + r * r)) / (2.0 * (q - r))
            if s > z[k]:
                break
            k -= 1
        k += 1
        v[k] = q
        z[k] = s
        z[k + 1] = inf

    k = 0
    for q in range(n):
        while z[k + 1] < q:
            k += 1
        dx = q - v[k]
        d[q] = dx * dx + f[v[k]]
    return d


def compute_edt_2d(binary_grid: List[List[bool]]) -> List[List[float]]:
    """Compute exact 2D Euclidean distance to the nearest True pixel."""
    h = len(binary_grid)
    w = len(binary_grid[0])
    inf = 1e18

    tmp = [[0.0] * w for _ in range(h)]
    col = [0.0] * h
    for x in range(w):
        for y in range(h):
            col[y] = 0.0 if binary_grid[y][x] else inf
        d_col = _edt_1d(col, h)
        for y in range(h):
            tmp[y][x] = d_col[y]

    dist = [[0.0] * w for _ in range(h)]
    for y in range(h):
        d_row = _edt_1d(tmp[y], w)
        for x in range(w):
            dist[y][x] = math.sqrt(d_row[x])
    return dist


def generate_sdf_atlas(
    png_path: str, size: int = 32, scale_q4: int = 16, pad_frac: float = 0.20
) -> List[int]:
    """Compute a size x size int8_t SDF atlas (negative inside, positive outside).

    Automatically crops to the shape's bounding box and adds pad_frac transparent
    margin on all sides so shapes without borders never clip or corrupt at atlas edges.
    """
    w0, h0, raw_inside = load_png_mask(png_path)

    xs = [x for y in range(h0) for x in range(w0) if raw_inside[y][x]]
    ys = [y for y in range(h0) for x in range(w0) if raw_inside[y][x]]
    if xs and ys:
        x_min, x_max = min(xs), max(xs)
        y_min, y_max = min(ys), max(ys)
        bw = x_max - x_min + 1
        bh = y_max - y_min + 1
        side = max(bw, bh)
        pad = max(1, int(round(side * pad_frac)))
        w = side + 2 * pad
        h = side + 2 * pad
        ox = pad + (side - bw) // 2 - x_min
        oy = pad + (side - bh) // 2 - y_min
        inside = [[False] * w for _ in range(h)]
        for y in range(y_min, y_max + 1):
            for x in range(x_min, x_max + 1):
                if raw_inside[y][x]:
                    inside[y + oy][x + ox] = True
    else:
        w, h, inside = w0, h0, raw_inside

    outside = [[not inside[y][x] for x in range(w)] for y in range(h)]

    dist_to_inside = compute_edt_2d(inside)
    dist_to_outside = compute_edt_2d(outside)

    # Signed distance in source pixels, shifted by 0.5 at the boundary
    sdf_src = [[0.0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if inside[y][x]:
                sdf_src[y][x] = -(dist_to_outside[y][x] - 0.5)
            else:
                sdf_src[y][x] = dist_to_inside[y][x] - 0.5

    # Sample into target atlas using bilinear interpolation, scaled to target texels
    scale_x = w / float(size)
    scale_y = h / float(size)
    avg_scale = 0.5 * (scale_x + scale_y)

    atlas: List[int] = []
    for ty in range(size):
        sy = (ty + 0.5) * scale_y - 0.5
        y0 = max(0, min(h - 1, int(math.floor(sy))))
        y1 = max(0, min(h - 1, y0 + 1))
        fy = max(0.0, min(1.0, sy - y0))
        for tx in range(size):
            sx = (tx + 0.5) * scale_x - 0.5
            x0 = max(0, min(w - 1, int(math.floor(sx))))
            x1 = max(0, min(w - 1, x0 + 1))
            fx = max(0.0, min(1.0, sx - x0))

            v00 = sdf_src[y0][x0]
            v10 = sdf_src[y0][x1]
            v01 = sdf_src[y1][x0]
            v11 = sdf_src[y1][x1]
            d_src = (
                (v00 * (1.0 - fx) + v10 * fx) * (1.0 - fy)
                + (v01 * (1.0 - fx) + v11 * fx) * fy
            )
            d_texels = d_src / avg_scale
            q = int(round(d_texels * scale_q4))
            q = max(-127, min(127, q))
            atlas.append(q)

    return atlas


def sanitize_symbol(name: str) -> str:
    base = os.path.splitext(os.path.basename(name))[0]
    cleaned = "".join(c if c.isalnum() else "_" for c in base)
    if not cleaned or cleaned[0].isdigit():
        cleaned = "Sdf_" + cleaned
    return cleaned


def write_header(
    entries: List[Tuple[str, int, int, List[int]]], out_path: str
) -> None:
    lines = [
        "#pragma once",
        "#include <cstdint>",
        "",
        "struct SdfAtlas {",
        "    const int8_t *data;",
        "    uint16_t width;",
        "    uint16_t height;",
        "    uint8_t  scale_q4; // SDF units per texel (e.g. 16 = 1.0 texel)",
        "};",
        "",
    ]

    for sym, size, scale_q4, atlas in entries:
        arr_name = f"SDF_{sym.upper()}_DATA"
        lines.append(f"constexpr int8_t {arr_name}[{size * size}] = {{")
        for row in range(size):
            slice_vals = atlas[row * size : (row + 1) * size]
            lines.append("    " + ", ".join(f"{v:4d}" for v in slice_vals) + ",")
        lines.append("};")
        lines.append("")
        lines.append(
            f"constexpr SdfAtlas SDF_{sym.upper()} = {{ {arr_name}, {size}, {size}, {scale_q4} }};"
        )
        lines.append("")

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert PNG image(s) into pre-baked 8-bit SDF C++ atlas header."
    )
    parser.add_argument("inputs", nargs="+", help="Input PNG file(s)")
    parser.add_argument(
        "-o", "--output", required=True, help="Output C++ header path"
    )
    parser.add_argument(
        "-n", "--name", help="Symbol name (only when converting a single PNG)"
    )
    parser.add_argument(
        "--size", type=int, default=32, help="Target SDF atlas size (default: 32)"
    )
    parser.add_argument(
        "--scale-q4",
        type=int,
        default=16,
        help="SDF quantization units per texel (default: 16)",
    )
    parser.add_argument(
        "--pad",
        type=float,
        default=0.20,
        help="Transparent border padding fraction around shape (default: 0.20)",
    )
    args = parser.parse_args()

    expanded_inputs: List[str] = []
    for pattern in args.inputs:
        matches = sorted(glob.glob(pattern))
        expanded_inputs.extend(matches if matches else [pattern])

    if args.name and len(expanded_inputs) > 1:
        parser.error("--name can only be used with a single input PNG")

    entries = []
    for path in expanded_inputs:
        sym = sanitize_symbol(args.name if args.name else path)
        atlas = generate_sdf_atlas(
            path, size=args.size, scale_q4=args.scale_q4, pad_frac=args.pad
        )
        entries.append((sym, args.size, args.scale_q4, atlas))
        print(f"Converted {path} -> SDF_{sym.upper()} ({args.size}x{args.size})")

    write_header(entries, args.output)
    print(f"Wrote {args.output}")


if __name__ == "__main__":
    main()
