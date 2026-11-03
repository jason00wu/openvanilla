#!/bin/sh
# Compile and run the built-in Simplex Shift-toggle logic tests.
# Does not build or install the input method.
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/simplex-shift-tests"
clang++ -std=c++17 -Wall -Wextra -Werror -ISource/Mac -o "$out" Tests/SimplexShiftToggleTests.cpp
"$out"
