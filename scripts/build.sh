#!/bin/bash
set -e
MODULE_ID="denis"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$SCRIPT_DIR/.."
WIN_ROOT="$(cd "$ROOT" && pwd -W 2>/dev/null || pwd)"

docker build -t schwung-builder "$SCRIPT_DIR"
mkdir -p "$ROOT/dist/$MODULE_ID"

CONTAINER_ID=$(MSYS_NO_PATHCONV=1 docker create -w /build schwung-builder bash -c "\
  mkdir -p dist/$MODULE_ID && \
  aarch64-linux-gnu-gcc \
    -O2 -shared -fPIC \
    -o dist/$MODULE_ID/dsp.so \
    src/dsp/$MODULE_ID.c \
    -lm -ffast-math -Wall -Wno-unused-variable")

docker cp "$WIN_ROOT/src" "$CONTAINER_ID:/build/src"
docker start -a "$CONTAINER_ID"
docker cp "$CONTAINER_ID:/build/dist/$MODULE_ID/dsp.so" "$ROOT/dist/$MODULE_ID/"
docker rm "$CONTAINER_ID" > /dev/null

cp "$ROOT/src/module.json" "$ROOT/dist/$MODULE_ID/"
tar -czf "$ROOT/dist/$MODULE_ID-module.tar.gz" -C "$ROOT/dist" "$MODULE_ID/"
echo "Built: dist/$MODULE_ID-module.tar.gz"
