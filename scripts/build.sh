#!/bin/bash
set -e
MODULE_ID="serge"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$SCRIPT_DIR/.."

docker build -t move-anything-builder "$SCRIPT_DIR"
mkdir -p "$ROOT/dist/$MODULE_ID"

docker run --rm \
  -v "$ROOT:/build" \
  move-anything-builder \
  aarch64-linux-gnu-gcc \
    -O2 -shared -fPIC \
    -o "/build/dist/$MODULE_ID/$MODULE_ID.so" \
    "/build/src/dsp/$MODULE_ID.c" \
    -lm

cp "$ROOT/src/module.json" "$ROOT/dist/$MODULE_ID/"
tar -czf "$ROOT/dist/$MODULE_ID-module.tar.gz" -C "$ROOT/dist" "$MODULE_ID/"
echo "Built: dist/$MODULE_ID-module.tar.gz"
