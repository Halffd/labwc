#!/bin/bash
# labwc build/install script

set -e

BUILD_DIR="${BUILD_DIR:-build}"
INSTALL="${INSTALL:-meson install}"

usage() {
    echo "Usage: $0 [build|install|rebuild|clean|distclean]"
    echo ""
    echo "Commands:"
    echo "  build      - Configure and build (default if no args)"
    echo "  install    - Install after building"
    echo "  rebuild    - Clean and rebuild"
    echo "  clean      - Remove build directory"
    echo "  distclean  - Full clean (remove build and reconfigure)"
    echo ""
    echo "Environment variables:"
    echo "  BUILD_DIR  - Build directory (default: build)"
    echo "  INSTALL    - Install command (default: meson install)"
}

case "${1:-build}" in
    build)
        if [ ! -d "$BUILD_DIR" ]; then
            meson setup "$BUILD_DIR"
        fi
        meson compile -C "$BUILD_DIR"
        ;;
    install)
        meson install -C "$BUILD_DIR"
        ;;
    rebuild)
        rm -rf "$BUILD_DIR"
        meson setup "$BUILD_DIR"
        meson compile -C "$BUILD_DIR"
        ;;
    clean)
        rm -rf "$BUILD_DIR"
        ;;
    distclean)
        rm -rf "$BUILD_DIR"
        meson setup "$BUILD_DIR"
        ;;
    help|--help|-h)
        usage
        ;;
    *)
        echo "Unknown command: $1"
        usage
        exit 1
        ;;
esac