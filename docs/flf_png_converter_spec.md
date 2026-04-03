# Spec: Python 3 FLF ↔ PNG converter (initial scope)

This document defines a **minimal, implementable** subset of TRSE’s **`.flf`** (FLUFF64) format for a standalone **Python 3** tool with:

- **`png2flf`** — encode a PNG into a valid `.flf`
- **`flf2png`** — decode a `.flf` to PNG

It is derived from TRSE source (`source/LeLib/limage/limageio.cpp`, `limage.cpp`, `limageqimage.cpp`, `limagefooter.*`, `lcolorlist.cpp`). **TRSE CLI does not expose FLF conversion** (`trse -cli` only has `project` / `orgasm`).

---

## 1. Scope (v1)

| Feature | In scope for v1 | Notes |
|--------|------------------|--------|
| Image type | **`QImageBitmap` only** | `LImage::Type` enum value → byte **`0`** (`TypeToChar`) |
| Palette type | **`C64` only** | `LColorList::Type` → byte **`0`** (`TypeToChar`) |
| Dimensions | **Fixed 320×200** | Matches `LImageQImage` default `Initialize(320,200)` in TRSE |
| Pixel payload | **Indexed 8-bit** | One byte per pixel: palette index 0…255 |
| Optional palette tail | **Omit in v1** | TRSE may append extra bytes when `m_savePalette` is true; v1 writer writes **no** tail; v1 reader consumes **`320*200` bytes** only, then footer |
| Other FLF types | **Out of scope** | Types 1–47 (multicolor, sprites, NES, …) have different `SaveBin` layouts |

**Rationale:** `QImageBitmap` + default size is the simplest path to round-trip PNG (resize/quantize input to 320×200 indexed).

Future versions can add flags: `--type`, `--palette`, `--width/--height` once those `SaveBin`/`LoadBin` layouts are specified.

---

## 2. File format (FLUFF64 container)

All integers **little-endian** unless stated. Offsets are from start of file.

| Offset | Size | Field | Value / rule |
|--------|------|--------|----------------|
| 0 | 7 | Magic | ASCII **`FLUFF64`** (**7 bytes** — `QString("FLUFF64")` in TRSE, not 8) |
| 7 | 4 | Version | **`flfVersion`** from TRSE: integer **2** (`Data::data.flfVersion` in `source/LeLib/data.h`). **Read/write as 4 bytes** (same as `sizeof(int)` in `Load`). *Note:* `Save` in TRSE uses `memcpy` into 4 bytes but writes with `sizeof(float)` length — still 4 bytes on typical platforms. |
| 11 | 1 | Image type | **`0`** = `QImageBitmap` |
| 12 | 1 | Palette type | **`0`** = `LColorList::C64` |
| 13 | *N* | Payload | Type-specific binary (`SaveBin`) — see §3 |
| 13+N | 256 | Footer | `LImageFooter` block — see §4 |

**Total size (v1):** `7 + 4 + 1 + 1 + 320*200 + 256 = 64_269` bytes (no palette tail).

---

## 3. Payload for image type 0 (`LImageQImage::SaveBin` / `LoadBin`)

When **image type = 0** and **`m_savePalette` is false** (default for generic bitmap):

1. **`width × height` bytes** — raw indices in **column-major** order as stored by TRSE:

   ```text
   index at file offset k (within payload) maps to:
     x = k % width
     y = k / width
   ```

   with **`width = 320`**, **`height = 200`** for v1.

2. **No** following palette blob (when `m_savePalette` is false, `SaveBin` does not write `toArray`).

**Indexed colour → RGBA for PNG:** TRSE uses `LColorList` type **C64**. For export, use the same palette as TRSE’s C64 colour list (implementations should either embed TRSE’s RGB values from `lcolorlist` / theme data or ship a static 256×RGB table matching the IDE). **v1 shortcut:** treat indices as positions into the **16-colour C64 palette** for indices 0–15 and map 16–255 to black or a fixed extended table — *document whichever you choose* and prefer importing a **reference `.flf`** from TRSE to validate colours.

**PNG → indices for `png2flf`:** Resize to **320×200**, quantize to **16 C64 colours** (or full 256-entry table if you embed TRSE’s palette), then write one byte per pixel in the same column-major order.

---

## 4. Footer (256 bytes)

After the payload, TRSE always writes **exactly 256 bytes** (`LImageFooter::Save` / `Load`).

| Check | Rule |
|--------|------|
| Size | Must be **256** bytes |
| ID bytes | `m_data[0] == 64` (`LImageID0`), `m_data[1] == 69` (`LImageID1`) |

Remaining bytes are editor state (grid, current pen, display flags, etc.). For **writing** new files from Python, **zero-initialize** all 256 bytes, then set:

- `footer[0] = 64`
- `footer[1] = 69`

For **reading**, validate ID bytes; you may ignore the rest for PNG export if you only need pixels.

Constants are in `source/LeLib/limage/limagefooter.h` (`POS_*` indices).

---

## 5. CLI interface (suggested)

```text
flf_tool png2flf INPUT.png OUTPUT.flf [--width 320 --height 200]   # v1 may fix 320×200 only
flf_tool flf2png INPUT.flf OUTPUT.png
```

**Behaviour:**

- **`png2flf`:** Load PNG → resize/cover to 320×200 → quantize to C64 palette → write FLUFF64 per §2–4.
- **`flf2png`:** Read file → verify magic `FLUFF64` → read version, types → **if types are not (0,0), exit with “unsupported type for v1”** → read 64,000 bytes payload → read 256-byte footer → map indices to RGB → save PNG RGBA.

---

## 6. Validation & tests

1. **Round-trip:** TRSE IDE saves a 320×200 “Bitmap image” `.flf`; your `flf2png` produces PNG; your `png2flf` produces `.flf`; byte-compare payload+footer or compare pixel RMS.
2. **Golden file:** Ship one **minimal** `.flf` from TRSE in `tests/data/` with known checksum.

---

## 7. Known TRSE quirks (for implementers)

- **Header case:** `Load` reads 8 bytes into a buffer; magic should be **`FLUFF64`** (ASCII).
- **Version field:** Use **4-byte little-endian int = 2** for compatibility with current TRSE.
- **Full format** supports dozens of `imageType` values; each has a different payload — do **not** assume `width*height` for other types.
- **`@export`** in `.ras` source can export FLF to platform binaries inside the **compiler**, not via this Python tool.

---

## 8. Dependencies (Python)

- **`Pillow` (PIL)** — PNG I/O, resize, quantization  
- No Qt required if palette RGB values are duplicated in Python

---

## 9. References (TRSE source)

| Topic | File |
|-------|------|
| Container write/read | `source/LeLib/limage/limageio.cpp` |
| Type byte map | `source/LeLib/limage/limage.cpp` — `TypeToChar` / `CharToType` |
| Palette type byte map | `source/LeLib/limage/lcolorlist.cpp` — `TypeToChar` / `CharToType` |
| QImageBitmap payload | `source/LeLib/limage/limageqimage.cpp` — `SaveBin`, `LoadBin` |
| Footer | `source/LeLib/limage/limagefooter.h`, `limagefooter.cpp` |
| Version constant | `source/LeLib/data.h` — `flfVersion` |

---

## 10. Out of scope (later)

- FLF types ≠ 0 (multicolor, charset maps, sprites, …)
- Animated / multi-bank FLF
- Palette tail (`m_savePalette`) and non-320×200 `LImageQImage` (requires matching TRSE’s `Initialize` + footer state)
