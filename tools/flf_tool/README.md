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
python3 flf_tool.py flf2png image.flf out.png
python3 flf_tool.py info image.flf
```

- **png2flf:** Resizes to 320×200, maps each pixel to the nearest **C64 palette** colour (`LColorList::InitC64` in TRSE), writes column/row order matching `LImageQImage::SaveBin`.
- **flf2png:** Only **image_type=0** (QImageBitmap), **palette_type=0** (C64), **64269-byte** files. If your `.flf` is a charset, sprite sheet, multicolor screen, etc., use **`info`** to see TRSE’s type bytes — those need a different decoder (or export from TRSE).
- **info:** Prints magic, version, image/palette type names, and file size.

## Limits

Only this single FLF layout is supported. Other TRSE image types (sprites, multicolour, etc.) need different parsers.
