#!/usr/bin/env python3
"""
TRSE FLF (FLUFF64): C64 palette — flf2png decodes types 0 (QImageBitmap), 1 (MultiColor),
2 (Hires C64), and 10 (Sprites2). png2flf writes type 0 only.

See docs/flf_png_converter_spec.md. Requires: pip install pillow
"""
from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path
from typing import Optional, Tuple

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
IMAGE_TYPE_MULTICOLOR_C64 = 1
IMAGE_TYPE_HIRES_C64 = 2  # StandardColorImage — MultiColorImage with m_bitMask=1, m_scale=1 (320×200)
IMAGE_TYPE_SPRITES2 = 10
PALETTE_TYPE_C64 = 0

# LSprite — limagesprites2.h / limagesprites2.cpp
SPRITE_HEADER_SIZE = 16
SPRITE_PC_WIDTH = 3
SPRITE_PC_HEIGHT = 3
PENS_BIN_SIZE = 5  # CharsetImage::SavePensBin
WIDTH = 320
HEIGHT = 200
FOOTER_SIZE = 256
FOOTER_ID0 = 64
FOOTER_ID1 = 69
# limagefooter.h — saved with every .flf; TRSE uses this for MultiColorBitmap hires vs multicolor display
FOOTER_POS_DISPLAY_MULTICOLOR = 5
PAYLOAD = WIDTH * HEIGHT
V1_TOTAL = HEADER_PREFIX + PAYLOAD + FOOTER_SIZE

# MultiColorImage::SaveBin — source/LeLib/limage/multicolorimage.cpp
# 2-byte header + 1000 * sizeof(PixelChar) where PixelChar is 8 + 4 = 12 bytes.
MC_CHAR_WIDTH = 40
MC_CHAR_HEIGHT = 25
MC_PIXELCHAR_BYTES = 12
MC_BITMASK = 0b11  # multicolor mode in TRSE
MC_SCALE = 2
MC_WIDTH = 160
MC_HEIGHT = 200
MC_PAYLOAD = 2 + MC_CHAR_WIDTH * MC_CHAR_HEIGHT * MC_PIXELCHAR_BYTES  # 12002
MC_TOTAL = HEADER_PREFIX + MC_PAYLOAD + FOOTER_SIZE

# Hires C64 uses same SaveBin payload size as multicolor (1000 PixelChars); TRSE getPixel uses bitmask 1, scale 1.
HIRES_BITMASK = 0b1
HIRES_WIDTH = 320

# LImage::TypeToString — source/LeLib/limage/limage.cpp (subset)
IMAGE_TYPE_NAMES: dict[int, str] = {
    0: "QImageBitmap",
    1: "MultiColorBitmap (C64)",
    2: "HiresBitmap",
    3: "LevelEditor",
    4: "CharMapMulticolor",
    5: "Sprites",
    6: "CharmapRegular",
    7: "FullScreenChar",
    8: "CharMapMultiColorFixed",
    9: "VIC20_MultiColorbitmap",
    10: "Sprites2",
    11: "CGA",
    12: "AMIGA320x200",
    13: "AMIGA320x256",
    14: "OK64_256x256",
    15: "X16_640x480",
    16: "NES",
    17: "LMetaChunk",
    18: "LevelEditorNES",
    19: "SpritesNES",
    20: "GAMEBOY",
    21: "LevelEditorGameboy",
    22: "ATARI320x200",
    23: "HybridCharset",
    24: "AmstradCPC",
    25: "AmstradCPCGeneric",
    26: "BBC",
    27: "VGA",
    28: "Spectrum",
    29: "SNES",
    30: "LevelEditorSNES",
    31: "VZ200",
    32: "CustomC64",
    33: "JDH8",
    34: "LImageGeneric",
    35: "GenericSprites",
    36: "CGA160x100",
    37: "AmstradSprites",
    38: "SNESGeneric",
    39: "TIM",
    40: "TVC",
    41: "COCO3",
    42: "THOMSON",
    43: "TIMG",
    44: "LevelEditorGeneric",
    45: "AGON",
    46: "PRIMO",
    47: "CGA_HIRES",
}

def _hint_wrong_file_for_flf2png(raw: bytes) -> str:
    """Extra help when input is not FLUFF64 (usually wrong subcommand or file type)."""
    if not raw:
        return "The file is empty."
    if raw.startswith(b"\x89PNG"):
        return (
            "The file looks like a PNG image, not a TRSE .flf.\n"
            "  • PNG → .flf:  python3 flf_tool.py png2flf  <image.png>  <out.flf>\n"
            "  • .flf → PNG:  python3 flf_tool.py flf2png  <in.flf>   <out.png>"
        )
    if raw.startswith((b"GIF87a", b"GIF89a")):
        return (
            "The file looks like a GIF. flf2png expects a .flf file. "
            "Convert to PNG first, then use png2flf if you need a .flf."
        )
    if raw.startswith(b"\xff\xd8\xff"):
        return (
            "The file looks like a JPEG. flf2png expects a .flf file. "
            "Use a PNG workflow (convert to PNG, then png2flf if needed)."
        )
    if raw.startswith(b"BM"):
        return "The file looks like a BMP. flf2png expects a TRSE .flf (FLUFF64), not a bitmap image."
    return (
        f"TRSE .flf files must begin with the ASCII magic {MAGIC.decode('ascii')!r} (7 bytes)."
    )


def _hint_wrong_file_for_png2flf(raw: bytes) -> None:
    """Raise ValueError if the user passed a .flf where png2flf expects PNG."""
    if raw.startswith(MAGIC):
        raise ValueError(
            "This file looks like a TRSE .flf, not a PNG image.\n"
            "  • .flf → PNG:  python3 flf_tool.py flf2png  <in.flf>   <out.png>\n"
            "  • PNG → .flf:  python3 flf_tool.py png2flf  <image.png>  <out.flf>"
        )


PALETTE_TYPE_NAMES: dict[int, str] = {
    0: "C64",
    1: "C64_ORG",
    2: "CGA1_LOW",
    3: "CGA1_HIGH",
    4: "CGA2_LOW",
    5: "CGA2_HIGH",
    6: "VIC20",
    7: "PICO8",
    8: "OK64",
    9: "X16",
    10: "NES",
    11: "AMSTRADCPC",
    12: "BBC",
    13: "VGA",
    14: "SPECTRUM",
    15: "VZ200",
    16: "DOS",
    17: "TIM",
    18: "TVC",
    19: "COCO3",
    20: "THOMSON",
    21: "MONO",
}


def footer_bytes(raw: bytes) -> bytes:
    """Last 256 bytes of a complete .flf (LImageFooter)."""
    if len(raw) < FOOTER_SIZE:
        return b""
    return raw[-FOOTER_SIZE:]


def multicolor_file_is_hires(raw: bytes) -> bool:
    """
    MultiColorBitmap (.flf image_type=1) stores the same SaveBin for hires and multicolor.
    TRSE records the mode in the footer: POS_DISPLAY_MULTICOLOR==0 → hires (320×200), ==1 → multicolor.
    See formimageeditor.cpp / limagefooter.h.
    """
    foot = footer_bytes(raw)
    if len(foot) < FOOTER_POS_DISPLAY_MULTICOLOR + 1:
        return True
    return foot[FOOTER_POS_DISPLAY_MULTICOLOR] == 0


def describe_flf_header(raw: bytes) -> tuple[int, int, int, str]:
    """Return (version, image_type, palette_type, summary_line). Raises ValueError if too small or bad magic."""
    if len(raw) < HEADER_PREFIX:
        extra = ""
        if not raw or raw.startswith(b"\x89PNG") or raw.startswith(b"GIF"):
            extra = "\n\n" + _hint_wrong_file_for_flf2png(raw)
        raise ValueError(
            f"Not a TRSE .flf file: too small ({len(raw)} bytes); "
            f"need at least {HEADER_PREFIX} bytes for the FLUFF64 header.{extra}"
        )
    if raw[: len(MAGIC)] != MAGIC:
        raise ValueError(
            "Not a TRSE .flf file (missing FLUFF64 magic at the start of the file).\n\n"
            + _hint_wrong_file_for_flf2png(raw)
        )
    ver = struct.unpack_from("<i", raw, len(MAGIC))[0]
    off = len(MAGIC) + 4
    img_type = raw[off]
    pal_type = raw[off + 1]
    iname = IMAGE_TYPE_NAMES.get(img_type, f"unknown({img_type})")
    pname = PALETTE_TYPE_NAMES.get(pal_type, f"unknown({pal_type})")
    line = f"FLUFF64 version={ver} image_type={img_type} ({iname}) palette_type={pal_type} ({pname}), file size={len(raw)} bytes"
    return ver, img_type, pal_type, line


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


def pixelchar_get(
    x: int, y: int, bit_mask: int, p: bytes, c: tuple[int, int, int, int]
) -> int:
    """Match PixelChar::get (pixelchar.cpp) — index into c[] from packed multicolor/hires bits."""
    if x < 0 or x >= 8 or y < 0 or y >= 8:
        return 0
    pp = (p[y] >> x) & bit_mask
    return c[pp]


def lsprite_get_set_data(
    x: float,
    y: float,
    bit_mask: int,
    sx: int,
    sy: int,
    m_data: list[tuple[bytes, tuple[int, int, int, int]]],
) -> Optional[Tuple[int, int, Tuple[bytes, Tuple[int, int, int, int]]]]:
    """LSprite::GetSetData (limagesprites2.cpp) — x,y in [0,1). Returns (ix, iy_line, pc) or None."""
    ix = x * float(sx * SPRITE_PC_WIDTH)
    iy = y * float(sy * SPRITE_PC_HEIGHT) * 21.0 / 24.0
    if iy < 0 or iy >= float(sy * SPRITE_PC_HEIGHT):
        return None
    if ix < 0 or ix >= float(sx * SPRITE_PC_WIDTH):
        return None
    scale = 2 if bit_mask == 0b11 else 1
    v = int(ix) + int(iy) * SPRITE_PC_HEIGHT * sx
    if v < 0 or v >= len(m_data):
        return None
    iy_line = int(iy * 8.0)
    if iy_line > sy * 21:
        return None
    iy_line = iy_line & 7
    if scale == 1:
        ix_sub = int(ix * 8.0) & 7
    else:
        ix_sub = (int(ix * 8.0) // scale) & ((8 // scale) - 1)
        ix_sub = ix_sub * scale
    p, c = m_data[v]
    return ix_sub, iy_line, (p, c)


def lsprite_get_pixel(
    x: float,
    y: float,
    bit_mask: int,
    sx: int,
    sy: int,
    m_data: list[tuple[bytes, tuple[int, int, int, int]]],
) -> int:
    """LSprite::getPixel — palette index 0..15."""
    r = lsprite_get_set_data(x, y, bit_mask, sx, sy, m_data)
    if r is None:
        return 0
    ix_sub, iy_line, (p, c) = r
    return pixelchar_get(ix_sub, iy_line, bit_mask, p, c)


def parse_sprites2_body(blob: bytes) -> list[dict[str, object]]:
    """
    After FLF header: CharsetImage::SavePensBin (5) + LImageSprites2::SaveBin.
    Each sprite: sx, sy, 16-byte header, sx*sy*9 PixelChars (12 bytes each).
    """
    off = PENS_BIN_SIZE
    if len(blob) < off + 1:
        raise ValueError("Sprites2 FLF: truncated after pens")
    cnt = blob[off]
    off += 1
    sprites: list[dict[str, object]] = []
    if cnt == 0:
        if off != len(blob):
            raise ValueError(
                f"Sprites2 FLF: expected no sprite payload ({off} bytes), got {len(blob)}"
            )
        return sprites
    for _ in range(cnt):
        if off + 2 > len(blob):
            raise ValueError("Sprites2 FLF: truncated (sprite size)")
        sx = blob[off]
        sy = blob[off + 1]
        off += 2
        if sx == 0 or sy == 0:
            raise ValueError("Sprites2 FLF: invalid sprite size sx/sy")
        if off + SPRITE_HEADER_SIZE > len(blob):
            raise ValueError("Sprites2 FLF: truncated (header)")
        header = blob[off : off + SPRITE_HEADER_SIZE]
        off += SPRITE_HEADER_SIZE
        n_pc = sx * sy * SPRITE_PC_WIDTH * SPRITE_PC_HEIGHT
        need = n_pc * MC_PIXELCHAR_BYTES
        if off + need > len(blob):
            raise ValueError("Sprites2 FLF: truncated (pixel data)")
        raw_pc = blob[off : off + need]
        off += need
        pcs: list[tuple[bytes, tuple[int, int, int, int]]] = []
        for c in range(n_pc):
            base = c * MC_PIXELCHAR_BYTES
            chunk = raw_pc[base : base + MC_PIXELCHAR_BYTES]
            p = chunk[0:8]
            col = (chunk[8], chunk[9], chunk[10], chunk[11])
            pcs.append((p, col))
        sprites.append({"sx": sx, "sy": sy, "header": header, "pcs": pcs})
    if off != len(blob):
        raise ValueError(
            f"Sprites2 FLF: extra bytes after sprites ({len(blob) - off} unparsed)"
        )
    return sprites


def render_sprites2_sprite_rgba(
    spr: dict[str, object],
) -> Image.Image:
    """Rasterize one C64 sprite block (LSprite) to RGBA — matches editor preview sampling."""
    sx = int(spr["sx"])
    sy = int(spr["sy"])
    header = bytes(spr["header"])
    pcs = spr["pcs"]
    multicolor = header[0] != 0 if len(header) > 0 else False
    bit_mask = 0b11 if multicolor else 0b1
    w_px = (12 if multicolor else 24) * sx
    h_px = 21 * sy
    im = Image.new("RGBA", (w_px, h_px), (0, 0, 0, 0))
    pix = im.load()
    assert pix is not None
    for j in range(h_px):
        for i in range(w_px):
            fx = (i + 0.5) / float(w_px)
            fy = (j + 0.5) / float(h_px)
            col_idx = lsprite_get_pixel(fx, fy, bit_mask, sx, sy, pcs)
            r, g, b = rgb_for_index(col_idx)
            # TRSE: index 0 is often transparent in sprite pixels
            if col_idx == 0:
                pix[i, j] = (0, 0, 0, 0)
            else:
                pix[i, j] = (r, g, b, 255)
    return im


def flf_sprites2_to_png(raw: bytes, desc: str, png_path: Path) -> None:
    """Decode FLF image_type=10 (Sprites2) — LImageSprites2::SaveBin."""
    if len(raw) < HEADER_PREFIX + PENS_BIN_SIZE + 1 + FOOTER_SIZE:
        raise ValueError(f"{desc}\n\nSprites2 FLF: file too short.")
    p0 = HEADER_PREFIX
    footer = raw[-FOOTER_SIZE:]
    blob = raw[p0:-FOOTER_SIZE]
    if len(footer) != FOOTER_SIZE:
        raise ValueError("Footer missing or wrong size")
    if footer[0] != FOOTER_ID0 or footer[1] != FOOTER_ID1:
        print("Warning: footer ID bytes do not match TRSE default (64, 69)", file=sys.stderr)

    sprites = parse_sprites2_body(blob)
    if not sprites:
        Image.new("RGBA", (1, 1), (0, 0, 0, 0)).save(png_path, "PNG")
        return

    rendered = [render_sprites2_sprite_rgba(s) for s in sprites]
    gap = 1
    total_w = sum(im.width for im in rendered) + gap * (len(rendered) - 1)
    max_h = max(im.height for im in rendered)
    out = Image.new("RGBA", (total_w, max_h), (0, 0, 0, 0))
    ox = 0
    for im in rendered:
        out.paste(im, (ox, 0), im)
        ox += im.width + gap
    out.save(png_path, "PNG")


def flf_multicolorimage_to_png(
    raw: bytes, desc: str, png_path: Path, *, hires: bool
) -> None:
    """
    Decode MultiColorImage::SaveBin — types 1 (multicolor) and 2 (hires C64).
    Multicolor: m_bitMask=0b11, m_scale=2 → 160×200. Hires: m_bitMask=1, m_scale=1 → 320×200.
    """
    label = "HiresBitmap (C64)" if hires else "MultiColorBitmap (C64)"
    if len(raw) < MC_TOTAL:
        raise ValueError(
            f"{desc}\n\n"
            f"Expected {MC_TOTAL} bytes for {label} + footer, got {len(raw)}."
        )
    p0 = HEADER_PREFIX
    blob = raw[p0 : p0 + MC_PAYLOAD]
    footer = raw[p0 + MC_PAYLOAD : p0 + MC_PAYLOAD + FOOTER_SIZE]
    if len(footer) != FOOTER_SIZE:
        raise ValueError("Footer missing or wrong size")
    if footer[0] != FOOTER_ID0 or footer[1] != FOOTER_ID1:
        print("Warning: footer ID bytes do not match TRSE default (64, 69)", file=sys.stderr)

    # Skip 2-byte prefix (background / unused) — see MultiColorImage::LoadBin
    off = 2
    out_w = HIRES_WIDTH if hires else MC_WIDTH
    im = Image.new("RGBA", (out_w, MC_HEIGHT))
    pix = im.load()
    for cy in range(MC_CHAR_HEIGHT):
        for cx in range(MC_CHAR_WIDTH):
            base = off + (cx + cy * MC_CHAR_WIDTH) * MC_PIXELCHAR_BYTES
            pc = blob[base : base + MC_PIXELCHAR_BYTES]
            p = pc[0:8]
            c = (pc[8], pc[9], pc[10], pc[11])
            for ly in range(8):
                if hires:
                    for lx in range(8):
                        gx = cx * 8 + lx
                        gy = cy * 8 + ly
                        col_idx = pixelchar_get(lx, ly, HIRES_BITMASK, p, c)
                        r, g, b = rgb_for_index(col_idx)
                        pix[gx, gy] = (r, g, b, 255)
                else:
                    for lx in range(4):
                        gx = cx * 4 + lx
                        gy = cy * 8 + ly
                        col_idx = pixelchar_get(MC_SCALE * lx, ly, MC_BITMASK, p, c)
                        r, g, b = rgb_for_index(col_idx)
                        pix[gx, gy] = (r, g, b, 255)
    im.save(png_path, "PNG")


def png_to_flf(png_path: Path, flf_path: Path) -> None:
    raw = png_path.read_bytes()
    _hint_wrong_file_for_png2flf(raw)
    try:
        im = Image.open(png_path).convert("RGBA")
    except Exception as e:
        raise ValueError(
            f"Could not read {png_path.name!r} as an image ({e}). "
            "png2flf expects a bitmap file Pillow can open (e.g. PNG or JPEG)."
        ) from e
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
    ver, img_type, pal_type, desc = describe_flf_header(raw)
    if ver != VERSION:
        print(f"Warning: version is {ver}, expected {VERSION}", file=sys.stderr)

    if pal_type != PALETTE_TYPE_C64:
        raise ValueError(
            f"{desc}\n\n"
            f"flf2png only supports palette_type={PALETTE_TYPE_C64} (C64). "
            f"See TRSE source/LeLib/limage/ for other palette types."
        )

    if img_type == IMAGE_TYPE_MULTICOLOR_C64:
        # Same TRSE type for C64 bitmap; footer byte 5 selects hires vs multicolor pixel layout.
        flf_multicolorimage_to_png(
            raw, desc, png_path, hires=multicolor_file_is_hires(raw)
        )
        return

    if img_type == IMAGE_TYPE_HIRES_C64:
        flf_multicolorimage_to_png(raw, desc, png_path, hires=True)
        return

    if img_type == IMAGE_TYPE_SPRITES2:
        flf_sprites2_to_png(raw, desc, png_path)
        return

    if img_type != IMAGE_TYPE_QIMAGE:
        raise ValueError(
            f"{desc}\n\n"
            f"flf2png supports image_type={IMAGE_TYPE_QIMAGE} (QImageBitmap), "
            f"{IMAGE_TYPE_MULTICOLOR_C64} (MultiColor C64), {IMAGE_TYPE_HIRES_C64} (Hires C64), "
            f"or {IMAGE_TYPE_SPRITES2} (Sprites2). "
            f"For image_type {img_type}, open in TRSE or extend flf_tool (SaveBin in TRSE source)."
        )

    if len(raw) < V1_TOTAL:
        raise ValueError(
            f"{desc}\n\n"
            f"Expected {V1_TOTAL} bytes for QImageBitmap 320x200 + footer, got {len(raw)}. "
            f"File may be truncated or a different sub-format."
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


def cmd_info(path: Path) -> None:
    raw = path.read_bytes()
    _ver, img_type, pal_type, line = describe_flf_header(raw)
    print(line)
    if pal_type != PALETTE_TYPE_C64:
        print("  flf2png: palette type not supported (C64 only).")
        return
    if img_type == IMAGE_TYPE_QIMAGE:
        print(f"  flf2png (QImageBitmap): expects {V1_TOTAL} bytes, have {len(raw)}")
    elif img_type == IMAGE_TYPE_MULTICOLOR_C64:
        print(f"  flf2png (MultiColor C64): expects {MC_TOTAL} bytes, have {len(raw)}")
        foot = footer_bytes(raw)
        if len(foot) > FOOTER_POS_DISPLAY_MULTICOLOR:
            mc = foot[FOOTER_POS_DISPLAY_MULTICOLOR]
            mode = "hires (320×200)" if mc == 0 else "multicolor (160×200)"
            print(
                f"  footer[{FOOTER_POS_DISPLAY_MULTICOLOR}] (display multicolor)={mc} → decode as {mode}"
            )
    elif img_type == IMAGE_TYPE_HIRES_C64:
        print(f"  flf2png (Hires C64): expects {MC_TOTAL} bytes, have {len(raw)}")
    elif img_type == IMAGE_TYPE_SPRITES2:
        print("  flf2png (Sprites2): variable size (pens + sprite list + 256-byte footer)")
    else:
        print("  flf2png: this image type is not implemented.")


def main() -> None:
    p = argparse.ArgumentParser(
        description="TRSE FLF → PNG: QImageBitmap, MultiColor C64, or Sprites2 (subset)"
    )
    sub = p.add_subparsers(dest="cmd", required=True)

    p2f = sub.add_parser("png2flf", help="Convert PNG to .flf")
    p2f.add_argument("input", type=Path)
    p2f.add_argument("output", type=Path)

    f2p = sub.add_parser(
        "flf2png",
        help="Extract PNG (QImageBitmap, MultiColor C64, or Sprites2 sheet)",
    )
    f2p.add_argument("input", type=Path)
    f2p.add_argument("output", type=Path)

    inf = sub.add_parser("info", help="Print FLF header (magic, types, size)")
    inf.add_argument("input", type=Path)

    args = p.parse_args()
    try:
        if args.cmd == "png2flf":
            png_to_flf(args.input, args.output)
        elif args.cmd == "flf2png":
            flf_to_png(args.input, args.output)
        else:
            cmd_info(args.input)
    except ValueError as e:
        print(f"Error:\n{e}", file=sys.stderr)
        sys.exit(2)
    except OSError as e:
        print(f"Error: could not read or write a file: {e}", file=sys.stderr)
        sys.exit(2)


if __name__ == "__main__":
    main()
