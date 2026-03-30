# TRSE — headless compiler CLI (multi-arch: arm64 + amd64 Debian Bookworm).
#
# Build:  docker build -f docker/Dockerfile -t trse:cli .
# Run:    docker run --rm -v /path/to/project:/work -w /work trse:cli \
#            trse -cli op=project project=my.trse input_file=main.ras settings=/etc/trse/trse.ini
#
# Uses system liblua5.4 (bundled liblua.a is x86-64 only). Default trse.ini: /etc/trse/trse.ini
# If offscreen fails: docker run -e QT_QPA_PLATFORM=minimal ...  or  xvfb-run docker run ...

FROM debian:bookworm-slim AS builder

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
    && qmake PREFIX=/usr TRSE.pro \
    && make -j"$(nproc)"

# Install binary + published resources (themes, ini template) like TRSE linux package.
RUN install -d /opt/trse/bin /opt/trse/Publish \
    && install -m755 trse /opt/trse/bin/ \
    && cp -a Publish /opt/trse/

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
    xvfb \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /opt/trse /opt/trse
COPY docker/entrypoint.sh /usr/local/bin/docker-entrypoint.sh
RUN chmod +x /usr/local/bin/docker-entrypoint.sh \
    && install -d /etc/trse \
    && cp /opt/trse/Publish/publish_linux/trse.ini /etc/trse/trse.ini

ENV PATH="/opt/trse/bin:${PATH}" \
    QT_QPA_PLATFORM=offscreen \
    XDG_RUNTIME_DIR=/tmp/trse-runtime

ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
# No args → runs CLI without parameters so usage / errors go to stdout (no GUI).
CMD ["trse", "-cli"]
