#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cmake --preset macos-release
cmake --build --preset macos-release --parallel
ctest --preset macos-release
if [ "${1:-}" = "--run" ]; then
    exec open "$HOME/buildfiles/usbtree/macos-release/portable/UsbTree/UsbTree.app"
fi
