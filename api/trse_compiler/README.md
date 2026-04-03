# TRSE compile HTTP API

PHP endpoint that accepts project files from a web IDE (same JSON shape as the KickAss API), runs **[TRSE](https://github.com/leuat/TRSE)** in **CLI mode** (`trse -cli`), and returns build artifacts as base64.

**Production deployment (8bitworkshop-rgc):** use **`api/trse/`** — same files as here, path `api/trse/compile.php`.

**Script location (this repo):** `api/trse_compiler/compile.php` (duplicate of `api/trse/compile.php`)

---

## Prerequisites

1. **TRSE binary** available either:
   - **Docker (recommended):** image `trse:cli` built from this repo’s `docker/Dockerfile`, or  
   - **Native:** `trse` installed on the server and on `PATH` (or set `TRSE_NATIVE_BIN`).

2. **Global settings file** `trse.ini` on the host (see [Configuration](#configuration)). The Docker image installs a default at `/etc/trse/trse.ini` when you build it.

3. **Optional:** a checkout of the TRSE **`units/`** directory for `#include` of standard units (see [Units and includes](#units-and-includes)).

4. If using **Docker**, the PHP user (often `www-data`) must be allowed to run Docker (e.g. member of the `docker` group).

---

## HTTP

| Method | Behavior |
|--------|----------|
| `POST` | Run compile with JSON body |
| `OPTIONS` | CORS preflight (200, empty) |

**Headers:** `Content-Type: application/json`

CORS headers are set for browser clients (`Access-Control-Allow-Origin: *`).

---

## Request body (JSON)

| Field | Required | Description |
|-------|----------|-------------|
| `sessionID` | Yes | Isolates each workspace under `/tmp/trse-api-{sessionID}`. Use a long random string per user/session (8–128 chars: `a-z`, `A-Z`, `0-9`, `_`, `-`). |
| `updates` | Yes | Non-empty array of `{ "path": "...", "data": "..." }`. |
| `buildStep` | Yes | Object with `project`, `path`, and optional `assemble` (see below). |

### `updates[]`

- **`path`:** Relative path inside the session workspace. No `..`, no absolute paths. Subdirectories are allowed (`files/main.ras`, `charset.bin`).
- **`data`:** File contents as a UTF-8 string, **or** binary with prefix `data:base64,` followed by base64 (same pattern as the KickAss API).

You must include at least:

- The **project file** (e.g. `project.trse`),
- The **main source** (e.g. `files/main.ras`),
- Any **assets** referenced by the project (sprites, `.bin`, other `.ras` / `.tru`, etc.).

### `buildStep`

| Field | Required | Description |
|-------|----------|-------------|
| `project` | Yes | Path to the `.trse` project file **relative to the session root** (e.g. `project.trse`). |
| `path` | Yes | Main entry source: **`.ras`** or **`.tru`** (e.g. `files/main.ras`). |
| `assemble` | No | Default `true`. If `false`, TRSE compiles to **`.asm`** but does not run the assembler step (`assemble=no`). |

---

## Response (JSON)

### Success

| Field | Type | Description |
|-------|------|-------------|
| `artifacts` | array | Built files: `{ "name": "...", "mime": "application/octet-stream", "data": "<base64>" }`. |
| `log` | string | Combined stdout/stderr from TRSE. |

Artifacts are collected heuristically: same basename as the main source (e.g. `main.prg`, `main.d64`, `main.asm`, …) plus some other patterns in the session directory. Extend `trseCollectArtifacts()` in `compile.php` if you need more extensions.

### Failure

| Field | Type | Description |
|-------|------|-------------|
| `errors` | array | `{ "line": number, "msg": string, "path": string }` (parsing is best-effort). |
| `log` | string | Compiler output for debugging. |

---

## Configuration

Environment variables (e.g. in Apache `SetEnv`, php-fpm pool, or systemd):

| Variable | Default | Description |
|----------|---------|-------------|
| `TRSE_MODE` | `docker` | `docker` or `native`. |
| `TRSE_DOCKER_IMAGE` | `trse:cli` | Docker image name when `TRSE_MODE=docker`. |
| `TRSE_NATIVE_BIN` | `trse` | Path to `trse` when `TRSE_MODE=native`. |
| `TRSE_SETTINGS` | `/etc/trse/trse.ini` | **Host** path to global `trse.ini`; copied into the session as `trse.ini` before `trse` runs. |
| `TRSE_UNITS_STOCK` | *(empty)* | If set, this directory is **recursively copied** into `{session}/units/` **before** `updates` are applied. Point at a TRSE repo’s top-level **`units/`** folder. |

If `TRSE_SETTINGS` is not readable and the client did not upload `trse.ini` in `updates`, the API returns an error.

---

## How TRSE is invoked

Working directory is the session directory. The command is equivalent to:

```bash
trse -cli op=project project=<buildStep.project> input_file=<buildStep.path> settings=trse.ini assemble=yes|no
```

Docker mode wraps this in:

```bash
docker run --rm -v {sessionDir}:/work -w /work {trse:cli} trse -cli ...
```

---

## Project file (`.trse`) essentials

The `.trse` file is INI text (`key = value`). Important keys:

| Key | Role |
|-----|------|
| `system` | Target platform string, e.g. `C64`, `VIC20`, `C128`, `NES`, `GAMEBOY`, `SPECTRUM`, … See `AbstractSystem::SystemFromString` in `source/Compiler/systems/abstractsystem.cpp` for the full list (matching is case-insensitive). |
| `output_type` | Common values: `prg` (executable), `crt` (cartridge), `d64` (disk image). |
| `disk1_paw`, `disk2_paw`, … | For `output_type=d64`, references **`.paw`** files that describe extra files on the disk. |

Templates under `Publish/project_templates/` are good references for valid combinations.

**Full table of platform names and output formats:** [PLATFORMS_AND_OUTPUTS.md](./PLATFORMS_AND_OUTPUTS.md).

---

## Units and includes

- **`@use "screen/screen"`** (and similar) loads **`.tru`** units via `Parser::HandleUseTPU`. TRSE searches, in order:
  1. The **project directory** (`m_currentDir`),
  2. **`units/<System>/`** (e.g. `units/C64/`),
  3. **`units/cpu_specific/<CPU>/`** (e.g. `units/cpu_specific/MOS6502/`),
  4. **`units/global/`**,
  under **`Util::GetSystemPrefix()`** — on Linux, the directory **above** the `trse` binary (so **`/opt/trse/units/...`** when `trse` is `/opt/trse/bin/trse`).

- The **`trse:cli` Docker image** (current `docker/Dockerfile`) copies the repo’s **`units/`** tree to **`/opt/trse/units/`**, so built-in units work in the container **without** extra configuration.

- **Native `trse` on the host:** install or symlink **`units/`** next to your install layout (same as desktop TRSE), or point the PHP API’s **`TRSE_UNITS_STOCK`** at a checkout’s **`units/`** directory (copied into each session as `./units/`).

- **`#include "file"`** (assembler/includes) uses similar logic; see `Parser::PreprocessIncludeFiles`.

Upload any **project-only** extra files via **`updates`** with the paths your sources reference.

---

## Security notes

- **Path traversal:** `compile.php` rejects `..` and absolute paths in `updates[]` and enforces writes under the session directory.
- **Concurrency:** Each `sessionID` maps to a separate directory. Parallel requests with **different** `sessionID` values do not conflict. Parallel requests with the **same** `sessionID` can race; serialize on the client or use unique IDs per build.
- **Cleanup:** Session dirs under `/tmp/trse-api-*` are not deleted automatically. Use a cron job, e.g.  
  `find /tmp -maxdepth 1 -name 'trse-api-*' -mtime +1 -exec rm -rf {} +`

---

## Example `curl`

```bash
curl -s -X POST 'https://your-server/api/trse_compiler/compile.php' \
  -H 'Content-Type: application/json' \
  -d @- <<'EOF'
{
  "sessionID": "demo-session-001xxxxxxxx",
  "buildStep": {
    "project": "project.trse",
    "path": "files/main.ras",
    "assemble": true
  },
  "updates": [
    { "path": "project.trse", "data": "system = C64\nmain_ras_file = none\nopen_files = ,files/main.ras\n" },
    { "path": "files/main.ras", "data": "program MyProg;\nbegin\n  // ...\nend.\n" }
  ]
}
EOF
```

Replace the minimal `project.trse` content with a real export from TRSE or a template from `Publish/project_templates/`.

---

## Related files in this repo

| Path | Description |
|------|-------------|
| `api/trse_compiler/compile.php` | API implementation |
| `docker/Dockerfile` | Multi-stage build for `trse:cli` (headless Qt, ARM-friendly Lua link, `resources_big` for RCC) |
| `docker/entrypoint.sh` | Sets `QT_QPA_PLATFORM` and `XDG_RUNTIME_DIR` for headless runs |
| `Publish/publish_linux/trse.ini` | Default global settings shipped with Linux packages / Docker |

---

## Building the Docker image

From the repository root:

```bash
docker build -f docker/Dockerfile -t trse:cli .
```

See comments in `docker/Dockerfile` for build args (`MAKE_JOBS`, etc.).
