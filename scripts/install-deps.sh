#!/bin/bash
# OpenLock - Dependency Installation Script
# Supports Ubuntu/Debian and Fedora/RHEL

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

if [ "$EUID" -ne 0 ]; then
    error "Please run as root: sudo $0"
fi

if command -v apt-get &>/dev/null; then
    info "Detected Debian/Ubuntu-based system"

    apt-get update

    apt-get install -y \
        build-essential \
        cmake \
        pkg-config \
        git \
        qt6-base-dev \
        qt6-webengine-dev \
        qt6-webengine-dev-tools \
        libqt6webenginewidgets6 \
        libqt6webenginecore6 \
        libgl1-mesa-dev \
        libx11-dev \
        libxext-dev \
        libxrandr-dev \
        libxinerama-dev \
        libxcb1-dev \
        libxcb-keysyms1-dev \
        libxcb-xfixes0-dev \
        libxcb-randr0-dev \
        libxkbcommon-dev \
        libssl-dev \
        libcap-dev \
        nlohmann-json3-dev \
        libgtest-dev \
        cage

    info "All dependencies installed successfully (apt)"

elif command -v dnf &>/dev/null; then
    info "Detected Fedora/RHEL-based system"

    dnf install -y \
        gcc-c++ \
        cmake \
        pkg-config \
        git \
        qt6-qtbase-devel \
        qt6-qtwebengine-devel \
        mesa-libGL-devel \
        libX11-devel \
        libXext-devel \
        libXrandr-devel \
        libXinerama-devel \
        libxcb-devel \
        libxkbcommon-devel \
        openssl-devel \
        libcap-devel \
        json-devel \
        gtest-devel \
        cage

    info "All dependencies installed successfully (dnf)"

elif command -v pacman &>/dev/null; then
    info "Detected Arch-based system"

    pacman -Syu --noconfirm \
        base-devel \
        cmake \
        pkgconf \
        git \
        qt6-base \
        qt6-webengine \
        mesa \
        libx11 \
        libxext \
        libxrandr \
        libxinerama \
        libxcb \
        libxkbcommon \
        openssl \
        libcap \
        nlohmann-json \
        gtest \
        cage

    info "All dependencies installed successfully (pacman)"

else
    error "Unsupported package manager. Install dependencies manually."
fi

info "OpenLock dependencies installed. Run 'cmake -B build && cmake --build build' to build."
