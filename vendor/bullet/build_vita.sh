#!/bin/bash

# Build Bullet Physics for PS Vita
# This script cross-compiles Bullet using VitaSDK

set -e

# Check if VitaSDK is set
if [ -z "$VITASDK" ]; then
    echo "Error: VITASDK environment variable is not set!"
    echo "Please set VITASDK to your VitaSDK installation path"
    exit 1
fi

echo "Building Bullet Physics for PS Vita..."
echo "VitaSDK path: $VITASDK"

# Create build directory
BUILD_DIR="build_vita"
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Configure with CMake for Vita - MINIMAL BUILD
echo "Configuring CMake for Vita (minimal build)..."
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DUSE_DOUBLE_PRECISION=OFF \
    -DUSE_GRAPHICAL_BENCHMARK=OFF \
    -DUSE_SOFT_BODY_MULTI_BODY_DYNAMICS_WORLD=OFF \
    -DUSE_OPENVR=OFF \
    -DENABLE_VHACD=OFF \
    -DBULLET2_MULTITHREADING=OFF \
    -DBUILD_EXTRAS=OFF \
    -DBUILD_UNIT_TESTS=OFF \
    -DBUILD_OPENGL3_DEMOS=OFF \
    -DBUILD_BULLET2_DEMOS=OFF \
    -DBUILD_BULLET3=OFF \
    -DBUILD_CPU_DEMOS=OFF \
    -DBUILD_CLSOCKET=OFF \
    -DBUILD_ENET=OFF \
    -DBUILD_HACD=OFF \
    -DBUILD_INVERSE_DYNAMICS=OFF \
    -DBUILD_MINICL_OPENCL_DEMOS=OFF \
    -DBUILD_OPENCL_DEMOS=OFF \
    -DBUILD_PYBULLET=OFF \
    -DBUILD_PYBULLET_NUMPY=OFF \
    -DBUILD_SERIALIZE=OFF \
    -DBUILD_SOFT_BODY_DEMOS=OFF \
    -DBUILD_TWO_DEMOS=OFF \
    -DBUILD_VR_DEMOS=OFF \
    -DBUILD_SOFT_BODY=OFF \
    -DBUILD_HEIGHTFIELD_TERRAIN=OFF \
    -DBUILD_MULTITHREADING=OFF \
    -DBUILD_MLCPSOLVERS=OFF \
    -DBUILD_FEATHERSTONE=OFF \
    -DBUILD_VEHICLE=OFF \
    -DBUILD_CHARACTER=OFF \
    -DBUILD_DYNAMICS=OFF \
    -DBUILD_CONSTRAINTS=OFF \
    -DCMAKE_INSTALL_PREFIX=../lib_vita \
    -DCMAKE_CXX_FLAGS="-fno-exceptions -std=gnu++11 -fpermissive -ffast-math" \
    -DCMAKE_C_FLAGS="-ffast-math"

# Build
echo "Building Bullet for Vita..."
make -j$(nproc)

# Install to lib_vita directory
echo "Installing to lib_vita directory..."
make install

echo "Build completed successfully!"
echo "Vita-compatible libraries are in: vendor/bullet/lib_vita"
