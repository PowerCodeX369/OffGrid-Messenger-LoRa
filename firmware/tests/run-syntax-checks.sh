#!/bin/sh
# Header-based syntax check ONLY. Mocked public types do not validate real IDF linking.
set -eu
cd "$(dirname "$0")/../.."
clang -std=gnu11 -Werror=implicit-function-declaration -Werror=incompatible-pointer-types \
    -fsyntax-only -DCONFIG_OFFGRID_USB_IRQ_RX=1 \
    -Ifirmware/tests/host_stubs -Ifirmware/main \
    -Ifirmware/components/offgrid_sx1278/include firmware/main/main.c
clang -std=gnu11 -Werror=implicit-function-declaration -Werror=incompatible-pointer-types \
    -fsyntax-only -Ifirmware/tests/host_stubs \
    -Ifirmware/components/offgrid_sx1278/include \
    firmware/components/offgrid_sx1278/offgrid_sx1278.c
printf 'PASS: C source syntax against host API stubs (not ESP-IDF build)\n'
