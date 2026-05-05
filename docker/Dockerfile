# TRSE — headless compiler CLI (multi-arch: arm64 + amd64 Debian Bookworm).
#
# Build:  docker build -f docker/Dockerfile -t trse:cli .
# Run:    docker run --rm -v /path/to/project:/work -w /work trse:cli \
#            trse -cli op=project project=my.trse input_file=main.ras settings=/etc/trse/trse.ini
#
# Uses system liblua5.4 (bundled liblua.a is x86-64 only). Default trse.ini: /etc/trse/trse.ini
# If offscreen fails: docker run -e QT_QPA_PLATFORM=minimal ...  or  xvfb-run docker run ...
#
# Parallel compile: default -j1 — qrc_resources.cpp is huge and OOMs easily.
# Override: docker build --build-arg MAKE_JOBS=4 ...
# resources_big: Qt RCC uses a lower-memory path for large .qrc (see QTBUG-50468).

FROM debian:bookworm-slim AS builder

ARG MAKE_JOBS=1

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    qtbase5-dev \
    qt5-qmake \
    qtdeclarative5-dev \
    libqt5opengl5-dev \
    libgl1-mesa-dev \
    libgomp1 \
    ca-certificates \
    liblua5.4-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Replace bundled x86 liblua link with system liblua5.4 (required on ARM).
RUN sed -i 's|LIBS += -L$$PWD/libs/lua/ -llua -ldl|LIBS += -llua5.4 -ldl|' TRSE.pro \
    && sed -i '/^CONFIG += c++17$/a CONFIG += resources_big' TRSE.pro \
    && qmake PREFIX=/usr TRSE.pro \
    && make -j"${MAKE_JOBS}"

# Install binary + resources + built-in TRU units (@use "screen/screen", etc.).
# Parser::HandleUseTPU looks under Util::GetSystemPrefix()+units/<System>/ (Linux: ../units from trse binary).
RUN install -d /opt/trse/bin /opt/trse/Publish \
    && install -m755 trse /opt/trse/bin/ \
    && cp -a Publish /opt/trse/ \
    && cp -a units /opt/trse/

# vasm — m68k cross assembler for ATARI520ST + AMIGA TRSE targets.
# Not in Debian apt; build from upstream Volker Barthelmann sources.
# Produces vasmm68k_mot (Motorola syntax) which TRSE invokes via the
# `vasmm` ini key. ATARI520ST tutorials reference this binary directly.
FROM debian:bookworm-slim AS vasm-builder
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential curl ca-certificates \
    && rm -rf /var/lib/apt/lists/*
# Canonical "latest stable" tarball from Volker Barthelmann
# (http://sun.hasenbraten.de/vasm/index.php?view=downloads). The tagged
# server.owl.de mirror returns 404; this URL is the same source the
# upstream docs point at and gives a current vasm.
RUN set -eux \
    && curl -fsSL "http://sun.hasenbraten.de/vasm/release/vasm.tar.gz" -o /tmp/vasm.tar.gz \
    && mkdir -p /tmp/vasm && tar -xzf /tmp/vasm.tar.gz -C /tmp/vasm --strip-components=1 \
    && cd /tmp/vasm \
    && make CPU=m68k SYNTAX=mot \
    && install -d /opt/vasm/bin \
    && install -m755 vasmm68k_mot /opt/vasm/bin/vasmm68k_mot \
    && install -m755 vobjdump      /opt/vasm/bin/vobjdump

# cc65 — 65xx cross compiler/assembler suite (cl65, ca65, ld65, …).
# TRSE invokes cl65 for SNES (65C816) builds. Not packaged in Debian
# bookworm; build from upstream tag. Installs to /opt/cc65 so it
# matches the path TRSE writes into session trse.ini (cl65 =
# /opt/cc65/bin/cl65) without needing host bind-mounts.
FROM debian:bookworm-slim AS cc65-builder
ARG CC65_VERSION=V2.19
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential curl ca-certificates \
    && rm -rf /var/lib/apt/lists/*
RUN set -eux \
    && curl -fsSL "https://github.com/cc65/cc65/archive/refs/tags/${CC65_VERSION}.tar.gz" -o /tmp/cc65.tar.gz \
    && mkdir -p /tmp/cc65 && tar -xzf /tmp/cc65.tar.gz -C /tmp/cc65 --strip-components=1 \
    && cd /tmp/cc65 \
    && make -j"$(nproc)" PREFIX=/opt/cc65 \
    && make install PREFIX=/opt/cc65

# vbcc + vlink — portable C cross compiler + linker (Volker Barthelmann).
# Same upstream family as vasm. Produces working m68k executables for
# AmigaOS (.exe hunk format) and Atari ST (.tos) — bypassing TRSE's
# broken m68k Pascal codegen on EmuTOS/AROS. Compiler built from
# source so it's native arm64; targets pulled as precompiled archives.
# Sources: ibaug.de mirror for vbcc + phoenix.owl.de mirror for the
# m68k targets (the canonical compilers.de paths 404 today).
FROM debian:bookworm-slim AS vbcc-builder
ARG VBCC_TARGET_DATE=2022-05-22
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential bison flex curl ca-certificates lhasa \
    && rm -rf /var/lib/apt/lists/*
# vbcc compiler (interactive Makefile prompts go to stdin via `yes ''`)
RUN set -eux \
    && curl -fsSL "http://www.ibaug.de/vbcc/vbcc.tar.gz" -o /tmp/vbcc.tar.gz \
    && mkdir -p /tmp/vbcc && tar -xzf /tmp/vbcc.tar.gz -C /tmp/vbcc --strip-components=1 \
    && cd /tmp/vbcc \
    && mkdir -p bin \
    && yes '' | make TARGET=m68k \
    && install -d /opt/vbcc/bin \
    && install -m755 bin/* /opt/vbcc/bin/
# vlink linker (separate Volker tool, needed by vbcc m68k targets)
RUN set -eux \
    && curl -fsSL "http://sun.hasenbraten.de/vlink/release/vlink.tar.gz" -o /tmp/vlink.tar.gz \
    && mkdir -p /tmp/vlink && tar -xzf /tmp/vlink.tar.gz -C /tmp/vlink --strip-components=1 \
    && cd /tmp/vlink \
    && make \
    && install -m755 vlink /opt/vbcc/bin/vlink
# Unix `vc` driver config (per-target .vc files) — needed for vc m68k-* invocations
RUN set -eux \
    && curl -fsSL "http://phoenix.owl.de/vbcc/${VBCC_TARGET_DATE}/vbcc_unix_config.tar.gz" -o /tmp/vbcc_cfg.tar.gz \
    && mkdir -p /opt/vbcc/config \
    && tar -xzf /tmp/vbcc_cfg.tar.gz -C /opt/vbcc/config --strip-components=1
# AmigaOS m68k target (LHA archive)
RUN set -eux \
    && curl -fsSL "http://phoenix.owl.de/vbcc/${VBCC_TARGET_DATE}/vbcc_target_m68k-amigaos.lha" -o /tmp/vbcc_amigaos.lha \
    && mkdir -p /opt/vbcc/targets/m68k-amigaos \
    && cd /opt/vbcc/targets/m68k-amigaos \
    && lhasa x /tmp/vbcc_amigaos.lha
# Atari ST m68k target (tar.gz mirror)
RUN set -eux \
    && curl -fsSL "http://phoenix.owl.de/vbcc/${VBCC_TARGET_DATE}/vbcc_target_m68k-atari.tar.gz" -o /tmp/vbcc_atari.tar.gz \
    && mkdir -p /opt/vbcc/targets/m68k-atari \
    && tar -xzf /tmp/vbcc_atari.tar.gz -C /opt/vbcc/targets/m68k-atari

# RGBDS (Game Boy assembler) — not in Debian bookworm main; build from
# upstream release tarball. Produces rgbasm / rgblink / rgbfix / rgbgfx.
# Pin version for reproducible builds.
FROM debian:bookworm-slim AS rgbds-builder
ARG RGBDS_VERSION=0.8.0
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential bison cmake pkg-config libpng-dev curl ca-certificates \
    && rm -rf /var/lib/apt/lists/*
RUN set -eux \
    && curl -fsSL "https://github.com/gbdev/rgbds/releases/download/v${RGBDS_VERSION}/rgbds-${RGBDS_VERSION}.tar.gz" -o /tmp/rgbds.tar.gz \
    && mkdir -p /tmp/rgbds && tar -xzf /tmp/rgbds.tar.gz -C /tmp/rgbds --strip-components=1 \
    && cmake -S /tmp/rgbds -B /tmp/rgbds/build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/rgbds \
    && cmake --build /tmp/rgbds/build -j"$(nproc)" \
    && cmake --install /tmp/rgbds/build

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libqt5core5a \
    libqt5gui5 \
    libqt5widgets5 \
    libqt5opengl5 \
    libqt5qml5 \
    libqt5quick5 \
    libqt5network5 \
    libgl1 \
    libgomp1 \
    liblua5.4-0 \
    libpng16-16 \
    xvfb \
    ca-certificates \
    mame \
    nasm \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /opt/trse /opt/trse
COPY --from=rgbds-builder /opt/rgbds/bin/rgbasm /usr/local/bin/rgbasm
COPY --from=rgbds-builder /opt/rgbds/bin/rgblink /usr/local/bin/rgblink
COPY --from=rgbds-builder /opt/rgbds/bin/rgbfix /usr/local/bin/rgbfix
COPY --from=rgbds-builder /opt/rgbds/bin/rgbgfx /usr/local/bin/rgbgfx
COPY --from=rgbds-builder /opt/rgbds/share/man /usr/local/share/man
COPY --from=vasm-builder  /opt/vasm/bin/vasmm68k_mot /usr/local/bin/vasmm68k_mot
COPY --from=vasm-builder  /opt/vasm/bin/vobjdump      /usr/local/bin/vobjdump
COPY --from=cc65-builder  /opt/cc65 /opt/cc65
COPY --from=vbcc-builder  /opt/vbcc /opt/vbcc
ENV VBCC=/opt/vbcc
COPY docker/entrypoint.sh /usr/local/bin/docker-entrypoint.sh
# NASM: x86/DOS TRSE targets invoke nasm (expected on PATH, typically /usr/bin/nasm).
# CoCo/Dragon PostProcess looks for imgtool next to the "emulator" binary (see systemtrs80coco.cpp:
# getEmulatorName() -> trse.ini pokemonmini_emulator). Debian installs mame + imgtool in /usr/games/.
RUN chmod +x /usr/local/bin/docker-entrypoint.sh \
    && install -d /etc/trse \
    && cp /opt/trse/Publish/publish_linux/trse.ini /etc/trse/trse.ini \
    && grep -q '^pokemonmini_emulator' /etc/trse/trse.ini \
    || echo 'pokemonmini_emulator = /usr/games/mame' >> /etc/trse/trse.ini \
    && sed -i 's|^vasmm *=.*|vasmm = /usr/local/bin/vasmm68k_mot|' /etc/trse/trse.ini \
    && sed -i 's|^vasmm_target_dir *=.*|vasmm_target_dir = /tmp|' /etc/trse/trse.ini \
    && sed -i 's|^vasmm_target_dir_atarist *=.*|vasmm_target_dir_atarist = /tmp|' /etc/trse/trse.ini \
    && (grep -q '^cl65' /etc/trse/trse.ini && sed -i 's|^cl65 *=.*|cl65 = /opt/cc65/bin/cl65|' /etc/trse/trse.ini || echo 'cl65 = /opt/cc65/bin/cl65' >> /etc/trse/trse.ini)

ENV PATH="/opt/trse/bin:${PATH}" \
    QT_QPA_PLATFORM=offscreen \
    XDG_RUNTIME_DIR=/tmp/trse-runtime

ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
# No args → runs CLI without parameters so usage / errors go to stdout (no GUI).
CMD ["trse", "-cli"]
