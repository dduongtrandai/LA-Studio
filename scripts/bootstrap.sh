#!/usr/bin/env bash
#
# One-command bootstrap for building LA Studio on macOS.
# Validates required tools, resolves Qt/vcpkg paths, initializes submodules,
# and calls scripts/build.sh.

set -euo pipefail

PRESET="macos-release"
QT_ROOT=""
VCPKG_ROOT_ARG=""
VERSION=""
CLEAN=0
SKIP_DEPLOY=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --preset) PRESET="$2"; shift 2 ;;
    --qt-root) QT_ROOT="$2"; shift 2 ;;
    --vcpkg-root) VCPKG_ROOT_ARG="$2"; shift 2 ;;
    --version) VERSION="$2"; shift 2 ;;
    --clean) CLEAN=1; shift ;;
    --skip-deploy) SKIP_DEPLOY=1; shift ;;
    *) echo "Unknown argument: $1" >&2; exit 1 ;;
  esac
done

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Required command not found: $1" >&2
    exit 1
  fi
}

require_command git
require_command cmake
require_command ninja

if [[ -z "$QT_ROOT" ]]; then
  if [[ -n "${LA_QT:-}" ]]; then
    QT_ROOT="$LA_QT"
  elif command -v brew >/dev/null 2>&1; then
    QT_ROOT="$(brew --prefix qt 2>/dev/null || true)"
  fi
fi
if [[ -z "$QT_ROOT" || ! -f "$QT_ROOT/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
  echo "Qt root not detected. Pass --qt-root <path>, e.g. --qt-root \"\$(brew --prefix qt)\"." >&2
  exit 1
fi

LOCAL_VCPKG="$REPO_ROOT/.deps/vcpkg"
if [[ -z "$VCPKG_ROOT_ARG" ]]; then
  if [[ -n "${VCPKG_ROOT:-}" && -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]]; then
    VCPKG_ROOT_ARG="$VCPKG_ROOT"
  elif [[ -f "$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake" ]]; then
    VCPKG_ROOT_ARG="$HOME/vcpkg"
  elif [[ -f "$LOCAL_VCPKG/scripts/buildsystems/vcpkg.cmake" ]]; then
    VCPKG_ROOT_ARG="$LOCAL_VCPKG"
  else
    echo ">> Cloning vcpkg to $LOCAL_VCPKG"
    mkdir -p "$(dirname "$LOCAL_VCPKG")"
    git clone https://github.com/microsoft/vcpkg.git "$LOCAL_VCPKG"
    echo ">> Bootstrapping vcpkg"
    "$LOCAL_VCPKG/bootstrap-vcpkg.sh"
    VCPKG_ROOT_ARG="$LOCAL_VCPKG"
  fi
fi

echo ">> Initializing Git submodules"
git submodule update --init --recursive

echo ">> Qt root: $QT_ROOT"
echo ">> vcpkg root: $VCPKG_ROOT_ARG"
echo ">> Preset: $PRESET"

BUILD_ARGS=(--preset "$PRESET" --qt-root "$QT_ROOT" --vcpkg-root "$VCPKG_ROOT_ARG" --version "$VERSION")
if [[ "$CLEAN" -eq 1 ]]; then
  BUILD_ARGS+=(--clean)
fi
if [[ "$SKIP_DEPLOY" -eq 1 ]]; then
  BUILD_ARGS+=(--skip-deploy)
fi

"$REPO_ROOT/scripts/build.sh" "${BUILD_ARGS[@]}"
