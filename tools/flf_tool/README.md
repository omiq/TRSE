# flf_tool — TRSE `.flf` helpers (PNG, `@export`-style bins, info)

Standalone Python 3 tool for **C64 palette** TRSE image files (FLUFF64). It does **not** require the TRSE IDE.

**Spec:** [`docs/flf_png_converter_spec.md`](../../docs/flf_png_converter_spec.md) (QImage layout and container format).

---

## Requirements

```bash
pip install pillow
```

Python **3.8+** recommended.

---

## Subcommands

| Command | Purpose |
|--------|---------|
| `png2flf` | PNG → `.flf` (QImageBitmap **type 0** only) |
| `flf2png` | `.flf` → PNG (types **0**, **1**, **2**, **10** — see below) |
| `info` | Print FLF header (version, image/palette types, size; footer hints for type 1) |
| `export-bin` | Emit **`.bin`** files like TRSE **`@export`** for supported types |

```bash
python3 flf_tool.py png2flf   <image.png>  <out.flf>
python3 flf_tool.py flf2png   <in.flf>      <out.png>
python3 flf_tool.py info      <file.flf>
python3 flf_tool.py export-bin <in.flf> <out[.bin]> [int [int]]
```

Wrong file type (e.g. **`flf2png`** on a PNG) prints a short hint instead of a raw traceback.

---

## `flf2png` — supported FLF image types

All expect **palette type 0** (C64).

| `image_type` | Meaning | PNG output |
|--------------|---------|------------|
| **0** | QImageBitmap | **320×200** indexed RGB |
| **1** | MultiColorBitmap (C64) | **160×200** (multicolor) or **320×200** (hires), from **footer byte 5** (`POS_DISPLAY_MULTICOLOR`: 0 = hires, non‑zero = multicolor) |
| **2** | HiresBitmap | **320×200** |
| **10** | Sprites2 | Horizontal strip of blocks; palette index **0** → transparent |

---

## `export-bin` — match TRSE `@export` binaries

Same idea as **`Parser::HandleExport`** → **`LImage::ExportBin`** (see `source/Compiler/parser.cpp` and the image classes below).

**Integer arguments** (optional): same parsing as TRSE — **one** number *N* means `export1=N`; **two** numbers *A B* mean `export1=B`, `param2=A`.

### Examples (equivalent TRSE lines)

```trse
@export "neo_rider_by_the_diad.flf" "neo_rider.bin" 0
@export "squiddy_hires.flf" "squiddy_hires.bin" 0
@export "octo.flf" "octo.bin" 8
```

```bash
python3 flf_tool.py export-bin neo_rider_by_the_diad.flf neo_rider.bin 0
python3 flf_tool.py export-bin squiddy_hires.flf squiddy_hires.bin 0
python3 flf_tool.py export-bin octo.flf octo.bin 8
```

### What gets written

| FLF type | TRSE code path | Output files |
|----------|----------------|--------------|
| **1** / **2** (and type **1** with hires footer) | `MultiColorImage::ExportBin` | **`<base>_data.bin`** + **`<base>_color.bin`** — `<base>` is the output path **without** `.bin` (same as TRSE). |
| **0** | `LImageQImage::ExportBlackWhite` **type 0** (packed 1 bpp; `ExportBin` is empty in TRSE) | **Single file** at the given path (e.g. `octo.bin`), **8000** bytes for a full 320×200 screen. Extra integer args are accepted for parity with `@export` but **do not** change the packed layout. |
| **10** | `LImageSprites2::ExportBin` | **Single file** — sequential **64-byte** C64 sprite blocks per TRSE. |

**`*_color.bin`:** The two leading background bytes use **footer pen** values when present (`LImageFooter` / `FooterToPen`).

---

## `png2flf`

- Resizes to **320×200**, maps pixels to the **16-colour C64** palette.
- Writes **row-major** indices: offset `k` → `x = k % 320`, `y = k // 320` (matches `LImageQImage::SaveBin` / `LoadBin` in TRSE).

---

## Limits

- **C64 palette** FLFs only for these commands.
- **`png2flf`** only writes **type 0** FLFs.
- Other TRSE image types (charset-only, other platforms, etc.) are not covered unless listed above.
