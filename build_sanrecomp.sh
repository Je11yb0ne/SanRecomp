#!/bin/bash
# SanRecomp Windows Build Script
# Run from the project root directory

export VCPKG_ROOT="$(pwd)/thirdparty/vcpkg"
export PATH="/c/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin:/c/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja:$PATH"

CMAKE="/c/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
BUILD_DIR="$(pwd)/out/build/x64-Clang-RelWithDebInfo"

echo "=== Configuring CMake ==="
"$CMAKE" -S "$(pwd)" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_C_COMPILER=clang-cl.exe \
  -DCMAKE_CXX_COMPILER=clang-cl.exe \
  -DCMAKE_LINKER=lld-link.exe \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  "-DCMAKE_LIBRARY_PATH=C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64"

if [ $? -ne 0 ]; then
    echo "CMake configure failed!"
    exit 1
fi

echo ""
echo "=== Building SanRecompLib (PPC Library) ==="
"$CMAKE" --build "$BUILD_DIR" --target SanRecompLib -j$(nproc)

if [ $? -ne 0 ]; then
    echo "SanRecompLib build failed!"
    exit 1
fi

echo ""
echo "=== Building SanRecomp ==="
"$CMAKE" --build "$BUILD_DIR" --target SanRecomp -j$(nproc)

if [ $? -ne 0 ]; then
    echo "SanRecomp build has errors - see above for details"
else
    echo ""
    echo "=== BUILD SUCCESS ==="
    echo "Output: $BUILD_DIR/SanRecomp/SanRecomp.exe"
fi
