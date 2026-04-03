# flf_tool — PNG ↔ TRSE `.flf` (v1 subset)

Implements **`docs/flf_png_converter_spec.md`**: FLUFF64 container (**7-byte** magic), image type **0** (`QImageBitmap`), palette type **0** (C64), **320×200** indexed pixels, 256-byte footer.

## Requirements

```bash
pip install pillow
```

Python **3.8+** recommended.

## Usage

```bash
python3 flf_tool.py png2flf image.png out.flf
python3 flf_tool.py flf2png image.png out.png
```

- **png2flf:** Resizes to 320×200, maps each pixel to the nearest **C64 palette** colour (`LColorList::InitC64` in TRSE), writes column/row order matching `LImageQImage::SaveBin`.
- **flf2png:** Rejects non-(0,0) type bytes with a clear error; maps indices with **index & 15** into the 16-colour C64 table.

## Limits

Only this single FLF layout is supported. Other TRSE image types (sprites, multicolour, etc.) need different parsers.
