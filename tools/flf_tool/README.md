# flf_tool — PNG ↔ TRSE `.flf` (subset)

Implements **`docs/flf_png_converter_spec.md`** for **QImageBitmap**: FLUFF64 (**7-byte** magic), image type **0**, palette **0** (C64), **320×200** indexed pixels, 256-byte footer.

**`flf2png`** also decodes:

- **Image type 1** (MultiColorBitmap C64): **160×200** pixels, fixed layout (`MultiColorImage::SaveBin`).
- **Image type 10** (Sprites2): variable length (`LImageSprites2::SaveBin` — pens + sprite list). Output is a **horizontal strip** of each sprite block (24×21 hires or 12×21 multicolor per cell, TRSE header byte 0 toggles mode). Palette index **0** is exported as **transparent** in the PNG.

**`png2flf`** still writes type **0** only.

## Requirements

```bash
pip install pillow
```

Python **3.8+** recommended.

## Usage

```bash
python3 flf_tool.py png2flf image.png out.flf
python3 flf_tool.py flf2png image.flf out.png
python3 flf_tool.py info image.flf
```

- **png2flf:** Resizes to 320×200, maps each pixel to the nearest **C64 palette** colour (`LColorList::InitC64` in TRSE), writes column/row order matching `LImageQImage::SaveBin`.
- **flf2png:** **image_type=0** (QImageBitmap, **64269** bytes) or **image_type=1** (MultiColor C64, **12271** bytes), **palette_type=0**. Other types: use **`info`**, then TRSE or a future decoder.
- **info:** Prints magic, version, image/palette type names, and file size.

## Limits

**png2flf** only emits QImageBitmap (type 0). **flf2png** supports types **0** and **1** (C64 multicolor screen). Sprites, charsets, other platforms, etc. need different parsers.
