#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Sill through Felucca's engines on the host (tests/si_felucca.c): a groove to a WAV.
#   tests/si_felucca.sh OUTDIR [SECONDS]     (needs build/gen: run ./build.sh once, or tools/build.py's generate)
set -eu
OUT=${1:?usage: tests/si_felucca.sh OUTDIR [SECONDS]}
mkdir -p build/host "$OUT"
[ -f build/gen/felucca_tables.h ] || python3 -c "import sys; sys.argv=['x']; sys.path.insert(0,'tools'); import build; build.generate()"
cc -O2 -w -DFELUCCA_SLICE=0 -Ibuild/gen -Ifirmware/src -o build/host/si_felucca tests/si_felucca.c firmware/src/si_groove.c -lm
build/host/si_felucca "$OUT" "${2:-0}"
