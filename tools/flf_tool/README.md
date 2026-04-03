# flf_tool — PNG ↔ TRSE `.flf` (subset)

Implements **`docs/flf_png_converter_spec.md`** for **QImageBitmap**: FLUFF64 (**7-byte** magic), image type **0**, palette **0** (C64), **320×200** indexed pixels, 256-byte footer.

**`flf2png`** decodes:

- **Type 0** (QImageBitmap): **64269** bytes.
- **Type 1** (MultiColorBitmap C64): **12271** bytes; footer byte **5** selects hires **320×200** vs multicolor **160×200** (`POS_DISPLAY_MULTICOLOR`).
- **Type 2** (HiresBitmap C64): same payload as type 1; **320×200** hires.
- **Type 10** (Sprites2): variable length. Output is a **horizontal strip** of sprite blocks. Palette index **0** → transparent PNG.

**`png2flf`** writes type **0** only (row-major indices, `x + y*320`).

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

### `export-bin` — same binaries as TRSE `@export`

Mirrors **`Parser::HandleExport`** → **`ExportBin`** for supported C64 types (see `source/Compiler/parser.cpp`, `multicolorimage.cpp`, `limageqimage.cpp`, `limagesprites2.cpp`):

```bash
# C64 multicolor / hires bitmap → neo_rider_data.bin + neo_rider_color.bin
python3 flf_tool.py export-bin neo_rider_by_the_diad.flf neo_rider.bin 0

# QImage (type 0) → single packed 1-bpp file (ExportBlackWhite type 0; non-zero palette index → bit 1)
python3 flf_tool.py export-bin octo.flf octo.bin 8

# Sprites2 → sequential 64-byte C64 sprite blocks
python3 flf_tool.py export-bin sprites.flf sprites.bin
```

- **Types 1 / 2** (and type 1 with hires footer): writes **`<base>_data.bin`** and **`<base>_color.bin`** (strip `.bin` from the output path for the base name, same as TRSE).
- **Type 0**: writes **one** file at the given path (packed bitmap, **8000** bytes for a full 320×200 screen). Integer arguments are accepted for parity with `@export` but do not change the layout (TRSE’s `LImageQImage::ExportBin` is empty; packing uses `ExportBlackWhite` type 0).
- **Type 10**: writes **one** raw sprite binary.

**Note:** Pen colours for the two-byte background header in `*_color.bin` are taken from the FLF footer pen slots when present (`FooterToPen`).

If you use the wrong subcommand (e.g. **`flf2png`** on a **PNG**), the tool prints a short hint instead of a raw traceback.

## Limits

Not every TRSE image type or export variant is implemented—only what the C++ paths above cover for C64 palette FLFs.
