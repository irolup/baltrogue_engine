#!/bin/bash
set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

echo "Installing PulseAudio development libraries..."
sudo apt-get update
sudo apt-get install -y libpulse-dev

echo "Rebuilding OpenAL Soft..."
cd vendor/openal-soft
rm -rf build_linux install_linux
mkdir -p build_linux
cd build_linux

echo "Configuring OpenAL Soft with CMake..."
cmake .. -DCMAKE_INSTALL_PREFIX=../install_linux

echo "Building OpenAL Soft..."
cmake --build . -j$(nproc)

echo "Installing OpenAL Soft..."
cmake --install .

echo "OpenAL Soft rebuilt successfully!"
echo "Available backends:"
grep "ALSOFT_BACKEND" CMakeCache.txt | grep "BOOL=ON"
