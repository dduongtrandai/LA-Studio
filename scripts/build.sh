#!/usr/bin/env bash
#
# Configure and build LA Studio on macOS using CMake presets.
# Portable build entrypoint for local development.

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

require_command cmake
require_command ninja
require_command git

resolve_qt_root() {
  if [[ -n "$QT_ROOT" && -f "$QT_ROOT/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
    echo "$QT_ROOT"
    return
  fi
  if [[ -n "${LA_QT:-}" && -f "${LA_QT}/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
    echo "$LA_QT"
    return
  fi
  if command -v brew >/dev/null 2>&1; then
    local brew_qt
    brew_qt="$(brew --prefix qt 2>/dev/null || true)"
    if [[ -n "$brew_qt" && -f "$brew_qt/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
      echo "$brew_qt"
      return
    fi
  fi
  echo ""
}

resolve_vcpkg_root() {
  if [[ -n "$VCPKG_ROOT_ARG" ]]; then
    echo "$VCPKG_ROOT_ARG"
    return
  fi
  if [[ -n "${VCPKG_ROOT:-}" && -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]]; then
    echo "$VCPKG_ROOT"
    return
  fi
  for candidate in "$REPO_ROOT/.deps/vcpkg" "$HOME/vcpkg"; do
    if [[ -f "$candidate/scripts/buildsystems/vcpkg.cmake" ]]; then
      echo "$candidate"
      return
    fi
  done
  echo ""
}

get_source_app_version() {
  sed -nE 's/set\(LASTUDIO_VERSION "([^"]+)".*/\1/p' "$REPO_ROOT/CMakeLists.txt" | head -1
}

normalize_app_version() {
  local value="$1"
  if [[ -z "$value" ]]; then
    value="$(get_source_app_version)"
  fi
  value="${value#v}"
  if [[ ! "$value" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Version must use MAJOR.MINOR.PATCH format; got '$value'" >&2
    exit 1
  fi
  echo "$value"
}

RESOLVED_QT_ROOT="$(resolve_qt_root)"
if [[ -z "$RESOLVED_QT_ROOT" ]]; then
  echo "Qt root not found. Pass --qt-root, set LA_QT, or 'brew install qt'." >&2
  exit 1
fi

RESOLVED_VCPKG_ROOT="$(resolve_vcpkg_root)"
if [[ -z "$RESOLVED_VCPKG_ROOT" ]]; then
  echo "vcpkg root not found. Pass --vcpkg-root or set VCPKG_ROOT." >&2
  exit 1
fi

TOOLCHAIN_FILE="$RESOLVED_VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
RESOLVED_VERSION="$(normalize_app_version "$VERSION")"

# Isolate this build from machine-level vcpkg overlays/triplets.
export VCPKG_ROOT="$RESOLVED_VCPKG_ROOT"
if [[ -n "${VCPKG_OVERLAY_TRIPLETS:-}" ]]; then
  echo "Warning: ignoring VCPKG_OVERLAY_TRIPLETS from environment for reproducible build." >&2
fi
if [[ -n "${VCPKG_DEFAULT_TRIPLET:-}" ]]; then
  echo "Warning: ignoring VCPKG_DEFAULT_TRIPLET from environment for reproducible build." >&2
fi
unset VCPKG_OVERLAY_TRIPLETS
unset VCPKG_DEFAULT_TRIPLET

ARCH="$(uname -m)"
if [[ "$ARCH" == "arm64" ]]; then
  VCPKG_TRIPLET="arm64-osx"
else
  VCPKG_TRIPLET="x64-osx"
fi

BUILD_DIR="$REPO_ROOT/out/build/$PRESET"
if [[ "$CLEAN" -eq 1 && -d "$BUILD_DIR" ]]; then
  rm -rf "$BUILD_DIR"
fi

echo ">> Qt root: $RESOLVED_QT_ROOT"
echo ">> vcpkg root: $RESOLVED_VCPKG_ROOT"
echo ">> vcpkg triplet: $VCPKG_TRIPLET"
echo ">> Preset: $PRESET"

echo ">> Configuring CMake preset: $PRESET"
cmake --preset "$PRESET" \
  -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" \
  -DCMAKE_PREFIX_PATH="$RESOLVED_QT_ROOT" \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
  -DVCPKG_ROOT="$RESOLVED_VCPKG_ROOT" \
  -DVCPKG_TARGET_TRIPLET="$VCPKG_TRIPLET" \
  -DLASTUDIO_VERSION="$RESOLVED_VERSION"

echo ">> Building CMake preset: $PRESET"
cmake --build --preset "$PRESET" --parallel

APP_BUNDLE="$BUILD_DIR/LA Studio.app"
if [[ ! -d "$APP_BUNDLE" ]]; then
  echo "Build completed but app bundle was not found: $APP_BUNDLE" >&2
  exit 1
fi

if [[ "$SKIP_DEPLOY" -eq 0 ]]; then
  MACDEPLOYQT="$RESOLVED_QT_ROOT/bin/macdeployqt"
  if [[ -x "$MACDEPLOYQT" ]]; then
    echo ">> Running macdeployqt"
    "$MACDEPLOYQT" "$APP_BUNDLE" -qmldir="$REPO_ROOT/qml"
  else
    echo "Warning: macdeployqt not found at $MACDEPLOYQT. Skipping deployment." >&2
  fi
fi

echo "[SUCCESS] Build output: $APP_BUNDLE"
