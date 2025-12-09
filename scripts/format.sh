#!/bin/sh -e

BASEDIR=$(realpath "$(dirname "$0")")
ROOTDIR=$(realpath "$BASEDIR/..")

MODE="${1:-format}"

run_clang_format() {
  ACTION=$1
  shift

  if [ "$ACTION" = "check" ]; then
    find "$@" \( -name '*.cpp' -o -name '*.h' \) -print0 \
      | xargs -0 clang-format --dry-run --Werror
  else
    find "$@" \( -name '*.cpp' -o -name '*.h' \) -print0 \
      | xargs -0 clang-format -i
  fi
}

case "$MODE" in
  --format|format)
    # Format only this project's sources in-place.
    run_clang_format format "$ROOTDIR/src"
    ;;
  --check)
    # Check only this project's sources.
    run_clang_format check "$ROOTDIR/src"
    ;;
  *)
    echo "Usage: $0 [--check]" >&2
    echo "  no args or --format: format sources" >&2
    echo "  --check: verify formatting only" >&2
    exit 1
    ;;
esac
