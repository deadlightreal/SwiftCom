cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=DEBUG -DCMAKE_TOOLCHAIN_FILE=/Users/dev/vcpkg/scripts/buildsystems/vcpkg.cmake .

make -j8
sudo ./output/main
