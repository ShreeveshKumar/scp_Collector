#!/usr/bin/env bash
# Build (if needed) and run a single discovery pass locally.
# Reads configuration from .env automatically. Usage: ./scripts/run_once.sh
set -euo pipefail
cd "$(dirname "$0")/.."

# Load .env if present (export every variable it defines).
if [ -f .env ]; then
  set -a; . ./.env; set +a
fi

: "${MONGODB_URI:?set MONGODB_URI in .env, e.g. mongodb://localhost:27017 or an Atlas SRV string}"

# On macOS, let the binary find the Homebrew-installed mongocxx/curl dylibs.
if [ -d /opt/homebrew/lib ]; then
  export DYLD_FALLBACK_LIBRARY_PATH="/opt/homebrew/lib:${DYLD_FALLBACK_LIBRARY_PATH:-}"
fi

if [ ! -x build/sia ]; then
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build --parallel
fi

exec ./build/sia --once --log-level "${SIA_LOG_LEVEL:-info}"
