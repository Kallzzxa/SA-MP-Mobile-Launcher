#!/usr/bin/env bash
# Compiles the real Localization.cpp and SafeMode.h against small host stubs and
# runs the assertions. Every run copies the current sources, so the test always
# reflects what is in the tree.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
work="$here/build"
samp="$root/app/src/main/cpp/samp"

rm -rf "$work"
mkdir -p "$work/game" "$work/vendor/shadowhook"
cp "$here/stubs/main.h" "$work/main.h"
cp "$here/stubs/vendor/shadowhook/patch.h" "$work/vendor/shadowhook/patch.h"
cp "$samp/game/Localization.cpp" "$samp/game/Localization.h" "$samp/game/SafeMode.h" "$work/game/"

c++ -std=c++17 -Wall -Wextra -Wno-unused-parameter \
    -I"$work" -I"$work/game" \
    "$here/tests.cpp" "$work/game/Localization.cpp" \
    -o "$work/hosttest"

"$work/hosttest"
