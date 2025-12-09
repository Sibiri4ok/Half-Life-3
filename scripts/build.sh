#!/bin/sh -e

BUILD_TYPE=Debug
JOBS=${JOBS:-$(command -v nproc >/dev/null 2>&1 && nproc || echo 4)}

if [ "$1" = "--release" ]; then
  BUILD_TYPE=Release
  shift
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build build --config "$BUILD_TYPE" -j"$JOBS"
