#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Sill's host tests: the generated files up to date (the splash, the FM6 patches), the groove engine strict about
# warnings and single precision and its own tests (tests/si_groove_test.c), the screen and its layout lint
# (tests/si_ui_test.sh).
#   sh tests/si_tests.sh        (exit 0: all passed)
set -eu
mkdir -p build/host
python3 tools/si_logo.py --check
python3 tools/si_fm6.py --check
cc -std=c11 -O2 -Wall -Wextra -Wshadow -Wdouble-promotion -Werror -Ifirmware/src -c firmware/src/si_groove.c \
   -o build/host/si_groove.o
cc -std=c11 -O2 -Wall -Ifirmware/src -o build/host/si_groove_test tests/si_groove_test.c firmware/src/si_groove.c -lm
build/host/si_groove_test
sh tests/si_ui_test.sh build/si_ui
echo "si_tests: all passed"
