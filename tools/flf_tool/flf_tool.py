#!/usr/bin/env python3
"""
TRSE FLF (FLUFF64) v1 subset: QImageBitmap 320x200, C64 palette (type bytes 0,0).

See docs/flf_png_converter_spec.md. Requires: pip install pillow
"""
from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError as e:
    print("flf_tool requires Pillow: pip install pillow", file=sys.stderr)
    raise SystemExit(1) from e

# TRSE LImageIO::m_ID — QString "FLUFF64" is 7 bytes (not 8).
MAGIC = b"FLUFF64"
HEADER_PREFIX = len(MAGIC) + 4 + 1 + 1  # 13
VERSION = 2
IMAGE_TYPE_QIMAGE = 0
PALETTE_TYPE_C64 = 0
WIDTH = 320
HEIGHT = 200
FOOTER_SIZE = 256
FOOTER_ID0 = 64
FOOTER_ID1 = 69
PAYLOAD = WIDTH * HEIGHT

# LColorList::InitC64() — source/LeLib/limage/lcolorlist.cpp (16 colours)
C64_PALETTE: list[tuple[int, int, int]] = [
    (0x00, 0x00, 0x00),
    (0xFF, 0xFF, 0xFF),
    (0x68, 0x37, 0x2B),
    (0x70, 0xA4, 0xB2),
    (0x6F, 0x3D, 0x86),
    (0x58, 0x8D, 0x43),
    (0x35, 0x28, 0x79),
    (0xB8, 0xC7, 0x6F),
    (0x90, 0x5F, 0x25),
    (0x43, 0x39, 0x00),
    (0x9A, 0x67, 0x59),
    (0x44, 0x44, 0x44),
    (0x6C, 0x6C, 0x6C),
    (0x9A, 0xD2, 0x84),
    (0x6C, 0x5E, 0xB5),
    (0x95, 0x95, 0x95),
]


def _dist2(a: tuple[int, int, int], b: tuple[int, int, int]) -> float:
    return float(sum((int(x) - int(y)) ** 2 for x, y in zip(a, b)))


def nearest_c64_index(r: int, g: int, b: int) -> int:
    best = 0
    best_d = math.inf
    for i, p in enumerate(C64_PALETTE):
        d = _dist2((r, g, b), p)
        if d < best_d:
            best_d = d
            best = i
    return best


def build_footer() -> bytes:
    foot = bytearray(FOOTER_SIZE)
    foot[0] = FOOTER_ID0
    foot[1] = FOOTER_ID1
    return bytes(foot)


def rgb_for_index(idx: int) -> tuple[int, int, int]:
    return C64_PALETTE[idx & 15]


def png_to_flf(png_path: Path, flf_path: Path) -> None:
    im = Image.open(png_path).convert("RGBA")
    im = im.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    data = bytearray(PAYLOAD)
    px = im.load()
    for y in range(HEIGHT):
        for x in range(WIDTH):
            r, g, b, a = px[x, y]
            if a < 128:
                idx = 0
            else:
                idx = nearest_c64_index(r, g, b)
            k = x + y * WIDTH
            data[k] = idx

    out = bytearray()
    out += MAGIC
    out += struct.pack("<i", VERSION)
    out += bytes([IMAGE_TYPE_QIMAGE, PALETTE_TYPE_C64])
    out += data
    out += build_footer()
    expect = HEADER_PREFIX + PAYLOAD + FOOTER_SIZE
    if len(out) != expect:
        raise RuntimeError(f"internal size mismatch: {len(out)} != {expect}")
    flf_path.write_bytes(out)


def flf_to_png(flf_path: Path, png_path: Path) -> None:
    raw = flf_path.read_bytes()
    need = HEADER_PREFIX + PAYLOAD + FOOTER_SIZE
    if len(raw) < need:
        raise ValueError(f"File too short ({len(raw)} bytes); expected at least {need}")
    if raw[: len(MAGIC)] != MAGIC:
        raise ValueError(f"Bad magic: expected {MAGIC!r}, got {raw[: len(MAGIC)]!r}")
    ver = struct.unpack_from("<i", raw, len(MAGIC))[0]
    if ver != VERSION:
        print(f"Warning: version is {ver}, expected {VERSION}", file=sys.stderr)
    off = len(MAGIC) + 4
    img_type = raw[off]
    pal_type = raw[off + 1]
    if img_type != IMAGE_TYPE_QIMAGE or pal_type != PALETTE_TYPE_C64:
        raise ValueError(
            f"Unsupported FLF for v1 tool: image_type={img_type}, palette_type={pal_type} "
            f"(need {IMAGE_TYPE_QIMAGE}, {PALETTE_TYPE_C64}). Other TRSE image types differ."
        )
    p0 = HEADER_PREFIX
    payload = raw[p0 : p0 + PAYLOAD]
    footer = raw[p0 + PAYLOAD : p0 + PAYLOAD + FOOTER_SIZE]
    if len(footer) != FOOTER_SIZE:
        raise ValueError("Footer missing or wrong size")
    if footer[0] != FOOTER_ID0 or footer[1] != FOOTER_ID1:
        print("Warning: footer ID bytes do not match TRSE default (64, 69)", file=sys.stderr)

    im = Image.new("RGBA", (WIDTH, HEIGHT))
    pix = im.load()
    for y in range(HEIGHT):
        for x in range(WIDTH):
            k = x + y * WIDTH
            idx = payload[k]
            r, g, b = rgb_for_index(idx)
            pix[x, y] = (r, g, b, 255)
    im.save(png_path, "PNG")


def main() -> None:
    p = argparse.ArgumentParser(description="TRSE FLF v1 (320x200 QImageBitmap, C64) ↔ PNG")
    sub = p.add_subparsers(dest="cmd", required=True)

    p2f = sub.add_parser("png2flf", help="Convert PNG to .flf")
    p2f.add_argument("input", type=Path)
    p2f.add_argument("output", type=Path)

    f2p = sub.add_parser("flf2png", help="Extract PNG from .flf")
    f2p.add_argument("input", type=Path)
    f2p.add_argument("output", type=Path)

    args = p.parse_args()
    if args.cmd == "png2flf":
        png_to_flf(args.input, args.output)
    else:
        flf_to_png(args.input, args.output)


if __name__ == "__main__":
    main()
