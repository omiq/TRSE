# TRSE platforms (`system`) and output formats

This is derived from the TRSE source (`AbstractSystem::SystemFromString`, `StringFromSystem`, `dialogprojectsettings.ui`, and per-system assemblers). Use it when building or generating `.trse` files for the API.

---

## `system =` — platform identifier

The project file uses **`system = <Name>`**. Matching is **case-insensitive** in `SystemFromString` (`source/Compiler/systems/abstractsystem.cpp`). The canonical spelling stored in TRSE is the **`StringFromSystem`** form (column **Canonical** below).

| Canonical `system` | Aliases accepted in `SystemFromString` |
|--------------------|----------------------------------------|
| C64 | `c64` |
| C128 | `c128` |
| PET | `pet` |
| VIC20 | `vic20` |
| NES | `nes` |
| BBCM | `bbcm` |
| AMIGA | `amiga` |
| PLUS4 | `plus4` |
| OK64 | `ok64` |
| X16 | `x16` |
| X86 | `x86` |
| GAMEBOY | `gameboy` |
| SPECTRUM | `spectrum` |
| TIKI100 | `tiki100` |
| ATARI2600 | `atari2600` |
| ATARI520ST | `atari520st` |
| AMSTRADCPC | `amstradcpc`, `amstradcpc464` |
| COLECO | `coleco` |
| MEGA65 | `mega65` |
| MSX | `msx` |
| ATARI800 | `atari800` |
| APPLEII | `appleii` |
| M1ARM | `m1arm` |
| ORIC | `oric` |
| SNES | `snes` |
| CUSTOM | `custom` |
| VZ200 | `vz200` |
| ACORN | `acorn` |
| JDH8 | `jdh8` |
| POKEMONMINI | `pokemonmini` |
| TRS80 | `trs80` |
| TRS80COCO | `trs80coco` |
| WONDERSWAN | `wonderswan` |
| TIM | `tim` |
| TVC | `tvc` |
| VECTREX | `vectrex` |
| THOMSON | `thomson` |
| CHIP8 | `chip8` |
| PCW | `pcw` |
| BK0010 | `bk0010` |
| DRAGON | `dragon` |
| FOENIX | `foenix` |
| AGON | `agon` |
| PRIMO | `primo` |

If the string is not recognized, the code **defaults to C64** (see `abstractsystem.cpp`).

---

## `output_type` — primary UI values (Commodore-style targets)

In **Project settings**, the **Output type** combo box (`cmbOutputType` in `dialogprojectsettings.ui`) offers exactly three string values:

| `output_type` | Meaning (high level) |
|---------------|----------------------|
| **prg** | Build a loadable program (default path for most tutorials). |
| **d64** | Build a **1541 disk image** (`.d64`). Requires disk metadata in the project (`disk1_paw`, etc.). |
| **crt** | Build a **C64 cartridge** image (`.crt`). |

Special handling in code:

- **`d64`** — Implemented in `SystemMOS6502::PostProcess` (`systemmos6502.cpp`): needs at least one `diskN_paw` entry; otherwise TRSE reports that disk setup is missing.
- **`crt`** — Wraps the compiled `.prg` in a CRT container using `resources/bin/crt_header.bin`.
- **`prg`** — Normal assemble pipeline; final file is typically **`basename.prg`** for 6502 targets that use OrgAsm.

VICE autostart uses `filename + "." + output_type` when `output_type` is not `d64` or when using Sparkle (`applyEmulatorParametersVICE`), so the extension usually matches **`output_type`** for those cases.

---

## Typical **artifact extensions** by platform (after assemble)

TRSE does not centralize one table of “platform → extension”. The assembler / linker for each **system** chooses the filename. Common cases:

| Family / `system` | Typical main binary (after successful build) |
|-------------------|-----------------------------------------------|
| C64, C128, PET, VIC20, NES (6502 path), … | **`.prg`** (OrgAsm default for many MOS targets) |
| GAMEBOY | **`.gb`** (see `systemgameboy.cpp`) |
| SNES | **`.smc`** (assembler params in `systemsnes.cpp`) |
| X86 | Controlled by **`cpu_x86_output`** in the project file (separate combo in project settings), not only `output_type` |
| AMIGA, ATARI ST, … | Platform-specific (`.tos`, etc.) — follow **`Publish/project_templates/<platform>/`** |

**Rule of thumb:** open a working template under **`Publish/project_templates/`** for your target and copy its **`project.trse`** keys (`system`, `output_type`, and any `cpu_*` or disk fields).

---

## Related keys (not a full `.trse` schema)

- **`disk1_paw`**, **`disk2_paw`**, … — PAW project files listing files to place on disk when building **`d64`**.
- **`disk1_type`** — Used with VICE params when `output_type` is `d64` (see `applyEmulatorParametersVICE`).
- **`cpu_x86_output`** — x86-specific output format (separate from `output_type`).

---

## Source references

| Topic | File |
|-------|------|
| Platform string parsing | `source/Compiler/systems/abstractsystem.cpp` — `SystemFromString`, `StringFromSystem` |
| `output_type` UI values | `source/dialogprojectsettings.ui` — `cmbOutputType` items `prg`, `d64`, `crt` |
| CRT / D64 post-processing | `source/Compiler/systems/systemmos6502.cpp` — `PostProcess` |

For the complete set of **tutorial** `.trse` examples, search under **`Publish/tutorials/**/*.trse`** for `system =` and `output_type =`.
