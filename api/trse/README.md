# TRSE compile API (`api/trse`)

Deploy this folder to your PHP host (e.g. **8bitworkshop-rgc** `api/trse/`). Same JSON contract as the KickAss compile endpoint.

## Host requirements

- **`trse`** via Docker (`TRSE_MODE=docker`, image `trse:cli`) or native binary.
- **`TRSE_SETTINGS`**: host `trse.ini` (e.g. from TRSE `Publish/publish_linux/trse.ini`). Must use **`key = value`** lines only; **`#` is not a comment** for TRSE — this API strips `#` lines before load.
- **`trse.defaults.ini`**: merged for **missing** keys (adds **`assembler_6809 = OrgAsm`** if absent).
- **TRS80COCO / DRAGON**: session **`project.trse`** is rewritten to **`output_type = bin`** so the CoCo pipeline skips MAME **imgtool** (requires TRSE build with `SystemTRS80CoCo::PostProcess` early-return when `output_type == bin`).

## Environment

| Variable | Default | Purpose |
|----------|---------|---------|
| `TRSE_MODE` | `docker` | `docker` or `native` |
| `TRSE_DOCKER_IMAGE` | `trse:cli` | Image name |
| `TRSE_NATIVE_BIN` | `trse` | Native binary path |
| `TRSE_SETTINGS` | `/etc/trse/trse.ini` | Host global INI copied per session |
| `TRSE_UNITS_STOCK` | *(empty)* | Optional path to TRSE repo **`units/`** |
| `TRSE_DEFAULTS_INI` | `__DIR__/trse.defaults.ini` | API defaults merge file |

## Copy into another repo

```bash
rsync -a api/trse/ /path/to/8bitworkshop-rgc/api/trse/
```

Commit and deploy the PHP + `trse.defaults.ini` together.

See also: **`../trse_compiler/PLATFORMS_AND_OUTPUTS.md`** in the TRSE tree for platform names.
