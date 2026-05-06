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
    && install -m755 vobjdump      /opt/vasm/bin/vobjdump \
    && make clean \
    && make CPU=6502 SYNTAX=oldstyle \
    && install -m755 vasm6502_oldstyle /opt/vasm/bin/vasm6502_oldstyle

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
# vbcc compiler (interactive Makefile prompts go to stdin via `yes '').
# Build BOTH the m68k backend (used by Atari ST + Amiga) and the 65816
# backend (used by the SNES target — configs + libs come from the
# vbcc65816 r2 distribution further down). Same source tree handles
# both targets via TARGET=<arch>; binaries are named per-target
# (vbccm68k, vbcc65816) so they coexist in /opt/vbcc/bin.
RUN set -eux \
    && curl -fsSL "http://www.ibaug.de/vbcc/vbcc.tar.gz" -o /tmp/vbcc.tar.gz \
    && mkdir -p /tmp/vbcc && tar -xzf /tmp/vbcc.tar.gz -C /tmp/vbcc --strip-components=1 \
    && cd /tmp/vbcc \
    && mkdir -p bin \
    && yes '' | make TARGET=m68k \
    && yes '' | make TARGET=65816 \
    && install -d /opt/vbcc/bin \
    && install -m755 bin/* /opt/vbcc/bin/
# vlink linker (separate Volker tool, needed by vbcc m68k targets)
RUN set -eux \
    && curl -fsSL "http://sun.hasenbraten.de/vlink/release/vlink.tar.gz" -o /tmp/vlink.tar.gz \
    && mkdir -p /tmp/vlink && tar -xzf /tmp/vlink.tar.gz -C /tmp/vlink --strip-components=1 \
    && cd /tmp/vlink \
    && make \
    && install -m755 vlink /opt/vbcc/bin/vlink
# Unix `vc` driver config + per-target configs (aos68k, tos, …).
# Archive layout: vbcc_unix_config/{config,...} → strip 1 to land at /opt/vbcc/config.
# Amiga + Atari target archives have a wrapper dir + nested
# `config/`, `targets/<arch>/{lib,include}` — flatten into /opt/vbcc
# so paths line up with what `vc` expects ($VBCC/config/<name> +
# $VBCC/targets/<arch>/{lib,include}).
RUN set -eux \
    && mkdir -p /opt/vbcc/config /opt/vbcc/targets \
    && curl -fsSL "http://phoenix.owl.de/vbcc/${VBCC_TARGET_DATE}/vbcc_unix_config.tar.gz" -o /tmp/vbcc_cfg.tar.gz \
    && tar -xzf /tmp/vbcc_cfg.tar.gz -C /opt/vbcc \
    \
    && curl -fsSL "http://phoenix.owl.de/vbcc/${VBCC_TARGET_DATE}/vbcc_target_m68k-amigaos.lha" -o /tmp/vbcc_amigaos.lha \
    && mkdir -p /tmp/extract_amigaos && cd /tmp/extract_amigaos \
    && lhasa x /tmp/vbcc_amigaos.lha \
    && cp -a /tmp/extract_amigaos/vbcc_target_m68k-amigaos/targets/. /opt/vbcc/targets/ 2>/dev/null || true \
    \
    && curl -fsSL "http://phoenix.owl.de/vbcc/${VBCC_TARGET_DATE}/vbcc_target_m68k-atari.tar.gz" -o /tmp/vbcc_atari.tar.gz \
    && mkdir -p /tmp/extract_atari && cd /tmp/extract_atari \
    && tar -xzf /tmp/vbcc_atari.tar.gz \
    && cp -a /tmp/extract_atari/vbcc_target_m68k-atari/targets/. /opt/vbcc/targets/ 2>/dev/null || true \
    \
    && rm -rf /tmp/extract_amigaos /tmp/extract_atari /tmp/vbcc_*.lha /tmp/vbcc_*.tar.gz
# Note: target archives also ship Amiga-style assign-path configs
# (`-Ivincludeos3:`) that would override vbcc_unix_config's
# linux-friendly `$VBCC/targets/...` paths. We only copy targets/
# from the archives (lib/+include/) — never config/.

# Overlay the m68k-amigaos target with kusma/amiga-dev's NDK headers +
# Kickstart 1.3 runtime. The stock vbcc target archive ships only
# stdlib headers (no <exec/types.h>, no <proto/dos.h>) and a single
# startup.o that calls dos.library v34+ functions during libnix init —
# fine on AROS / KS 2.0+, hangs hard on KS 1.3.
#
# kusma/amiga-dev bundles a full NDK header tree under
# targets/m68k-amigaos/include and ships startup13.o + m13.lib for
# KS 1.3 specifically. Overlay (cp -a) so any file vbcc's archive
# already provided wins on conflict — we only add what's missing.
#
# A new vbcc config 'aos68k13' linked against startup13.o + m13.lib
# is dropped alongside the stock aos68k so api/vbcc/compile.php can
# pick the KS 1.3 variant via 'vc +aos68k13' for the broadest compat
# (AROS + KS 1.3 + KS 2.0+).
RUN set -eux \
    && apt-get update && apt-get install -y --no-install-recommends git \
    && git clone --depth 1 https://github.com/kusma/amiga-dev /tmp/amiga-dev \
    && cp -an /tmp/amiga-dev/targets/m68k-amigaos/include/. /opt/vbcc/targets/m68k-amigaos/include/ \
    && cp -an /tmp/amiga-dev/targets/m68k-amigaos/lib/. /opt/vbcc/targets/m68k-amigaos/lib/ \
    && rm -rf /tmp/amiga-dev /var/lib/apt/lists/*
# vbcc config for KS 1.3 builds — same toolchain as +aos68k but link
# with startup13.o + m13.lib so libnix init stays inside the
# dos.library v33 ABI.
RUN set -eux \
    && cp /opt/vbcc/config/aos68k /opt/vbcc/config/aos68k13 \
    && sed -i 's|startup\.o|startup13.o|g' /opt/vbcc/config/aos68k13 \
    && sed -i 's|-lmieee|-lm13|g'           /opt/vbcc/config/aos68k13

# vbcc65816 r2 — pull the SNES target's configs + libs + headers from
# the upstream zip (ibaug.de). The included Linux binaries are x86_64
# only and would not run on the production arm64 host, so we ignore
# them and rely on the vbcc 65816 backend we just source-built above
# (vbcc65816), plus vasm6502_oldstyle from the vasm-builder stage and
# the existing source-built vlink. The lib/*.a archives are vobj-
# format — vlink reads them on any host architecture, so portable.
# Layout under /opt/vbcc65816/vbcc:
#   config/{snes-mini,snes-lo,snes-hi,snes-los,snes-his,iigs,iigss,...}
#   targets/65816-snes/{lib,include}
#   bin/  (symlinks to /opt/vbcc/bin tools so vc finds them via $PATH)
RUN set -eux \
    && apt-get update && apt-get install -y --no-install-recommends unzip \
    && curl -fsSL "http://www.ibaug.de/vbcc/vbcc65816_r2.zip" -o /tmp/vbcc65816.zip \
    && unzip -q /tmp/vbcc65816.zip -d /tmp \
    && mkdir -p /opt/vbcc65816/vbcc/bin \
    && cp -a /tmp/vbcc65816/vbcc65816_linux/vbcc/config  /opt/vbcc65816/vbcc/config \
    && cp -a /tmp/vbcc65816/vbcc65816_linux/vbcc/targets /opt/vbcc65816/vbcc/targets \
    && rm -rf /tmp/vbcc65816 /tmp/vbcc65816.zip \
    && rm -rf /var/lib/apt/lists/*

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
    curl \
    mame \
    nasm \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /opt/trse /opt/trse
COPY --from=rgbds-builder /opt/rgbds/bin/rgbasm /usr/local/bin/rgbasm
COPY --from=rgbds-builder /opt/rgbds/bin/rgblink /usr/local/bin/rgblink
COPY --from=rgbds-builder /opt/rgbds/bin/rgbfix /usr/local/bin/rgbfix
COPY --from=rgbds-builder /opt/rgbds/bin/rgbgfx /usr/local/bin/rgbgfx
COPY --from=rgbds-builder /opt/rgbds/share/man /usr/local/share/man
COPY --from=vasm-builder  /opt/vasm/bin/vasmm68k_mot       /usr/local/bin/vasmm68k_mot
COPY --from=vasm-builder  /opt/vasm/bin/vasm6502_oldstyle  /usr/local/bin/vasm6502_oldstyle
COPY --from=vasm-builder  /opt/vasm/bin/vobjdump           /usr/local/bin/vobjdump
COPY --from=cc65-builder  /opt/cc65 /opt/cc65
COPY --from=vbcc-builder  /opt/vbcc /opt/vbcc
COPY --from=vbcc-builder  /opt/vbcc65816 /opt/vbcc65816
ENV VBCC=/opt/vbcc

# Disk-image packagers — wrap raw m68k executables into emulator-loadable
# media so the IDE pipeline can produce .st (Atari ST floppy via
# makeflop.py) and .adf (Amiga floppy via amitools.xdftool). Both
# Python; pip-install into the runtime image.
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 python3-pip python3-setuptools \
    && rm -rf /var/lib/apt/lists/*
RUN pip3 install --no-cache-dir --break-system-packages amitools \
    && curl -fsSL "https://raw.githubusercontent.com/bbbradsmith/makeflop.py/TwoSide/makeflop.py" -o /usr/local/bin/makeflop.py \
    && chmod +x /usr/local/bin/makeflop.py

# OpenJDK 21 from Adoptium (Temurin). Debian bookworm main only ships
# JDK 17; portamos-0.0.1-beta8.jar is compiled with class file
# version 65 (Java 21), which 17 can't load. Tarball install keeps
# the apt deps minimal.
ARG TEMURIN_VERSION=21.0.5+11
ARG TEMURIN_TAG=jdk-21.0.5%2B11
RUN set -eux \
    && arch="$(dpkg --print-architecture)" \
    && case "$arch" in \
         arm64) j_arch=aarch64 ;; \
         amd64) j_arch=x64 ;; \
         *) echo "unsupported arch $arch" >&2; exit 1 ;; \
       esac \
    && curl -fsSL "https://github.com/adoptium/temurin21-binaries/releases/download/${TEMURIN_TAG}/OpenJDK21U-jre_${j_arch}_linux_hotspot_21.0.5_11.tar.gz" \
        -o /tmp/temurin.tar.gz \
    && mkdir -p /opt/jvm \
    && tar -xzf /tmp/temurin.tar.gz -C /opt/jvm --strip-components=1 \
    && rm /tmp/temurin.tar.gz
ENV JAVA_HOME=/opt/jvm
ENV PATH="/opt/jvm/bin:${PATH}"

# AMOS tokenizer — Fredrik Rambris's portamos, distributed as a
# runnable fat JAR (the repo is Kotlin/Gradle, not pip-installable).
# Pin to a tagged release so rebuilds are reproducible.
ARG PORTAMOS_VERSION=v0.0.1-beta8
ARG PORTAMOS_JAR=portamos-0.0.1-beta.8-all.jar
RUN mkdir -p /opt/portamos \
    && curl -fsSL "https://github.com/fredrik-rambris/portamos/releases/download/${PORTAMOS_VERSION}/${PORTAMOS_JAR}" \
        -o /opt/portamos/portamos.jar \
    && printf '#!/bin/sh\nexec java -jar /opt/portamos/portamos.jar "$@"\n' > /usr/local/bin/portamos \
    && chmod +x /usr/local/bin/portamos

# stos2asc — Java tool from STOSCoders VS Code extension. Class file
# declares `package stos2asc;` so it must live at
# /opt/stos/stos2asc/stos2asc.class for `java -cp /opt/stos stos2asc.stos2asc`
# to load it. Wrap the invocation in a /usr/local/bin/stos2asc shim.
COPY docker/stos2asc.class /opt/stos/stos2asc/stos2asc.class
RUN printf '#!/bin/sh\nexec java -cp /opt/stos stos2asc.stos2asc "$@"\n' > /usr/local/bin/stos2asc \
    && chmod +x /usr/local/bin/stos2asc
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

ENV PATH="/opt/trse/bin:/opt/vbcc/bin:${PATH}" \
    QT_QPA_PLATFORM=offscreen \
    XDG_RUNTIME_DIR=/tmp/trse-runtime

ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
# No args → runs CLI without parameters so usage / errors go to stdout (no GUI).
CMD ["trse", "-cli"]
