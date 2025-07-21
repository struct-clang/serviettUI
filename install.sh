#!/bin/bash

BREW_PREFIX=$(brew --prefix)
SRC_DIR="./src"
INCLUDE_DIR="./include"
BUILD_DIR="./build"
INSTALL_LIB_DIR="$BREW_PREFIX/lib"
INSTALL_INCLUDE_DIR="$BREW_PREFIX/include"

CXX=clang++
CXXFLAGS="-std=c++17 -Wall -Wextra -I$INCLUDE_DIR -I$INSTALL_INCLUDE_DIR $(sdl2-config --cflags)"
LDFLAGS="$(sdl2-config --libs) -lSDL2_ttf -lSDL2_gfx"

mkdir -p $BUILD_DIR

SRCS=$(find "$SRC_DIR" -name "*.cpp")
OBJS=""

for src in $SRCS; do
  obj="$BUILD_DIR/$(basename ${src%.cpp}.o)"
  $CXX $CXXFLAGS -c $src -o $obj || { echo "Compilation failed for $src"; exit 1; }
  OBJS="$OBJS $obj"
done

ar rcs $BUILD_DIR/libserviettUI.a $OBJS || { echo "Archiving failed"; exit 1; }

if [ ! -w "$INSTALL_LIB_DIR" ] || [ ! -w "$INSTALL_INCLUDE_DIR" ]; then
  echo "No write permission to $INSTALL_LIB_DIR or $INSTALL_INCLUDE_DIR. Run with sudo."
  exit 1
fi

cp $BUILD_DIR/libserviettUI.a $INSTALL_LIB_DIR/
cp $INCLUDE_DIR/serviettUI.h $INSTALL_INCLUDE_DIR/

echo "Library installed successfully:"
echo " - $INSTALL_LIB_DIR/libserviettUI.a"
echo " - $INSTALL_INCLUDE_DIR/serviettUI.h"
