#!/bin/bash
mkdir cbuildKindle
cd buildKindle
cmake -G "Ninja" -DCMAKE_INSTALL_PREFIX=dist -DWITH_SDL=off  -DCMAKE_TOOLCHAIN_FILE=/home/vagrant/dev/buildroot/buildroot-2016.11.1/output/host/usr/share/buildroot/toolchainfile.cmake ..
cd ..
# -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS_DEBUG="-g3 -gdwarf-2" -DCMAKE_CXX_FLAGS_DEBUG="-g3 -gdwarf-2" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DWITH_SDL=off  -DCMAKE_TOOLCHAIN_FILE=/home/vagrant/dev/buildroot/buildroot-2016.11.1/output/host/usr/share/buildroot/toolchainfile.cmake ..