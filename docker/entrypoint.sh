#!/bin/sh
set -e
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/trse-runtime}"
mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
exec "$@"
