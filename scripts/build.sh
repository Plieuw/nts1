#!/usr/bin/env bash
#
# Build the NTS-1 units in this repository.
#
#   ./scripts/build.sh                 build every unit into dist/
#   ./scripts/build.sh noise lpg       build only the named units
#   ./scripts/build.sh --setup         only fetch the SDK and the toolchain
#
# The logue SDK and the toolchain are downloaded into .sdk/ on first use, which
# is git-ignored. Set SDK_DIR or GCC_BIN_PATH to use your own copies.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK_DIR="${SDK_DIR:-$ROOT/.sdk/logue-sdk}"
SDK_REF="${SDK_REF:-main}"
PLATFORM_DIR="$SDK_DIR/platform/nutekt-digital"
GCC_PATH="${GCC_BIN_PATH:-$SDK_DIR/tools/gcc/gcc-arm-none-eabi-5_4-2016q3/bin}"
DIST="$ROOT/dist"

ALL_UNITS=(noise lpg reso elements barber irbody)

fetch_sdk() {
  if [ ! -d "$SDK_DIR" ]; then
    echo ">> cloning the logue SDK into $SDK_DIR"
    mkdir -p "$(dirname "$SDK_DIR")"
    git clone --depth 1 --branch "$SDK_REF" \
      https://github.com/korginc/logue-sdk.git "$SDK_DIR"
  fi

  # The platform headers include arm_math.h from the CMSIS submodule, so it has
  # to be checked out before anything builds.
  if [ ! -f "$SDK_DIR/platform/ext/CMSIS/CMSIS/Include/arm_math.h" ]; then
    echo ">> fetching the CMSIS headers"
    git -C "$SDK_DIR" submodule update --init --depth 1 platform/ext/CMSIS
  fi

  # The NTS-1 (mkI) units are built with gcc 5.4; anything else may produce
  # binaries the firmware refuses to load.
  if [ ! -x "$GCC_PATH/arm-none-eabi-gcc" ]; then
    echo ">> fetching the gcc 5.4 toolchain"
    ( cd "$SDK_DIR/tools/gcc" && bash get_gcc_5_4-2016q3_linux.sh ) || true
  fi

  if [ ! -x "$GCC_PATH/arm-none-eabi-gcc" ]; then
    if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
      echo "!! gcc 5.4 is unavailable, using $(command -v arm-none-eabi-gcc)"
      echo "!! builds made with it are not guaranteed to run on the hardware"
      GCC_PATH="$(dirname "$(command -v arm-none-eabi-gcc)")"
    else
      echo "!! no ARM toolchain found, cannot build" >&2
      exit 1
    fi
  fi

  echo ">> using $("$GCC_PATH/arm-none-eabi-gcc" --version | head -n 1)"
}

build_unit() {
  local unit="$1"
  echo ">> building $unit"
  make -C "$ROOT/units/$unit" \
    PLATFORMDIR="$PLATFORM_DIR" \
    TOOLSDIR="$SDK_DIR/tools" \
    EXTDIR="$SDK_DIR/platform/ext" \
    GCC_BIN_PATH="$GCC_PATH" \
    INSTALLDIR="$DIST" \
    install
}

mkdir -p "$DIST"

if [ "${1:-}" = "--setup" ]; then
  fetch_sdk
  exit 0
fi

fetch_sdk

if [ "$#" -eq 0 ]; then
  targets=("${ALL_UNITS[@]}")
else
  targets=("$@")
fi

for unit in "${targets[@]}"; do
  if [ ! -d "$ROOT/units/$unit" ]; then
    echo "!! unknown unit: $unit" >&2
    exit 1
  fi
  build_unit "$unit"
done

echo ">> done, unit files are in $DIST"
