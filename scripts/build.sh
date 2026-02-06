#!/bin/bash
# OpenLock Build Script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

echo "Building OpenLock..."
echo "Project: $PROJECT_DIR"
echo "Build:   $BUILD_DIR"

# Configure
cmake -B "$BUILD_DIR" -S "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE="${1:-Debug}" \
    -DOPENLOCK_BUILD_TESTS=ON

# Build
cmake --build "$BUILD_DIR" -j "$(nproc)"

echo ""
echo "Build complete!"
echo "Binary: $BUILD_DIR/openlock"

if [ "${1}" = "test" ] || [ "${2}" = "test" ]; then
    echo ""
    echo "Running tests..."
    cd "$BUILD_DIR" && ctest --output-on-failure
fi
