cmake -B build_x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 -DVCPKG_TARGET_TRIPLET=x64-osx
cmake -B build_arm64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DVCPKG_TARGET_TRIPLET=arm64-osx
cd build_x64
cmake --build .
cd ..
cd build_arm64
cmake --build .
cd ..
mkdir build
lipo -create build_arm64/libafv_native.dylib build_x64/libafv_native.dylib -output build/libafv_native.dylib
