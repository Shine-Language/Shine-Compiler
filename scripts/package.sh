#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
DIST_DIR="$ROOT_DIR/dist/shine-v0.5.0-BETA"
TOOLCHAIN_DIR="$DIST_DIR/toolchain"
TOOLCHAIN_BIN="$TOOLCHAIN_DIR/bin"
TOOLCHAIN_LIB="$TOOLCHAIN_DIR/lib"
TOOLCHAIN_GCC="$TOOLCHAIN_LIB/gcc/current"
INSTALLER_TOOLCHAIN_DIR="$ROOT_DIR/build/release-toolchain"
INSTALLER_TOOLCHAIN_BIN="$INSTALLER_TOOLCHAIN_DIR/bin"
INSTALLER_TOOLCHAIN_LIB="$INSTALLER_TOOLCHAIN_DIR/lib"
INSTALLER_TOOLCHAIN_GCC="$INSTALLER_TOOLCHAIN_LIB/gcc/current"
GCC_DIR="$(dirname "$(/mingw64/bin/g++ -print-libgcc-file-name)")"

mkdir -p "$DIST_DIR"
mkdir -p "$TOOLCHAIN_BIN" "$TOOLCHAIN_LIB" "$TOOLCHAIN_GCC"
mkdir -p "$INSTALLER_TOOLCHAIN_BIN" "$INSTALLER_TOOLCHAIN_LIB" "$INSTALLER_TOOLCHAIN_GCC"

cp "$BUILD_DIR/shinec.exe" "$DIST_DIR/"
cp -r "$ROOT_DIR/examples" "$DIST_DIR/"
cp /mingw64/bin/ld.exe "$TOOLCHAIN_BIN/"
cp /mingw64/bin/ld.exe "$INSTALLER_TOOLCHAIN_BIN/"

for dll in \
  libintl-8.dll \
  libiconv-2.dll \
  zlib1.dll \
  libzstd.dll
do
  cp "/mingw64/bin/$dll" "$TOOLCHAIN_BIN/"
  cp "/mingw64/bin/$dll" "$INSTALLER_TOOLCHAIN_BIN/"
done

for file in \
  crt2.o \
  default-manifest.o \
  libmingw32.a \
  libmingwex.a \
  libmsvcrt.a \
  libkernel32.a \
  libuser32.a \
  libshell32.a \
  libadvapi32.a \
  libpthread.a \
  libwinpthread.a \
  libgcc_s.a
do
  cp "/mingw64/lib/$file" "$TOOLCHAIN_LIB/"
  cp "/mingw64/lib/$file" "$INSTALLER_TOOLCHAIN_LIB/"
done

for file in \
  crtbegin.o \
  crtend.o \
  libgcc.a \
  libgcc_eh.a
do
  cp "$GCC_DIR/$file" "$TOOLCHAIN_GCC/"
  cp "$GCC_DIR/$file" "$INSTALLER_TOOLCHAIN_GCC/"
done

for dll in \
  libgcc_s_seh-1.dll \
  libwinpthread-1.dll \
  libstdc++-6.dll \
  zlib1.dll \
  libzstd.dll
do
  cp "/mingw64/bin/$dll" "$DIST_DIR/"
done

echo "Packaged release into: $DIST_DIR"
