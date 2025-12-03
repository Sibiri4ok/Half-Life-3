#!/bin/sh -e

BUILD_TYPE=Debug

if [ "$1" = "--release" ]; then
  BUILD_TYPE=Release
  shift
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build build --config "$BUILD_TYPE"
