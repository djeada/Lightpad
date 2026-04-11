#!/usr/bin/env bash
set -euo pipefail

SCRIPT_NAME="$(basename "$0")"
DRY_RUN=0

usage() {
  cat <<EOF
Usage: $SCRIPT_NAME [--dry-run] [--help]

Install the default Lightpad development dependencies.

Supported platforms:
  - Ubuntu/Debian via apt
  - macOS via Homebrew

Options:
  --dry-run   Print the commands without executing them
  --help      Show this help
EOF
}

run() {
  if (( DRY_RUN )); then
    printf '[dry-run] %q' "$1"
    shift
    for arg in "$@"; do
      printf ' %q' "$arg"
    done
    printf '\n'
    return 0
  fi

  "$@"
}

while (($#)); do
  case "$1" in
    --dry-run)
      DRY_RUN=1
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
  shift
done

if [[ "$(uname -s)" == "Linux" ]] && command -v apt-get >/dev/null 2>&1; then
  packages=(
    build-essential
    cmake
    ninja-build
    pkg-config
    qt6-base-dev
    qt6-base-dev-tools
    qt6-tools-dev-tools
    qt6-pdf-dev
    qt6-webengine-dev
  )

  echo "Installing Lightpad dependencies with apt..."
  run sudo apt-get update
  run sudo apt-get install -y "${packages[@]}"
  exit 0
fi

if [[ "$(uname -s)" == "Darwin" ]]; then
  if ! command -v brew >/dev/null 2>&1; then
    echo "Homebrew is required on macOS. Install it from https://brew.sh/ and re-run this script." >&2
    exit 1
  fi

  packages=(
    cmake
    ninja
    pkg-config
    qt
  )

  echo "Installing Lightpad dependencies with Homebrew..."
  run brew install "${packages[@]}"
  exit 0
fi

cat >&2 <<EOF
Unsupported platform for automatic dependency installation.

Install these tools manually:
  - C++17 compiler
  - CMake 3.16+
  - pkg-config
  - Ninja (recommended)
  - Qt 6 development packages
  - Optional: Qt6Pdf / Qt6PdfWidgets / Qt6WebEngineWidgets
EOF
exit 1
